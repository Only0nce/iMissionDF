#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QMetaObject>
#include <QMutex>
#include <QPointer>
#include <QQuickPaintedItem>
#include <QThread>
#include <QTimer>
#include <QVariantList>
#include <QVector>

class QPainter;
class WebSocketClient;
class SpectrumCudaWorker;

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
    Q_PROPERTY(bool waterfallPaused READ waterfallPaused WRITE setWaterfallPaused NOTIFY waterfallPausedChanged)
    Q_PROPERTY(double minDb READ minDb WRITE setMinDb NOTIFY levelsChanged)
    Q_PROPERTY(double maxDb READ maxDb WRITE setMaxDb NOTIFY levelsChanged)
    // CUDA1.17: shared Spectrum plot geometry. QML grid and native trace use
    // the same top inset so dBFS grid lines and rendered FFT amplitudes map to
    // identical pixels. Waterfall ignores this property.
    Q_PROPERTY(double plotTopInset READ plotTopInset WRITE setPlotTopInset NOTIFY plotGeometryChanged)
    Q_PROPERTY(double fullStartFreq READ fullStartFreq WRITE setFullStartFreq NOTIFY frequencyMappingChanged)
    Q_PROPERTY(double viewStartFreq READ viewStartFreq WRITE setViewStartFreq NOTIFY frequencyMappingChanged)
    Q_PROPERTY(double viewStopFreq READ viewStopFreq WRITE setViewStopFreq NOTIFY frequencyMappingChanged)
    Q_PROPERTY(double sampleRate READ sampleRate WRITE setSampleRate NOTIFY frequencyMappingChanged)
    Q_PROPERTY(QColor spectrumColor READ spectrumColor WRITE setSpectrumColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor maxHoldColor READ maxHoldColor WRITE setMaxHoldColor NOTIFY colorsChanged)
    Q_PROPERTY(QVariantList palette READ palette WRITE setPalette NOTIFY paletteChanged)

    Q_PROPERTY(bool measurementsValid READ measurementsValid NOTIFY measurementsChanged)
    Q_PROPERTY(double peakDb READ peakDb NOTIFY measurementsChanged)
    Q_PROPERTY(double noiseFloorDb READ noiseFloorDb NOTIFY measurementsChanged)
    Q_PROPERTY(double snrDb READ snrDb NOTIFY measurementsChanged)
    Q_PROPERTY(double peakFrequencyHz READ peakFrequencyHz NOTIFY measurementsChanged)

    // CUDA1.15/1.16: receiver-local FFT measurements remain independent from
    // the legacy viewport-wide Peak/Noise values above. AstraRX's existing
    // per-receiver S-meter remains the authoritative RF LEVEL. CUDA1.18 also
    // derives a filtered presentation-only offset between that RF reference and
    // the selected FFT level so the Spectrum Y-axis can be read meaningfully.
    // SNR itself remains a pure FFT-domain difference.
    Q_PROPERTY(double measurementFrequencyHz READ measurementFrequencyHz WRITE setMeasurementFrequencyHz NOTIFY measurementReferenceChanged)
    Q_PROPERTY(double measurementBandwidthHz READ measurementBandwidthHz WRITE setMeasurementBandwidthHz NOTIFY measurementReferenceChanged)
    Q_PROPERTY(double measurementLowCutHz READ measurementLowCutHz WRITE setMeasurementLowCutHz NOTIFY measurementReferenceChanged)
    Q_PROPERTY(double measurementHighCutHz READ measurementHighCutHz WRITE setMeasurementHighCutHz NOTIFY measurementReferenceChanged)
    Q_PROPERTY(bool selectedMeasurementsValid READ selectedMeasurementsValid NOTIFY selectedMeasurementsChanged)
    Q_PROPERTY(double selectedLevelDb READ selectedLevelDb NOTIFY selectedMeasurementsChanged)
    Q_PROPERTY(double selectedNoiseFloorDb READ selectedNoiseFloorDb NOTIFY selectedMeasurementsChanged)
    Q_PROPERTY(double selectedSnrDb READ selectedSnrDb NOTIFY selectedMeasurementsChanged)
    Q_PROPERTY(double selectedFrequencyHz READ selectedFrequencyHz NOTIFY selectedMeasurementsChanged)
    Q_PROPERTY(bool receiverLevelValid READ receiverLevelValid NOTIFY receiverLevelChanged)
    Q_PROPERTY(double receiverLevelDb READ receiverLevelDb NOTIFY receiverLevelChanged)
    Q_PROPERTY(double receiverLevelFrequencyHz READ receiverLevelFrequencyHz NOTIFY receiverLevelChanged)

    // CUDA1.19: session-latched RF-reference calibration for the Spectrum Y axis.
    // Raw FFT samples remain untouched in dBFS. A short trusted startup sample
    // set establishes one RF-reference offset; that offset is then held fixed
    // for the backend session so the Y axis never breathes with live S-meter/FFT
    // jitter. Small absolute error is preferred over a moving scale.
    Q_PROPERTY(bool spectrumCalibrationValid READ spectrumCalibrationValid NOTIFY spectrumCalibrationChanged)
    Q_PROPERTY(double spectrumCalibrationOffsetDb READ spectrumCalibrationOffsetDb NOTIFY spectrumCalibrationChanged)

    // R20.4-SPECTRUM-CUDA1 telemetry only. The QML page does not depend on
    // these values; they are exposed for diagnostics/A-B testing.
    Q_PROPERTY(QString computeBackend READ computeBackend NOTIFY computeBackendChanged)
    Q_PROPERTY(bool cudaAccelerationActive READ cudaAccelerationActive NOTIFY computeBackendChanged)

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

    bool waterfallPaused() const noexcept { return m_waterfallPaused; }
    void setWaterfallPaused(bool paused);

    double minDb() const noexcept { return m_minDb; }
    void setMinDb(double value);
    double maxDb() const noexcept { return m_maxDb; }
    void setMaxDb(double value);
    double plotTopInset() const noexcept { return m_plotTopInset; }
    void setPlotTopInset(double value);

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

    bool measurementsValid() const noexcept { return m_measurementsValid; }
    double peakDb() const noexcept { return m_peakDb; }
    double noiseFloorDb() const noexcept { return m_noiseFloorDb; }
    double snrDb() const noexcept { return m_snrDb; }
    double peakFrequencyHz() const noexcept { return m_peakFrequencyHz; }

    double measurementFrequencyHz() const noexcept { return m_measurementFrequencyHz; }
    void setMeasurementFrequencyHz(double value);
    double measurementBandwidthHz() const noexcept { return m_measurementBandwidthHz; }
    void setMeasurementBandwidthHz(double value);
    double measurementLowCutHz() const noexcept { return m_measurementLowCutHz; }
    void setMeasurementLowCutHz(double value);
    double measurementHighCutHz() const noexcept { return m_measurementHighCutHz; }
    void setMeasurementHighCutHz(double value);

    bool selectedMeasurementsValid() const noexcept { return m_selectedMeasurementsValid; }
    double selectedLevelDb() const noexcept { return m_selectedLevelDb; }
    double selectedNoiseFloorDb() const noexcept { return m_selectedNoiseFloorDb; }
    double selectedSnrDb() const noexcept { return m_selectedSnrDb; }
    double selectedFrequencyHz() const noexcept { return m_selectedFrequencyHz; }

    bool receiverLevelValid() const noexcept { return m_receiverLevelValid; }
    double receiverLevelDb() const noexcept { return m_receiverLevelDb; }
    double receiverLevelFrequencyHz() const noexcept { return m_receiverLevelFrequencyHz; }

    bool spectrumCalibrationValid() const noexcept { return m_spectrumCalibrationValid; }
    double spectrumCalibrationOffsetDb() const noexcept { return m_spectrumCalibrationOffsetDb; }

    QString computeBackend() const;
    bool cudaAccelerationActive() const noexcept { return m_cudaAccelerationActive; }

    Q_INVOKABLE void requestPaint();
    void presentOnSharedClock();
    Q_INVOKABLE void clearPeaks();
    Q_INVOKABLE void clearHistory();

    void paint(QPainter *painter) override;

signals:
    void backendChanged();
    void modeChanged();
    void renderEnabledChanged();
    void showMaxHoldChanged();
    void clearBeforeNextPaintChanged();
    void waterfallPausedChanged();
    void levelsChanged();
    void plotGeometryChanged();
    void frequencyMappingChanged();
    void colorsChanged();
    void paletteChanged();
    void measurementsChanged();
    void measurementReferenceChanged();
    void selectedMeasurementsChanged();
    void receiverLevelChanged();
    void spectrumCalibrationChanged();
    void computeBackendChanged();

    // Internal queued worker requests. Large FFT/history buffers remain native
    // Qt containers and never enter the QML/JavaScript heap.
    void processWaterfallRequested(quint64 generation,
                                   QVector<float> frame,
                                   int outputBins,
                                   float minDb,
                                   float maxDb,
                                   QVector<quint32> paletteLut);
    void recolorHistoryRequested(quint64 generation,
                                 QVector<float> historyDb,
                                 float minDb,
                                 float maxDb,
                                 QVector<quint32> paletteLut);

protected:
    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private slots:
    void onSpectrumFrame(const QVector<float> &frame);
    void onWaterfallFrame(const QVector<float> &frame);
    void onMaxHoldFrame(const QVector<float> &frame);
    void onSmeterValueUpdated(double smeterDb);

    void onComputeBackendReady(QString backendName, bool cudaActive, QString detail);
    void onWaterfallRowReady(quint64 generation,
                             QVector<float> dbRow,
                             QVector<quint32> argbRow,
                             bool usedCuda,
                             qint64 elapsedUsec);
    void onHistoryRecolorReady(quint64 generation,
                               QVector<quint32> argbHistory,
                               bool usedCuda,
                               qint64 elapsedUsec);

private:
    void disconnectBackend();
    void startComputeWorker();
    void stopComputeWorker();

    void clearHistoryLocked();
    void ensureWaterfallHistory(int sourceBins);
    void rebuildPaletteLutLocked();
    void requestHistoryRecolor();
    void submitWaterfallWork(const QVector<float> &frame);
    void refreshPresentationClock();
    void appendProcessedWaterfallRowLocked(const QVector<float> &dbRow,
                                           const QVector<quint32> &argbRow);
    void updateMeasurementsLocked(const QVector<float> &frame, bool force = false);
    void updateSelectedMeasurementsLocked(const QVector<float> &frame, bool force = false);
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
    bool m_waterfallPaused = false;

    double m_minDb = -130.0;
    double m_maxDb = -80.0;
    double m_plotTopInset = 18.0;
    double m_fullStartFreq = 0.0;
    double m_viewStartFreq = 0.0;
    double m_viewStopFreq = 0.0;
    double m_sampleRate = 1.0;

    QColor m_spectrumColor = QColor(QStringLiteral("#35D5BD"));
    QColor m_maxHoldColor = QColor(QStringLiteral("#FFC55A"));
    QVector<QColor> m_palette;
    QVector<quint32> m_paletteLut;

    QVector<float> m_spectrumFrame;
    QVector<float> m_previousSpectrumFrame;
    QVector<float> m_maxHoldFrame;

    // CUDA1.26/FPS90-SYNC: Spectrum and Waterfall share one process-wide
    // presentation clock. Keeping the target interval here is diagnostic-only;
    // the actual timer is centralized so both items are invalidated on the same
    // GUI-thread tick instead of drifting on independent timers.
    int m_presentIntervalMs = 11;
    qint64 m_spectrumTransitionStartMs = 0;
    qint64 m_lastSpectrumArrivalMs = 0;
    double m_spectrumSourcePeriodMs = 40.0;
    qint64 m_lastWaterfallArrivalMs = 0;
    double m_waterfallSourcePeriodMs = 40.0;

    // R20.4-SPECTRUM-CUDA1: ring-buffer history removes the per-frame multi-MB
    // memmove while preserving A3 semantics: every row is stored on the full
    // acquisition/full-span RF axis. Zoom/pan is applied only during paint.
    // Newest row is m_waterfallHeadRow and older rows follow by increasing
    // physical row index with one wrap at the end of the surface.
    QVector<quint32> m_waterfallColorHistory;
    QVector<float> m_waterfallDbHistory;
    int m_waterfallHistoryWidth = 0;
    int m_waterfallHistoryRows = 1024;
    int m_waterfallHistoryMaxBins = 2048;
    int m_waterfallHeadRow = 0;
    int m_waterfallValidRows = 0;

    // Worker/CUDA state. Only one row or recolor job is allowed in flight;
    // bursts are coalesced to the newest frame so latency stays bounded.
    QThread m_computeThread;
    QPointer<SpectrumCudaWorker> m_computeWorker;
    QString m_computeBackend = QStringLiteral("starting");
    bool m_cudaAccelerationActive = false;
    bool m_waterfallWorkBusy = false;
    bool m_recolorBusy = false;
    bool m_recolorRequested = false;
    QVector<float> m_pendingWaterfallFrame;
    quint64 m_colorGeneration = 1;
    quint64 m_coalescedWaterfallFrames = 0;
    quint64 m_cudaRows = 0;
    quint64 m_cpuRows = 0;
    quint64 m_cudaRecolors = 0;
    quint64 m_cpuRecolors = 0;
    qint64 m_lastRecolorUsec = 0;
    QElapsedTimer m_computeStatsTimer;
    QTimer m_recolorDebounceTimer;

    bool m_measurementsValid = false;
    double m_peakDb = 0.0;
    double m_noiseFloorDb = 0.0;
    double m_snrDb = 0.0;
    double m_peakFrequencyHz = 0.0;
    QElapsedTimer m_measurementTimer;

    // CUDA1.15 selected-frequency FFT reference/results. CUDA1.16 keeps these
    // as the local FFT-domain NOISE/SNR source while adding the existing
    // AstraRX S-meter as a separate authoritative receiver LEVEL.
    double m_measurementFrequencyHz = 0.0;
    double m_measurementBandwidthHz = 0.0;
    double m_measurementLowCutHz = 0.0;
    double m_measurementHighCutHz = 0.0;
    bool m_selectedMeasurementsValid = false;
    double m_selectedLevelDb = 0.0;       // FFT point level, diagnostic only
    double m_selectedNoiseFloorDb = 0.0;  // local FFT noise floor
    double m_selectedSnrDb = 0.0;         // FFT point level - local FFT noise
    double m_selectedFrequencyHz = 0.0;

    bool m_receiverLevelValid = false;
    double m_receiverLevelDb = 0.0;
    double m_receiverLevelFrequencyHz = 0.0;
    qint64 m_receiverLevelAcceptAfterMs = 0;

    // CUDA1.19 calibration is latched once per backend session. It never modifies
    // FFT samples, Max Hold, Waterfall data, or CUDA processing. Five trusted
    // candidates are collected and the median is frozen until backend reset.
    bool m_spectrumCalibrationValid = false;
    double m_spectrumCalibrationOffsetDb = 0.0;
    quint32 m_spectrumCalibrationSamples = 0;
    QVector<double> m_spectrumCalibrationCandidates;
    QElapsedTimer m_rfMetricsTelemetryTimer;

    // Paint cadence telemetry. Updated only by the Qt Quick render path.
    QElapsedTimer m_paintStatsTimer;
    quint64 m_paintStatsFrames = 0;

    mutable QMutex m_dataMutex;
};
