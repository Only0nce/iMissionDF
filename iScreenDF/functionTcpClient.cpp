#include "iScreenDF.h"

void iScreenDF::updateFromTcpServer(const QJsonObject &obj)
{
    const QString menuID = obj.value("menuID").toString();

    // =====================================================
    // 1) ส่งต่อไปยัง TCP Clients ที่ต่อเข้ามาหา iScreenDF
    //    (DoAResult / Ack / อื่นๆ) -> broadcastLine
    // =====================================================
    if (tcpServerDF) {
        const QString jsonLine = QString::fromUtf8(
            QJsonDocument(obj).toJson(QJsonDocument::Compact)
            );
        tcpServerDF->broadcastLine(jsonLine);
    }

    // =====================================================
    // 2) แยก handle ตาม menuID
    // =====================================================
    if (menuID == "DoAResult") {

        // ---- basic state ----
        const bool streamEnabled   = obj.value("stream_enabled").toBool(false);
        const bool doaEnabled      = obj.value("doa_enabled").toBool(false);
        const bool spectrumEnabled = obj.value("spectrum_enabled").toBool(false);

        const double txHz          = obj.value("tx_hz").toDouble(0.0);
        const double fcHz          = obj.value("frequency_hz").toDouble(0.0);
        const double fsHz          = obj.value("fs_hz").toDouble(0.0);

        const double doaOffsetHz   = obj.value("doa_offset_hz").toDouble(0.0);
        const double doaBwHz       = obj.value("doa_bw_hz").toDouble(0.0);

        const bool signalPresent   = obj.value("signal_present").toBool(false);
        const double bandPeakDb    = obj.value("band_peak_db").toDouble(-200.0);
        const double gateThDb      = obj.value("gate_th_db").toDouble(-65.0);
        const double doaSigPower   = obj.value("doa_sig_power").toDouble(0.0);

        const int fftChannel       = obj.value("fft_channel").toInt(0);

        // ---- DOA peak arrays ----
        double doaDeg = 0.0;
        double conf   = 0.0;
        {
            const QJsonArray doas = obj.value("doas").toArray();
            const QJsonArray confArr = obj.value("confidence").toArray();
            if (!doas.isEmpty()) doaDeg = doas.at(0).toDouble();
            if (!confArr.isEmpty()) conf = confArr.at(0).toDouble();
        }

        // ---- MUSIC spectrum arrays ----
        // theta_deg / spectrum (อาจยาวมาก)
        const QJsonArray thetaDegArr = obj.value("theta_deg").toArray();
        const QJsonArray specArr     = obj.value("spectrum").toArray();

        // ---- FFT arrays (ถ้า spectrum_enabled = true จะมีข้อมูล) ----
        const QJsonArray fftFreqArr  = obj.value("fft_freq_hz").toArray();
        const QJsonArray fftMagArr   = obj.value("fft_mag_db").toArray();

        // =====================================================
        // ✅ ตรงนี้คือ “อัปเดตตัวแปรภายใน” ของ iScreenDF ตามที่คุณมีจริง
        // (ผมใส่เป็น qDebug ให้ก่อน ถ้าคุณมี member/Signal ก็แทนได้เลย)
        // =====================================================
        // qDebug() << "[iScreenDF][updateFromTcpServer] DoAResult:"
        //          << "fc_hz=" << fcHz
        //          << "fs_hz=" << fsHz
        //          << "stream=" << streamEnabled
        //          << "doa=" << doaEnabled
        //          << "spectrum=" << spectrumEnabled
        //          << "tx_hz=" << txHz
        //          << "offset_hz=" << doaOffsetHz
        //          << "bw_hz=" << doaBwHz
        //          << "sig=" << signalPresent
        //          << "band_peak_db=" << bandPeakDb
        //          << "gate_th_db=" << gateThDb
        //          << "doa_deg=" << doaDeg
        //          << "conf=" << conf
        //          << "fft_ch=" << fftChannel
        //          << "thetaN=" << thetaDegArr.size()
        //          << "specN=" << specArr.size()
        //          << "fftN=" << fftFreqArr.size();

        // ตัวอย่างถ้าคุณมี member ใน iScreenDF (ค่อย uncomment/ปรับชื่อเอง)
        // m_streamEnabled = streamEnabled;
        // m_doaEnabled = doaEnabled;
        // m_spectrumEnabled = spectrumEnabled;
        // m_txHz = txHz;
        // m_fcHz = fcHz;
        // m_doaOffsetHz = doaOffsetHz;
        // m_doaBwHz = doaBwHz;
        // m_signalPresent = signalPresent;
        // m_bandPeakDb = bandPeakDb;
        // m_gateThDb = gateThDb;
        // m_doaDeg = doaDeg;
        // m_confidence = conf;
        // m_fftChannel = fftChannel;
        // emit doaUpdated(); / emit spectrumUpdated(); / emit fftUpdated();

        onDoAResultReceived(obj);
    }
    else if (menuID == "Ack") {

        const QString ackFor = obj.value("ackFor").toString();
        const QJsonObject st = obj.value("state").toObject();

        // state ที่ python ส่งมาใน Ack มี key พวกนี้:
        // stream_enabled, doa_enabled, spectrum_enabled,
        // fft_points, fft_downsample, fft_channel,
        // tx_hz, doa_offset_hz, doa_bw_hz, gate_th_db

        qDebug() << "[iScreenDF][updateFromTcpServer] Ack:"
                 << "ackFor=" << ackFor
                 << "state keys=" << st.keys();

        if (!st.isEmpty()) {
            const bool streamEnabled   = st.value("stream_enabled").toBool(false);
            const bool doaEnabled      = st.value("doa_enabled").toBool(false);
            const bool spectrumEnabled = st.value("spectrum_enabled").toBool(false);

            const int fftPoints        = st.value("fft_points").toInt(0);
            const int fftDownsample    = st.value("fft_downsample").toInt(1);
            const int fftChannel       = st.value("fft_channel").toInt(0);

            const double txHz          = st.value("tx_hz").toDouble(0.0);
            const double doaOffsetHz   = st.value("doa_offset_hz").toDouble(0.0);
            const double doaBwHz       = st.value("doa_bw_hz").toDouble(0.0);
            const double gateThDb      = st.value("gate_th_db").toDouble(-65.0);

            qDebug() << "[iScreenDF][updateFromTcpServer] Ack state:"
                     << "stream=" << streamEnabled
                     << "doa=" << doaEnabled
                     << "spectrum=" << spectrumEnabled
                     << "fft_points=" << fftPoints
                     << "fft_downsample=" << fftDownsample
                     << "fft_channel=" << fftChannel
                     << "tx_hz=" << txHz
                     << "offset_hz=" << doaOffsetHz
                     << "bw_hz=" << doaBwHz
                     << "gate_th_db=" << gateThDb;

            // m_streamEnabled = streamEnabled; emit streamEnabledChanged();
            // ...
        }
    }
    else {
        qDebug() << "[iScreenDF][updateFromTcpServer] menuID=" << menuID;
    }
}

void iScreenDF::onDoAResultReceived(const QJsonObject &obj)
{
    double doaDeg = 0.0;
    double conf   = 0.0;

    QJsonArray doasArr = obj.value("doas").toArray();
    QJsonArray confArr = obj.value("confidence").toArray();

    if (!doasArr.isEmpty())
        doaDeg = doasArr.at(0).toDouble();
    if (!confArr.isEmpty())
        conf = confArr.at(0).toDouble();

    QVariantList thetaList;
    QVariantList specList;

    QJsonArray thetaArr = obj.value("theta_deg").toArray();
    QJsonArray specArr  = obj.value("spectrum").toArray();

    int n = qMin(thetaArr.size(), specArr.size());
    thetaList.reserve(n);
    specList .reserve(n);

    for (int i = 0; i < n; ++i) {
        thetaList.append(thetaArr.at(i).toDouble());
        specList .append(specArr.at(i).toDouble());
    }

    const QString serial = this->Serialnumber;     // หรือแหล่งจริงของคุณ
    const QString name   = this->controllerName;   // ให้ตรงกับ UpdateGPSMarker ที่ส่ง "name"

    QJsonObject frameObj;
    frameObj["menuID"]         = "doaFrameUpdated";
    frameObj["thetaList"]      = QJsonArray::fromVariantList(thetaList);
    frameObj["specList"]       = QJsonArray::fromVariantList(specList);
    frameObj["doaDeg"]         = doaDeg;
    frameObj["confidence"]     = conf;

    broadcastMessageServerandClient(frameObj);
    // qDebug() << "[iScreenDF::onDoAResultReceived]"
    //          << "DOA =" << doaDeg
    //          << "conf =" << conf
    //          << "points =" << n;

    emit doaFrameUpdated(Serialnumber ,controllerName,thetaList, specList, doaDeg, conf);
}
