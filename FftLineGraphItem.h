#pragma once

#include <QColor>
#include <QMutex>
#include <QQuickItem>
#include <QTimer>
#include <QVariantList>
#include <QVector>

class QSGNode;

class FftLineGraphItem : public QQuickItem
{
    Q_OBJECT
public:
    Q_PROPERTY(bool renderEnabled READ renderEnabled WRITE setRenderEnabled NOTIFY renderEnabledChanged)
    Q_PROPERTY(int targetFps READ targetFps WRITE setTargetFps NOTIFY targetFpsChanged)
    Q_PROPERTY(double minDb READ minDb WRITE setMinDb NOTIFY levelsChanged)
    Q_PROPERTY(double maxDb READ maxDb WRITE setMaxDb NOTIFY levelsChanged)
    Q_PROPERTY(double fullStartFreq READ fullStartFreq WRITE setFullStartFreq NOTIFY frequencyMappingChanged)
    Q_PROPERTY(double viewStartFreq READ viewStartFreq WRITE setViewStartFreq NOTIFY frequencyMappingChanged)
    Q_PROPERTY(double viewStopFreq READ viewStopFreq WRITE setViewStopFreq NOTIFY frequencyMappingChanged)
    Q_PROPERTY(double sampleRate READ sampleRate WRITE setSampleRate NOTIFY frequencyMappingChanged)
    Q_PROPERTY(double plotTopInset READ plotTopInset WRITE setPlotTopInset NOTIFY plotGeometryChanged)
    Q_PROPERTY(QColor spectrumColor READ spectrumColor WRITE setSpectrumColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor fillColor READ fillColor WRITE setFillColor NOTIFY colorsChanged)

    explicit FftLineGraphItem(QQuickItem *parent = nullptr);
    ~FftLineGraphItem() override;

    bool renderEnabled() const noexcept { return m_renderEnabled; }
    void setRenderEnabled(bool enabled);

    int targetFps() const noexcept { return m_targetFps; }
    void setTargetFps(int fps);

    double minDb() const noexcept { return m_minDb; }
    void setMinDb(double value);
    double maxDb() const noexcept { return m_maxDb; }
    void setMaxDb(double value);

    double fullStartFreq() const noexcept { return m_fullStartFreq; }
    void setFullStartFreq(double value);
    double viewStartFreq() const noexcept { return m_viewStartFreq; }
    void setViewStartFreq(double value);
    double viewStopFreq() const noexcept { return m_viewStopFreq; }
    void setViewStopFreq(double value);
    double sampleRate() const noexcept { return m_sampleRate; }
    void setSampleRate(double value);
    double plotTopInset() const noexcept { return m_plotTopInset; }
    void setPlotTopInset(double value);

    QColor spectrumColor() const noexcept { return m_spectrumColor; }
    void setSpectrumColor(const QColor &color);
    QColor fillColor() const noexcept { return m_fillColor; }
    void setFillColor(const QColor &color);

    // DOA-VIEWER1.5: feed live FFT magnitudes directly to a Qt SceneGraph
    // geometry item. QML still owns controls/labels/target overlay, but the
    // high-rate spectrum trace/fill no longer goes through QQuickPaintedItem
    // or QPainter.
    Q_INVOKABLE bool submitExternalFrame(const QVariantList &values);
    Q_INVOKABLE void clearFrame();

signals:
    void renderEnabledChanged();
    void targetFpsChanged();
    void levelsChanged();
    void frequencyMappingChanged();
    void plotGeometryChanged();
    void colorsChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;
    void releaseResources() override;
    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    void scheduleUpdate(bool force = false);
    int targetIntervalMs() const noexcept;

    mutable QMutex m_mutex;
    QVector<float> m_frame;

    bool m_renderEnabled = true;
    int m_targetFps = 25;
    bool m_updatePending = false;
    qint64 m_lastUpdateRequestMs = 0;

    double m_minDb = -150.0;
    double m_maxDb = -50.0;
    double m_fullStartFreq = 0.0;
    double m_viewStartFreq = 0.0;
    double m_viewStopFreq = 1.0;
    double m_sampleRate = 1.0;
    double m_plotTopInset = 0.0;

    QColor m_spectrumColor = QColor(QStringLiteral("#38BDF8"));
    QColor m_fillColor = QColor(56, 189, 248, 46);

    QTimer m_budgetTimer;

    quint64 m_framesIn = 0;
    quint64 m_paints = 0;
    quint64 m_skipBudget = 0;
    quint64 m_skipPending = 0;
    qint64 m_statsStartMs = 0;
};
