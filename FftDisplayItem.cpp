#include "FftDisplayItem.h"

#include "websocketclient.h"
#include "CrashDiagnostics.h"

#include <QPainter>
#include <QMutexLocker>
#include <QPen>
#include <QPolygonF>
#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
bool sameReal(double a, double b)
{
    return qFuzzyCompare(a + 1.0, b + 1.0);
}
}

FftDisplayItem::FftDisplayItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(false);
    setOpaquePainting(false);
    // QQuickPaintedItem::Image is deliberately used for R18. Frame/image state
    // is mutex-protected because paint() may run on the Qt Quick render thread.
    // This avoids the old QML Canvas/QV4 FFT rendering path entirely.
    setRenderTarget(QQuickPaintedItem::Image);
    m_palette = { QColor(0, 0, 0), QColor(0, 0, 128), QColor(0, 128, 255),
                  QColor(0, 255, 128), QColor(255, 255, 0), QColor(255, 0, 0) };
}

FftDisplayItem::~FftDisplayItem()
{
    disconnectBackend();
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
                m_maxHoldFrame.clear();
                m_waterfallImage = QImage();
            }
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
        if (m_mode == Waterfall)
            ensureWaterfallImage();
    }
    emit modeChanged();
    update();
}

void FftDisplayItem::setRenderEnabled(bool enabled)
{
    bool clearWaterfall = false;
    {
        QMutexLocker locker(&m_dataMutex);
        if (m_renderEnabled == enabled)
            return;
        m_renderEnabled = enabled;
        clearWaterfall = !enabled && m_mode == Waterfall;
    }
    if (clearWaterfall)
        clearHistory();
    emit renderEnabledChanged();
    update();
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
    bool clearWaterfall = false;
    {
        QMutexLocker locker(&m_dataMutex);
        if (m_clearBeforeNextPaint == clear)
            return;
        m_clearBeforeNextPaint = clear;
        clearWaterfall = clear && m_mode == Waterfall;
    }
    emit clearBeforeNextPaintChanged();
    if (clearWaterfall)
        clearHistory();
}

void FftDisplayItem::setMinDb(double value)
{
    if (!std::isfinite(value))
        return;
    {
        QMutexLocker locker(&m_dataMutex);
        if (sameReal(m_minDb, value))
            return;
        m_minDb = value;
    }
    emit levelsChanged();
    update();
}

void FftDisplayItem::setMaxDb(double value)
{
    if (!std::isfinite(value))
        return;
    {
        QMutexLocker locker(&m_dataMutex);
        if (sameReal(m_maxDb, value))
            return;
        m_maxDb = value;
    }
    emit levelsChanged();
    update();
}

void FftDisplayItem::setFullStartFreq(double value)
{
    if (!std::isfinite(value))
        return;
    {
        QMutexLocker locker(&m_dataMutex);
        if (sameReal(m_fullStartFreq, value))
            return;
        m_fullStartFreq = value;
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
    }
    emit frequencyMappingChanged();
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
    }
    emit frequencyMappingChanged();
    update();
}

void FftDisplayItem::setSampleRate(double value)
{
    if (!std::isfinite(value) || value <= 0.0)
        return;
    {
        QMutexLocker locker(&m_dataMutex);
        if (sameReal(m_sampleRate, value))
            return;
        m_sampleRate = value;
    }
    emit frequencyMappingChanged();
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
            if (ok)
                color = QColor(static_cast<int>((packed >> 16) & 0xffU),
                               static_cast<int>((packed >> 8) & 0xffU),
                               static_cast<int>(packed & 0xffU));
        }
        if (color.isValid())
            parsed.append(color);
    }

    if (parsed.isEmpty())
        parsed.append(Qt::black);
    {
        QMutexLocker locker(&m_dataMutex);
        if (parsed == m_palette)
            return;
        m_palette = parsed;
    }
    emit paletteChanged();
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

void FftDisplayItem::clearHistory()
{
    bool clearedFlag = false;
    {
        QMutexLocker locker(&m_dataMutex);
        ensureWaterfallImage();
        if (!m_waterfallImage.isNull())
            m_waterfallImage.fill(Qt::transparent);
        if (m_clearBeforeNextPaint) {
            m_clearBeforeNextPaint = false;
            clearedFlag = true;
        }
    }
    if (clearedFlag)
        emit clearBeforeNextPaintChanged();
    update();
}

void FftDisplayItem::onSpectrumFrame(const QVector<float> &frame)
{
    CrashDiagnostics::checkpoint(CrashDiagnostics::CpNativeSpectrumFrame);
    {
        QMutexLocker locker(&m_dataMutex);
        if (m_mode != Spectrum || !m_renderEnabled)
            return;
        m_spectrumFrame = frame;
    }
    update();
}

void FftDisplayItem::onWaterfallFrame(const QVector<float> &frame)
{
    CrashDiagnostics::checkpoint(CrashDiagnostics::CpNativeWaterfallFrame);
    {
        QMutexLocker locker(&m_dataMutex);
        if (m_mode != Waterfall || !m_renderEnabled)
            return;
        appendWaterfallRow(frame);
    }
    update();
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

void FftDisplayItem::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChanged(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size()) {
        QMutexLocker locker(&m_dataMutex);
        if (m_mode == Waterfall) {
            m_waterfallImage = QImage();
            ensureWaterfallImage();
        }
    }
}

void FftDisplayItem::ensureWaterfallImage()
{
    const int imageWidth = std::max(1, static_cast<int>(std::ceil(width())));
    const int imageHeight = std::max(1, static_cast<int>(std::ceil(height())));
    if (!m_waterfallImage.isNull()
            && m_waterfallImage.width() == imageWidth
            && m_waterfallImage.height() == imageHeight)
        return;

    m_waterfallImage = QImage(imageWidth, imageHeight, QImage::Format_ARGB32_Premultiplied);
    m_waterfallImage.fill(Qt::transparent);
}

void FftDisplayItem::appendWaterfallRow(const QVector<float> &frame)
{
    if (frame.isEmpty())
        return;

    ensureWaterfallImage();
    if (m_waterfallImage.isNull())
        return;

    const int imageWidth = m_waterfallImage.width();
    const int imageHeight = m_waterfallImage.height();
    const int bytesPerLine = m_waterfallImage.bytesPerLine();

    if (m_clearBeforeNextPaint) {
        m_waterfallImage.fill(Qt::transparent);
        m_clearBeforeNextPaint = false;
    } else if (imageHeight > 1) {
        std::memmove(m_waterfallImage.bits() + bytesPerLine,
                     m_waterfallImage.bits(),
                     static_cast<size_t>(bytesPerLine) * static_cast<size_t>(imageHeight - 1));
    }

    QRgb *row = reinterpret_cast<QRgb *>(m_waterfallImage.scanLine(0));
    const int start = mappedStartIndex(frame.size());
    const int end = mappedEndIndex(frame.size(), start);
    const int visible = std::max(1, end - start + 1);
    const double range = std::max(1e-9, m_maxDb - m_minDb);
    const int paletteLast = std::max(0, m_palette.size() - 1);

    for (int x = 0; x < imageWidth; ++x) {
        const int sourceBegin = start + (static_cast<qint64>(x) * visible) / imageWidth;
        int sourceEnd = start + (static_cast<qint64>(x + 1) * visible) / imageWidth;
        sourceEnd = std::max(sourceBegin + 1, sourceEnd);
        sourceEnd = std::min(end + 1, sourceEnd);

        float peak = frame.at(std::min(sourceBegin, end));
        if (!std::isfinite(peak))
            peak = static_cast<float>(m_minDb);
        for (int i = sourceBegin + 1; i < sourceEnd; ++i) {
            const float value = frame.at(i);
            if (std::isfinite(value) && value > peak)
                peak = value;
        }

        const double clamped = std::max(m_minDb, std::min(m_maxDb, static_cast<double>(peak)));
        const double norm = (clamped - m_minDb) / range;
        const int paletteIndex = std::max(0, std::min(paletteLast,
            static_cast<int>(std::floor(norm * paletteLast))));
        row[x] = m_palette.at(paletteIndex).rgba();
    }
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
    QMutexLocker locker(&m_dataMutex);
    if (!m_renderEnabled || m_spectrumFrame.size() < 2)
        return;

    const qreal itemWidth = width();
    const qreal itemHeight = height();
    if (itemWidth <= 0.0 || itemHeight <= 0.0)
        return;

    const qreal xAxisHeight = std::min<qreal>(18.0, itemHeight);
    const qreal plotHeight = std::max<qreal>(1.0, itemHeight - xAxisHeight);
    const int start = std::min(m_spectrumFrame.size() - 2,
                               mappedStartIndex(m_spectrumFrame.size()));
    const int end = std::max(start + 1, mappedEndIndex(m_spectrumFrame.size(), start));
    const int visible = std::max(2, end - start + 1);

    const int maxPoints = std::max(2, static_cast<int>(std::ceil(itemWidth)));
    const int step = std::max(1, visible / maxPoints);

    auto buildPolyline = [&](const QVector<float> &frame) {
        QPolygonF polyline;
        if (frame.size() != m_spectrumFrame.size())
            return polyline;
        polyline.reserve((visible / step) + 2);
        for (int i = start; i <= end; i += step) {
            const qreal x = static_cast<qreal>(i - start)
                          / static_cast<qreal>(std::max(1, end - start)) * itemWidth;
            polyline.append(QPointF(x, yForDb(frame.at(i), plotHeight)));
        }
        if (polyline.isEmpty() || polyline.last().x() < itemWidth)
            polyline.append(QPointF(itemWidth, yForDb(frame.at(end), plotHeight)));
        return polyline;
    };

    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setPen(QPen(m_spectrumColor, 1.0));
    const QPolygonF spectrum = buildPolyline(m_spectrumFrame);
    if (spectrum.size() >= 2)
        painter->drawPolyline(spectrum);

    if (m_showMaxHold && m_maxHoldFrame.size() == m_spectrumFrame.size()) {
        painter->setPen(QPen(m_maxHoldColor, 1.0));
        const QPolygonF maxHold = buildPolyline(m_maxHoldFrame);
        if (maxHold.size() >= 2)
            painter->drawPolyline(maxHold);
    }
}

void FftDisplayItem::paintWaterfall(QPainter *painter)
{
    CrashDiagnostics::checkpoint(CrashDiagnostics::CpNativeWaterfallPaint);
    QMutexLocker locker(&m_dataMutex);
    if (!m_renderEnabled)
        return;
    ensureWaterfallImage();
    if (!m_waterfallImage.isNull())
        painter->drawImage(QRectF(0.0, 0.0, width(), height()), m_waterfallImage);
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
}
