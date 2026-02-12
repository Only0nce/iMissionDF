#include "iScreenDF.h"

// void iScreenDF::TextMessageReceived(const QString &message)
// {
//     qDebug() << "[iScreenDF::TextMessageReceived]" << message;

//     // ถ้าอยาก parse JSON ก็ทำได้แบบนี้
//     QJsonParseError err;
//     QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &err);
//     if (err.error != QJsonParseError::NoError) {
//         qWarning() << "[iScreenDF::TextMessageReceived] JSON parse error:" << err.errorString();
//         return;
//     }

//     QJsonObject obj = doc.object();
//     const QString menuID = obj.value("menuID").toString();

//     // ตัวอย่าง: broadcast ต่อไปยัง web dashboard
//     if (chatServerDF) {
//         chatServerDF->broadcastMessage(message);
//     }

//     // ตรงนี้คุณจะเช็ค menuID แล้วแตกเคสก็ได้
//     // if (menuID == "SomeCommand") { ... }
// }

// void iScreenDF::SendNetworkiScreentoServerKraken(const QString &message)
// {
//     qDebug() << "[iScreenDF::SendNetworkiScreentoServerKraken]" << message;

//     // ตรงนี้ตามชื่อเหมือนเอา network config จาก iScreen ไปให้ Kraken Server
//     // คุณสามารถ parse JSON แล้วเรียก networkMng / db / netServerKraken อะไรก็ได้
//     // ตอนนี้ขอทำตัวอย่างง่าย ๆ: broadcast ต่อไปก่อน

//     if (chatServerDF) {
//         chatServerDF->broadcastMessage(message);
//     }

//     // หรือถ้าคุณมีฟังก์ชันเดิมอยู่แล้ว เช่น:
//     // SendNetworkiScreentoServerKraken_old(message);
// }

// void iScreenDF::onDoAResultReceived(const QJsonObject &obj)
// {
//     double doaDeg = 0.0;
//     double conf   = 0.0;

//     QJsonArray doasArr = obj.value("doas").toArray();
//     QJsonArray confArr = obj.value("confidence").toArray();

//     if (!doasArr.isEmpty())
//         doaDeg = doasArr.at(0).toDouble();
//     if (!confArr.isEmpty())
//         conf = confArr.at(0).toDouble();

//     QVariantList thetaList;
//     QVariantList specList;

//     QJsonArray thetaArr = obj.value("theta_deg").toArray();
//     QJsonArray specArr  = obj.value("spectrum").toArray();

//     int n = qMin(thetaArr.size(), specArr.size());
//     thetaList.reserve(n);
//     specList .reserve(n);

//     for (int i = 0; i < n; ++i) {
//         thetaList.append(thetaArr.at(i).toDouble());
//         specList .append(specArr.at(i).toDouble());
//     }

//     // qDebug() << "[iScreenDF::onDoAResultReceived]"
//     //          << "DOA =" << doaDeg
//     //          << "conf =" << conf
//     //          << "points =" << n;

//     emit doaFrameUpdated(thetaList, specList, doaDeg, conf);
// }


