#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QVariantList>
#include <QVariantMap>

class DoaClient : public QObject
{
    Q_OBJECT

    // ================= RF AGC =================
    Q_PROPERTY(bool rfAgcAvailable READ rfAgcAvailable NOTIFY rfAgcChanged)
    Q_PROPERTY(bool rfAgcEnabled READ rfAgcEnabled NOTIFY rfAgcChanged)
    Q_PROPERTY(QVariantList rfAgcChannels READ rfAgcChannels NOTIFY rfAgcChanged)
    Q_PROPERTY(double scannerAttDb READ scannerAttDb WRITE setScannerAttDbLocal NOTIFY scannerAttDbChanged)

    // connection
    Q_PROPERTY(QString host READ host WRITE setHost NOTIFY hostChanged)
    Q_PROPERTY(int port READ port WRITE setPort NOTIFY portChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)

    // stream states (server side)
    Q_PROPERTY(bool streamEnabled READ streamEnabled WRITE setStreamEnabled NOTIFY streamEnabledChanged)
    Q_PROPERTY(bool doaEnabled READ doaEnabled WRITE setDoaEnabled NOTIFY doaEnabledChanged)
    Q_PROPERTY(bool spectrumEnabled READ spectrumEnabled WRITE setSpectrumEnabled NOTIFY spectrumEnabledChanged)

    // FFT config
    Q_PROPERTY(int fftChannel READ fftChannel WRITE setFftChannel NOTIFY fftChannelChanged)
    Q_PROPERTY(int fftPoints READ fftPoints WRITE setFftPoints NOTIFY fftPointsChanged)
    Q_PROPERTY(int fftDownsample READ fftDownsample WRITE setFftDownsample NOTIFY fftDownsampleChanged)

    // rate limit
    Q_PROPERTY(double txHz READ txHz WRITE setTxHz NOTIFY txHzChanged)

    // DOA tone select
    Q_PROPERTY(double doaOffsetHz READ doaOffsetHz WRITE setDoaOffsetHz NOTIFY doaOffsetHzChanged)
    Q_PROPERTY(double doaBwHz READ doaBwHz WRITE setDoaBwHz NOTIFY doaBwHzChanged)
    Q_PROPERTY(double gateThDb READ gateThDb WRITE setGateThDb NOTIFY gateThDbChanged)

    // ===== NEW: Array config =====
    Q_PROPERTY(double ucaRadiusM READ ucaRadiusM WRITE setUcaRadiusM NOTIFY ucaRadiusMChanged)
    Q_PROPERTY(int numAntennas READ numAntennas WRITE setNumAntennas NOTIFY numAntennasChanged)

    // ===== DOA algorithm selection =====
    // values example: "uca_rb_music", "uca_esprit", "music_1d"
    Q_PROPERTY(QString doaAlgo READ doaAlgo WRITE setDoaAlgo NOTIFY doaAlgoChanged)

    // live results
    Q_PROPERTY(bool signalPresent READ signalPresent NOTIFY signalChanged)
    Q_PROPERTY(double bandPeakDb READ bandPeakDb NOTIFY signalChanged)
    Q_PROPERTY(double doaSigPower READ doaSigPower NOTIFY signalChanged)

    Q_PROPERTY(double doaDeg READ doaDeg NOTIFY doaChanged)
    Q_PROPERTY(double confidence READ confidence NOTIFY doaChanged)

    // MUSIC spectrum (optional)
    Q_PROPERTY(QVariantList theta READ theta NOTIFY spectrumChanged)
    Q_PROPERTY(QVariantList spectrum READ spectrum NOTIFY spectrumChanged)

    // RF FFT
    Q_PROPERTY(QVariantList fftFreqHz READ fftFreqHz NOTIFY fftChanged)
    Q_PROPERTY(QVariantList fftMagDb READ fftMagDb NOTIFY fftChanged)

    // frequency state from server ACK/packet
    Q_PROPERTY(double fcHz READ fcHz NOTIFY fcHzChanged)
    Q_PROPERTY(int ncoUpdateEn READ ncoUpdateEn NOTIFY ncoUpdateEnChanged)

public:
    explicit DoaClient(QObject *parent = nullptr);

    QString host() const { return m_host; }
    int port() const { return m_port; }
    bool connected() const { return m_connected; }
    QString statusText() const { return m_statusText; }

    bool streamEnabled() const { return m_streamEnabled; }
    bool doaEnabled() const { return m_doaEnabled; }
    bool spectrumEnabled() const { return m_spectrumEnabled; }

    int fftChannel() const { return m_fftChannel; }
    int fftPoints() const { return m_fftPoints; }
    int fftDownsample() const { return m_fftDownsample; }

    double txHz() const { return m_txHz; }

    double doaOffsetHz() const { return m_doaOffsetHz; }
    double doaBwHz() const { return m_doaBwHz; }
    double gateThDb() const { return m_gateThDb; }

    double ucaRadiusM() const { return m_ucaRadiusM; }
    int numAntennas() const { return m_numAntennas; }

    QString doaAlgo() const { return m_doaAlgo; }

    bool signalPresent() const { return m_signalPresent; }
    double bandPeakDb() const { return m_bandPeakDb; }
    double doaSigPower() const { return m_doaSigPower; }

    double doaDeg() const { return m_doaDeg; }
    double confidence() const { return m_confidence; }

    QVariantList theta() const { return m_theta; }
    QVariantList spectrum() const { return m_spectrum; }

    QVariantList fftFreqHz() const { return m_fftFreqHz; }
    QVariantList fftMagDb() const { return m_fftMagDb; }

    double fcHz() const { return m_fcHz; }
    int ncoUpdateEn() const { return m_ncoUpdateEn; }

public slots:
    void setHost(const QString &v);
    void setPort(int v);

    void setStreamEnabled(bool v);
    void setDoaEnabled(bool v);
    void setSpectrumEnabled(bool v);

    void setFftChannel(int v);
    void setFftPoints(int v);
    void setFftDownsample(int v);

    void setTxHz(double v);

    void setDoaOffsetHz(double v);
    void setDoaBwHz(double v);
    void setGateThDb(double v);

    void setUcaRadiusM(double v);
    void setNumAntennas(int v);

    void setDoaAlgo(const QString &algo);

public:
    Q_INVOKABLE void connectToServer();
    Q_INVOKABLE void disconnectFromServer();
    Q_INVOKABLE void requestState();

    Q_INVOKABLE void setRfAgcEnable(int ch, bool enable, bool needAck = true);
    Q_INVOKABLE void setRfAgcChannel(int ch, double targetDb, bool needAck = true);
    Q_INVOKABLE void setScannerAttDb(double attDb, bool needAck = true);

    bool rfAgcAvailable() const { return m_rfAgcAvailable; }
    bool rfAgcEnabled()   const { return m_rfAgcEnabled; }
    QVariantList rfAgcChannels() const { return m_rfAgcChannels; }

    double scannerAttDb() const { return m_scannerAttDb; }
    void setScannerAttDbLocal(double v) { if (qFuzzyCompare(m_scannerAttDb, v)) return; m_scannerAttDb = v; emit scannerAttDbChanged(); }

    Q_INVOKABLE bool sendJson(const QVariantMap &obj);

    // convenience QML calls
    Q_INVOKABLE void applyDoaTone();
    Q_INVOKABLE void applyFftConfig();
    Q_INVOKABLE void setAdcChannel(int ch);

    // frequency commands
    Q_INVOKABLE void setFrequencyHz(qint64 freqHz, int updateEn = -1, bool needAck = true);
    Q_INVOKABLE void setFcHz(qint64 fcHz, bool needAck = true);

    // algorithm command (explicit)
    Q_INVOKABLE void setDoaAlgorithm(const QString &algo, bool needAck = true);

signals:

    void rfAgcChanged();
    void scannerAttDbChanged();
    void hostChanged();
    void portChanged();
    void connectedChanged();
    void statusTextChanged();

    void streamEnabledChanged();
    void doaEnabledChanged();
    void spectrumEnabledChanged();

    void fftChannelChanged();
    void fftPointsChanged();
    void fftDownsampleChanged();

    void txHzChanged();

    void doaOffsetHzChanged();
    void doaBwHzChanged();
    void gateThDbChanged();

    void ucaRadiusMChanged();
    void numAntennasChanged();

    void doaAlgoChanged();

    void signalChanged();
    void doaChanged();
    void spectrumChanged();
    void fftChanged();

    void fcHzChanged();
    void ncoUpdateEnChanged();

    void logMessage(const QString &msg);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError);

private:
    void setStatus(const QString &s);
    void setConnected(bool c);

    void parseJsonLine(const QByteArray &line);
    void applyStateObject(const QVariantMap &st);

private:
    QTcpSocket *m_sock = nullptr;
    QByteArray m_rx;

    QString m_host = "0.0.0.0";
    int m_port = 9000;
    bool m_connected = false;
    QString m_statusText = "DISCONNECTED";

    // desired state / last known state
    bool m_streamEnabled = true;
    bool m_doaEnabled = true;
    bool m_spectrumEnabled = false;

    int m_fftChannel = 0;
    int m_fftPoints = 4096;
    int m_fftDownsample = 2;

    double m_txHz = 10.0;

    double m_doaOffsetHz = 0.0;
    double m_doaBwHz = 2000.0;
    double m_gateThDb = -65.0;

    double m_ucaRadiusM = 0.8;
    int m_numAntennas = 5;

    QString m_doaAlgo = "uca_rb_music";

    // live
    bool m_signalPresent = false;
    double m_bandPeakDb = -200.0;
    double m_doaSigPower = 0.0;

    double m_doaDeg = 0.0;
    double m_confidence = 0.0;

    QVariantList m_theta;
    QVariantList m_spectrum;

    QVariantList m_fftFreqHz;
    QVariantList m_fftMagDb;

    double m_fcHz = 0.0;
    int m_ncoUpdateEn = 0;

    // ---- RF AGC state ----
    bool m_rfAgcAvailable = false;
    bool m_rfAgcEnabled = false;
    QVariantList m_rfAgcChannels;
    double m_scannerAttDb = 0.0;
};
