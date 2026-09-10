#include "FftDisplayItem.h"

#include "SpectrumCudaWorker.h"
#include "websocketclient.h"
#include "CrashDiagnostics.h"

#include <QDebug>
#include <QImage>
#include <QLinearGradient>
#include <QMetaType>
#include <QMutexLocker>
#include <QPainter>
#include <QPen>
#include <QPolygonF>
#include <QtGlobal>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

namespace {
bool sameReal(double a, double b)
{
    return qFuzzyCompare(a + 1.0, b + 1.0);
}

int envPositiveIntLocal(const char *name, int defaultValue, int maxValue)
{
    bool ok = false;
    const int parsed = qgetenv(name).trimmed().toInt(&ok);
    if (!ok || parsed <= 0)
        return defaultValue;
    return qBound(1, parsed, maxValue);
}

qint64 monotonicMs()
{
    using Clock = std::chrono::steady_clock;
    return std::chrono::duration_cast<std::chrono::milliseconds>(
                Clock::now().time_since_epoch()).count();
}
}

FftDisplayItem::FftDisplayItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(false);
    setOpaquePainting(false);

#ifdef PLATFORM_JETSON
    // Keep the QPainter-facing API/visual output unchanged, but let Qt Quick's
    // OpenGL backend paint into an FBO by default on Jetson. Can be disabled
    // for A/B diagnostics with ISCAN_SPECTRUM_FBO=0.
    const QByteArray fboOverride = qgetenv("ISCAN_SPECTRUM_FBO").trimmed().toLower();
    const bool useFbo = !(fboOverride == "0" || fboOverride == "off" || fboOverride == "false");
    setRenderTarget(useFbo ? QQuickPaintedItem::FramebufferObject
                           : QQuickPaintedItem::Image);
#else
    setRenderTarget(QQuickPaintedItem::Image);
#endif

    // CUDA1.1/FPS60: decouple Spectrum presentation from AstraRX frame
    // cadence. 16 ms targets a 60 Hz display; Qt Quick/vsync may coalesce
    // redundant updates, so this does not busy-loop the GUI thread.
    m_presentIntervalMs = envPositiveIntLocal("ISCAN_SPECTRUM_PRESENT_MS", 16, 100);
    m_waterfallHistoryMaxBins = envPositiveIntLocal("ISCAN_WATERFALL_DISPLAY_BINS", 2048, 8192);
    m_presentTimer.setInterval(m_presentIntervalMs);
    m_presentTimer.setTimerType(Qt::PreciseTimer);
    m_presentTimer.setSingleShot(false);
    connect(&m_presentTimer, &QTimer::timeout, this, [this]() {
        bool present = false;
        {
            QMutexLocker locker(&m_dataMutex);
            present = m_renderEnabled
                    && ((m_mode == Spectrum && !m_spectrumFrame.isEmpty())
                        || (m_mode == Waterfall && !m_waterfallPaused && m_waterfallValidRows > 0));
        }
        if (present)
            update();
    });

    m_palette = {
        QColor(QStringLiteral("#030712")),
        QColor(QStringLiteral("#071B4D")),
        QColor(QStringLiteral("#0A3B92")),
        QColor(QStringLiteral("#0D72D8")),
        QColor(QStringLiteral("#12B6E8")),
        QColor(QStringLiteral("#28D6C0")),
        QColor(QStringLiteral("#54DC7B")),
        QColor(QStringLiteral("#A6E34B")),
        QColor(QStringLiteral("#F0DE43")),
        QColor(QStringLiteral("#F6A53A")),
        QColor(QStringLiteral("#EF5B36")),
        QColor(QStringLiteral("#E82F37")),
        QColor(QStringLiteral("#FFF2D0"))
    };

    rebuildPaletteLutLocked();
    m_measurementTimer.start();
    m_computeStatsTimer.start();
    m_recolorDebounceTimer.setSingleShot(true);
    m_recolorDebounceTimer.setInterval(12);
    connect(&m_recolorDebounceTimer, &QTimer::timeout,
            this, &FftDisplayItem::requestHistoryRecolor);
    // CUDA worker is started lazily by the Waterfall item on its first real
    // frame. The Spectrum item no longer creates an unused CUDA context/thread.
    refreshPresentationClock();

    qInfo() << "[R20.4-SPECTRUM-CUDA1.1-FPS60]"
            << "asyncComputeWorker=1"
            << "waterfallRingBuffer=1"
            << "cudaPeakPoolPalette=1"
            << "cudaHistoryRecolor=1"
            << "lazyCudaWorker=1"
            << "targetPresentMs=" << m_presentIntervalMs
            << "waterfallGpuBins=" << m_waterfallHistoryMaxBins
#ifdef PLATFORM_JETSON
            << "paintTarget=" << (renderTarget() == QQuickPaintedItem::FramebufferObject ? "fbo" : "image")
#else
            << "paintTarget=image"
#endif
            ;
}

FftDisplayItem::~FftDisplayItem()
{
    disconnectBackend();
    stopComputeWorker();
}

void FftDisplayItem::startComputeWorker()
{
    if (m_computeThread.isRunning())
        return;

    qRegisterMetaType<QVector<float>>("QVector<float>");
    qRegisterMetaType<QVector<quint32>>("QVector<quint32>");

    SpectrumCudaWorker *worker = new SpectrumCudaWorker();
    m_computeWorker = worker;
    worker->moveToThread(&m_computeThread);

    connect(&m_computeThread, &QThread::started,
            worker, &SpectrumCudaWorker::initialize);
    connect(&m_computeThread, &QThread::finished,
            worker, &QObject::deleteLater);

    connect(this, &FftDisplayItem::processWaterfallRequested,
            worker, &SpectrumCudaWorker::processWaterfallRow,
            Qt::QueuedConnection);
    connect(this, &FftDisplayItem::recolorHistoryRequested,
            worker, &SpectrumCudaWorker::recolorHistory,
            Qt::QueuedConnection);

    connect(worker, &SpectrumCudaWorker::backendReady,
            this, &FftDisplayItem::onComputeBackendReady,
            Qt::QueuedConnection);
    connect(worker, &SpectrumCudaWorker::waterfallRowReady,
            this, &FftDisplayItem::onWaterfallRowReady,
            Qt::QueuedConnection);
    connect(worker, &SpectrumCudaWorker::historyRecolorReady,
            this, &FftDisplayItem::onHistoryRecolorReady,
            Qt::QueuedConnection);

    m_computeThread.setObjectName(QStringLiteral("SpectrumCudaWorker"));
    m_computeThread.start();
}

void FftDisplayItem::stopComputeWorker()
{
    if (!m_computeThread.isRunning())
        return;

    if (m_computeWorker) {
        disconnect(this, nullptr, m_computeWorker.data(), nullptr);
        disconnect(m_computeWorker.data(), nullptr, this, nullptr);
    }

    m_computeThread.quit();
    if (!m_computeThread.wait(5000)) {
        // Never QThread::terminate(): preserving resource ownership is more
        // important than a forced shutdown. CUDA kernels here are bounded and
        // normally complete in sub-millisecond/millisecond time.
        qWarning() << "[SPECTRUM-CUDA] worker shutdown exceeded 5s; waiting safely";
        m_computeThread.wait();
    }
    m_computeWorker.clear();
}

QString FftDisplayItem::computeBackend() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_computeBackend;
}

QObject *FftDisplayItem::backend() const noexcept
{
    return m_backend.data();
}

void FftDisplayItem::disconnectBackend()
{
    for (const QMetaObject::Connection &connection : m_backendConnections)
        QObject::disconnect(connection);
    m_backendConnections.clear();
    m_backend.clear();
}

void FftDisplayItem::setBackend(QObject *backendObject)
{
    WebSocketClient *client = qobject_cast<WebSocketClient *>(backendObject);
    if (m_backend.data() == client)
        return;

    disconnectBackend();
    m_backend = client;

    if (m_backend) {
        m_backendConnections.append(connect(m_backend.data(), &WebSocketClient::spectrumDisplayFrame,
                                            this, &FftDisplayItem::onSpectrumFrame,
                                            Qt::DirectConnection));
        m_backendConnections.append(connect(m_backend.data(), &WebSocketClient::waterfallDisplayFrame,
                                            this, &FftDisplayItem::onWaterfallFrame,
                                            Qt::DirectConnection));
        m_backendConnections.append(connect(m_backend.data(), &WebSocketClient::maxHoldDisplayFrame,
                                            this, &FftDisplayItem::onMaxHoldFrame,
                                            Qt::DirectConnection));
        m_backendConnections.append(connect(m_backend.data(), &QObject::destroyed,
                                            this, [this]() {
            m_backendConnections.clear();
            m_backend.clear();
            {
                QMutexLocker locker(&m_dataMutex);
                m_spectrumFrame.clear();
                m_previousSpectrumFrame.clear();
                m_maxHoldFrame.clear();
                clearHistoryLocked();
                m_measurementsValid = false;
            }
            emit measurementsChanged();
            update();
        }));
    }

    emit backendChanged();
}

void FftDisplayItem::setMode(Mode mode)
{
    {
        QMutexLocker locker(&m_dataMutex);
        if (m_mode == mode)
            return;
        m_mode = mode;
    }
    emit modeChanged();
    refreshPresentationClock();
    update();
}

void FftDisplayItem::setRenderEnabled(bool enabled)
{
    {
        QMutexLocker locker(&m_dataMutex);
        if (m_renderEnabled == enabled)
            return;
        m_renderEnabled = enabled;
        if (!enabled) {
            m_pendingWaterfallFrame.clear();
            ++m_colorGeneration; // invalidate any row currently in flight
        }
    }
    emit renderEnabledChanged();
    refreshPresentationClock();
    update();
}

void FftDisplayItem::refreshPresentationClock()
{
    bool shouldRun = false;
    {
        QMutexLocker locker(&m_dataMutex);
        shouldRun = m_renderEnabled && (m_mode == Spectrum || !m_waterfallPaused);
    }

    if (shouldRun) {
        if (!m_presentTimer.isActive())
            m_presentTimer.start();
    } else {
        m_presentTimer.stop();
    }
}

void FftDisplayItem::setShowMaxHold(bool enabled)
{
    {
        QMutexLocker locker(&m_dataMutex);
        if (m_showMaxHold == enabled)
            return;
        m_showMaxHold = enabled;
    }
    emit showMaxHoldChanged();
    update();
}

void FftDisplayItem::setClearBeforeNextPaint(bool clear)
{
    bool cleared = false;
    {
        QMutexLocker locker(&m_dataMutex);
        if (m_clearBeforeNextPaint == clear)
            return;
        m_clearBeforeNextPaint = clear;
        if (clear && m_mode == Waterfall) {
            clearHistoryLocked();
            m_clearBeforeNextPaint = false;
            cleared = true;
        }
    }
    emit clearBeforeNextPaintChanged();
    if (cleared)
        update();
}

void FftDisplayItem::setWaterfallPaused(bool paused)
{
    {
        QMutexLocker locker(&m_dataMutex);
        if (m_waterfallPaused == paused)
            return;
        m_waterfallPaused = paused;
        if (paused) {
            m_pendingWaterfallFrame.clear();
            ++m_colorGeneration; // stale in-flight row must not appear after Pause
        }
    }
    emit waterfallPausedChanged();
    refreshPresentationClock();
    update();
}

void FftDisplayItem::setMinDb(double value)
{
    if (!std::isfinite(value))
        return;
    bool recolor = false;
    {
        QMutexLocker locker(&m_dataMutex);
        if (sameReal(m_minDb, value))
            return;
        m_minDb = value;
        ++m_colorGeneration;
        recolor = m_mode == Waterfall && m_waterfallValidRows > 0;
        if (recolor)
            m_recolorRequested = true;
    }
    emit levelsChanged();
    if (recolor)
        m_recolorDebounceTimer.start();
    update();
}

void FftDisplayItem::setMaxDb(double value)
{
    if (!std::isfinite(value))
        return;
    bool recolor = false;
    {
        QMutexLocker locker(&m_dataMutex);
        if (sameReal(m_maxDb, value))
            return;
        m_maxDb = value;
        ++m_colorGeneration;
        recolor = m_mode == Waterfall && m_waterfallValidRows > 0;
        if (recolor)
            m_recolorRequested = true;
    }
    emit levelsChanged();
    if (recolor)
        m_recolorDebounceTimer.start();
    update();
}

void FftDisplayItem::setFullStartFreq(double value)
{
    if (!std::isfinite(value))
        return;

    bool clearEpoch = false;
    {
        QMutexLocker locker(&m_dataMutex);
        if (sameReal(m_fullStartFreq, value))
            return;
        clearEpoch = m_mode == Waterfall && m_waterfallHistoryWidth > 0;
        m_fullStartFreq = value;
        if (clearEpoch)
            clearHistoryLocked();
    }
    emit frequencyMappingChanged();
    update();
}

void FftDisplayItem::setViewStartFreq(double value)
{
    if (!std::isfinite(value))
        return;
    {
        QMutexLocker locker(&m_dataMutex);
        if (sameReal(m_viewStartFreq, value))
            return;
        m_viewStartFreq = value;
        if (m_mode == Spectrum && !m_spectrumFrame.isEmpty())
            updateMeasurementsLocked(m_spectrumFrame, true);
    }
    emit frequencyMappingChanged();
    if (m_mode == Spectrum)
        emit measurementsChanged();
    update();
}

void FftDisplayItem::setViewStopFreq(double value)
{
    if (!std::isfinite(value))
        return;
    {
        QMutexLocker locker(&m_dataMutex);
        if (sameReal(m_viewStopFreq, value))
            return;
        m_viewStopFreq = value;
        if (m_mode == Spectrum && !m_spectrumFrame.isEmpty())
            updateMeasurementsLocked(m_spectrumFrame, true);
    }
    emit frequencyMappingChanged();
    if (m_mode == Spectrum)
        emit measurementsChanged();
    update();
}

void FftDisplayItem::setSampleRate(double value)
{
    if (!std::isfinite(value) || value <= 0.0)
        return;

    bool clearEpoch = false;
    {
        QMutexLocker locker(&m_dataMutex);
        if (sameReal(m_sampleRate, value))
            return;
        clearEpoch = m_mode == Waterfall && m_waterfallHistoryWidth > 0;
        m_sampleRate = value;
        if (clearEpoch)
            clearHistoryLocked();
        if (m_mode == Spectrum && !m_spectrumFrame.isEmpty())
            updateMeasurementsLocked(m_spectrumFrame, true);
    }
    emit frequencyMappingChanged();
    if (m_mode == Spectrum)
        emit measurementsChanged();
    update();
}

void FftDisplayItem::setSpectrumColor(const QColor &color)
{
    if (!color.isValid())
        return;
    {
        QMutexLocker locker(&m_dataMutex);
        if (m_spectrumColor == color)
            return;
        m_spectrumColor = color;
    }
    emit colorsChanged();
    update();
}

void FftDisplayItem::setMaxHoldColor(const QColor &color)
{
    if (!color.isValid())
        return;
    {
        QMutexLocker locker(&m_dataMutex);
        if (m_maxHoldColor == color)
            return;
        m_maxHoldColor = color;
    }
    emit colorsChanged();
    update();
}

QVariantList FftDisplayItem::palette() const
{
    QMutexLocker locker(&m_dataMutex);
    QVariantList result;
    result.reserve(m_palette.size());
    for (const QColor &color : m_palette)
        result.append(color);
    return result;
}

void FftDisplayItem::setPalette(const QVariantList &colors)
{
    QVector<QColor> parsed;
    parsed.reserve(colors.size());
    for (const QVariant &entry : colors) {
        QColor color;
        if (entry.canConvert<QColor>())
            color = entry.value<QColor>();
        if (!color.isValid()) {
            bool ok = false;
            const quint32 packed = entry.toUInt(&ok);
            if (ok) {
                color = QColor(static_cast<int>((packed >> 16) & 0xffU),
                               static_cast<int>((packed >> 8) & 0xffU),
                               static_cast<int>(packed & 0xffU));
            }
        }
        if (color.isValid())
            parsed.append(color);
    }

    if (parsed.isEmpty())
        parsed.append(Qt::black);

    bool recolor = false;
    {
        QMutexLocker locker(&m_dataMutex);
        if (parsed == m_palette)
            return;
        m_palette = parsed;
        rebuildPaletteLutLocked();
        ++m_colorGeneration;
        recolor = m_mode == Waterfall && m_waterfallValidRows > 0;
        if (recolor)
            m_recolorRequested = true;
    }
    emit paletteChanged();
    if (recolor)
        m_recolorDebounceTimer.start();
    update();
}

void FftDisplayItem::requestPaint()
{
    update();
}

void FftDisplayItem::clearPeaks()
{
    {
        QMutexLocker locker(&m_dataMutex);
        m_maxHoldFrame.clear();
    }
    if (m_backend)
        m_backend->resetMaxHold();
    update();
}

void FftDisplayItem::clearHistoryLocked()
{
    std::fill(m_waterfallColorHistory.begin(), m_waterfallColorHistory.end(), 0U);
    std::fill(m_waterfallDbHistory.begin(), m_waterfallDbHistory.end(),
              static_cast<float>(m_minDb));
    m_waterfallHeadRow = 0;
    m_waterfallValidRows = 0;
    m_lastWaterfallArrivalMs = 0;
    m_waterfallSourcePeriodMs = 40.0;
    m_pendingWaterfallFrame.clear();
    m_recolorRequested = false;
    ++m_colorGeneration;
}

void FftDisplayItem::clearHistory()
{
    m_recolorDebounceTimer.stop();
    bool clearFlagChanged = false;
    {
        QMutexLocker locker(&m_dataMutex);
        clearHistoryLocked();
        if (m_clearBeforeNextPaint) {
            m_clearBeforeNextPaint = false;
            clearFlagChanged = true;
        }
    }
    if (clearFlagChanged)
        emit clearBeforeNextPaintChanged();
    update();
}

void FftDisplayItem::onSpectrumFrame(const QVector<float> &frame)
{
    CrashDiagnostics::checkpoint(CrashDiagnostics::CpNativeSpectrumFrame);
    if (frame.size() < 2)
        return;

    bool metricsChanged = false;
    const qint64 nowMs = monotonicMs();
    {
        QMutexLocker locker(&m_dataMutex);
        if (m_mode != Spectrum || !m_renderEnabled)
            return;

        if (!m_spectrumFrame.isEmpty() && m_spectrumFrame.size() == frame.size())
            m_previousSpectrumFrame = m_spectrumFrame;
        else
            m_previousSpectrumFrame = frame;

        if (m_lastSpectrumArrivalMs > 0) {
            const qint64 rawPeriod = nowMs - m_lastSpectrumArrivalMs;
            if (rawPeriod > 0 && rawPeriod < 500) {
                const double bounded = std::max(8.0, std::min(120.0, static_cast<double>(rawPeriod)));
                // Low-pass source period so occasional WebSocket jitter does
                // not make interpolation speed jump from frame to frame.
                m_spectrumSourcePeriodMs = 0.80 * m_spectrumSourcePeriodMs + 0.20 * bounded;
            }
        }
        m_lastSpectrumArrivalMs = nowMs;
        m_spectrumTransitionStartMs = nowMs;
        m_spectrumFrame = frame;

        if (!m_measurementsValid || !m_measurementTimer.isValid()
                || m_measurementTimer.elapsed() >= 200) {
            updateMeasurementsLocked(frame, true);
            m_measurementTimer.restart();
            metricsChanged = true;
        }
    }

    if (metricsChanged)
        emit measurementsChanged();
    // Presentation is clocked by m_presentTimer at ~60 Hz. Avoid issuing a
    // second immediate update for every source frame, which used to create
    // bursty render scheduling when WebSocket arrival jittered.
}

void FftDisplayItem::onWaterfallFrame(const QVector<float> &frame)
{
    CrashDiagnostics::checkpoint(CrashDiagnostics::CpNativeWaterfallFrame);
    submitWaterfallWork(frame);
}

void FftDisplayItem::onMaxHoldFrame(const QVector<float> &frame)
{
    bool repaint = false;
    {
        QMutexLocker locker(&m_dataMutex);
        if (m_mode != Spectrum || !m_renderEnabled)
            return;
        m_maxHoldFrame = frame;
        repaint = m_showMaxHold;
    }
    if (repaint)
        update();
}

void FftDisplayItem::onComputeBackendReady(QString backendName, bool cudaActive, QString detail)
{
    {
        QMutexLocker locker(&m_dataMutex);
        m_computeBackend = backendName;
        m_cudaAccelerationActive = cudaActive;
    }

    qInfo() << "[SPECTRUM-CUDA]"
            << "backend=" << backendName
            << "cudaActive=" << cudaActive
            << "detail=" << detail;
    emit computeBackendChanged();
}

void FftDisplayItem::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChanged(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size())
        update();
}

void FftDisplayItem::ensureWaterfallHistory(int sourceBins)
{
    if (sourceBins <= 0)
        return;

    const int width = std::max(1, std::min(m_waterfallHistoryMaxBins, sourceBins));
    const int total = width * m_waterfallHistoryRows;
    if (m_waterfallHistoryWidth == width
            && m_waterfallDbHistory.size() == total
            && m_waterfallColorHistory.size() == total)
        return;

    m_waterfallHistoryWidth = width;
    m_waterfallDbHistory.resize(total);
    m_waterfallColorHistory.resize(total);
    std::fill(m_waterfallDbHistory.begin(), m_waterfallDbHistory.end(),
              static_cast<float>(m_minDb));
    std::fill(m_waterfallColorHistory.begin(), m_waterfallColorHistory.end(), 0U);
    m_waterfallHeadRow = 0;
    m_waterfallValidRows = 0;
}

void FftDisplayItem::rebuildPaletteLutLocked()
{
    constexpr int kLutSize = 1024;
    m_paletteLut.resize(kLutSize);

    if (m_palette.isEmpty()) {
        std::fill(m_paletteLut.begin(), m_paletteLut.end(), qRgb(0, 0, 0));
        return;
    }

    if (m_palette.size() == 1) {
        std::fill(m_paletteLut.begin(), m_paletteLut.end(), m_palette.first().rgb());
        return;
    }

    for (int i = 0; i < kLutSize; ++i) {
        const double normalized = static_cast<double>(i) / static_cast<double>(kLutSize - 1);
        const double scaled = normalized * static_cast<double>(m_palette.size() - 1);
        const int lower = std::max(0, std::min(m_palette.size() - 1,
                                               static_cast<int>(std::floor(scaled))));
        const int upper = std::max(lower, std::min(m_palette.size() - 1, lower + 1));
        const double t = scaled - static_cast<double>(lower);
        const QColor &a = m_palette.at(lower);
        const QColor &b = m_palette.at(upper);
        const int r = static_cast<int>(std::lround(a.red() + (b.red() - a.red()) * t));
        const int g = static_cast<int>(std::lround(a.green() + (b.green() - a.green()) * t));
        const int bl = static_cast<int>(std::lround(a.blue() + (b.blue() - a.blue()) * t));
        m_paletteLut[i] = qRgb(r, g, bl);
    }
}

void FftDisplayItem::submitWaterfallWork(const QVector<float> &frame)
{
    if (frame.isEmpty())
        return;

    // Waterfall is the only current CUDA consumer. Start its worker/context
    // on demand instead of paying the cost for both Spectrum and Waterfall
    // QML items at startup.
    if (!m_computeThread.isRunning())
        startComputeWorker();

    quint64 generation = 0;
    int outputBins = 0;
    float minDb = 0.0f;
    float maxDb = 0.0f;
    QVector<quint32> palette;
    bool submit = false;

    {
        QMutexLocker locker(&m_dataMutex);
        if (m_mode != Waterfall || !m_renderEnabled || m_waterfallPaused)
            return;

        if (m_waterfallWorkBusy || m_recolorBusy) {
            m_pendingWaterfallFrame = frame; // implicit sharing; newest frame wins
            ++m_coalescedWaterfallFrames;
            return;
        }

        m_waterfallWorkBusy = true;
        generation = m_colorGeneration;
        outputBins = std::max(1, std::min(m_waterfallHistoryMaxBins, frame.size()));
        minDb = static_cast<float>(m_minDb);
        maxDb = static_cast<float>(m_maxDb);
        palette = m_paletteLut;
        submit = true;
    }

    if (submit)
        emit processWaterfallRequested(generation, frame, outputBins, minDb, maxDb, palette);
}

void FftDisplayItem::requestHistoryRecolor()
{
    // If a row completion triggered recolor before the debounce timeout, cancel
    // that pending timeout so the same Min/Max epoch is not recolored twice.
    m_recolorDebounceTimer.stop();

    quint64 generation = 0;
    QVector<float> historySnapshot;
    QVector<quint32> palette;
    float minDb = 0.0f;
    float maxDb = 0.0f;
    bool submit = false;

    {
        QMutexLocker locker(&m_dataMutex);
        if (m_waterfallValidRows <= 0 || m_waterfallDbHistory.isEmpty()) {
            m_recolorRequested = false;
            return;
        }

        m_recolorRequested = true;
        if (m_waterfallWorkBusy || m_recolorBusy)
            return;

        // While recolor is in flight, incoming rows are coalesced and no writes
        // touch m_waterfallDbHistory. The shallow QVector copy therefore stays
        // immutable without forcing a multi-MB detach on the GUI thread.
        historySnapshot = m_waterfallDbHistory;
        palette = m_paletteLut;
        generation = m_colorGeneration;
        minDb = static_cast<float>(m_minDb);
        maxDb = static_cast<float>(m_maxDb);
        m_recolorBusy = true;
        m_recolorRequested = false;
        submit = true;
    }

    if (submit)
        emit recolorHistoryRequested(generation, historySnapshot, minDb, maxDb, palette);
}

void FftDisplayItem::appendProcessedWaterfallRowLocked(const QVector<float> &dbRow,
                                                       const QVector<quint32> &argbRow)
{
    if (dbRow.isEmpty() || dbRow.size() != argbRow.size())
        return;

    ensureWaterfallHistory(dbRow.size());
    if (m_waterfallHistoryWidth <= 0)
        return;

    if (m_clearBeforeNextPaint) {
        clearHistoryLocked();
        m_clearBeforeNextPaint = false;
    }

    // Decrementing head makes the newest row the first row of the first
    // contiguous segment. No history image/db memmove is needed anymore.
    m_waterfallHeadRow = (m_waterfallHeadRow - 1 + m_waterfallHistoryRows)
                      % m_waterfallHistoryRows;
    const int offset = m_waterfallHeadRow * m_waterfallHistoryWidth;

    std::memcpy(m_waterfallDbHistory.data() + offset,
                dbRow.constData(),
                static_cast<size_t>(m_waterfallHistoryWidth) * sizeof(float));
    std::memcpy(m_waterfallColorHistory.data() + offset,
                argbRow.constData(),
                static_cast<size_t>(m_waterfallHistoryWidth) * sizeof(quint32));

    m_waterfallValidRows = std::min(m_waterfallHistoryRows, m_waterfallValidRows + 1);
}

void FftDisplayItem::onWaterfallRowReady(quint64 generation,
                                         QVector<float> dbRow,
                                         QVector<quint32> argbRow,
                                         bool usedCuda,
                                         qint64 elapsedUsec)
{
    bool painted = false;
    bool needRecolor = false;
    QVector<float> nextFrame;

    {
        QMutexLocker locker(&m_dataMutex);
        m_waterfallWorkBusy = false;

        if (usedCuda)
            ++m_cudaRows;
        else
            ++m_cpuRows;

        if (generation == m_colorGeneration
                && m_mode == Waterfall && m_renderEnabled && !m_waterfallPaused
                && !dbRow.isEmpty() && dbRow.size() == argbRow.size()) {
            appendProcessedWaterfallRowLocked(dbRow, argbRow);
            const qint64 nowMs = monotonicMs();
            if (m_lastWaterfallArrivalMs > 0) {
                const qint64 rawPeriod = nowMs - m_lastWaterfallArrivalMs;
                if (rawPeriod > 0 && rawPeriod < 500) {
                    const double bounded = std::max(8.0, std::min(150.0, static_cast<double>(rawPeriod)));
                    m_waterfallSourcePeriodMs = 0.80 * m_waterfallSourcePeriodMs + 0.20 * bounded;
                }
            }
            m_lastWaterfallArrivalMs = nowMs;
            painted = true;
        }

        needRecolor = m_recolorRequested && !m_recolorBusy;
        if (!needRecolor && !m_pendingWaterfallFrame.isEmpty()) {
            nextFrame = m_pendingWaterfallFrame;
            m_pendingWaterfallFrame.clear();
        }

        if (m_computeStatsTimer.isValid() && m_computeStatsTimer.elapsed() >= 10000) {
            qInfo() << "[SPECTRUM-COMPUTE-10S]"
                    << "backend=" << m_computeBackend
                    << "cudaRows=" << m_cudaRows
                    << "cpuRows=" << m_cpuRows
                    << "cudaRecolors=" << m_cudaRecolors
                    << "cpuRecolors=" << m_cpuRecolors
                    << "coalesced=" << m_coalescedWaterfallFrames
                    << "lastRowUsec=" << elapsedUsec
                    << "lastRecolorUsec=" << m_lastRecolorUsec;
            m_cudaRows = 0;
            m_cpuRows = 0;
            m_cudaRecolors = 0;
            m_cpuRecolors = 0;
            m_coalescedWaterfallFrames = 0;
            m_computeStatsTimer.restart();
        }
    }

    // Normal Waterfall presentation is driven by the fixed ~60 Hz clock.
    // Avoid a second bursty update on every worker completion.
    if (painted && !m_presentTimer.isActive())
        update();
    if (needRecolor)
        requestHistoryRecolor();
    else if (!nextFrame.isEmpty())
        submitWaterfallWork(nextFrame);
}

void FftDisplayItem::onHistoryRecolorReady(quint64 generation,
                                           QVector<quint32> argbHistory,
                                           bool usedCuda,
                                           qint64 elapsedUsec)
{
    bool applied = false;
    bool rerun = false;
    QVector<float> nextFrame;

    {
        QMutexLocker locker(&m_dataMutex);
        m_recolorBusy = false;

        if (generation == m_colorGeneration
                && argbHistory.size() == m_waterfallColorHistory.size()) {
            m_waterfallColorHistory.swap(argbHistory);
            applied = true;
        } else if (m_waterfallValidRows > 0) {
            m_recolorRequested = true;
        }

        if (usedCuda)
            ++m_cudaRecolors;
        else
            ++m_cpuRecolors;
        m_lastRecolorUsec = elapsedUsec;

        rerun = m_recolorRequested;
        if (!rerun && !m_pendingWaterfallFrame.isEmpty()) {
            nextFrame = m_pendingWaterfallFrame;
            m_pendingWaterfallFrame.clear();
        }
    }

    if (applied && !m_presentTimer.isActive())
        update();
    if (rerun)
        requestHistoryRecolor();
    else if (!nextFrame.isEmpty())
        submitWaterfallWork(nextFrame);
}

void FftDisplayItem::updateMeasurementsLocked(const QVector<float> &frame, bool force)
{
    Q_UNUSED(force)
    if (frame.size() < 2 || m_sampleRate <= 0.0) {
        m_measurementsValid = false;
        return;
    }

    const int start = std::min(frame.size() - 2, mappedStartIndex(frame.size()));
    const int end = std::max(start + 1, mappedEndIndex(frame.size(), start));
    const int visible = std::max(2, end - start + 1);

    int peakIndex = -1;
    double peak = -1.0e9;
    QVector<double> noiseSamples;
    const int targetSamples = 256;
    const int stride = std::max(1, visible / targetSamples);
    noiseSamples.reserve((visible / stride) + 2);

    for (int i = start; i <= end; ++i) {
        const double value = static_cast<double>(frame.at(i));
        if (!std::isfinite(value))
            continue;
        if (value > peak) {
            peak = value;
            peakIndex = i;
        }
        if (((i - start) % stride) == 0)
            noiseSamples.append(value);
    }

    if (peakIndex < 0 || noiseSamples.isEmpty()) {
        m_measurementsValid = false;
        return;
    }

    std::sort(noiseSamples.begin(), noiseSamples.end());
    const int noiseIndex = std::max(0, std::min(noiseSamples.size() - 1,
        static_cast<int>(std::floor((noiseSamples.size() - 1) * 0.30))));
    const double noise = noiseSamples.at(noiseIndex);

    m_measurementsValid = true;
    m_peakDb = peak;
    m_noiseFloorDb = noise;
    m_snrDb = peak - noise;
    m_peakFrequencyHz = m_fullStartFreq
            + (static_cast<double>(peakIndex) / static_cast<double>(frame.size() - 1))
              * m_sampleRate;
}

int FftDisplayItem::mappedStartIndex(int count) const
{
    if (count <= 1)
        return 0;
    const double span = std::max(1.0, m_sampleRate);
    const double ratio = std::max(0.0, std::min(1.0,
        (m_viewStartFreq - m_fullStartFreq) / span));
    return std::max(0, std::min(count - 1,
        static_cast<int>(std::floor(ratio * static_cast<double>(count - 1)))));
}

int FftDisplayItem::mappedEndIndex(int count, int startIndex) const
{
    if (count <= 1)
        return 0;
    const double span = std::max(1.0, m_sampleRate);
    const double startRatio = std::max(0.0, std::min(1.0,
        (m_viewStartFreq - m_fullStartFreq) / span));
    const double stopRatio = std::max(startRatio, std::min(1.0,
        (m_viewStopFreq - m_fullStartFreq) / span));
    const int mapped = static_cast<int>(std::ceil(stopRatio * static_cast<double>(count - 1)));
    return std::max(startIndex, std::min(count - 1, mapped));
}

qreal FftDisplayItem::yForDb(float value, qreal plotHeight) const
{
    double db = std::isfinite(value) ? static_cast<double>(value) : m_minDb;
    db = std::max(m_minDb, std::min(m_maxDb, db));
    const double range = std::max(1e-9, m_maxDb - m_minDb);
    return static_cast<qreal>(plotHeight - ((db - m_minDb) / range) * plotHeight);
}

void FftDisplayItem::paintSpectrum(QPainter *painter)
{
    CrashDiagnostics::checkpoint(CrashDiagnostics::CpNativeSpectrumPaint);

    QVector<float> currentFrame;
    QVector<float> previousFrame;
    QVector<float> maxHoldFrame;
    QColor spectrumColor;
    QColor maxHoldColor;
    bool showMaxHold = false;
    bool renderEnabled = false;
    double minDb = -130.0;
    double maxDb = -80.0;
    double fullStartFreq = 0.0;
    double viewStartFreq = 0.0;
    double viewStopFreq = 0.0;
    double sampleRate = 1.0;
    qint64 transitionStartMs = 0;
    double sourcePeriodMs = 40.0;

    // Snapshot only small metadata + implicitly-shared FFT vectors while the
    // lock is held. QPainter work happens after unlock, so the WebSocket/GUI
    // path is never blocked by polyline allocation or raster/OpenGL painting.
    {
        QMutexLocker locker(&m_dataMutex);
        renderEnabled = m_renderEnabled;
        if (!renderEnabled || m_spectrumFrame.size() < 2)
            return;
        currentFrame = m_spectrumFrame;
        previousFrame = m_previousSpectrumFrame;
        maxHoldFrame = m_maxHoldFrame;
        spectrumColor = m_spectrumColor;
        maxHoldColor = m_maxHoldColor;
        showMaxHold = m_showMaxHold;
        minDb = m_minDb;
        maxDb = m_maxDb;
        fullStartFreq = m_fullStartFreq;
        viewStartFreq = m_viewStartFreq;
        viewStopFreq = m_viewStopFreq;
        sampleRate = m_sampleRate;
        transitionStartMs = m_spectrumTransitionStartMs;
        sourcePeriodMs = m_spectrumSourcePeriodMs;
    }

    const qreal itemWidth = width();
    const qreal itemHeight = height();
    if (itemWidth <= 0.0 || itemHeight <= 0.0)
        return;

    const qreal xAxisHeight = std::min<qreal>(18.0, itemHeight);
    const qreal plotHeight = std::max<qreal>(1.0, itemHeight - xAxisHeight);
    const int count = currentFrame.size();
    const double span = std::max(1.0, sampleRate);
    const double startRatio = std::max(0.0, std::min(1.0,
        (viewStartFreq - fullStartFreq) / span));
    const double stopRatio = std::max(startRatio, std::min(1.0,
        (viewStopFreq - fullStartFreq) / span));
    const int start = std::max(0, std::min(count - 2,
        static_cast<int>(std::floor(startRatio * static_cast<double>(count - 1)))));
    const int end = std::max(start + 1, std::min(count - 1,
        static_cast<int>(std::ceil(stopRatio * static_cast<double>(count - 1)))));
    const int visible = std::max(2, end - start + 1);

    const int maxPoints = std::max(2, static_cast<int>(std::ceil(itemWidth)));
    const int step = std::max(1, visible / maxPoints);
    const double rangeDb = std::max(1e-9, maxDb - minDb);

    const auto yForDbLocal = [&](float value) -> qreal {
        double db = std::isfinite(value) ? static_cast<double>(value) : minDb;
        db = std::max(minDb, std::min(maxDb, db));
        return static_cast<qreal>(plotHeight - ((db - minDb) / rangeDb) * plotHeight);
    };

    // Smooth the visual transition between independent RF snapshots. This is
    // presentation interpolation only; measurements/Max Hold still use real
    // source frames. The window is bounded to keep added visual latency low.
    double blend = 1.0;
    if (previousFrame.size() == currentFrame.size() && transitionStartMs > 0) {
        const qint64 ageMs = std::max<qint64>(0, monotonicMs() - transitionStartMs);
        const double interpolationMs = std::max(16.0, std::min(42.0, sourcePeriodMs * 0.75));
        blend = std::max(0.0, std::min(1.0, static_cast<double>(ageMs) / interpolationMs));
        // Smoothstep removes the small constant-velocity jerk at frame boundaries.
        blend = blend * blend * (3.0 - 2.0 * blend);
    }

    auto buildPolyline = [&](const QVector<float> &frame, bool interpolateLive) {
        QPolygonF polyline;
        if (frame.size() != currentFrame.size())
            return polyline;
        polyline.reserve((visible / step) + 2);
        for (int i = start; i <= end; i += step) {
            float value = frame.at(i);
            if (interpolateLive && previousFrame.size() == currentFrame.size()) {
                const float prev = previousFrame.at(i);
                value = static_cast<float>(prev + (value - prev) * blend);
            }
            const qreal x = static_cast<qreal>(i - start)
                          / static_cast<qreal>(std::max(1, end - start)) * itemWidth;
            polyline.append(QPointF(x, yForDbLocal(value)));
        }
        if (polyline.isEmpty() || polyline.last().x() < itemWidth) {
            float value = frame.at(end);
            if (interpolateLive && previousFrame.size() == currentFrame.size()) {
                const float prev = previousFrame.at(end);
                value = static_cast<float>(prev + (value - prev) * blend);
            }
            polyline.append(QPointF(itemWidth, yForDbLocal(value)));
        }
        return polyline;
    };

    painter->setRenderHint(QPainter::Antialiasing, false);

    const QPolygonF spectrum = buildPolyline(currentFrame, true);
    if (spectrum.size() >= 2) {
        QPolygonF fill = spectrum;
        fill.append(QPointF(itemWidth, plotHeight));
        fill.append(QPointF(0.0, plotHeight));
        // CUDA1.11 visual polish: a restrained vertical gradient makes the
        // live trace easier to follow over a busy grid without hiding RF data.
        // Keep this to one fill pass + one line pass to preserve FPS60 budget.
        QColor fillTop = spectrumColor;
        QColor fillMid = spectrumColor.darker(125);
        QColor fillBottom = spectrumColor.darker(175);
        fillTop.setAlpha(58);
        fillMid.setAlpha(32);
        fillBottom.setAlpha(6);

        QLinearGradient fillGradient(0.0, 0.0, 0.0, plotHeight);
        fillGradient.setColorAt(0.0, fillTop);
        fillGradient.setColorAt(0.58, fillMid);
        fillGradient.setColorAt(1.0, fillBottom);

        painter->setPen(Qt::NoPen);
        painter->setBrush(fillGradient);
        painter->drawPolygon(fill);

        painter->setBrush(Qt::NoBrush);
        QPen livePen(spectrumColor, 1.35);
        livePen.setCapStyle(Qt::RoundCap);
        livePen.setJoinStyle(Qt::RoundJoin);
        painter->setPen(livePen);
        painter->drawPolyline(spectrum);
    }

    if (showMaxHold && maxHoldFrame.size() == currentFrame.size()) {
        QPen maxHoldPen(maxHoldColor, 1.15);
        maxHoldPen.setCapStyle(Qt::RoundCap);
        maxHoldPen.setJoinStyle(Qt::RoundJoin);
        painter->setPen(maxHoldPen);
        const QPolygonF maxHold = buildPolyline(maxHoldFrame, false);
        if (maxHold.size() >= 2)
            painter->drawPolyline(maxHold);
    }
}

void FftDisplayItem::paintWaterfall(QPainter *painter)
{
    CrashDiagnostics::checkpoint(CrashDiagnostics::CpNativeWaterfallPaint);
    QMutexLocker locker(&m_dataMutex);
    if (!m_renderEnabled || m_waterfallHistoryWidth <= 0
            || m_waterfallColorHistory.isEmpty() || m_waterfallValidRows <= 0)
        return;

    const int start = mappedStartIndex(m_waterfallHistoryWidth);
    const int end = mappedEndIndex(m_waterfallHistoryWidth, start);
    const int sourceWidth = std::max(1, end - start + 1);

    QImage historyView(reinterpret_cast<uchar *>(m_waterfallColorHistory.data()),
                       m_waterfallHistoryWidth,
                       m_waterfallHistoryRows,
                       m_waterfallHistoryWidth * static_cast<int>(sizeof(quint32)),
                       QImage::Format_ARGB32);

    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

    // FPS60: render approximately one acquired FFT row per display pixel
    // instead of resampling all 1024 retained history rows into a ~200 px
    // surface on every paint. The full 1024-row history is still retained for
    // persistence; only the visible tail is sampled for presentation.
    const int displayRowCapacity = std::max(1, static_cast<int>(std::ceil(height())));
    const int rowsToDraw = std::min(m_waterfallValidRows, displayRowCapacity);
    const qreal rowScale = height() / static_cast<qreal>(displayRowCapacity);

    // Smooth presentation between real rows without manufacturing RF values.
    // We only translate the already-acquired image by at most one row; the
    // next real row resets the phase. If source cadence is >=60 Hz this phase
    // naturally stays close to zero.
    qreal scrollOffset = 0.0;
    if (!m_waterfallPaused && m_lastWaterfallArrivalMs > 0
            && m_waterfallSourcePeriodMs > 1.0) {
        const qint64 ageMs = std::max<qint64>(0, monotonicMs() - m_lastWaterfallArrivalMs);
        const double phase = std::max(0.0, std::min(1.0,
            static_cast<double>(ageMs) / std::max(8.0, m_waterfallSourcePeriodMs)));
        scrollOffset = static_cast<qreal>(phase) * rowScale;
    }

    // Newest -> oldest is at most two contiguous physical segments because the
    // history is a ring. Limit both segments to the rows actually visible.
    const int firstRows = std::min(rowsToDraw,
                                   m_waterfallHistoryRows - m_waterfallHeadRow);
    const int secondRows = rowsToDraw - firstRows;

    if (firstRows > 0) {
        const QRect src(start, m_waterfallHeadRow, sourceWidth, firstRows);
        const QRectF dst(0.0, scrollOffset, width(), firstRows * rowScale);
        painter->drawImage(dst, historyView, src);
    }
    if (secondRows > 0) {
        const QRect src(start, 0, sourceWidth, secondRows);
        const QRectF dst(0.0, scrollOffset + firstRows * rowScale,
                         width(), secondRows * rowScale);
        painter->drawImage(dst, historyView, src);
    }

    // Fill the fractional top gap with the newest real RF row. This is a
    // presentation stretch only and never enters retained waterfall history.
    if (scrollOffset > 0.01) {
        const QRect newestSrc(start, m_waterfallHeadRow, sourceWidth, 1);
        const QRectF newestDst(0.0, 0.0, width(), scrollOffset);
        painter->drawImage(newestDst, historyView, newestSrc);
    }
}

void FftDisplayItem::paint(QPainter *painter)
{
    if (!painter)
        return;

    painter->setCompositionMode(QPainter::CompositionMode_Source);
    painter->fillRect(boundingRect(), Qt::transparent);
    painter->setCompositionMode(QPainter::CompositionMode_SourceOver);

    Mode modeSnapshot = Spectrum;
    {
        QMutexLocker locker(&m_dataMutex);
        modeSnapshot = m_mode;
    }

    if (modeSnapshot == Spectrum)
        paintSpectrum(painter);
    else
        paintWaterfall(painter);

    // Actual Qt Quick paint cadence, independent from AstraRX source FPS.
    // This is intentionally low-rate logging so diagnostics do not perturb
    // the render loop.
    if (!m_paintStatsTimer.isValid())
        m_paintStatsTimer.start();
    ++m_paintStatsFrames;
    if (m_paintStatsTimer.elapsed() >= 5000) {
        const double sec = std::max(0.001, m_paintStatsTimer.elapsed() / 1000.0);
        const double fps = static_cast<double>(m_paintStatsFrames) / sec;
        qInfo() << "[SPECTRUM-RENDER-5S]"
                << "mode=" << (modeSnapshot == Spectrum ? "spectrum" : "waterfall")
                << "fps=" << QString::number(fps, 'f', 1)
                << "targetMs=" << (modeSnapshot == Spectrum ? m_presentIntervalMs : 0);
        m_paintStatsFrames = 0;
        m_paintStatsTimer.restart();
    }
}
