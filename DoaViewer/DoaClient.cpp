#include "DoaClient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QtGlobal>

// =======================================================
// Helpers for QVariantMap defaults (Qt 5.15 compatible)
// =======================================================
static bool vmHas(const QVariantMap &m, const char *k)
{
    return m.contains(QString::fromLatin1(k));
}

static bool vmGetBool(const QVariantMap &m, const char *k, bool def)
{
    const QString kk = QString::fromLatin1(k);
    if (!m.contains(kk)) return def;
    return m.value(kk).toBool();
}

static int vmGetInt(const QVariantMap &m, const char *k, int def)
{
    const QString kk = QString::fromLatin1(k);
    if (!m.contains(kk)) return def;
    bool ok = false;
    int v = m.value(kk).toInt(&ok);
    return ok ? v : def;
}

static double vmGetDouble(const QVariantMap &m, const char *k, double def)
{
    const QString kk = QString::fromLatin1(k);
    if (!m.contains(kk)) return def;
    bool ok = false;
    const double v = m.value(kk).toDouble(&ok);
    return ok ? v : def;
}

static QVariantList vmGetList(const QVariantMap &m, const char *k)
{
    const QString kk = QString::fromLatin1(k);
    if (!m.contains(kk)) return {};
    return m.value(kk).toList();
}
// =======================================================

DoaClient::DoaClient(QObject *parent) : QObject(parent)
{
    m_sock = new QTcpSocket(this);

    connect(m_sock, &QTcpSocket::connected,    this, &DoaClient::onConnected);
    connect(m_sock, &QTcpSocket::disconnected, this, &DoaClient::onDisconnected);
    connect(m_sock, &QTcpSocket::readyRead,    this, &DoaClient::onReadyRead);

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(m_sock, &QTcpSocket::errorOccurred, this, &DoaClient::onSocketError);
#else
    connect(m_sock, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(onSocketError(QAbstractSocket::SocketError)));
#endif

    setStatus("DISCONNECTED");
}

// ---------------- setters ----------------

void DoaClient::setHost(const QString &v)
{
    if (m_host == v) return;
    m_host = v;
    emit hostChanged();
}

void DoaClient::setPort(int v)
{
    if (m_port == v) return;
    m_port = v;
    emit portChanged();
}

void DoaClient::setStreamEnabled(bool v)
{
    if (m_streamEnabled == v) return;
    m_streamEnabled = v;
    emit streamEnabledChanged();

    QVariantMap m;
    m["menuID"] = "setStreamEnable";
    m["enable"] = v;
    m["needAck"] = true;
    sendJson(m);
}

void DoaClient::setDoaEnabled(bool v)
{
    if (m_doaEnabled == v) return;
    m_doaEnabled = v;
    emit doaEnabledChanged();

    QVariantMap m;
    m["menuID"] = "setDoaEnable";
    m["enable"] = v;
    m["needAck"] = true;
    sendJson(m);
}

void DoaClient::setSpectrumEnabled(bool v)
{
    if (m_spectrumEnabled == v) return;
    m_spectrumEnabled = v;
    emit spectrumEnabledChanged();

    QVariantMap m;
    m["menuID"] = "setSpectrumEnable";
    m["enable"] = v;
    m["needAck"] = true;
    sendJson(m);
}

void DoaClient::setFftChannel(int v)
{
    if (v < 0) v = 0;
    if (v > 4) v = 4;
    if (m_fftChannel == v) return;
    m_fftChannel = v;
    emit fftChannelChanged();

    QVariantMap m;
    m["menuID"] = "setAdcChannel";
    m["channel"] = v;
    m["needAck"] = true;
    sendJson(m);
}

void DoaClient::setFftPoints(int v)
{
    if (v < 256) v = 256;
    if (m_fftPoints == v) return;
    m_fftPoints = v;
    emit fftPointsChanged();
}

void DoaClient::setFftDownsample(int v)
{
    if (v < 1) v = 1;
    if (m_fftDownsample == v) return;
    m_fftDownsample = v;
    emit fftDownsampleChanged();
}

void DoaClient::setTxHz(double v)
{
    if (v < 0.2) v = 0.2;
    if (v > 60.0) v = 60.0;
    if (qFuzzyCompare(m_txHz + 1.0, v + 1.0)) return;
    m_txHz = v;
    emit txHzChanged();

    QVariantMap m;
    m["menuID"] = "setTxHz";
    m["hz"] = v;
    m["needAck"] = true;
    sendJson(m);
}

void DoaClient::setDoaOffsetHz(double v)
{
    if (qFuzzyCompare(m_doaOffsetHz + 1.0, v + 1.0)) return;
    m_doaOffsetHz = v;
    emit doaOffsetHzChanged();
}

void DoaClient::setDoaBwHz(double v)
{
    if (v < 50.0) v = 50.0;
    if (qFuzzyCompare(m_doaBwHz + 1.0, v + 1.0)) return;
    m_doaBwHz = v;
    emit doaBwHzChanged();
}

void DoaClient::setGateThDb(double v)
{
    if (qFuzzyCompare(m_gateThDb + 1.0, v + 1.0)) return;
    m_gateThDb = v;
    emit gateThDbChanged();

    QVariantMap m;
    m["menuID"] = "setDoaPowerThresholdDb";
    m["th_db"] = v;
    m["needAck"] = true;
    sendJson(m);
}

void DoaClient::setUcaRadiusM(double v)
{
    if (v < 0.01) v = 0.01;
    if (qFuzzyCompare(m_ucaRadiusM + 1.0, v + 1.0)) return;
    m_ucaRadiusM = v;
    emit ucaRadiusMChanged();

    QVariantMap m;
    m["menuID"] = "setUcaRadiusM";
    m["radius_m"] = v;
    m["needAck"] = true;
    sendJson(m);
}

void DoaClient::setNumAntennas(int v)
{
    if (v < 2) v = 2;
    if (v > 32) v = 32;
    if (m_numAntennas == v) return;
    m_numAntennas = v;
    emit numAntennasChanged();

    QVariantMap m;
    m["menuID"] = "setNumAntennas";
    m["num"] = v;
    m["needAck"] = true;
    sendJson(m);
}

void DoaClient::setDoaAlgo(const QString &algo)
{
    const QString a = algo.trimmed();
    if (m_doaAlgo == a) return;
    m_doaAlgo = a;
    emit doaAlgoChanged();

    setDoaAlgorithm(a, true);
}

// ---------------- connection API ----------------

void DoaClient::connectToServer()
{
    if (!m_sock) return;

    const auto st = m_sock->state();
    if (st == QAbstractSocket::ConnectedState || st == QAbstractSocket::ConnectingState) {
        emit logMessage("Already connected/connecting");
        return;
    }

    m_rx.clear();
    setStatus(QString("CONNECTING %1:%2").arg(m_host).arg(m_port));
    emit logMessage(QString("[TCP] connectToHost %1:%2").arg(m_host).arg(m_port));

    m_sock->connectToHost(m_host, quint16(m_port));
}

void DoaClient::disconnectFromServer()
{
    if (!m_sock) return;
    emit logMessage("[TCP] disconnectFromHost");
    m_sock->disconnectFromHost();
}

void DoaClient::requestState()
{
    QVariantMap m;
    m["menuID"] = "getState";
    m["needAck"] = true;
    sendJson(m);
}
void DoaClient::setRfAgcEnable(int ch, bool enable, bool needAck)
{
    QVariantMap obj;
    obj["menuID"] = "setRfAgcEnable";
    obj["ch"] = ch;
    obj["enable"] = enable;
    obj["needAck"] = needAck;
    sendJson(obj);
}

void DoaClient::setRfAgcChannel(int ch, double targetDb, bool needAck)
{
    QVariantMap obj;
    obj["menuID"] = "setRfAgcChannel";
    obj["ch"] = ch;
    obj["target_db"] = targetDb;
    obj["needAck"] = needAck;
    sendJson(obj);
}

void DoaClient::setScannerAttDb(double attDb, bool needAck)
{
    QJsonObject msg;
    msg[QStringLiteral("menuID")] = QStringLiteral("setScannerAttDb");
    msg[QStringLiteral("att_db")] = attDb;
    msg[QStringLiteral("needAck")] = needAck;
    sendJson(msg.toVariantMap());
}

bool DoaClient::sendJson(const QVariantMap &obj)
{
    if (!m_sock || m_sock->state() != QAbstractSocket::ConnectedState) {
        emit logMessage("[TCP] sendJson ignored (not connected)");
        return false;
    }

    QJsonObject jo = QJsonObject::fromVariantMap(obj);
    QJsonDocument doc(jo);
    QByteArray line = doc.toJson(QJsonDocument::Compact);
    line.append('\n');

    qint64 n = m_sock->write(line);
    if (n < 0) {
        emit logMessage(QString("[TCP] write failed: %1").arg(m_sock->errorString()));
        return false;
    }
    return true;
}

// ---------------- convenience commands ----------------

void DoaClient::applyDoaTone()
{
    QVariantMap m1;
    m1["menuID"] = "setDoaTargetOffsetHz";
    m1["offset_hz"] = m_doaOffsetHz;
    m1["needAck"] = true;
    sendJson(m1);

    QVariantMap m2;
    m2["menuID"] = "setDoaBwHz";
    m2["bw_hz"] = m_doaBwHz;
    m2["needAck"] = true;
    sendJson(m2);
}

void DoaClient::applyFftConfig()
{
    QVariantMap m;
    m["menuID"] = "setFftConfig";
    m["fft_points"] = m_fftPoints;
    m["fft_downsample"] = m_fftDownsample;
    m["needAck"] = true;
    sendJson(m);
}

void DoaClient::setAdcChannel(int ch)
{
    setFftChannel(ch);
}

// frequency commands

void DoaClient::setFrequencyHz(qint64 freqHz, int updateEn, bool needAck)
{
    QVariantMap m;
    m["menuID"] = "setFrequencyHz";
    m["freq_hz"] = QVariant::fromValue(freqHz);
    if (updateEn >= 0)
        m["update_en"] = updateEn;
    m["needAck"] = needAck;
    sendJson(m);
}

void DoaClient::setFcHz(qint64 fcHz, bool needAck)
{
    QVariantMap m;
    m["menuID"] = "setFcHz";
    m["fc_hz"] = QVariant::fromValue(fcHz);
    m["needAck"] = needAck;
    sendJson(m);
}

void DoaClient::setDoaAlgorithm(const QString &algo, bool needAck)
{
    QVariantMap m;
    m["menuID"] = "setDoaAlgorithm";
    m["algo"] = algo;
    m["needAck"] = needAck;
    sendJson(m);
}

// ---------------- socket callbacks ----------------

void DoaClient::onConnected()
{
    setConnected(true);
    setStatus(QString("CONNECTED %1:%2").arg(m_host).arg(m_port));
    emit logMessage("[TCP] connected");
    requestState();
}

void DoaClient::onDisconnected()
{
    setConnected(false);
    setStatus("DISCONNECTED");
    emit logMessage("[TCP] disconnected");
}

void DoaClient::onSocketError(QAbstractSocket::SocketError)
{
    const QString err = m_sock ? m_sock->errorString() : QString("unknown");
    setConnected(false);
    setStatus(QString("ERROR: %1").arg(err));
    emit logMessage(QString("[TCP] error: %1").arg(err));
}

void DoaClient::onReadyRead()
{
    if (!m_sock) return;

    m_rx.append(m_sock->readAll());

    while (true) {
        int nl = m_rx.indexOf('\n');
        if (nl < 0) break;

        QByteArray line = m_rx.left(nl).trimmed();
        m_rx.remove(0, nl + 1);

        if (!line.isEmpty())
            parseJsonLine(line);
    }
}

void DoaClient::parseJsonLine(const QByteArray &line)
{
    QJsonParseError pe;
    QJsonDocument doc = QJsonDocument::fromJson(line, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        emit logMessage("[RX] JSON parse error");
        return;
    }

    QVariantMap obj = doc.object().toVariantMap();
    const QString menu = obj.value("menuID").toString();

    // ---- RF AGC parse (if present) ----
    const QVariantMap agc = obj.value("rf_agc").toMap();
    if (!agc.isEmpty()) {
        const bool avail = agc.value("available").toBool();
        const bool en    = agc.value("enabled").toBool();
        const QVariantList chs = agc.value("channels").toList();

        const bool changed = (avail != m_rfAgcAvailable) || (en != m_rfAgcEnabled) || (chs != m_rfAgcChannels);
        m_rfAgcAvailable = avail;
        m_rfAgcEnabled   = en;
        m_rfAgcChannels  = chs;
        if (changed) emit rfAgcChanged();
    }

    if (menu == "Ack") {
        QVariantMap st = obj.value("state").toMap();
        if (!st.isEmpty())
            applyStateObject(st);

        emit logMessage(QString("[RX] Ack for %1").arg(obj.value("ackFor").toString()));
        return;
    }

    if (menu == "DoAResult") {
        // --- signal present + band peak ---
        const bool sp = vmGetBool(obj, "signal_present", false);
        const double bp = vmGetDouble(obj, "band_peak_db", -200.0);
        const double pw = vmGetDouble(obj, "doa_sig_power", 0.0);

        const bool sigChanged =
            (sp != m_signalPresent) ||
            !qFuzzyCompare(bp + 1.0, m_bandPeakDb + 1.0) ||
            !qFuzzyCompare(pw + 1.0, m_doaSigPower + 1.0);

        m_signalPresent = sp;
        m_bandPeakDb = bp;
        m_doaSigPower = pw;
        if (sigChanged) emit signalChanged();

        // --- DOA peak ---
        const QVariantList doas = vmGetList(obj, "doas");
        const QVariantList conf = vmGetList(obj, "confidence");

        double newDoa = m_doaDeg;
        double newConf = m_confidence;

        if (!doas.isEmpty()) newDoa = doas.first().toDouble();
        if (!conf.isEmpty()) newConf = conf.first().toDouble();

        const bool doaCh =
            !qFuzzyCompare(newDoa + 1.0, m_doaDeg + 1.0) ||
            !qFuzzyCompare(newConf + 1.0, m_confidence + 1.0);

        m_doaDeg = newDoa;
        m_confidence = newConf;
        if (doaCh) emit doaChanged();

        // --- MUSIC spectrum (optional) ---
        m_theta = vmGetList(obj, "theta_deg");
        m_spectrum = vmGetList(obj, "spectrum");
        emit spectrumChanged();

        // --- RF FFT spectrum ---
        m_fftFreqHz = vmGetList(obj, "fft_freq_hz");
        m_fftMagDb  = vmGetList(obj, "fft_mag_db");
        emit fftChanged();

        // --- frequency tracking ---
        const double fc = vmGetDouble(obj, "frequency_hz", m_fcHz);
        if (!qFuzzyCompare(fc + 1.0, m_fcHz + 1.0)) {
            m_fcHz = fc;
            emit fcHzChanged();
        }

        // algorithm name (optional)
        if (vmHas(obj, "doa_algo")) {
            const QString a = obj.value("doa_algo").toString();
            if (!a.isEmpty() && m_doaAlgo != a) {
                m_doaAlgo = a;
                emit doaAlgoChanged();
            }
        }

        return;
    }

    emit logMessage(QString("[RX] menuID=%1").arg(menu));
}

void DoaClient::applyStateObject(const QVariantMap &st)
{
    // server side states (reflect)
    if (vmHas(st, "stream_enabled")) {
        const bool v = vmGetBool(st, "stream_enabled", m_streamEnabled);
        if (m_streamEnabled != v) { m_streamEnabled = v; emit streamEnabledChanged(); }
    }
    if (vmHas(st, "doa_enabled")) {
        const bool v = vmGetBool(st, "doa_enabled", m_doaEnabled);
        if (m_doaEnabled != v) { m_doaEnabled = v; emit doaEnabledChanged(); }
    }
    if (vmHas(st, "spectrum_enabled")) {
        const bool v = vmGetBool(st, "spectrum_enabled", m_spectrumEnabled);
        if (m_spectrumEnabled != v) { m_spectrumEnabled = v; emit spectrumEnabledChanged(); }
    }

    if (vmHas(st, "fft_channel")) {
        const int v = vmGetInt(st, "fft_channel", m_fftChannel);
        if (m_fftChannel != v) { m_fftChannel = v; emit fftChannelChanged(); }
    }
    if (vmHas(st, "fft_points")) {
        const int v = vmGetInt(st, "fft_points", m_fftPoints);
        if (m_fftPoints != v) { m_fftPoints = v; emit fftPointsChanged(); }
    }
    if (vmHas(st, "fft_downsample")) {
        const int v = vmGetInt(st, "fft_downsample", m_fftDownsample);
        if (m_fftDownsample != v) { m_fftDownsample = v; emit fftDownsampleChanged(); }
    }

    if (vmHas(st, "tx_hz")) {
        const double v = vmGetDouble(st, "tx_hz", m_txHz);
        if (!qFuzzyCompare(v + 1.0, m_txHz + 1.0)) { m_txHz = v; emit txHzChanged(); }
    }

    if (vmHas(st, "doa_offset_hz")) {
        const double v = vmGetDouble(st, "doa_offset_hz", m_doaOffsetHz);
        if (!qFuzzyCompare(v + 1.0, m_doaOffsetHz + 1.0)) { m_doaOffsetHz = v; emit doaOffsetHzChanged(); }
    }
    if (vmHas(st, "doa_bw_hz")) {
        const double v = vmGetDouble(st, "doa_bw_hz", m_doaBwHz);
        if (!qFuzzyCompare(v + 1.0, m_doaBwHz + 1.0)) { m_doaBwHz = v; emit doaBwHzChanged(); }
    }
    if (vmHas(st, "gate_th_db")) {
        const double v = vmGetDouble(st, "gate_th_db", m_gateThDb);
        if (!qFuzzyCompare(v + 1.0, m_gateThDb + 1.0)) { m_gateThDb = v; emit gateThDbChanged(); }
    }

    if (vmHas(st, "uca_radius_m")) {
        const double v = vmGetDouble(st, "uca_radius_m", m_ucaRadiusM);
        if (!qFuzzyCompare(v + 1.0, m_ucaRadiusM + 1.0)) { m_ucaRadiusM = v; emit ucaRadiusMChanged(); }
    }
    if (vmHas(st, "num_antennas")) {
        const int v = vmGetInt(st, "num_antennas", m_numAntennas);
        if (m_numAntennas != v) { m_numAntennas = v; emit numAntennasChanged(); }
    }

    if (vmHas(st, "doa_algo")) {
        const QString a = st.value("doa_algo").toString();
        if (!a.isEmpty() && m_doaAlgo != a) { m_doaAlgo = a; emit doaAlgoChanged(); }
    }

    if (vmHas(st, "fc_hz")) {
        const double v = vmGetDouble(st, "fc_hz", m_fcHz);
        if (!qFuzzyCompare(v + 1.0, m_fcHz + 1.0)) { m_fcHz = v; emit fcHzChanged(); }
    }
    if (vmHas(st, "nco_update_en")) {
        const int v = vmGetInt(st, "nco_update_en", m_ncoUpdateEn);
        if (m_ncoUpdateEn != v) { m_ncoUpdateEn = v; emit ncoUpdateEnChanged(); }
    }

    // ---- RF ATT AGC state (flat keys, from Ack.state) ----
    bool _rfAgcChanged = false;

    if (vmHas(st, "rf_agc_enabled")) {
        const bool v = vmGetBool(st, "rf_agc_enabled", m_rfAgcEnabled);
        if (m_rfAgcEnabled != v) { m_rfAgcEnabled = v; _rfAgcChanged = true; }
    }
    if (vmHas(st, "rf_agc_ch_enabled") || vmHas(st, "rf_agc_target_db")) {
        const QVariantList chEn = st.value(QStringLiteral("rf_agc_ch_enabled")).toList();
        const QVariantList tgs  = st.value(QStringLiteral("rf_agc_target_db")).toList();

        QVariantList out;
        const int N = 5; // IQ server provides 5 channels (0..4)
        for (int i = 0; i < N; ++i) {
            QJsonObject o;
            o[QStringLiteral("ch")] = i;
            if (i < chEn.size()) o[QStringLiteral("enabled")] = chEn[i].toBool();
            if (i < tgs.size())  o[QStringLiteral("target_db")] = tgs[i].toDouble();
            out.append(o.toVariantMap());
        }
        if (m_rfAgcChannels != out) { m_rfAgcChannels = out; _rfAgcChanged = true; }
    }
    if (vmHas(st, "scanner_att_db")) {
        const double v = vmGetDouble(st, "scanner_att_db", m_scannerAttDb);
        if (!qFuzzyCompare(m_scannerAttDb + 1.0, v + 1.0)) { m_scannerAttDb = v; emit scannerAttDbChanged(); }
    }
    if (_rfAgcChanged) emit rfAgcChanged();
}

// ---------------- internal state setters ----------------

void DoaClient::setStatus(const QString &s)
{
    if (m_statusText == s) return;
    m_statusText = s;
    emit statusTextChanged();
}

void DoaClient::setConnected(bool c)
{
    if (m_connected == c) return;
    m_connected = c;
    emit connectedChanged();
}
