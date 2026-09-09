#pragma once

#include <QColor>
#include <QImage>
#include <QMetaObject>
#include <QMutex>
#include <QPointer>
#include <QQuickPaintedItem>
#include <QVariantList>
#include <QVector>

class QPainter;
class WebSocketClient;

class FftDisplayItem : public QQuickPaintedItem
{
    Q_OBJECT
public:
    enum Mode {
        Spectrum = 0,
        Waterfall = 1
    };
    Q_ENUM(Mode)

    Q_PROPERTY(QObject* backend READ backend WRITE setBackend NOTIFY backendChanged)
    Q_PROPERTY(Mode mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(bool renderEnabled READ renderEnabled WRITE setRenderEnabled NOTIFY renderEnabledChanged)
    Q_PROPERTY(bool showMaxHold READ showMaxHold WRITE setShowMaxHold NOTIFY showMaxHoldChanged)
    Q_PROPERTY(bool clearBeforeNextPaint READ clearBeforeNextPaint WRITE setClearBeforeNextPaint NOTIFY clearBeforeNextPaintChanged)
    Q_PROPERTY(double minDb READ minDb WRITE setMinDb NOTIFY levelsChanged)
    Q_PROPERTY(double maxDb READ maxDb WRITE setMaxDb NOTIFY levelsChanged)
    Q_PROPERTY(double fullStartFreq READ fullStartFreq WRITE setFullStartFreq NOTIFY frequencyMappingChanged)
    Q_PROPERTY(double viewStartFreq READ viewStartFreq WRITE setViewStartFreq NOTIFY frequencyMappingChanged)
    Q_PROPERTY(double viewStopFreq READ viewStopFreq WRITE setViewStopFreq NOTIFY frequencyMappingChanged)
    Q_PROPERTY(double sampleRate READ sampleRate WRITE setSampleRate NOTIFY frequencyMappingChanged)
    Q_PROPERTY(QColor spectrumColor READ spectrumColor WRITE setSpectrumColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor maxHoldColor READ maxHoldColor WRITE setMaxHoldColor NOTIFY colorsChanged)
    Q_PROPERTY(QVariantList palette READ palette WRITE setPalette NOTIFY paletteChanged)

    explicit FftDisplayItem(QQuickItem *parent = nullptr);
    ~FftDisplayItem() override;

    QObject *backend() const noexcept;
    void setBackend(QObject *backendObject);

    Mode mode() const noexcept { return m_mode; }
    void setMode(Mode mode);

    bool renderEnabled() const noexcept { return m_renderEnabled; }
    void setRenderEnabled(bool enabled);

    bool showMaxHold() const noexcept { return m_showMaxHold; }
    void setShowMaxHold(bool enabled);

    bool clearBeforeNextPaint() const noexcept { return m_clearBeforeNextPaint; }
    void setClearBeforeNextPaint(bool clear);

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

    QColor spectrumColor() const noexcept { return m_spectrumColor; }
    void setSpectrumColor(const QColor &color);
    QColor maxHoldColor() const noexcept { return m_maxHoldColor; }
    void setMaxHoldColor(const QColor &color);

    QVariantList palette() const;
    void setPalette(const QVariantList &colors);

    Q_INVOKABLE void requestPaint();
    Q_INVOKABLE void clearPeaks();
    Q_INVOKABLE void clearHistory();

    void paint(QPainter *painter) override;

signals:
    void backendChanged();
    void modeChanged();
    void renderEnabledChanged();
    void showMaxHoldChanged();
    void clearBeforeNextPaintChanged();
    void levelsChanged();
    void frequencyMappingChanged();
    void colorsChanged();
    void paletteChanged();

protected:
    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private slots:
    void onSpectrumFrame(const QVector<float> &frame);
    void onWaterfallFrame(const QVector<float> &frame);
    void onMaxHoldFrame(const QVector<float> &frame);

private:
    void disconnectBackend();
    void ensureWaterfallImage();
    void appendWaterfallRow(const QVector<float> &frame);
    void paintSpectrum(QPainter *painter);
    void paintWaterfall(QPainter *painter);
    int mappedStartIndex(int count) const;
    int mappedEndIndex(int count, int startIndex) const;
    qreal yForDb(float value, qreal plotHeight) const;

    QPointer<WebSocketClient> m_backend;
    QVector<QMetaObject::Connection> m_backendConnections;
    Mode m_mode = Spectrum;
    bool m_renderEnabled = true;
    bool m_showMaxHold = true;
    bool m_clearBeforeNextPaint = false;

    double m_minDb = -130.0;
    double m_maxDb = -80.0;
    double m_fullStartFreq = 0.0;
    double m_viewStartFreq = 0.0;
    double m_viewStopFreq = 0.0;
    double m_sampleRate = 1.0;

    QColor m_spectrumColor = QColor(QStringLiteral("#00FF00"));
    QColor m_maxHoldColor = QColor(QStringLiteral("#FFD54F"));
    QVector<QColor> m_palette;

    QVector<float> m_spectrumFrame;
    QVector<float> m_maxHoldFrame;
    QImage m_waterfallImage;
    mutable QMutex m_dataMutex;
};
