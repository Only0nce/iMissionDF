#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QImage>
#include <QMutex>
#include <QPointer>
#include <QQuickItem>
#include <QThread>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVector>

class SpectrumCudaWorker;

class FftWaterfallTextureItem : public QQuickItem
{
    Q_OBJECT
public:
    Q_PROPERTY(bool renderEnabled READ renderEnabled WRITE setRenderEnabled NOTIFY renderEnabledChanged)
    Q_PROPERTY(int targetFps READ targetFps WRITE setTargetFps NOTIFY targetFpsChanged)
    Q_PROPERTY(double minDb READ minDb WRITE setMinDb NOTIFY levelsChanged)
    Q_PROPERTY(double maxDb READ maxDb WRITE setMaxDb NOTIFY levelsChanged)
    Q_PROPERTY(QVariantList palette READ palette WRITE setPalette NOTIFY paletteChanged)
    Q_PROPERTY(int rowHeightPx READ rowHeightPx WRITE setRowHeightPx NOTIFY rowHeightPxChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY colorsChanged)
    // DOA-VIEWER1.7: optional CUDA row processor for DoA waterfall.
    // This keeps the main application build free from CUDA headers/libraries;
    // the existing runtime plugin is loaded by SpectrumCudaWorker when present.
    Q_PROPERTY(bool cudaProcessingEnabled READ cudaProcessingEnabled WRITE setCudaProcessingEnabled NOTIFY cudaProcessingEnabledChanged)
    Q_PROPERTY(QString computeBackend READ computeBackend NOTIFY computeBackendChanged)
    Q_PROPERTY(bool cudaAccelerationActive READ cudaAccelerationActive NOTIFY computeBackendChanged)

    explicit FftWaterfallTextureItem(QQuickItem *parent = nullptr);
    ~FftWaterfallTextureItem() override;

    bool renderEnabled() const noexcept { return m_renderEnabled; }
    void setRenderEnabled(bool enabled);

    int targetFps() const noexcept { return m_targetFps; }
    void setTargetFps(int fps);

    double minDb() const noexcept { return m_minDb; }
    void setMinDb(double value);
    double maxDb() const noexcept { return m_maxDb; }
    void setMaxDb(double value);

    QVariantList palette() const;
    void setPalette(const QVariantList &colors);

    int rowHeightPx() const noexcept { return m_rowHeightPx; }
    void setRowHeightPx(int px);

    QColor backgroundColor() const noexcept { return m_backgroundColor; }
    void setBackgroundColor(const QColor &color);

    bool cudaProcessingEnabled() const noexcept { return m_cudaProcessingEnabled; }
    void setCudaProcessingEnabled(bool enabled);
    QString computeBackend() const;
    bool cudaAccelerationActive() const noexcept { return m_cudaAccelerationActive; }

    Q_INVOKABLE bool submitExternalFrame(const QVariantList &values);
    Q_INVOKABLE void clearHistory();

signals:
    void renderEnabledChanged();
    void targetFpsChanged();
    void levelsChanged();
    void paletteChanged();
    void rowHeightPxChanged();
    void colorsChanged();
    void cudaProcessingEnabledChanged();
    void computeBackendChanged();

    void processWaterfallRequested(quint64 generation,
                                   QVector<float> frame,
                                   int outputBins,
                                   float minDb,
                                   float maxDb,
                                   QVector<quint32> paletteLut);

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;
    void releaseResources() override;
    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    void rebuildPaletteLocked();
    QVector<quint32> paletteLutLocked() const;
    void ensureHistoryLocked(int width, int rows);
    void appendFrameLocked(const QVector<float> &frame, int outputWidth);
    void appendArgbRowLocked(const QVector<quint32> &argbRow, int outputWidth);
    void appendTestPatternRowLocked(int outputWidth);
    void appendFirstVisibleCpuFallbackLocked(const QVector<float> &frame, int outputWidth, const char *reason);
    void startComputeWorker();
    void stopComputeWorker();
    void enqueueCudaFrame(const QVector<float> &frame, int outputWidth);
    void onComputeBackendReady(const QString &backendName, bool cudaActive, const QString &detail);
    void onWaterfallRowReady(quint64 generation,
                             QVector<float> dbRow,
                             QVector<quint32> argbRow,
                             bool usedCuda,
                             qint64 elapsedUsec);
    void scheduleUpdate(bool force = false);
    int targetIntervalMs() const noexcept;
    QRgb colorForDbLocked(float value) const;

    mutable QMutex m_mutex;
    QImage m_historyImage;      // physical ring buffer, Format_ARGB32
    QImage m_presentImage;      // top-to-bottom image handed to SceneGraph texture
    QVector<QRgb> m_paletteRgb;
    QVariantList m_paletteValues;

    bool m_renderEnabled = true;
    int m_targetFps = 18;
    int m_rowHeightPx = 1;
    double m_minDb = -150.0;
    double m_maxDb = -50.0;
    QColor m_backgroundColor = QColor(QStringLiteral("#060B16"));

    bool m_cudaProcessingEnabled = true;
    QString m_backendMode = QStringLiteral("auto");
    bool m_forceCpuBackend = false;
    bool m_testPatternBackend = false;
    bool m_cudaAccelerationActive = false;
    QString m_computeBackend = QStringLiteral("not-started");
    QThread m_computeThread;
    QPointer<SpectrumCudaWorker> m_computeWorker;
    bool m_rowInFlight = false;
    QVector<float> m_pendingFrame;
    int m_pendingOutputWidth = 0;
    int m_inFlightOutputWidth = 0;
    quint64 m_pendingGeneration = 0;
    quint64 m_generation = 0;
    quint64 m_cancelBeforeGeneration = 0;

    int m_historyWidth = 0;
    int m_historyRows = 0;
    int m_headRow = 0;          // newest physical row
    int m_validRows = 0;
    bool m_dirty = false;
    bool m_updatePending = false;
    qint64 m_lastUpdateRequestMs = 0;

    QTimer m_budgetTimer;
    QElapsedTimer m_statsTimer;
    quint64 m_framesIn = 0;
    quint64 m_textureUpdates = 0;
    quint64 m_skipBudget = 0;
    quint64 m_skipPending = 0;
    quint64 m_clearCount = 0;
    quint64 m_cudaRows = 0;
    quint64 m_cpuRows = 0;
    quint64 m_cudaDispatches = 0;
    quint64 m_coalescedFrames = 0;
    quint64 m_staleRows = 0;
    quint64 m_testPatternRows = 0;
    quint64 m_emptyRows = 0;
    quint64 m_bootstrapCpuRows = 0;
    quint64 m_deferredSubmits = 0;
    quint64 m_widthNormalizedRows = 0;
    quint64 m_streamTextureFrames = 0;
    quint64 m_recreatedTextureFrames = 0;
    bool m_streamTextureUpload = true;
    QString m_uploadMode = QStringLiteral("stream");
    int m_lastArgbInputWidth = 0;
    int m_lastDesiredOutputWidth = 0;
    qint64 m_lastCudaUsec = 0;
    qint64 m_lastCpuUsec = 0;
    qint64 m_lastDispatchMs = 0;
    qint64 m_lastAcceptedRowMs = 0;
};
