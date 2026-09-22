#include "FftWaterfallTextureItem.h"
#include "SpectrumCudaWorker.h"

#include <QDebug>
#include <QMutexLocker>
#include <QMetaObject>
#include <QMetaType>
#include <QQuickWindow>
#include <QSGDynamicTexture>
#include <QSGSimpleTextureNode>
#include <QSGTexture>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QtGlobal>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

namespace {

class DoaWaterfallStreamingTexture final : public QSGDynamicTexture,
                                         protected QOpenGLFunctions
{
public:
    DoaWaterfallStreamingTexture()
    {
        setFiltering(QSGTexture::Nearest);
        setHorizontalWrapMode(QSGTexture::ClampToEdge);
        setVerticalWrapMode(QSGTexture::ClampToEdge);
    }

    ~DoaWaterfallStreamingTexture() override
    {
        if (m_textureId != 0 && QOpenGLContext::currentContext()) {
            initializeOpenGLFunctions();
            glDeleteTextures(1, &m_textureId);
        }
        m_textureId = 0;
    }

    void setImage(const QImage &image)
    {
        if (image.isNull())
            return;
        // Keep the image in a GL-friendly byte order.  This copy is still much
        // cheaper and safer than creating a brand new QSGTexture/QSGNode pair
        // every frame, and it gives the next phase a stable PBO/CUDA interop
        // insertion point.
        m_pendingImage = image.convertToFormat(QImage::Format_RGBA8888);
        m_pendingSize = m_pendingImage.size();
        m_dirty = true;
    }

    int textureId() const override { return static_cast<int>(m_textureId); }
    QSize textureSize() const override { return m_pendingSize; }
    bool hasAlphaChannel() const override { return true; }
    bool hasMipmaps() const override { return false; }

    void bind() override
    {
        if (m_dirty || m_textureId == 0)
            updateTexture();
        glBindTexture(GL_TEXTURE_2D, m_textureId);
    }

    bool updateTexture() override
    {
        if (m_pendingImage.isNull() || !QOpenGLContext::currentContext())
            return false;
        initializeOpenGLFunctions();

        if (m_textureId == 0) {
            glGenTextures(1, &m_textureId);
            glBindTexture(GL_TEXTURE_2D, m_textureId);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            m_allocatedSize = QSize();
        } else {
            glBindTexture(GL_TEXTURE_2D, m_textureId);
        }

        const QSize sz = m_pendingImage.size();
        if (m_allocatedSize != sz) {
            glTexImage2D(GL_TEXTURE_2D,
                         0,
                         GL_RGBA,
                         sz.width(),
                         sz.height(),
                         0,
                         GL_RGBA,
                         GL_UNSIGNED_BYTE,
                         m_pendingImage.constBits());
            m_allocatedSize = sz;
        } else {
            glTexSubImage2D(GL_TEXTURE_2D,
                            0,
                            0,
                            0,
                            sz.width(),
                            sz.height(),
                            GL_RGBA,
                            GL_UNSIGNED_BYTE,
                            m_pendingImage.constBits());
        }
        m_dirty = false;
        return true;
    }

private:
    GLuint m_textureId = 0;
    QSize m_allocatedSize;
    QSize m_pendingSize;
    QImage m_pendingImage;
    bool m_dirty = false;
};

qint64 monotonicMsWaterfall()
{
    using Clock = std::chrono::steady_clock;
    return std::chrono::duration_cast<std::chrono::milliseconds>(
                Clock::now().time_since_epoch()).count();
}

bool sameRealWaterfall(double a, double b)
{
    return qFuzzyCompare(a + 1.0, b + 1.0);
}

QRgb rgbFromVariant(const QVariant &value, QRgb fallback)
{
    bool ok = false;
    const quint32 raw = value.toUInt(&ok);
    if (!ok)
        return fallback;
    const int r = static_cast<int>((raw >> 16) & 0xffU);
    const int g = static_cast<int>((raw >> 8) & 0xffU);
    const int b = static_cast<int>(raw & 0xffU);
    return qRgb(r, g, b);
}


QString doaWaterfallUploadMode()
{
    QString mode = QString::fromLocal8Bit(qgetenv("ISCAN_DOA_WATERFALL_UPLOAD")).trimmed().toLower();
    if (mode.isEmpty())
        mode = QStringLiteral("stream");
    if (mode != QStringLiteral("stream") && mode != QStringLiteral("safe")) {
        qWarning() << "[DOA-WF-UPLOAD] invalid ISCAN_DOA_WATERFALL_UPLOAD=" << mode
                   << "using stream";
        mode = QStringLiteral("stream");
    }
    return mode;
}

QString doaWaterfallBackendMode()
{
    QString mode = QString::fromLocal8Bit(qgetenv("ISCAN_DOA_WATERFALL_BACKEND")).trimmed().toLower();
    if (mode.isEmpty())
        mode = QStringLiteral("auto");
    if (mode != QStringLiteral("auto")
            && mode != QStringLiteral("cuda")
            && mode != QStringLiteral("cpu")
            && mode != QStringLiteral("testpattern")) {
        qWarning() << "[DOA-WF-BACKEND] invalid ISCAN_DOA_WATERFALL_BACKEND=" << mode
                   << "using auto";
        mode = QStringLiteral("auto");
    }
    return mode;
}
}

FftWaterfallTextureItem::FftWaterfallTextureItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(QQuickItem::ItemHasContents, true);
    m_budgetTimer.setSingleShot(true);
    m_budgetTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_budgetTimer, &QTimer::timeout, this, [this]() {
        scheduleUpdate(true);
    });
    m_statsTimer.start();
    rebuildPaletteLocked();

    const QString backendMode = doaWaterfallBackendMode();
    const QString uploadMode = doaWaterfallUploadMode();
    {
        QMutexLocker locker(&m_mutex);
        m_backendMode = backendMode;
        m_uploadMode = uploadMode;
        m_streamTextureUpload = (uploadMode == QStringLiteral("stream"));
        m_forceCpuBackend = (backendMode == QStringLiteral("cpu"));
        m_testPatternBackend = (backendMode == QStringLiteral("testpattern"));
        if (m_forceCpuBackend || m_testPatternBackend) {
            m_cudaProcessingEnabled = false;
            m_cudaAccelerationActive = false;
            m_computeBackend = m_testPatternBackend
                    ? QStringLiteral("testpattern")
                    : QStringLiteral("cpu-forced");
        } else if (backendMode == QStringLiteral("cuda")) {
            m_computeBackend = QStringLiteral("cuda-forced-starting");
        }
    }

    qInfo() << "[DOA-VIEWER1.6-GPU-WATERFALL]"
            << "sceneGraphTexture=1"
            << "qPainterWaterfall=0"
            << "textureRingHistory=1"
            << "eventDriven=1"
            << "defaultTargetFps=" << m_targetFps;
    qInfo() << "[DOA-VIEWER1.8-STREAMING-GPU-TEXTURE]"
            << "sceneGraphTexture=1"
            << "qPainterWaterfall=0"
            << "textureRingHistory=1"
            << "eventDriven=1"
            << "cudaPluginPipeline=1"
            << "streamTextureUpload=" << m_streamTextureUpload
            << "cudaGlInterop=upload-stage"
            << "defaultTargetFps=" << m_targetFps;
    qInfo() << "[DOA-WF-BACKEND]"
            << "mode=" << m_backendMode
            << "forceCpu=" << m_forceCpuBackend
            << "testPattern=" << m_testPatternBackend
            << "env=ISCAN_DOA_WATERFALL_BACKEND";
    qInfo() << "[DOA-WF-UPLOAD]"
            << "mode=" << m_uploadMode
            << "streamTexture=" << m_streamTextureUpload
            << "env=ISCAN_DOA_WATERFALL_UPLOAD";
}

FftWaterfallTextureItem::~FftWaterfallTextureItem()
{
    m_budgetTimer.stop();
    {
        QMutexLocker locker(&m_mutex);
        m_renderEnabled = false;
        m_updatePending = false;
        m_dirty = false;
        m_pendingFrame.clear();
        m_inFlightOutputWidth = 0;
        m_rowInFlight = false;
        m_cancelBeforeGeneration = m_generation + 1;
        m_historyImage = QImage();
        m_presentImage = QImage();
    }
    stopComputeWorker();
}

void FftWaterfallTextureItem::releaseResources()
{
    // Called by Qt Quick when the scenegraph resources for this item are about
    // to be released, typically during page transitions, window teardown, or
    // render-loop reset. Stop delayed update requests here so a stale
    // UpdateRequest cannot race the item/page lifetime. The QSG node itself is
    // owned and destroyed by the scenegraph after updatePaintNode returns a
    // replacement/null node.
    m_budgetTimer.stop();
    QMutexLocker locker(&m_mutex);
    m_updatePending = false;
    m_dirty = false;
    m_pendingFrame.clear();
    m_inFlightOutputWidth = 0;
    m_rowInFlight = false;
    m_cancelBeforeGeneration = m_generation + 1;
}

void FftWaterfallTextureItem::setRenderEnabled(bool enabled)
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_renderEnabled == enabled)
            return;
        m_renderEnabled = enabled;
        if (!enabled) {
            m_updatePending = false;
            m_pendingFrame.clear();
            m_inFlightOutputWidth = 0;
            m_rowInFlight = false;
            m_cancelBeforeGeneration = m_generation + 1;
        }
    }
    if (!enabled)
        m_budgetTimer.stop();
    emit renderEnabledChanged();
    if (enabled)
        scheduleUpdate(true);
    else
        update();
}

void FftWaterfallTextureItem::setTargetFps(int fps)
{
    fps = qBound(1, fps, 120);
    {
        QMutexLocker locker(&m_mutex);
        if (m_targetFps == fps)
            return;
        m_targetFps = fps;
    }
    emit targetFpsChanged();
    scheduleUpdate(true);
}

void FftWaterfallTextureItem::setMinDb(double value)
{
    if (!std::isfinite(value))
        return;
    {
        QMutexLocker locker(&m_mutex);
        if (sameRealWaterfall(m_minDb, value))
            return;
        m_minDb = value;
        // Existing history is already colorized. Recoloring the full history is
        // intentionally deferred to clear/source changes so level edits do not
        // spike CPU on the UI thread.
    }
    emit levelsChanged();
}

void FftWaterfallTextureItem::setMaxDb(double value)
{
    if (!std::isfinite(value))
        return;
    {
        QMutexLocker locker(&m_mutex);
        if (sameRealWaterfall(m_maxDb, value))
            return;
        m_maxDb = value;
    }
    emit levelsChanged();
}

QVariantList FftWaterfallTextureItem::palette() const
{
    QMutexLocker locker(&m_mutex);
    return m_paletteValues;
}

void FftWaterfallTextureItem::setPalette(const QVariantList &colors)
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_paletteValues == colors)
            return;
        m_paletteValues = colors;
        rebuildPaletteLocked();
    }
    emit paletteChanged();
}

void FftWaterfallTextureItem::setRowHeightPx(int px)
{
    px = qBound(1, px, 16);
    {
        QMutexLocker locker(&m_mutex);
        if (m_rowHeightPx == px)
            return;
        m_rowHeightPx = px;
    }
    emit rowHeightPxChanged();
}

void FftWaterfallTextureItem::setBackgroundColor(const QColor &color)
{
    if (!color.isValid())
        return;
    {
        QMutexLocker locker(&m_mutex);
        if (m_backgroundColor == color)
            return;
        m_backgroundColor = color;
    }
    emit colorsChanged();
}

void FftWaterfallTextureItem::setCudaProcessingEnabled(bool enabled)
{
    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        if (m_forceCpuBackend || m_testPatternBackend)
            enabled = false;
        if (m_cudaProcessingEnabled == enabled)
            return;
        m_cudaProcessingEnabled = enabled;
        changed = true;
        if (!enabled) {
            m_pendingFrame.clear();
            m_inFlightOutputWidth = 0;
            m_rowInFlight = false;
            m_cancelBeforeGeneration = m_generation + 1;
            m_cudaAccelerationActive = false;
            m_computeBackend = QStringLiteral("cpu-local");
        }
    }
    if (!enabled)
        stopComputeWorker();
    if (changed) {
        emit cudaProcessingEnabledChanged();
        emit computeBackendChanged();
    }
}

QString FftWaterfallTextureItem::computeBackend() const
{
    QMutexLocker locker(&m_mutex);
    return m_computeBackend;
}

void FftWaterfallTextureItem::rebuildPaletteLocked()
{
    m_paletteRgb.clear();
    m_paletteRgb.reserve(std::max(2, m_paletteValues.size()));
    const QRgb fallback = qRgb(0, 0, 0);
    for (const QVariant &v : m_paletteValues)
        m_paletteRgb.append(rgbFromVariant(v, fallback));
    if (m_paletteRgb.size() < 2) {
        m_paletteRgb.clear();
        m_paletteRgb << qRgb(0, 0, 4) << qRgb(0, 96, 255)
                     << qRgb(0, 255, 255) << qRgb(255, 255, 0)
                     << qRgb(255, 0, 0);
    }
}

QVector<quint32> FftWaterfallTextureItem::paletteLutLocked() const
{
    QVector<quint32> lut;
    lut.reserve(std::max(2, m_paletteRgb.size()));
    for (const QRgb rgb : m_paletteRgb)
        lut.append(static_cast<quint32>(qRgba(qRed(rgb), qGreen(rgb), qBlue(rgb), 255)));
    if (lut.size() < 2) {
        lut.clear();
        lut << static_cast<quint32>(qRgba(0, 0, 4, 255))
            << static_cast<quint32>(qRgba(0, 96, 255, 255))
            << static_cast<quint32>(qRgba(0, 255, 255, 255))
            << static_cast<quint32>(qRgba(255, 255, 0, 255))
            << static_cast<quint32>(qRgba(255, 0, 0, 255));
    }
    return lut;
}

int FftWaterfallTextureItem::targetIntervalMs() const noexcept
{
    return qMax(1, static_cast<int>(std::floor(1000.0
                / static_cast<double>(qMax(1, m_targetFps)))));
}

QRgb FftWaterfallTextureItem::colorForDbLocked(float value) const
{
    const double lo = std::isfinite(m_minDb) ? m_minDb : -150.0;
    double hi = std::isfinite(m_maxDb) ? m_maxDb : -50.0;
    if (hi <= lo)
        hi = lo + 1.0;
    double v = std::isfinite(value) ? static_cast<double>(value) : lo;
    v = std::max(lo, std::min(hi, v));
    const double t = (v - lo) / (hi - lo);
    const int last = std::max(0, m_paletteRgb.size() - 1);
    const int idx = qBound(0, static_cast<int>(std::floor(t * last + 0.000001)), last);
    return m_paletteRgb.value(idx, qRgb(0, 0, 0));
}

void FftWaterfallTextureItem::copyVisibleRowsToNewHistoryLocked(QImage &newHistory,
                                                               int newWidth,
                                                               int newRows,
                                                               int rowsToCopy) const
{
    if (newHistory.isNull() || m_historyImage.isNull()
            || m_historyWidth <= 0 || m_historyRows <= 0
            || newWidth <= 0 || newRows <= 0 || rowsToCopy <= 0)
        return;

    rowsToCopy = qBound(0, rowsToCopy, qMin(newRows, m_validRows));
    for (int y = 0; y < rowsToCopy; ++y) {
        const int srcRow = (m_headRow + y) % m_historyRows;
        const QRgb *src = reinterpret_cast<const QRgb *>(m_historyImage.constScanLine(srcRow));
        QRgb *dst = reinterpret_cast<QRgb *>(newHistory.scanLine(y));

        if (newWidth == m_historyWidth) {
            std::memcpy(dst, src, static_cast<size_t>(newWidth) * sizeof(QRgb));
            continue;
        }

        for (int x = 0; x < newWidth; ++x) {
            const int sx = qBound(0,
                                  static_cast<int>((static_cast<qint64>(x) * m_historyWidth)
                                                   / qMax(1, newWidth)),
                                  m_historyWidth - 1);
            dst[x] = src[sx];
        }
    }
}

void FftWaterfallTextureItem::ensureHistoryLocked(int width, int rows)
{
    width = qMax(1, width);
    rows = qMax(1, rows);
    if (m_historyWidth == width && m_historyRows == rows
            && !m_historyImage.isNull())
        return;

    const int oldWidth = m_historyWidth;
    const int oldRows = m_historyRows;
    const int oldValidRows = m_validRows;
    const bool canPreserve = !m_historyImage.isNull()
            && oldWidth > 0 && oldRows > 0 && oldValidRows > 0;

    QImage newHistory(width, rows, QImage::Format_ARGB32);
    QImage newPresent(width, rows, QImage::Format_ARGB32);
    const QRgb bg = m_backgroundColor.rgb();
    newHistory.fill(bg);
    newPresent.fill(bg);

    int preservedRows = 0;
    if (canPreserve) {
        preservedRows = qMin(oldValidRows, rows);
        copyVisibleRowsToNewHistoryLocked(newHistory, width, rows, preservedRows);
        m_geometryPreservedRows += static_cast<quint64>(preservedRows);
    }

    m_historyWidth = width;
    m_historyRows = rows;
    m_headRow = 0;
    m_validRows = preservedRows;
    m_historyImage = newHistory;
    m_presentImage = newPresent;
    m_dirty = true;

    if ((oldWidth > 0 || oldRows > 0) && (oldWidth != width || oldRows != rows)) {
        qInfo() << "[DOA-WF-HISTORY-RESIZE]"
                << "old=" << QString::number(oldWidth) + "x" + QString::number(oldRows)
                << "new=" << QString::number(width) + "x" + QString::number(rows)
                << "oldValidRows=" << oldValidRows
                << "preservedRows=" << preservedRows;
    }
}

void FftWaterfallTextureItem::appendFrameLocked(const QVector<float> &frame, int outputWidth)
{
    Q_UNUSED(outputWidth)
    const int rows = qMax(1, static_cast<int>(std::ceil(height())));
    const int desiredWidth = qMax(1, static_cast<int>(std::ceil(width())));
    ensureHistoryLocked(desiredWidth, rows);
    if (m_historyImage.isNull() || m_historyWidth <= 0 || frame.size() < 2)
        return;

    const int h = qBound(1, m_rowHeightPx, m_historyRows);
    m_headRow = (m_headRow - h + m_historyRows) % m_historyRows;

    QVector<QRgb> row;
    row.resize(m_historyWidth);
    const int n = frame.size();
    for (int px = 0; px < m_historyWidth; ++px) {
        // Peak-preserving bucket pooling: each output pixel represents a bin
        // bucket and keeps the strongest carrier in that bucket.
        const int b0 = static_cast<int>(std::floor(static_cast<double>(px) * n
                                / static_cast<double>(m_historyWidth)));
        const int b1 = static_cast<int>(std::floor(static_cast<double>(px + 1) * n
                                / static_cast<double>(m_historyWidth)));
        const int start = qBound(0, b0, n - 1);
        const int stop = qBound(start + 1, std::max(b1, start + 1), n);
        float peak = frame.at(start);
        for (int i = start + 1; i < stop; ++i) {
            const float v = frame.at(i);
            if (std::isfinite(v) && (!std::isfinite(peak) || v > peak))
                peak = v;
        }
        row[px] = colorForDbLocked(peak);
    }

    for (int r = 0; r < h; ++r) {
        const int physicalRow = (m_headRow + r) % m_historyRows;
        std::memcpy(m_historyImage.scanLine(physicalRow), row.constData(),
                    static_cast<size_t>(m_historyWidth) * sizeof(QRgb));
    }
    m_validRows = std::min(m_historyRows, m_validRows + h);
    m_dirty = true;
    m_lastAcceptedRowMs = monotonicMsWaterfall();
}

void FftWaterfallTextureItem::appendArgbRowLocked(const QVector<quint32> &argbRow, int outputWidth)
{
    Q_UNUSED(outputWidth)
    const int rows = qMax(1, static_cast<int>(std::ceil(height())));
    const int desiredWidth = qMax(1, static_cast<int>(std::ceil(width())));
    ensureHistoryLocked(desiredWidth, rows);
    if (m_historyImage.isNull() || m_historyWidth <= 0 || argbRow.isEmpty())
        return;

    const int h = qBound(1, m_rowHeightPx, m_historyRows);
    m_headRow = (m_headRow - h + m_historyRows) % m_historyRows;

    QVector<QRgb> row;
    if (argbRow.size() == m_historyWidth) {
        row.resize(m_historyWidth);
        std::memcpy(row.data(), argbRow.constData(),
                    static_cast<size_t>(m_historyWidth) * sizeof(QRgb));
    } else {
        row.resize(m_historyWidth);
        const int n = argbRow.size();
        for (int px = 0; px < m_historyWidth; ++px) {
            const int src = qBound(0, static_cast<int>((static_cast<qint64>(px) * n)
                             / qMax(1, m_historyWidth)), n - 1);
            row[px] = static_cast<QRgb>(argbRow.at(src));
        }
    }

    for (int r = 0; r < h; ++r) {
        const int physicalRow = (m_headRow + r) % m_historyRows;
        std::memcpy(m_historyImage.scanLine(physicalRow), row.constData(),
                    static_cast<size_t>(m_historyWidth) * sizeof(QRgb));
    }
    m_validRows = std::min(m_historyRows, m_validRows + h);
    m_dirty = true;
    m_lastAcceptedRowMs = monotonicMsWaterfall();
}

void FftWaterfallTextureItem::appendTestPatternRowLocked(int outputWidth)
{
    const int rows = qMax(1, static_cast<int>(std::ceil(height())));
    ensureHistoryLocked(qMax(1, outputWidth), rows);
    if (m_historyImage.isNull() || m_historyWidth <= 0)
        return;

    const int h = qBound(1, m_rowHeightPx, m_historyRows);
    m_headRow = (m_headRow - h + m_historyRows) % m_historyRows;

    QVector<QRgb> row;
    row.resize(m_historyWidth);
    const int paletteCount = qMax(1, m_paletteRgb.size());
    const int phase = static_cast<int>((m_testPatternRows * 7U) % static_cast<quint64>(paletteCount));
    for (int px = 0; px < m_historyWidth; ++px) {
        const int idx = (px * paletteCount / qMax(1, m_historyWidth) + phase) % paletteCount;
        row[px] = m_paletteRgb.value(idx, qRgb(255, 255, 255));
    }

    for (int r = 0; r < h; ++r) {
        const int physicalRow = (m_headRow + r) % m_historyRows;
        std::memcpy(m_historyImage.scanLine(physicalRow), row.constData(),
                    static_cast<size_t>(m_historyWidth) * sizeof(QRgb));
    }
    m_validRows = std::min(m_historyRows, m_validRows + h);
    m_dirty = true;
    m_lastAcceptedRowMs = monotonicMsWaterfall();
    ++m_testPatternRows;
}


void FftWaterfallTextureItem::appendFirstVisibleCpuFallbackLocked(const QVector<float> &frame,
                                                                 int outputWidth,
                                                                 const char *reason)
{
    if (m_validRows > 0 || frame.size() < 2)
        return;

    // Visibility safety net: the CUDA/plugin path is asynchronous. If the first
    // row is delayed, cancelled by early geometry changes, or the worker/plugin
    // does not return a usable ARGB row, the native SceneGraph item previously
    // stayed visually empty even though Spectrum was live. Add exactly one local
    // CPU row to prove the data/texture path and keep the UI usable while the
    // CUDA pipeline warms up or falls back.
    appendFrameLocked(frame, outputWidth);
    ++m_cpuRows;
    ++m_bootstrapCpuRows;
    m_lastAcceptedRowMs = monotonicMsWaterfall();
    qInfo() << "[DOA-WF-FIRST-ROW]"
            << "reason=" << (reason ? reason : "unknown")
            << "bins=" << frame.size()
            << "outWidth=" << outputWidth
            << "validRows=" << m_validRows;
}

void FftWaterfallTextureItem::startComputeWorker()
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
    connect(this, &FftWaterfallTextureItem::processWaterfallRequested,
            worker, &SpectrumCudaWorker::processWaterfallRow,
            Qt::QueuedConnection);
    connect(worker, &SpectrumCudaWorker::backendReady,
            this, &FftWaterfallTextureItem::onComputeBackendReady,
            Qt::QueuedConnection);
    connect(worker, &SpectrumCudaWorker::waterfallRowReady,
            this, &FftWaterfallTextureItem::onWaterfallRowReady,
            Qt::QueuedConnection);

    m_computeThread.setObjectName(QStringLiteral("DoaCudaWaterfall"));
    m_computeThread.start();
}

void FftWaterfallTextureItem::stopComputeWorker()
{
    if (!m_computeThread.isRunning())
        return;

    if (m_computeWorker) {
        disconnect(this, nullptr, m_computeWorker.data(), nullptr);
        disconnect(m_computeWorker.data(), nullptr, this, nullptr);
    }

    m_computeThread.quit();
    if (!m_computeThread.wait(5000)) {
        qWarning() << "[DOA-CUDA-WATERFALL] worker shutdown exceeded 5s; waiting safely";
        m_computeThread.wait();
    }
    m_computeWorker.clear();
}

void FftWaterfallTextureItem::enqueueCudaFrame(const QVector<float> &frame, int outputWidth)
{
    startComputeWorker();

    QVector<quint32> palette;
    float minDb = -150.0f;
    float maxDb = -50.0f;
    quint64 generation = 0;
    bool dispatchNow = false;

    {
        QMutexLocker locker(&m_mutex);
        if (!m_renderEnabled || !m_cudaProcessingEnabled)
            return;

        ensureHistoryLocked(outputWidth, qMax(1, static_cast<int>(std::ceil(height()))));
        palette = paletteLutLocked();
        minDb = static_cast<float>(std::isfinite(m_minDb) ? m_minDb : -150.0);
        maxDb = static_cast<float>(std::isfinite(m_maxDb) ? m_maxDb : -50.0);
        if (maxDb <= minDb)
            maxDb = minDb + 1.0f;

        generation = ++m_generation;
        if (m_rowInFlight) {
            // Latest-frame-only queue: keep only the freshest row while the
            // CUDA worker is busy. Spectrum/waterfall displays should not replay
            // stale backlog; they should converge to live edge.
            m_pendingFrame = frame;
            m_pendingOutputWidth = outputWidth;
            m_pendingGeneration = generation;
            ++m_coalescedFrames;
            return;
        }

        // Ensure a visible row exists immediately. This is intentionally only
        // active while the history is empty; normal CUDA rows still carry the
        // live waterfall after the first completion.
        appendFirstVisibleCpuFallbackLocked(frame, outputWidth, "cuda-bootstrap");

        m_rowInFlight = true;
        m_inFlightOutputWidth = outputWidth;
        m_lastDispatchMs = monotonicMsWaterfall();
        ++m_cudaDispatches;
        dispatchNow = true;
    }

    if (dispatchNow) {
        emit processWaterfallRequested(generation, frame, outputWidth,
                                       minDb, maxDb, palette);
    }
}

void FftWaterfallTextureItem::onComputeBackendReady(const QString &backendName,
                                                    bool cudaActive,
                                                    const QString &detail)
{
    {
        QMutexLocker locker(&m_mutex);
        m_computeBackend = backendName;
        m_cudaAccelerationActive = cudaActive;
    }
    qInfo() << "[DOA-CUDA-WATERFALL]"
            << "backend=" << backendName
            << "cudaActive=" << cudaActive
            << "detail=" << detail;
    emit computeBackendChanged();
}

void FftWaterfallTextureItem::onWaterfallRowReady(quint64 generation,
                                                  QVector<float> dbRow,
                                                  QVector<quint32> argbRow,
                                                  bool usedCuda,
                                                  qint64 elapsedUsec)
{
    QVector<float> nextFrame;
    int nextWidth = 0;
    quint64 nextGeneration = 0;
    QVector<quint32> nextPalette;
    float nextMinDb = -150.0f;
    float nextMaxDb = -50.0f;
    bool dispatchNext = false;
    bool appended = false;

    {
        QMutexLocker locker(&m_mutex);
        const int desiredOutputWidth = qMax(1, static_cast<int>(std::ceil(width())));
        if (generation < m_cancelBeforeGeneration || !m_renderEnabled) {
            ++m_staleRows;
        } else if (!argbRow.isEmpty()) {
            // DOA-VIEWER1.7-r2 visibility recovery:
            // The first 1.7 implementation dropped a completed CUDA/CPU row whenever
            // a newer pending frame existed. With inputFps > workerFps this condition
            // can be true for every completion, so the texture never receives a row.
            // Waterfall history does not need to replay every frame; it only needs a
            // steady stream of completed rows. Append the completed row, then dispatch
            // the latest pending row below to converge to live edge.
            if (argbRow.size() != desiredOutputWidth) {
                ++m_widthNormalizedRows;
                m_lastArgbInputWidth = argbRow.size();
                m_lastDesiredOutputWidth = desiredOutputWidth;
            }
            appendArgbRowLocked(argbRow, desiredOutputWidth);
            appended = true;
            if (usedCuda) {
                ++m_cudaRows;
                m_lastCudaUsec = elapsedUsec;
            } else {
                ++m_cpuRows;
                m_lastCpuUsec = elapsedUsec;
            }
        } else if (!dbRow.isEmpty()) {
            // Defensive fallback: some plugin/ABI failures can return dB data without
            // a color row. Do not leave the waterfall invisible; colorize locally.
            appendFrameLocked(dbRow, desiredOutputWidth);
            appended = true;
            ++m_cpuRows;
            ++m_bootstrapCpuRows;
            m_lastCpuUsec = elapsedUsec;
            qWarning() << "[DOA-WF-RECOVERY] empty ARGB row; used local dB fallback"
                       << "generation=" << generation
                       << "dbBins=" << dbRow.size();
        } else {
            ++m_emptyRows;
        }

        m_rowInFlight = false;
        m_inFlightOutputWidth = 0;

        if (m_cudaProcessingEnabled && m_renderEnabled && !m_pendingFrame.isEmpty()) {
            nextFrame.swap(m_pendingFrame);
            nextWidth = m_pendingOutputWidth;
            nextGeneration = m_pendingGeneration;
            m_pendingOutputWidth = 0;
            m_pendingGeneration = 0;
            nextPalette = paletteLutLocked();
            nextMinDb = static_cast<float>(std::isfinite(m_minDb) ? m_minDb : -150.0);
            nextMaxDb = static_cast<float>(std::isfinite(m_maxDb) ? m_maxDb : -50.0);
            if (nextMaxDb <= nextMinDb)
                nextMaxDb = nextMinDb + 1.0f;
            m_rowInFlight = true;
            m_inFlightOutputWidth = nextWidth;
            m_lastDispatchMs = monotonicMsWaterfall();
            ++m_cudaDispatches;
            dispatchNext = true;
        }
    }

    if (appended)
        scheduleUpdate(false);

    if (dispatchNext) {
        emit processWaterfallRequested(nextGeneration, nextFrame, nextWidth,
                                       nextMinDb, nextMaxDb, nextPalette);
    }
}

bool FftWaterfallTextureItem::submitExternalFrame(const QVariantList &values)
{
    const int outputWidth = qMax(1, static_cast<int>(std::ceil(width())));

    bool useCudaPipeline = false;
    bool useTestPattern = false;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_renderEnabled)
            return false;
        ++m_framesIn;
        useTestPattern = m_testPatternBackend;
        useCudaPipeline = m_cudaProcessingEnabled && !m_forceCpuBackend && !m_testPatternBackend;
    }

    if (useTestPattern) {
        {
            QMutexLocker locker(&m_mutex);
            appendTestPatternRowLocked(outputWidth);
        }
        scheduleUpdate(false);
        return true;
    }

    if (values.size() < 2)
        return false;

    QVector<float> frame;
    frame.reserve(values.size());
    for (const QVariant &value : values) {
        bool ok = false;
        const float f = value.toFloat(&ok);
        frame.append(ok && std::isfinite(f) ? f : static_cast<float>(m_minDb));
    }
    if (frame.size() < 2)
        return false;

    if (useCudaPipeline) {
        enqueueCudaFrame(frame, outputWidth);
        // enqueueCudaFrame may append the first visible CPU bootstrap row while
        // the CUDA worker warms up. Request an update if that made the texture dirty.
        scheduleUpdate(false);

        // If the async worker is wedged/delayed and we still have no accepted
        // rows after the bootstrap window, append a CPU row so the user never
        // sees a permanently blank waterfall. Normal operation remains latest-
        // frame CUDA because this path stops after m_validRows becomes > 0.
        bool needDeferredCpuFallback = false;
        {
            QMutexLocker locker(&m_mutex);
            const qint64 now = monotonicMsWaterfall();
            needDeferredCpuFallback = (m_validRows == 0
                    && m_lastDispatchMs > 0
                    && now - m_lastDispatchMs > 750);
            if (needDeferredCpuFallback)
                appendFirstVisibleCpuFallbackLocked(frame, outputWidth, "cuda-timeout");
        }
        if (needDeferredCpuFallback)
            scheduleUpdate(false);
        return true;
    }

    {
        QMutexLocker locker(&m_mutex);
        appendFrameLocked(frame, outputWidth);
        ++m_cpuRows;
    }
    scheduleUpdate(false);
    return true;
}

void FftWaterfallTextureItem::clearHistory()
{
    {
        QMutexLocker locker(&m_mutex);
        const int w = qMax(1, static_cast<int>(std::ceil(width())));
        const int h = qMax(1, static_cast<int>(std::ceil(height())));
        if (width() > 1.0 && height() > 1.0)
            ensureHistoryLocked(w, h);
        if (!m_historyImage.isNull())
            m_historyImage.fill(m_backgroundColor.rgb());
        if (!m_presentImage.isNull())
            m_presentImage.fill(m_backgroundColor.rgb());
        m_headRow = 0;
        m_validRows = 0;
        m_dirty = true;
        m_updatePending = false;
        m_pendingFrame.clear();
        m_inFlightOutputWidth = 0;
        m_rowInFlight = false;
        m_cancelBeforeGeneration = ++m_generation;
        ++m_clearCount;
        qInfo() << "[DOA-WF-CLEAR]"
                << "size=" << QString::number(m_historyWidth) + "x" + QString::number(m_historyRows)
                << "generation=" << m_generation
                << "clearCount=" << m_clearCount;
    }
    scheduleUpdate(true);
}

void FftWaterfallTextureItem::scheduleUpdate(bool force)
{
    const qint64 nowMs = monotonicMsWaterfall();
    bool requestNow = false;
    int delayMs = 0;

    {
        QMutexLocker locker(&m_mutex);
        if (!m_renderEnabled)
            return;
        if (!m_dirty && !force)
            return;
        if (m_updatePending) {
            ++m_skipPending;
            return;
        }
        const int intervalMs = targetIntervalMs();
        if (!force && m_lastUpdateRequestMs > 0) {
            const qint64 elapsed = nowMs - m_lastUpdateRequestMs;
            if (elapsed < intervalMs) {
                delayMs = static_cast<int>(intervalMs - elapsed);
                ++m_skipBudget;
            } else {
                requestNow = true;
            }
        } else {
            requestNow = true;
        }
        if (requestNow) {
            m_updatePending = true;
            m_lastUpdateRequestMs = nowMs;
        }
    }

    if (requestNow) {
        update();
    } else if (delayMs > 0 && !m_budgetTimer.isActive()) {
        m_budgetTimer.start(delayMs);
    }
}

QSGNode *FftWaterfallTextureItem::updatePaintNode(QSGNode *oldNode,
                                                  UpdatePaintNodeData *)
{
    if (!window() || width() <= 1.0 || height() <= 1.0) {
        delete oldNode;
        return nullptr;
    }

    QImage image;
    quint64 framesIn = 0;
    quint64 skipBudget = 0;
    quint64 skipPending = 0;
    quint64 clearCount = 0;
    quint64 cudaRows = 0;
    quint64 cpuRows = 0;
    quint64 cudaDispatches = 0;
    quint64 coalescedFrames = 0;
    quint64 staleRows = 0;
    quint64 testPatternRows = 0;
    quint64 emptyRows = 0;
    quint64 bootstrapCpuRows = 0;
    quint64 deferredSubmits = 0;
    quint64 widthNormalizedRows = 0;
    quint64 geometryPreservedRows = 0;
    quint64 streamTextureFrames = 0;
    quint64 recreatedTextureFrames = 0;
    bool streamTextureUpload = true;
    QString uploadMode;
    int lastArgbInputWidth = 0;
    int lastDesiredOutputWidth = 0;
    qint64 lastCudaUsec = 0;
    qint64 lastCpuUsec = 0;
    QString computeBackend;
    bool cudaActive = false;
    int validRows = 0;
    int textureWidth = 0;
    int textureRows = 0;

    {
        QMutexLocker locker(&m_mutex);
        if (!m_renderEnabled) {
            m_updatePending = false;
            delete oldNode;
            return nullptr;
        }

        // Create a texture node as soon as geometry is valid, even before the
        // first data row. Returning nullptr here made the waterfall panel look
        // missing after StackView/layout clears or before the async CUDA row
        // came back. A background texture proves the SceneGraph path is alive.
        ensureHistoryLocked(qMax(1, static_cast<int>(std::ceil(width()))),
                            qMax(1, static_cast<int>(std::ceil(height()))));

        if (m_historyImage.isNull()) {
            m_updatePending = false;
            delete oldNode;
            return nullptr;
        }

        if (m_presentImage.size() != m_historyImage.size())
            m_presentImage = QImage(m_historyImage.size(), QImage::Format_ARGB32);

        const QRgb bg = m_backgroundColor.rgb();
        m_presentImage.fill(bg);
        const int rowsToCopy = std::min(m_validRows, m_historyRows);
        for (int y = 0; y < rowsToCopy; ++y) {
            const int srcRow = (m_headRow + y) % m_historyRows;
            std::memcpy(m_presentImage.scanLine(y), m_historyImage.constScanLine(srcRow),
                        static_cast<size_t>(m_historyWidth) * sizeof(QRgb));
        }

        // Implicit-share the prepared image instead of deep-copying a full frame.
        // The present image is only rebuilt in this render-thread function, while
        // producers append to m_historyImage. This removes one full CPU memcpy per
        // texture update before the OpenGL upload path below.
        image = m_presentImage;
        m_dirty = false;
        m_updatePending = false;
        ++m_textureUpdates;
        framesIn = m_framesIn;
        skipBudget = m_skipBudget;
        skipPending = m_skipPending;
        clearCount = m_clearCount;
        cudaRows = m_cudaRows;
        cpuRows = m_cpuRows;
        cudaDispatches = m_cudaDispatches;
        coalescedFrames = m_coalescedFrames;
        staleRows = m_staleRows;
        testPatternRows = m_testPatternRows;
        emptyRows = m_emptyRows;
        bootstrapCpuRows = m_bootstrapCpuRows;
        deferredSubmits = m_deferredSubmits;
        widthNormalizedRows = m_widthNormalizedRows;
        geometryPreservedRows = m_geometryPreservedRows;
        streamTextureFrames = m_streamTextureFrames;
        recreatedTextureFrames = m_recreatedTextureFrames;
        streamTextureUpload = m_streamTextureUpload;
        uploadMode = m_uploadMode;
        lastArgbInputWidth = m_lastArgbInputWidth;
        lastDesiredOutputWidth = m_lastDesiredOutputWidth;
        lastCudaUsec = m_lastCudaUsec;
        lastCpuUsec = m_lastCpuUsec;
        computeBackend = m_computeBackend;
        cudaActive = m_cudaAccelerationActive;
        validRows = m_validRows;
        textureWidth = m_historyWidth;
        textureRows = m_historyRows;
    }

    // DOA-VIEWER1.8:
    // Keep a persistent SceneGraph texture/node by default.  This avoids
    // allocating a new QSGTexture and deleting/recreating a QSG node for every
    // waterfall frame.  It is still Qt/GL owned (not CUDA-GL zero-copy yet), but
    // it removes the biggest remaining CPU/driver churn and creates a stable
    // insertion point for the later PBO/CUDA interop step.  If a target driver
    // shows any issue, set ISCAN_DOA_WATERFALL_UPLOAD=safe to use the previous
    // conservative full texture recreation path.
    QSGSimpleTextureNode *node = nullptr;
    if (streamTextureUpload) {
        node = dynamic_cast<QSGSimpleTextureNode *>(oldNode);
        DoaWaterfallStreamingTexture *streamTexture = nullptr;
        if (node)
            streamTexture = dynamic_cast<DoaWaterfallStreamingTexture *>(node->texture());
        if (!node || !streamTexture) {
            delete oldNode;
            node = new QSGSimpleTextureNode;
            streamTexture = new DoaWaterfallStreamingTexture;
            node->setTexture(streamTexture);
            node->setOwnsTexture(true);
            node->setFiltering(QSGTexture::Nearest);
        }
        streamTexture->setImage(image);
        node->setRect(boundingRect());
        node->markDirty(QSGNode::DirtyMaterial | QSGNode::DirtyGeometry);
        ++m_streamTextureFrames;
    } else {
        QSGTexture *texture = window()->createTextureFromImage(image);
        if (!texture) {
            delete oldNode;
            return nullptr;
        }
        delete oldNode;
        node = new QSGSimpleTextureNode;
        node->setTexture(texture);
        node->setOwnsTexture(true);
        node->setFiltering(QSGTexture::Nearest);
        node->setRect(boundingRect());
        ++m_recreatedTextureFrames;
    }

    if (m_statsTimer.elapsed() >= 5000) {
        const double sec = std::max(0.001, m_statsTimer.elapsed() / 1000.0);
        const double inFps = static_cast<double>(framesIn) / sec;
        const double texFps = static_cast<double>(m_textureUpdates) / sec;
        qInfo() << "[DOA-GPU-WATERFALL]"
                << "textureFps=" << QString::number(texFps, 'f', 1)
                << "inputFps=" << QString::number(inFps, 'f', 1)
                << "targetFps=" << targetFps()
                << "validRows=" << validRows
                << "texture=" << QString::number(textureWidth) + "x" + QString::number(textureRows)
                << "skipBudget=" << skipBudget
                << "skipPending=" << skipPending
                << "clear=" << clearCount
                << "backend=" << computeBackend
                << "cudaActive=" << cudaActive
                << "cudaRows=" << cudaRows
                << "cpuRows=" << cpuRows
                << "dispatches=" << cudaDispatches
                << "coalesced=" << coalescedFrames
                << "staleRows=" << staleRows
                << "testPatternRows=" << testPatternRows
                << "emptyRows=" << emptyRows
                << "bootstrapCpuRows=" << bootstrapCpuRows
                << "deferredSubmits=" << deferredSubmits
                << "widthNormalizedRows=" << widthNormalizedRows
                << "geometryPreservedRows=" << geometryPreservedRows
                << "uploadMode=" << uploadMode
                << "streamTextureUpload=" << streamTextureUpload
                << "streamFrames=" << streamTextureFrames
                << "recreateFrames=" << recreatedTextureFrames
                << "argbInW=" << lastArgbInputWidth
                << "desiredW=" << lastDesiredOutputWidth
                << "lastCudaUsec=" << lastCudaUsec
                << "lastCpuUsec=" << lastCpuUsec
                << "sceneGraph=1"
                << "qPainter=0"
                << "cudaInterop=upload-stage";
        QMutexLocker locker(&m_mutex);
        m_framesIn = 0;
        m_textureUpdates = 0;
        m_skipBudget = 0;
        m_skipPending = 0;
        m_clearCount = 0;
        m_cudaRows = 0;
        m_cpuRows = 0;
        m_cudaDispatches = 0;
        m_coalescedFrames = 0;
        m_staleRows = 0;
        m_testPatternRows = 0;
        m_emptyRows = 0;
        m_bootstrapCpuRows = 0;
        m_deferredSubmits = 0;
        m_widthNormalizedRows = 0;
        m_geometryPreservedRows = 0;
        m_streamTextureFrames = 0;
        m_recreatedTextureFrames = 0;
        m_lastArgbInputWidth = 0;
        m_lastDesiredOutputWidth = 0;
        m_statsTimer.restart();
    }

    return node;
}

void FftWaterfallTextureItem::geometryChanged(const QRectF &newGeometry,
                                               const QRectF &oldGeometry)
{
    QQuickItem::geometryChanged(newGeometry, oldGeometry);

    const int newW = qMax(0, static_cast<int>(std::ceil(newGeometry.width())));
    const int newH = qMax(0, static_cast<int>(std::ceil(newGeometry.height())));
    const int oldW = qMax(0, static_cast<int>(std::ceil(oldGeometry.width())));
    const int oldH = qMax(0, static_cast<int>(std::ceil(oldGeometry.height())));
    if (newW == oldW && newH == oldH)
        return;

    {
        QMutexLocker locker(&m_mutex);
        // Do not destroy stream lifecycle on layout jitter.  Geometry changes
        // resize/preserve the backing ring and let any in-flight CUDA row land;
        // appendArgbRowLocked() resamples late rows to the current item width.
        if (newW > 1 && newH > 1) {
            ensureHistoryLocked(newW, newH);
            m_dirty = true;
        }
        m_updatePending = false;
        qInfo() << "[DOA-WF-GEOM]"
                << "old=" << QString::number(oldW) + "x" + QString::number(oldH)
                << "new=" << QString::number(newW) + "x" + QString::number(newH)
                << "validRows=" << m_validRows
                << "historyRows=" << m_historyRows
                << "rowInFlight=" << m_rowInFlight
                << "generation=" << m_generation
                << "lifecycleReset=0";
    }
    scheduleUpdate(true);
}
