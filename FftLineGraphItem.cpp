#include "FftLineGraphItem.h"

#include <QDebug>
#include <QMutexLocker>
#include <QSGFlatColorMaterial>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGNode>
#include <QtGlobal>

#include <algorithm>
#include <chrono>
#include <cmath>

namespace {
qint64 monotonicMsLineGraph()
{
    using Clock = std::chrono::steady_clock;
    return std::chrono::duration_cast<std::chrono::milliseconds>(
                Clock::now().time_since_epoch()).count();
}

bool sameRealLocal(double a, double b)
{
    return qFuzzyCompare(a + 1.0, b + 1.0);
}

qreal yForDbLocal(float value, double minDb, double maxDb, qreal height)
{
    const double rangeDb = std::max(1.0e-9, maxDb - minDb);
    double db = std::isfinite(value) ? static_cast<double>(value) : minDb;
    db = std::max(minDb, std::min(maxDb, db));
    return static_cast<qreal>(height - ((db - minDb) / rangeDb) * height);
}
}

FftLineGraphItem::FftLineGraphItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(QQuickItem::ItemHasContents, true);

    m_statsStartMs = monotonicMsLineGraph();
    m_budgetTimer.setSingleShot(true);
    m_budgetTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_budgetTimer, &QTimer::timeout, this, [this]() {
        scheduleUpdate(true);
    });

    qInfo() << "[DOA-VIEWER1.5-GPU-SPECTRUM]"
            << "sceneGraphLine=1"
            << "qPainterSpectrum=0"
            << "peakPoolPerPixel=1"
            << "coalescedUpdate=1"
            << "defaultTargetFps=" << m_targetFps;
}

FftLineGraphItem::~FftLineGraphItem()
{
    m_budgetTimer.stop();
    QMutexLocker locker(&m_mutex);
    m_renderEnabled = false;
    m_updatePending = false;
    m_frame.clear();
}

void FftLineGraphItem::releaseResources()
{
    // Prevent a queued budget timer from scheduling another update while Qt is
    // releasing scenegraph resources for the page/window. This keeps StackView
    // navigation from racing stale QSG updates.
    m_budgetTimer.stop();
    QMutexLocker locker(&m_mutex);
    m_updatePending = false;
}

void FftLineGraphItem::setRenderEnabled(bool enabled)
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_renderEnabled == enabled)
            return;
        m_renderEnabled = enabled;
        if (!enabled)
            m_updatePending = false;
    }
    if (!enabled)
        m_budgetTimer.stop();
    emit renderEnabledChanged();
    if (enabled)
        scheduleUpdate(true);
    else
        update();
}

void FftLineGraphItem::setTargetFps(int fps)
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

void FftLineGraphItem::setMinDb(double value)
{
    if (!std::isfinite(value))
        return;
    {
        QMutexLocker locker(&m_mutex);
        if (sameRealLocal(m_minDb, value))
            return;
        m_minDb = value;
    }
    emit levelsChanged();
    scheduleUpdate(true);
}

void FftLineGraphItem::setMaxDb(double value)
{
    if (!std::isfinite(value))
        return;
    {
        QMutexLocker locker(&m_mutex);
        if (sameRealLocal(m_maxDb, value))
            return;
        m_maxDb = value;
    }
    emit levelsChanged();
    scheduleUpdate(true);
}

void FftLineGraphItem::setFullStartFreq(double value)
{
    if (!std::isfinite(value))
        return;
    {
        QMutexLocker locker(&m_mutex);
        if (sameRealLocal(m_fullStartFreq, value))
            return;
        m_fullStartFreq = value;
    }
    emit frequencyMappingChanged();
    scheduleUpdate(true);
}

void FftLineGraphItem::setViewStartFreq(double value)
{
    if (!std::isfinite(value))
        return;
    {
        QMutexLocker locker(&m_mutex);
        if (sameRealLocal(m_viewStartFreq, value))
            return;
        m_viewStartFreq = value;
    }
    emit frequencyMappingChanged();
    scheduleUpdate(true);
}

void FftLineGraphItem::setViewStopFreq(double value)
{
    if (!std::isfinite(value))
        return;
    {
        QMutexLocker locker(&m_mutex);
        if (sameRealLocal(m_viewStopFreq, value))
            return;
        m_viewStopFreq = value;
    }
    emit frequencyMappingChanged();
    scheduleUpdate(true);
}

void FftLineGraphItem::setSampleRate(double value)
{
    if (!std::isfinite(value) || value <= 0.0)
        return;
    {
        QMutexLocker locker(&m_mutex);
        if (sameRealLocal(m_sampleRate, value))
            return;
        m_sampleRate = value;
    }
    emit frequencyMappingChanged();
    scheduleUpdate(true);
}

void FftLineGraphItem::setPlotTopInset(double value)
{
    if (!std::isfinite(value))
        return;
    value = std::max(0.0, value);
    {
        QMutexLocker locker(&m_mutex);
        if (sameRealLocal(m_plotTopInset, value))
            return;
        // Kept for FftDisplayItem API compatibility. FftLineGraphItem is already
        // positioned inside the plot rect by QML, so this value is diagnostic only.
        m_plotTopInset = value;
    }
    emit plotGeometryChanged();
}

void FftLineGraphItem::setSpectrumColor(const QColor &color)
{
    if (!color.isValid())
        return;
    {
        QMutexLocker locker(&m_mutex);
        if (m_spectrumColor == color)
            return;
        m_spectrumColor = color;
    }
    emit colorsChanged();
    scheduleUpdate(true);
}

void FftLineGraphItem::setFillColor(const QColor &color)
{
    if (!color.isValid())
        return;
    {
        QMutexLocker locker(&m_mutex);
        if (m_fillColor == color)
            return;
        m_fillColor = color;
    }
    emit colorsChanged();
    scheduleUpdate(true);
}

int FftLineGraphItem::targetIntervalMs() const noexcept
{
    return qMax(1, static_cast<int>(std::floor(1000.0
                / static_cast<double>(qMax(1, m_targetFps)))));
}

void FftLineGraphItem::scheduleUpdate(bool force)
{
    const qint64 nowMs = monotonicMsLineGraph();
    int delayMs = 0;
    bool requestNow = false;

    {
        QMutexLocker locker(&m_mutex);
        if (!m_renderEnabled)
            return;
        if (m_frame.size() < 2 && !force)
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

bool FftLineGraphItem::submitExternalFrame(const QVariantList &values)
{
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

    {
        QMutexLocker locker(&m_mutex);
        m_frame = frame;
        ++m_framesIn;
    }

    scheduleUpdate(false);
    return true;
}

void FftLineGraphItem::clearFrame()
{
    {
        QMutexLocker locker(&m_mutex);
        m_frame.clear();
        m_updatePending = false;
    }
    update();
}

void FftLineGraphItem::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChanged(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size())
        scheduleUpdate(true);
}

QSGNode *FftLineGraphItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data)
{
    Q_UNUSED(data)
    delete oldNode;

    QVector<float> frame;
    double minDb = -150.0;
    double maxDb = -50.0;
    double fullStartFreq = 0.0;
    double viewStartFreq = 0.0;
    double viewStopFreq = 1.0;
    double sampleRate = 1.0;
    QColor spectrumColor;
    QColor fillColor;
    quint64 framesIn = 0;
    quint64 paints = 0;
    quint64 skipBudget = 0;
    quint64 skipPending = 0;
    bool shouldLog = false;
    qint64 elapsedMs = 0;

    {
        QMutexLocker locker(&m_mutex);
        m_updatePending = false;
        if (!m_renderEnabled || m_frame.size() < 2)
            return nullptr;
        frame = m_frame;
        minDb = m_minDb;
        maxDb = m_maxDb;
        fullStartFreq = m_fullStartFreq;
        viewStartFreq = m_viewStartFreq;
        viewStopFreq = m_viewStopFreq;
        sampleRate = m_sampleRate;
        spectrumColor = m_spectrumColor;
        fillColor = m_fillColor;
        ++m_paints;

        elapsedMs = monotonicMsLineGraph() - m_statsStartMs;
        if (elapsedMs >= 5000) {
            framesIn = m_framesIn;
            paints = m_paints;
            skipBudget = m_skipBudget;
            skipPending = m_skipPending;
            m_framesIn = 0;
            m_paints = 0;
            m_skipBudget = 0;
            m_skipPending = 0;
            m_statsStartMs = monotonicMsLineGraph();
            shouldLog = true;
        }
    }

    const qreal itemWidth = width();
    const qreal itemHeight = height();
    if (itemWidth <= 1.0 || itemHeight <= 1.0 || frame.size() < 2)
        return nullptr;

    const int count = frame.size();
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

    const int targetPoints = qBound(32,
                                    static_cast<int>(std::ceil(itemWidth)),
                                    2048);
    const int buckets = std::max(2, std::min(targetPoints, visible));

    QVector<QPointF> points;
    points.reserve(buckets);
    for (int b = 0; b < buckets; ++b) {
        const int bucketStart = start + static_cast<int>(std::floor(
            static_cast<double>(b) * static_cast<double>(visible) / static_cast<double>(buckets)));
        const int bucketEnd = std::min(end, start + static_cast<int>(std::floor(
            static_cast<double>(b + 1) * static_cast<double>(visible) / static_cast<double>(buckets))) - 1);

        float peak = static_cast<float>(minDb);
        bool valid = false;
        for (int i = bucketStart; i <= bucketEnd; ++i) {
            const float v = frame.at(i);
            if (!std::isfinite(v))
                continue;
            if (!valid || v > peak) {
                peak = v;
                valid = true;
            }
        }
        if (!valid)
            peak = static_cast<float>(minDb);

        const qreal x = (buckets <= 1)
                ? 0.0
                : (static_cast<qreal>(b) / static_cast<qreal>(buckets - 1)) * itemWidth;
        const qreal y = yForDbLocal(peak, minDb, maxDb, itemHeight);
        points.append(QPointF(x, y));
    }

    if (points.size() < 2)
        return nullptr;

    QSGNode *root = new QSGNode();

    // Fill under the line with a triangle strip. This replaces the previous
    // QPainter gradient/fill polygon with GPU scenegraph geometry.
    QSGGeometryNode *fillNode = new QSGGeometryNode();
    QSGGeometry *fillGeometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(),
                                               points.size() * 2);
    fillGeometry->setDrawingMode(QSGGeometry::DrawTriangleStrip);
    QSGGeometry::Point2D *fillVertices = fillGeometry->vertexDataAsPoint2D();
    for (int i = 0; i < points.size(); ++i) {
        fillVertices[i * 2].set(static_cast<float>(points.at(i).x()),
                                static_cast<float>(points.at(i).y()));
        fillVertices[i * 2 + 1].set(static_cast<float>(points.at(i).x()),
                                    static_cast<float>(itemHeight));
    }
    QSGFlatColorMaterial *fillMaterial = new QSGFlatColorMaterial();
    fillMaterial->setColor(fillColor);
    fillNode->setGeometry(fillGeometry);
    fillNode->setMaterial(fillMaterial);
    fillNode->setFlag(QSGNode::OwnsGeometry);
    fillNode->setFlag(QSGNode::OwnsMaterial);
    root->appendChildNode(fillNode);

    QSGGeometryNode *lineNode = new QSGGeometryNode();
    QSGGeometry *lineGeometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(),
                                               points.size());
    lineGeometry->setDrawingMode(QSGGeometry::DrawLineStrip);
    lineGeometry->setLineWidth(1.4f);
    QSGGeometry::Point2D *lineVertices = lineGeometry->vertexDataAsPoint2D();
    for (int i = 0; i < points.size(); ++i) {
        lineVertices[i].set(static_cast<float>(points.at(i).x()),
                            static_cast<float>(points.at(i).y()));
    }
    QSGFlatColorMaterial *lineMaterial = new QSGFlatColorMaterial();
    lineMaterial->setColor(spectrumColor);
    lineNode->setGeometry(lineGeometry);
    lineNode->setMaterial(lineMaterial);
    lineNode->setFlag(QSGNode::OwnsGeometry);
    lineNode->setFlag(QSGNode::OwnsMaterial);
    root->appendChildNode(lineNode);

    if (shouldLog) {
        const double sec = std::max(0.001, static_cast<double>(elapsedMs) / 1000.0);
        qInfo() << "[DOA-GPU-SPECTRUM]"
                << "paintFps=" << QString::number(static_cast<double>(paints) / sec, 'f', 1)
                << "inputFps=" << QString::number(static_cast<double>(framesIn) / sec, 'f', 1)
                << "targetFps=" << m_targetFps
                << "bins=" << count
                << "points=" << points.size()
                << "skipBudget=" << skipBudget
                << "skipPending=" << skipPending
                << "sceneGraph=1"
                << "qPainter=0";
    }

    return root;
}
