#include "FftDisplayItem.h"

#include "SpectrumCudaWorker.h"
#include "websocketclient.h"
#include "CrashDiagnostics.h"

#include <QDebug>
#include <QGuiApplication>
#include <QList>
#include <QPointer>
#include <QScreen>
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

class AnalyzerPresentationClock final
{
public:
    AnalyzerPresentationClock()
    {
        m_requestedFps = envPositiveIntLocal("ISCAN_ANALYZER_PRESENT_FPS", 90, 240);
        QScreen *screen = QGuiApplication::primaryScreen();
        const double displayHz = screen ? screen->refreshRate() : 0.0;
        m_effectiveFps = (displayHz >= 30.0)
                ? qMin(m_requestedFps, qMax(30, qRound(displayHz)))
                : m_requestedFps;
        // Run slightly ahead of the requested cadence and let Qt Quick/vsync
        // coalesce onto the physical refresh boundary. This avoids choosing a
        // 17 ms timer for a 60 Hz panel (58.8 Hz) while keeping 90 Hz at 11 ms.
        m_intervalMs = qMax(1, static_cast<int>(std::floor(1000.0
                                         / static_cast<double>(m_effectiveFps))));
        m_timer.setTimerType(Qt::PreciseTimer);
        m_timer.setInterval(m_intervalMs);
        m_timer.setSingleShot(false);
        QObject::connect(&m_timer, &QTimer::timeout, [this]() { tick(); });
    }

    void add(FftDisplayItem *item)
    {
        if (!item)
            return;
        for (const QPointer<FftDisplayItem> &existing : m_items) {
            if (existing == item)
                return;
        }
        m_items.append(QPointer<FftDisplayItem>(item));
        if (!m_timer.isActive()) {
            m_statsTimer.start();
            m_ticks = 0;
            m_timer.start();
        }
    }

    void remove(FftDisplayItem *item)
    {
        for (int i = m_items.size() - 1; i >= 0; --i) {
            if (m_items.at(i).isNull() || m_items.at(i) == item)
                m_items.removeAt(i);
        }
        if (m_items.isEmpty())
            m_timer.stop();
    }

    int intervalMs() const noexcept { return m_intervalMs; }
    int requestedFps() const noexcept { return m_requestedFps; }
    int effectiveFps() const noexcept { return m_effectiveFps; }

private:
    void tick()
    {
        ++m_ticks;
        for (int i = m_items.size() - 1; i >= 0; --i) {
            FftDisplayItem *item = m_items.at(i).data();
            if (!item) {
                m_items.removeAt(i);
                continue;
            }
            item->presentOnSharedClock();
        }

        if (!m_statsTimer.isValid())
            m_statsTimer.start();
        if (m_statsTimer.elapsed() >= 5000) {
            const double sec = std::max(0.001, m_statsTimer.elapsed() / 1000.0);
            const double tickFps = static_cast<double>(m_ticks) / sec;
            QScreen *screen = QGuiApplication::primaryScreen();
            const double displayHz = screen ? screen->refreshRate() : 0.0;
            qInfo() << "[ANALYZER-PRESENT-5S]"
                    << "requestedFps=" << m_requestedFps
                    << "effectiveFps=" << m_effectiveFps
                    << "tickFps=" << QString::number(tickFps, 'f', 1)
                    << "intervalMs=" << m_intervalMs
                    << "displayHz=" << QString::number(displayHz, 'f', 1)
                    << "items=" << m_items.size();
            m_ticks = 0;
            m_statsTimer.restart();
        }
    }

    QList<QPointer<FftDisplayItem>> m_items;
    QTimer m_timer;
    QElapsedTimer m_statsTimer;
    quint64 m_ticks = 0;
    int m_requestedFps = 90;
    int m_effectiveFps = 90;
    int m_intervalMs = 11;
};

AnalyzerPresentationClock &sharedAnalyzerPresentationClock()
{
    // Intentionally process-lifetime. This avoids QObject/QTimer destruction
    // ordering hazards after QGuiApplication begins shutting down.
    static AnalyzerPresentationClock *clock = new AnalyzerPresentationClock();
    return *clock;
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

    // CUDA1.26/FPS90-SYNC: one shared precise timer invalidates both Spectrum
    // and Waterfall on the same GUI-thread tick. Independent item timers caused
    // phase drift and unequal frame pacing even when both were configured to
    // the same nominal interval.
    AnalyzerPresentationClock &presentClock = sharedAnalyzerPresentationClock();
    m_presentIntervalMs = presentClock.intervalMs();
    m_waterfallHistoryMaxBins = envPositiveIntLocal("ISCAN_WATERFALL_DISPLAY_BINS", 2048, 8192);
    presentClock.add(this);

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
    m_rfMetricsTelemetryTimer.start();
    m_computeStatsTimer.start();
    m_recolorDebounceTimer.setSingleShot(true);
    m_recolorDebounceTimer.setInterval(12);
    connect(&m_recolorDebounceTimer, &QTimer::timeout,
            this, &FftDisplayItem::requestHistoryRecolor);
    // CUDA worker is started lazily by the Waterfall item on its first real
    // frame. The Spectrum item no longer creates an unused CUDA context/thread.
    refreshPresentationClock();

    qInfo() << "[R20.4-SPECTRUM-CUDA1.26-FPS90-LOCKSTEP]"
            << "asyncComputeWorker=1"
            << "waterfallRingBuffer=1"
            << "cudaPeakPoolPalette=1"
            << "cudaHistoryRecolor=1"
            << "lazyCudaWorker=1"
            << "targetPresentMs=" << m_presentIntervalMs
            << "requestedPresentFps=" << sharedAnalyzerPresentationClock().requestedFps()
            << "effectivePresentFps=" << sharedAnalyzerPresentationClock().effectiveFps()
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
    sharedAnalyzerPresentationClock().remove(this);
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

    // CUDA1.19: a new backend session gets a new one-time RF reference.
    // Do not carry a calibration learned from another receiver/session.
    {
        QMutexLocker locker(&m_dataMutex);
        m_spectrumCalibrationValid = false;
        m_spectrumCalibrationOffsetDb = 0.0;
        m_spectrumCalibrationSamples = 0;
        m_spectrumCalibrationCandidates.clear();
    }

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
        // CUDA1.16: AstraRX already publishes the tuned receiver S-meter. Keep
        // this low-rate scalar on the GUI object's thread; no FFT/QML array is
        // involved and no CUDA synchronization is introduced.
        m_backendConnections.append(connect(m_backend.data(), &WebSocketClient::smeterValueUpdated,
                                            this, &FftDisplayItem::onSmeterValueUpdated,
                                            Qt::QueuedConnection));
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
                m_selectedMeasurementsValid = false;
                m_receiverLevelValid = false;
                m_spectrumCalibrationValid = false;
                m_spectrumCalibrationOffsetDb = 0.0;
                m_spectrumCalibrationSamples = 0;
                m_spectrumCalibrationCandidates.clear();
            }
            emit measurementsChanged();
            emit selectedMeasurementsChanged();
            emit receiverLevelChanged();
            emit spectrumCalibrationChanged();
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
    // CUDA1.26: presentation lifetime is centralized. Per-item state is checked
    // in presentOnSharedClock(), so mode/pause/render changes cannot de-phase
    // Spectrum and Waterfall by starting/stopping independent timers.
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

void FftDisplayItem::setPlotTopInset(double value)
{
    if (!std::isfinite(value))
        return;

    value = std::max(0.0, value);
    {
        QMutexLocker locker(&m_dataMutex);
        if (sameReal(m_plotTopInset, value))
            return;
        m_plotTopInset = value;
    }

    emit plotGeometryChanged();
    update();
}

void FftDisplayItem::setFullStartFreq(double value)
{
    if (!std::isfinite(value))
        return;

    bool clearEpoch = false;
    bool selectedMetricsChanged = false;
    {
        QMutexLocker locker(&m_dataMutex);
        if (sameReal(m_fullStartFreq, value))
            return;
        clearEpoch = m_mode == Waterfall && m_waterfallHistoryWidth > 0;
        m_fullStartFreq = value;
        if (clearEpoch)
            clearHistoryLocked();
        if (m_mode == Spectrum && !m_spectrumFrame.isEmpty()) {
            updateSelectedMeasurementsLocked(m_spectrumFrame, true);
            selectedMetricsChanged = true;
        }
    }
    emit frequencyMappingChanged();
    if (selectedMetricsChanged)
        emit selectedMeasurementsChanged();
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
        if (m_mode == Spectrum && !m_spectrumFrame.isEmpty()) {
            updateMeasurementsLocked(m_spectrumFrame, true);
            updateSelectedMeasurementsLocked(m_spectrumFrame, true);
        }
    }
    emit frequencyMappingChanged();
    if (m_mode == Spectrum) {
        emit measurementsChanged();
        emit selectedMeasurementsChanged();
    }
    update();
}

void FftDisplayItem::setMeasurementFrequencyHz(double value)
{
    if (!std::isfinite(value))
        return;

    bool receiverLevelInvalidated = false;
    {
        QMutexLocker locker(&m_dataMutex);
        if (sameReal(m_measurementFrequencyHz, value))
            return;
        m_measurementFrequencyHz = value;

        // The previously received S-meter sample belongs to the previous tuned
        // receiver. Never display it against the newly moved selection bar.
        receiverLevelInvalidated = m_receiverLevelValid;
        m_receiverLevelValid = false;
        m_receiverLevelFrequencyHz = value;
        // QML coalesces offset commands for ~20 ms, and AstraRX needs a short
        // processing interval before its S-meter belongs to the new receiver.
        // Ignore immediately arriving samples so an old-frequency value is not
        // relabelled as the new frequency while the user drags/tunes.
        m_receiverLevelAcceptAfterMs = monotonicMs() + 120;

        if (m_mode == Spectrum && !m_spectrumFrame.isEmpty())
            updateSelectedMeasurementsLocked(m_spectrumFrame, true);
        else
            m_selectedMeasurementsValid = false;
    }

    emit measurementReferenceChanged();
    emit selectedMeasurementsChanged();
    if (receiverLevelInvalidated)
        emit receiverLevelChanged();
}

void FftDisplayItem::setMeasurementBandwidthHz(double value)
{
    if (!std::isfinite(value))
        return;
    value = std::max(0.0, value);

    {
        QMutexLocker locker(&m_dataMutex);
        if (sameReal(m_measurementBandwidthHz, value))
            return;
        m_measurementBandwidthHz = value;
        if (m_mode == Spectrum && !m_spectrumFrame.isEmpty())
            updateSelectedMeasurementsLocked(m_spectrumFrame, true);
    }

    emit measurementReferenceChanged();
    emit selectedMeasurementsChanged();
}

void FftDisplayItem::setMeasurementLowCutHz(double value)
{
    if (!std::isfinite(value))
        return;

    {
        QMutexLocker locker(&m_dataMutex);
        if (sameReal(m_measurementLowCutHz, value))
            return;
        m_measurementLowCutHz = value;
        if (m_mode == Spectrum && !m_spectrumFrame.isEmpty())
            updateSelectedMeasurementsLocked(m_spectrumFrame, true);
    }

    emit measurementReferenceChanged();
    emit selectedMeasurementsChanged();
}

void FftDisplayItem::setMeasurementHighCutHz(double value)
{
    if (!std::isfinite(value))
        return;

    {
        QMutexLocker locker(&m_dataMutex);
        if (sameReal(m_measurementHighCutHz, value))
            return;
        m_measurementHighCutHz = value;
        if (m_mode == Spectrum && !m_spectrumFrame.isEmpty())
            updateSelectedMeasurementsLocked(m_spectrumFrame, true);
    }

    emit measurementReferenceChanged();
    emit selectedMeasurementsChanged();
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
    // Explicit UI invalidations are still allowed, but sustained analyzer
    // presentation is owned by the shared 90 Hz clock. QML schedules Spectrum
    // and Waterfall together so one-off interaction paints remain paired.
    update();
}

void FftDisplayItem::presentOnSharedClock()
{
    bool present = false;
    {
        QMutexLocker locker(&m_dataMutex);
        present = m_renderEnabled
                && ((m_mode == Spectrum && !m_spectrumFrame.isEmpty())
                    || (m_mode == Waterfall && !m_waterfallPaused && m_waterfallValidRows > 0));
    }
    if (present)
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
            updateSelectedMeasurementsLocked(frame, true);
            m_measurementTimer.restart();
            metricsChanged = true;
        }
    }

    if (metricsChanged) {
        emit measurementsChanged();
        emit selectedMeasurementsChanged();
    }
    // Presentation is clocked by the shared analyzer clock at ~90 Hz. Avoid
    // issuing a second immediate update for every source frame; source arrival
    // jitter must not perturb the common Spectrum/Waterfall presentation phase.
}

void FftDisplayItem::onWaterfallFrame(const QVector<float> &frame)
{
    CrashDiagnostics::checkpoint(CrashDiagnostics::CpNativeWaterfallFrame);
    submitWaterfallWork(frame);
}

void FftDisplayItem::onMaxHoldFrame(const QVector<float> &frame)
{
    {
        QMutexLocker locker(&m_dataMutex);
        if (m_mode != Spectrum || !m_renderEnabled)
            return;
        m_maxHoldFrame = frame;
    }
    // CUDA1.26: Max Hold arrival must not inject Spectrum-only paint bursts.
    // The next shared analyzer tick presents the updated line together with the
    // Waterfall, preserving equal steady-state paint cadence.
}

void FftDisplayItem::onSmeterValueUpdated(double smeterDb)
{
    if (!std::isfinite(smeterDb))
        return;

    bool changed = false;
    bool calibrationChanged = false;
    bool logTelemetry = false;
    double freqHz = 0.0;
    double fftPointDb = 0.0;
    double fftNoiseDb = 0.0;
    double fftSnrDb = 0.0;
    double calibrationCandidateDb = 0.0;
    double calibrationOffsetDb = 0.0;
    double bandwidthHz = 0.0;
    double lowCutHz = 0.0;
    double highCutHz = 0.0;
    bool fftValid = false;
    bool calibrationValid = false;
    quint32 calibrationSamples = 0;

    {
        QMutexLocker locker(&m_dataMutex);
        if (m_mode != Spectrum || !std::isfinite(m_measurementFrequencyHz)
                || m_measurementFrequencyHz <= 0.0)
            return;
        if (monotonicMs() < m_receiverLevelAcceptAfterMs)
            return;

        changed = !m_receiverLevelValid
                || !sameReal(m_receiverLevelDb, smeterDb)
                || !sameReal(m_receiverLevelFrequencyHz, m_measurementFrequencyHz);

        m_receiverLevelDb = smeterDb;
        m_receiverLevelFrequencyHz = m_measurementFrequencyHz;
        m_receiverLevelValid = true;

        // CUDA1.19: establish one stable visual RF-reference offset between
        // native FFT amplitude and AstraRX's tuned S-meter, then freeze it for
        // the backend session. CUDA1.18 continuously EMA-updated this offset,
        // which made the Y-axis labels visibly breathe. The user explicitly
        // prefers a fixed scale even if absolute calibration is a little off.
        //
        // Collect five trusted candidates after tune-settle. Do not publish any
        // calibration while collecting; when the fifth arrives, latch the median
        // once. Median rejection makes the one-time lock robust against a single
        // stale S-meter or FFT transient without introducing ongoing movement.
        if (!m_spectrumCalibrationValid
                && m_selectedMeasurementsValid
                && std::isfinite(m_selectedLevelDb)
                && std::isfinite(m_selectedSnrDb)) {
            calibrationCandidateDb = smeterDb - m_selectedLevelDb;

            const bool candidateSane = std::isfinite(calibrationCandidateDb)
                    && calibrationCandidateDb >= -60.0
                    && calibrationCandidateDb <= 60.0;
            const bool updateTrusted = m_selectedSnrDb >= 4.0;

            if (candidateSane && updateTrusted) {
                m_spectrumCalibrationCandidates.append(calibrationCandidateDb);
                m_spectrumCalibrationSamples =
                        static_cast<quint32>(m_spectrumCalibrationCandidates.size());

                constexpr int cCalibrationLockSamples = 5;
                if (m_spectrumCalibrationCandidates.size() >= cCalibrationLockSamples) {
                    QVector<double> sorted = m_spectrumCalibrationCandidates;
                    std::sort(sorted.begin(), sorted.end());
                    const double oldOffset = m_spectrumCalibrationOffsetDb;
                    m_spectrumCalibrationOffsetDb = sorted.at(sorted.size() / 2);
                    m_spectrumCalibrationValid = true;
                    calibrationChanged = !sameReal(oldOffset, m_spectrumCalibrationOffsetDb)
                            || m_spectrumCalibrationSamples == cCalibrationLockSamples;
                }
            }
        }

        // Low-rate field telemetry shows both raw and RF-referenced domains.
        // The offset is observational/presentation calibration only; it is not
        // fed back into the FFT pipeline or receiver tuning. Once valid it is
        // latched and no longer changes during this backend session.
        logTelemetry = !m_rfMetricsTelemetryTimer.isValid()
                || m_rfMetricsTelemetryTimer.elapsed() >= 5000;
        if (logTelemetry) {
            m_rfMetricsTelemetryTimer.restart();
            freqHz = m_receiverLevelFrequencyHz;
            fftPointDb = m_selectedLevelDb;
            fftNoiseDb = m_selectedNoiseFloorDb;
            fftSnrDb = m_selectedSnrDb;
            bandwidthHz = m_measurementBandwidthHz;
            lowCutHz = m_measurementLowCutHz;
            highCutHz = m_measurementHighCutHz;
            fftValid = m_selectedMeasurementsValid;
            calibrationValid = m_spectrumCalibrationValid;
            calibrationOffsetDb = m_spectrumCalibrationOffsetDb;
            calibrationSamples = m_spectrumCalibrationSamples;
        }
    }

    if (changed)
        emit receiverLevelChanged();
    if (calibrationChanged)
        emit spectrumCalibrationChanged();

    if (logTelemetry) {
        if (fftValid) {
            qInfo().nospace()
                    << "[RF-METRICS] freq=" << (freqHz / 1.0e6) << "MHz"
                    << " astra_smeter=" << smeterDb
                    << " fft_point_raw=" << fftPointDb
                    << " fft_local_noise_raw=" << fftNoiseDb
                    << " fft_snr=" << fftSnrDb
                    << " axis_cal_valid=" << (calibrationValid ? 1 : 0)
                    << " axis_cal_latched=" << (calibrationValid ? 1 : 0)
                    << " axis_cal_samples=" << calibrationSamples
                    << " axis_cal_db=" << calibrationOffsetDb
                    << " spectrum_rf=" << (fftPointDb + calibrationOffsetDb)
                    << " noise_rf=" << (fftNoiseDb + calibrationOffsetDb)
                    << " bw=" << bandwidthHz
                    << " low_cut=" << lowCutHz
                    << " high_cut=" << highCutHz;
        } else {
            qInfo().nospace()
                    << "[RF-METRICS] freq=" << (freqHz / 1.0e6) << "MHz"
                    << " astra_smeter=" << smeterDb
                    << " fft_valid=0"
                    << " axis_cal_valid=" << (calibrationValid ? 1 : 0)
                    << " axis_cal_latched=" << (calibrationValid ? 1 : 0)
                    << " axis_cal_samples=" << calibrationSamples
                    << " axis_cal_db=" << calibrationOffsetDb
                    << " bw=" << bandwidthHz
                    << " low_cut=" << lowCutHz
                    << " high_cut=" << highCutHz;
        }
    }
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

    // Normal Waterfall presentation is driven by the shared ~90 Hz clock.
    // Worker completion only updates retained data; the next common tick paints
    // both analyzer surfaces in lockstep.
    Q_UNUSED(painted);
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

    // Recolor completion is presented on the next shared analyzer tick.
    Q_UNUSED(applied);
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


void FftDisplayItem::updateSelectedMeasurementsLocked(const QVector<float> &frame, bool force)
{
    Q_UNUSED(force)

    m_selectedFrequencyHz = m_measurementFrequencyHz;

    if (frame.size() < 2 || m_sampleRate <= 0.0
            || !std::isfinite(m_measurementFrequencyHz)) {
        m_selectedMeasurementsValid = false;
        return;
    }

    const int lastIndex = frame.size() - 1;
    const double normalized = (m_measurementFrequencyHz - m_fullStartFreq) / m_sampleRate;
    if (!std::isfinite(normalized) || normalized < 0.0 || normalized > 1.0) {
        m_selectedMeasurementsValid = false;
        return;
    }

    // Measure the signal at the actual selected/listening frequency instead of
    // searching for the strongest peak elsewhere in the viewport. A fractional
    // bin position is linearly interpolated so tuning does not jump one whole FFT
    // bin at a time when the receiver frequency sits between adjacent bins.
    const double binPosition = normalized * static_cast<double>(lastIndex);
    const int lowerIndex = std::max(0, std::min(lastIndex,
                                static_cast<int>(std::floor(binPosition))));
    const int upperIndex = std::max(0, std::min(lastIndex, lowerIndex + 1));
    const double fraction = std::max(0.0, std::min(1.0,
                                  binPosition - static_cast<double>(lowerIndex)));

    const double lowerDb = static_cast<double>(frame.at(lowerIndex));
    const double upperDb = static_cast<double>(frame.at(upperIndex));
    double selectedLevel = 0.0;
    if (std::isfinite(lowerDb) && std::isfinite(upperDb))
        selectedLevel = lowerDb + (upperDb - lowerDb) * fraction;
    else if (std::isfinite(lowerDb))
        selectedLevel = lowerDb;
    else if (std::isfinite(upperDb))
        selectedLevel = upperDb;
    else {
        m_selectedMeasurementsValid = false;
        return;
    }

    // Estimate a LOCAL noise floor around the selected receiver channel. Use
    // the actual low_cut/high_cut passband when available (important for
    // asymmetric SSB modes); measurementBandwidthHz remains a compatibility
    // fallback for older QML callers. Two extra FFT bins on each side form a
    // guard so filter skirts/the wanted signal do not bias the noise estimate.
    const double binHz = m_sampleRate / static_cast<double>(lastIndex);
    double lowCutHz = m_measurementLowCutHz;
    double highCutHz = m_measurementHighCutHz;
    if (!std::isfinite(lowCutHz) || !std::isfinite(highCutHz)
            || highCutHz <= lowCutHz) {
        const double halfBw = 0.5 * std::fabs(m_measurementBandwidthHz);
        lowCutHz = -halfBw;
        highCutHz = halfBw;
    }

    const double channelLeftHz = m_measurementFrequencyHz + std::min(lowCutHz, highCutHz);
    const double channelRightHz = m_measurementFrequencyHz + std::max(lowCutHz, highCutHz);
    const double leftChannelBin = ((channelLeftHz - m_fullStartFreq) / m_sampleRate)
            * static_cast<double>(lastIndex);
    const double rightChannelBin = ((channelRightHz - m_fullStartFreq) / m_sampleRate)
            * static_cast<double>(lastIndex);

    const double guardBins = 2.0;
    const double channelSpanBins = std::max(1.0, rightChannelBin - leftChannelBin);
    const double minimumOuterBins = 24.0;
    const double preferredOuterBins = std::max(minimumOuterBins,
                                                2.0 * channelSpanBins + 8.0);
    const double outerCapBins = std::max(minimumOuterBins,
                                          0.12 * static_cast<double>(lastIndex));
    const double outerBins = std::min(preferredOuterBins, outerCapBins);

    const int leftEnd = std::min(lastIndex,
            static_cast<int>(std::floor(leftChannelBin - guardBins)));
    const int leftStart = std::max(0,
            static_cast<int>(std::ceil(leftChannelBin - guardBins - outerBins)));
    const int rightStart = std::max(0,
            static_cast<int>(std::ceil(rightChannelBin + guardBins)));
    const int rightEnd = std::min(lastIndex,
            static_cast<int>(std::floor(rightChannelBin + guardBins + outerBins)));

    const int leftCount = std::max(0, leftEnd - leftStart + 1);
    const int rightCount = std::max(0, rightEnd - rightStart + 1);
    const int candidateCount = leftCount + rightCount;
    if (candidateCount < 4) {
        m_selectedMeasurementsValid = false;
        return;
    }

    const int targetNoiseSamples = 256;
    const int stride = std::max(1, candidateCount / targetNoiseSamples);
    QVector<double> noiseSamples;
    noiseSamples.reserve(std::min(candidateCount, targetNoiseSamples + 4));

    int candidateOrdinal = 0;
    auto appendRange = [&](int first, int last) {
        for (int i = first; i <= last; ++i, ++candidateOrdinal) {
            if ((candidateOrdinal % stride) != 0)
                continue;
            const double value = static_cast<double>(frame.at(i));
            if (std::isfinite(value))
                noiseSamples.append(value);
        }
    };

    if (leftCount > 0)
        appendRange(leftStart, leftEnd);
    if (rightCount > 0)
        appendRange(rightStart, rightEnd);

    if (noiseSamples.size() < 4) {
        m_selectedMeasurementsValid = false;
        return;
    }

    std::sort(noiseSamples.begin(), noiseSamples.end());
    const int noiseIndex = std::max(0, std::min(noiseSamples.size() - 1,
        static_cast<int>(std::floor((noiseSamples.size() - 1) * 0.30))));
    const double localNoise = noiseSamples.at(noiseIndex);

    m_selectedMeasurementsValid = true;
    m_selectedLevelDb = selectedLevel;
    m_selectedNoiseFloorDb = localNoise;
    m_selectedSnrDb = selectedLevel - localNoise;
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
    double plotTopInset = 18.0;
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
        plotTopInset = m_plotTopInset;
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

    // CUDA1.17: one plot geometry contract shared with the QML grid. The top
    // inset is reserved for frequency labels; maxDb maps exactly to plotTop and
    // minDb maps exactly to itemHeight. Previously the native trace used a
    // shortened height but forgot the +top offset, producing up to 18 px drift.
    const qreal plotTop = std::max<qreal>(0.0,
        std::min<qreal>(static_cast<qreal>(plotTopInset), itemHeight));
    const qreal plotBottom = itemHeight;
    const qreal plotHeight = std::max<qreal>(1.0, plotBottom - plotTop);
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
        return static_cast<qreal>(plotBottom - ((db - minDb) / rangeDb) * plotHeight);
    };

    // Smooth the visual transition between independent RF snapshots. This is
    // presentation interpolation only; measurements/Max Hold still use real
    // source frames. The window is bounded to keep added visual latency low.
    double blend = 1.0;
    if (previousFrame.size() == currentFrame.size() && transitionStartMs > 0) {
        const qint64 ageMs = std::max<qint64>(0, monotonicMs() - transitionStartMs);
        const double interpolationMs = std::max(static_cast<double>(m_presentIntervalMs),
                                                std::min(33.0, sourcePeriodMs * 0.75));
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
        fill.append(QPointF(itemWidth, plotBottom));
        fill.append(QPointF(0.0, plotBottom));
        // CUDA1.13 green RF-instrument fill: one vertical gradient clipped by
        // the real live-Spectrum polygon. Strong signals are bright lime-green,
        // falling through saturated green/emerald into a very dark transparent
        // floor. This keeps the grid visible while giving the live trace a
        // substantial analyzer-style body. Max Hold is rendered separately in
        // red-orange, so current energy and historical maxima are unambiguous.
        // Still one fill pass: no glow/blur passes are added to the high-FPS path.
        QLinearGradient fillGradient(0.0, plotTop, 0.0, plotBottom);
        fillGradient.setColorAt(0.00, QColor(210, 255, 88, 220)); // strongest: lime
        fillGradient.setColorAt(0.16, QColor(165, 255, 55, 216));
        fillGradient.setColorAt(0.34, QColor(92, 244, 38, 205));  // vivid green
        fillGradient.setColorAt(0.54, QColor(32, 210, 50, 186));  // green
        fillGradient.setColorAt(0.72, QColor(12, 154, 55, 150));  // emerald
        fillGradient.setColorAt(0.88, QColor(6, 92, 45, 108));    // dark green
        fillGradient.setColorAt(1.00, QColor(3, 34, 27, 52));     // near-transparent floor

        painter->setPen(Qt::NoPen);
        painter->setBrush(fillGradient);
        painter->drawPolygon(fill);

        painter->setBrush(Qt::NoBrush);
        QPen livePen(spectrumColor, 1.45);
        livePen.setCapStyle(Qt::RoundCap);
        livePen.setJoinStyle(Qt::RoundJoin);
        painter->setPen(livePen);
        painter->drawPolyline(spectrum);
    }

    if (showMaxHold && maxHoldFrame.size() == currentFrame.size()) {
        QPen maxHoldPen(maxHoldColor, 1.25);
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

    // FPS90-SYNC: render approximately one acquired FFT row per display pixel
    // instead of resampling all 1024 retained history rows into a ~200 px
    // surface on every paint. The full 1024-row history is still retained for
    // persistence; only the visible tail is sampled for presentation.
    const int displayRowCapacity = std::max(1, static_cast<int>(std::ceil(height())));
    const int rowsToDraw = std::min(m_waterfallValidRows, displayRowCapacity);
    const qreal rowScale = height() / static_cast<qreal>(displayRowCapacity);

    // Smooth presentation between real rows without manufacturing RF values.
    // We only translate the already-acquired image by at most one row; the
    // next real row resets the phase. If source cadence is high this phase
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
                << "targetMs=" << m_presentIntervalMs
                << "sharedClock=1";
        m_paintStatsFrames = 0;
        m_paintStatsTimer.restart();
    }
}
