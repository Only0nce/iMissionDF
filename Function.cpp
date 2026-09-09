#include "PLCServer.h"

namespace {

void removeRawDataFiles()
{
    QDir dir(QStringLiteral("/home/pi/Rawdata"));
    if (!dir.exists()) {
        return;
    }

    const QFileInfoList files = dir.entryInfoList(QStringList{QStringLiteral("data*.raw"),
                                                              QStringLiteral("data*.download")},
                                                  QDir::Files | QDir::NoSymLinks,
                                                  QDir::Name);
    for (const QFileInfo &fileInfo : files) {
        QFile::remove(fileInfo.absoluteFilePath());
    }
}

} // namespace

void PLCServer::newdataParameterEquipment(QString SubstationName, int Voltage, QString TransmissionLineName, QString Distance, QString IPaddress, QString Brand, QString Model, QString SerialNo, QString ContractNumber, QString Date, QString LFLSerialNo) {
    SetupEquipment->SubstationName = SubstationName;
    SetupEquipment->Voltage = Voltage;
    SetupEquipment->TransmissionLineName = TransmissionLineName;
    SetupEquipment->Distance = Distance;
    SetupEquipment->IPaddress = networks->ip_address;
    SetupEquipment->Brand = Brand;
    SetupEquipment->Model = Model;
    SetupEquipment->SerialNo = SerialNo;
    SetupEquipment->ContractNumber = ContractNumber;
    SetupEquipment->Date = Date;
    SetupEquipment->LFLSerialNo = LFLSerialNo;
    //    qDebug()<< "get from SetupEquipmentdata" << "SubstationName" <<SetupEquipment->SubstationName << "Voltage"<<SetupEquipment->Voltage<<
    //                       "TransmissionLineName"<<SetupEquipment->TransmissionLineName<< "Distance" << SetupEquipment->Distance << "IPaddress"
    //                    << SetupEquipment->IPaddress << "Brand "<<SetupEquipment->Brand << "Model" << SetupEquipment->Model << "SerialNo"<<
    //                       SetupEquipment->SerialNo << "ContractNumber"<< SetupEquipment->ContractNumber << "Date"<<SetupEquipment->Date
    //                    <<"LFLSerialNo"<<SetupEquipment->LFLSerialNo;
}
void PLCServer::newdataSmtpParameter(QString senderEmail, QString NamesenderEmail, QString Password, QString recipientEmail, QString recipientName, QString SmtpServer, int SmtpPort) {
    Email_Config->senderEmail = senderEmail;
    Email_Config->senderName = NamesenderEmail;
    Email_Config->password = Password;
    Email_Config->recipientEmail = recipientEmail;
    Email_Config->recipientName = recipientName;
    Email_Config->smtpServer = SmtpServer;
    Email_Config->smtpPort = SmtpPort;
    //    qDebug() << "newdataMysql[1]" <<"senderEmail" << Email_Config->senderEmail << "NamesenderEmail" << Email_Config->senderName << "Password" << Email_Config->password << "recipientEmail" << Email_Config->recipientEmail
    //                << "recipientName" <<  Email_Config->recipientName << "SmtpServer" << Email_Config->smtpServer << "SmtpPort" << Email_Config->smtpPort;
}
void PLCServer::newdataMysql(QString ip, bool plcDO, bool plcDI, bool HiSpeedPhaseA, bool HiSpeedPhaseB, bool HiSpeedPhaseC, bool modbusPhaseA, bool modbusPhaseB, bool modbusPhaseC, bool gpsModule, bool systeminit, bool communication, bool relayStart, bool surgeStart, bool periodic, bool manualTest, bool lflFail, bool lflOperate) {
    networks->ip_snmp = ip;
    Monitor_param->PLC_DO_ERROR = plcDO;
    Monitor_param->PLC_DI_ERROR = plcDI;
    Monitor_param->MODULE_HI_SPEED_PHASE_A_ERROR = HiSpeedPhaseA;
    Monitor_param->MODULE_HI_SPEED_PHASE_B_ERROR = HiSpeedPhaseB;
    Monitor_param->MODULE_HI_SPEED_PHASE_C_ERROR = HiSpeedPhaseC;
    Monitor_param->INTERNAL_PHASE_A_ERROR = modbusPhaseA;
    Monitor_param->INTERNAL_PHASE_B_ERROR = modbusPhaseB;
    Monitor_param->INTERNAL_PHASE_C_ERROR = modbusPhaseC;
    Monitor_param->GPS_MODULE_FAIL = gpsModule;
    Monitor_param->SYSTEM_INITIAL = systeminit;
    Monitor_param->COMMUNICATION_ERROR = communication;
    Monitor_param->RELAY_START_EVENT = relayStart;
    Monitor_param->SURGE_START_EVENT = surgeStart;
    Monitor_param->PERIODIC_TEST_EVENT = periodic;
    Monitor_param->MANUAL_TEST_EVENT = manualTest;
    Monitor_param->LFL_FAIL = lflFail;
    Monitor_param->LFL_OPERATE = lflOperate;
    *snmp_param = *Monitor_param;
    qDebug() << "newdataMysql[2]" << "ipSnmpServer" << networks->ip_snmp << "plcDO" << Monitor_param->PLC_DO_ERROR << "plcDI" << Monitor_param->PLC_DI_ERROR << "HiSpeedA" << Monitor_param->MODULE_HI_SPEED_PHASE_A_ERROR << "HiSpeedB" << Monitor_param->MODULE_HI_SPEED_PHASE_B_ERROR << "HiSpeedC" << Monitor_param->MODULE_HI_SPEED_PHASE_C_ERROR << "modbusA" << Monitor_param->INTERNAL_PHASE_A_ERROR << "modbusB" << Monitor_param->INTERNAL_PHASE_B_ERROR << "modbusC" << Monitor_param->INTERNAL_PHASE_C_ERROR << "Gps" << Monitor_param->GPS_MODULE_FAIL << "systeminit" << Monitor_param->SYSTEM_INITIAL << "communication" << Monitor_param->COMMUNICATION_ERROR << "relayStart" << Monitor_param->RELAY_START_EVENT << "surge" << Monitor_param->SURGE_START_EVENT << "periodic" << Monitor_param->PERIODIC_TEST_EVENT << "manualTest" << Monitor_param->MANUAL_TEST_EVENT << "lflFail" << Monitor_param->LFL_FAIL << "lflOperate" << Monitor_param->LFL_OPERATE;
}

void PLCServer::updateEmailSenderConfig(QString senderEmail, QString senderName, QString password, QString smtpServer, int smtpPort) {
    QString filename = "/home/pi/.msmtprc";
    QString newData;

    newData = QString(
                  "defaults\n"
                  "auth on\n"
                  "tls on\n"
                  "tls_trust_file /etc/ssl/certs/ca-certificates.crt\n"
                  "account gmail\n"
                  "host %1\n"
                  "port %2\n"
                  "from %3\n"
                  "user %4\n"
                  "password %5\n"
                  "account default : gmail\n")
                  .arg(smtpServer)
                  .arg(smtpPort)
                  .arg(senderName)
                  .arg(senderEmail)
                  .arg(password);

    QFile file(filename);
    if (file.exists() && file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        QString existingData = in.readAll();
        file.close();
        if (existingData == newData) {
            qDebug() << "SMTP configuration is already up-to-date. No changes made.";
            return;
        }
    }
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << newData;
        file.close();
        qDebug() << "SMTP configuration file updated successfully.";
        //        if (snmp_address->state() == QAbstractSocket::ConnectedState) {
        //            QString msgs = QString("{\"menuID\":\"EmailConfig\", \"senderEmail\":\"%1\", \"senderName\":\"%2\", \"password\":\"%3\", \"recipientEmail\":\"%4\", \"recipientName\":\"%5\", \"Smtpserver\":\"%6\", \"SmtpPort\":\"%7\"}")
        //                               .arg(Email_Config->senderEmail)
        //                               .arg(Email_Config->senderName)
        //                               .arg(Email_Config->password)
        //                               .arg(Email_Config->recipientEmail)
        //                               .arg(Email_Config->recipientName)
        //                               .arg(Email_Config->smtpServer)
        //                               .arg(Email_Config->smtpPort);
        //            emit sendMessage(msgs,snmp_address);
        //            qDebug() << "SMTP update message sent.";
        //        } else {
        //            qDebug() << "SMTP_address not connected. State:" << snmp_address->state();
        //        }
        saveEmail_Config();
        QJsonDocument jsonDoc;
        QJsonObject Param;
        Param.insert("objectName", "EmailConfig");
        Param.insert("senderEmail", Email_Config->senderEmail);
        Param.insert("senderName", Email_Config->senderName);
        Param.insert("password", Email_Config->password);
        Param.insert("recipientEmail", Email_Config->recipientEmail);
        Param.insert("recipientName", Email_Config->recipientName);
        Param.insert("Smtpserver", Email_Config->smtpServer);
        Param.insert("SmtpPort", Email_Config->smtpPort);
        Param.insert("DELAY_EVENT_MAIL", Email_Param->DELAY_EVENT_MAIL);
        Param.insert("DELAY_ALARM_MAIL", Email_Param->DELAY_ALARM_MAIL);
        jsonDoc.setObject(Param);
        QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        if (snmp_address->state() == QAbstractSocket::ConnectedState)
            emit sendMessage(raw_data, snmp_address);
        else
            qDebug() << "snmp_address:" << snmp_address->state();
    } else {
        qDebug() << "Failed to write to SMTP configuration file.";
    }
}

void PLCServer::updateSnmpipServer(QString ip) {
    QString filename = "/etc/snmp/snmpd.conf";
    QString data;
    if (ip != "localhost") {
        data = QString(
                   "master  agentx\n"
                   "agentAddress udp:161,udp6:[::1]:161\n"
                   "rocommunity  public\n"
                   "rwcommunity  private\n"
                   "com2sec readonly default public\n"
                   "com2sec readwrite default private\n"
                   "trap2sink %1 public")
                   .arg(ip);
    } else {
        data = QString(
            "master  agentx\n"
            "agentAddress udp:161,udp6:[::1]:161\n"
            "rocommunity  public\n"
            "rwcommunity  private\n"
            "com2sec readonly default public\n"
            "com2sec readwrite default private\n"
            "trap2sink localhost public");
    }
    QByteArray dataAyyay(data.toLocal8Bit());
    QFile file(filename);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&file);
    out << dataAyyay;
    file.close();
    saveipServer();
    //    snmp_selection_state_list.ipSnmpServer = ip;
    QJsonDocument jsonDoc;
    QJsonObject Param;
    Param.insert("objectName", "UpdateSnmpIP");
    Param.insert("ip", networks->ip_snmp);
    jsonDoc.setObject(Param);
    QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
    //    qDebug()<<"sendMessage[UpdateSmtpIP]"<<raw_data;
    if (snmp_address->state() == QAbstractSocket::ConnectedState) {
        emit sendMessage(raw_data, snmp_address);
        //        qDebug() << "sendSNMPMessage:";
    } else
        qDebug() << "snmp_address:" << snmp_address->state();

    system("systemctl restart snmpd.service");

    //    system("systemctl restart iTransPLCsnmp.service");
}

void PLCServer::saveipServer() {
    QString snmpipserver = networks->ip_snmp;
    myDatabase->updateSnmpipserver(snmpipserver);
    //   qDebug() << "Command ip master :" << "Command_save" << snmpipserver;
}
void PLCServer::saveEmail_Config() {
    QString senderEmail = Email_Config->senderEmail;
    QString NamesenderEmail = Email_Config->senderName;
    QString Password = Email_Config->password;
    QString recipientEmail = Email_Config->recipientEmail;
    QString recipientName = Email_Config->recipientName;
    QString SmtpServer = Email_Config->smtpServer;
    int SmtpPort = Email_Config->smtpPort;
    //    qDebug()<<"5555555";
    myDatabase->updateEmail_Config(senderEmail, NamesenderEmail, Password, recipientEmail, recipientName, SmtpServer, SmtpPort);
    //     qDebug() << "saveEmail_Config[1]" <<"senderEmail" <<senderEmail << "NamesenderEmail" << NamesenderEmail << "Password" << Password << "recipientEmail" << recipientEmail
    //                 << "recipientName" <<  recipientName << "SmtpServer" << SmtpServer << "SmtpPort" << SmtpPort;
}
void PLCServer::updateSnmpSelectionState() {
    bool plcDO = snmp_param->PLC_DO_ERROR;
    bool plcDI = snmp_param->PLC_DI_ERROR;
    bool HiSpeedA = snmp_param->MODULE_HI_SPEED_PHASE_A_ERROR;
    bool HiSpeedB = snmp_param->MODULE_HI_SPEED_PHASE_B_ERROR;
    bool HiSpeedC = snmp_param->MODULE_HI_SPEED_PHASE_C_ERROR;
    bool modbusA = snmp_param->INTERNAL_PHASE_A_ERROR;
    bool modbusB = snmp_param->INTERNAL_PHASE_B_ERROR;
    bool modbusC = snmp_param->INTERNAL_PHASE_C_ERROR;
    bool Gps = snmp_param->GPS_MODULE_FAIL;
    bool systeminit = snmp_param->SYSTEM_INITIAL;
    bool communication = snmp_param->COMMUNICATION_ERROR;
    bool relayStart = snmp_param->RELAY_START_EVENT;
    bool surge = snmp_param->SURGE_START_EVENT;
    bool periodic = snmp_param->PERIODIC_TEST_EVENT;
    bool manualTest = snmp_param->MANUAL_TEST_EVENT;
    bool lflFail = snmp_param->LFL_FAIL;
    bool lflOperate = snmp_param->LFL_OPERATE;
    myDatabase->updateSnmpSelectionState(plcDO, plcDI, HiSpeedA, HiSpeedB, HiSpeedC, modbusA, modbusB, modbusC, Gps, systeminit, communication, relayStart, surge, periodic, manualTest, lflFail, lflOperate);
}
void PLCServer::updateNewRecipientgmail(QString msgs) {
    // qDebug() << "updateNewRecipientgmail" << msgs;
    //    emit sendMessage(msgs, snmp_address);

    QJsonDocument jsonDoc;
    QJsonObject Param;
    Param.insert("objectName", "updateRecipientgmail");
    Param.insert("email", msgs);
    jsonDoc.setObject(Param);
    QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
    //    qDebug()<<"sendMessage[UpdateSmtpIP]"<<raw_data;
    if (snmp_address->state() == QAbstractSocket::ConnectedState) {
        emit sendMessage(raw_data, snmp_address);
        //        qDebug() << "sendSNMPMessage:";
    } else
        qDebug() << "snmp_address:" << snmp_address->state();
}
void PLCServer::manageData(QString msgs, QWebSocket *wClient) {
    QtConcurrent::run([=]() {
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(msgs.toUtf8(), &parseError);

        if (parseError.error != QJsonParseError::NoError) {
            qDebug() << "[manageData] JSON parse error:" << parseError.errorString();
            return;
        }

        QJsonObject obj = doc.object();

        // 🔁 ให้ slot จริง ๆ กลับไปทำงานใน main thread ด้วย invokeMethod
        QMetaObject::invokeMethod(this, [=]() {
            handleManageObject(wClient, msgs); // แยก logic เดิมมาไว้ในฟังก์ชันนี้
        }, Qt::QueuedConnection);
    });
}

void PLCServer::handleManageObject(QWebSocket *wClient, QString msgs){
    QByteArray br = msgs.toUtf8();
    QJsonDocument doc = QJsonDocument::fromJson(br);
    QJsonObject obj = doc.object();
    QJsonObject command = doc.object();
    QString getCommand = QJsonValue(obj["objectName"]).toString();
    // qDebug() << "msgs" << msgs << wClient;

    // Event-process audit subscriber API for the existing web application.
    // No HTML is hosted by PLCServer.  A web client may use either the legacy
    // menuID request or the explicit objectName request and receives one
    // SNAPSHOT immediately, followed by real-time EVENT_START/STEP/
    // EVENT_COMPLETE messages on the same WebSocket connection.
    const bool eventAuditSnapshotRequest =
        obj["menuID"].toString() == QStringLiteral("getEventProcessAudit") ||
        obj["objectName"].toString() == QStringLiteral("GET_EVENT_PROCESS_AUDIT");

    if (eventAuditSnapshotRequest) {
        if (wClient && !webapp_address.contains(wClient)) {
            webapp_address.append(wClient);
        }
        sendEventAuditSnapshot(wClient);
        return;
    }

    if (getCommand == "CHANGE") {
        change_monitor = true;
        stopThread4 = true;
        // if(LogoutVNCCount > 10){
        //     stopThread4 = true;
        //     LogoutVNCCount = 0;
        // }
        qDebug() << "change_monitor CHANGE" << change_monitor;
    }
    // SMTP
    else if (getCommand == "UpdateSmtpParameter") {
        qDebug() << "Smtpport" << obj["Smtpport"].toInt();
        Email_Config->senderEmail = obj["senderEmail"].toString();
        Email_Config->senderName = obj["senderName"].toString();
        Email_Config->password = obj["password"].toString();
        Email_Config->recipientEmail = obj["recipientEmail"].toString();
        Email_Config->recipientName = obj["recipientName"].toString();
        Email_Config->smtpServer = obj["Smtpserver"].toString();
        Email_Config->smtpPort = obj["Smtpport"].toInt();
        updateSMTP();

        Email_Param->DELAY_EVENT_MAIL = obj["DELAY_EVENT_MAIL"].toInt();
        Email_Param->DELAY_ALARM_MAIL = obj["DELAY_ALARM_MAIL"].toInt();
        qDebug() << "DELAY_EVENT_MAIL" << obj["DELAY_EVENT_MAIL"].toInt() << "DELAY_ALARM_MAIL" << obj["DELAY_ALARM_MAIL"].toInt();
        updateEmailDelay();
        //         qDebug() << "UpdateSmtpParameter[1]" <<"senderEmail" << Email_Config->senderEmail << "NamesenderEmail" << Email_Config->senderName << "Password" << Email_Config->password << "recipientEmail" << Email_Config->recipientEmail
        //                     << "recipientName" <<  Email_Config->recipientName << "SmtpServer" << Email_Config->smtpServer << "SmtpPort" << Email_Config->smtpPort;
        updateEmailSenderConfig(Email_Config->senderEmail, Email_Config->senderName, Email_Config->password, Email_Config->smtpServer, Email_Config->smtpPort);
    } else if (obj["objectName"].toString() == "SurgePlot") {
        qDebug() << "objectName SurgePlot " << obj["phase"].toString();
        QJsonArray jsonArray = obj["data"].toArray();

        qDebug() << "start count at index" << fullPointLocal << "fullPointLocal" << fullPointLocal << " jsonArray.size()" << jsonArray.size() << " fullPointRemote" << fullPointRemote;
        for (int i = 0; i < fullPointRemote; ++i) {
            qDebug() << "for loop check" << i;
            //            if(i > fullPointLocal){
            if (jsonArray[i] == NULL || jsonArray[i] == "") {
                voltBkup.append(voltBkup.last());
                distBkup.append(currentDistanceSurge / 1000.00);
            } else {
                voltBkup.append(jsonArray[i]);
                distBkup.append(currentDistanceSurge / 1000.00);
            }
            //            }
            //            else{

            //            }
            currentDistanceSurge += (60 * sagFactor);
        }

        QJsonDocument jsonDoc;
        QJsonObject Param;
        QString raw_data;

        if (obj["phase"].toString() == "A") {
            Param.insert("objectName", "dataPlotingA");
            Param.insert("distance", distBkup);
            Param.insert("voltage", voltBkup);
            jsonDoc.setObject(Param);

            raw_data = jsonDoc.toJson(QJsonDocument::Compact);
            emit sendToMonitor(raw_data);
            emit sendToVNC(raw_data);
        } else if (obj["phase"].toString() == "B") {
            Param.insert("objectName", "dataPlotingB");
            Param.insert("distance", distBkup);
            Param.insert("voltage", voltBkup);
            jsonDoc.setObject(Param);

            raw_data = jsonDoc.toJson(QJsonDocument::Compact);
            emit sendToMonitor(raw_data);
            emit sendToVNC(raw_data);
        } else if (obj["phase"].toString() == "C") {
            Param.insert("objectName", "dataPlotingC");
            Param.insert("distance", distBkup);
            Param.insert("voltage", voltBkup);
            jsonDoc.setObject(Param);
            raw_data = jsonDoc.toJson(QJsonDocument::Compact);
            emit sendToMonitor(raw_data);
            emit sendToVNC(raw_data);
        }

    } else if (obj["objectName"].toString() == "GPIOTest") {
        updateMode("Manual");
        emit sendMessage(msgs, FPGA_address);
    } else if (obj["objectName"].toString() == "SurgeTest") {
        interlockPattern = false;
        updateMode("Surge");
    } else if (obj["objectName"].toString() == "RelayTest") {
        QJsonObject ParamP;
        ParamP.insert("menuID", "RelayTest");  // Name
        QString raw_dataP = QJsonDocument(ParamP).toJson(QJsonDocument::Compact).toStdString().c_str();
        emit sendToSocketPLC(raw_dataP);
        sendDIO = true;

        QJsonObject ParamPop;
        ParamPop.insert("objectName", "eventRecord");
        ParamPop.insert("testMode", "RelayTest");
        QString msg_pop = QJsonDocument(ParamPop).toJson(QJsonDocument::Compact).toStdString().c_str();
        emit sendToVNC(msg_pop);
        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(msg_pop, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }

        // qWarning() << "objectName RelayTest:" << raw_dataP;
    } else if(obj["objectName"].toString() == "RelayTestStop"){
        QJsonObject ParamP;
        ParamP.insert("menuID", "RelayTestStop");  // Name
        QString raw_dataP = QJsonDocument(ParamP).toJson(QJsonDocument::Compact).toStdString().c_str();
        emit sendToSocketPLC(raw_dataP);
        sendDIO = false;
        // qWarning() << "objectName RelayTestStop:" << raw_dataP;
    } else if (obj["objectName"].toString() == "PatternTest") {
        //        updateMode("Pattern");
        interlockPattern = true;
        maxNumOfPattern = 0;
        numOfPattern = 0;
        maxNumOfPattern = obj["number"].toInt();
        qDebug() << "maxNumOfPattern" << maxNumOfPattern << " numOfPattern" << numOfPattern;
        // qWarning() << "maxNumOfPattern" << maxNumOfPattern << " numOfPattern" << numOfPattern;
        QJsonDocument jsonDoc;
        QJsonObject Param;
        Param.insert("objectName", "PatternTest");
        Param.insert("number", maxNumOfPattern);
        QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        emit sendMessage(raw_data, FPGA_address);

        QJsonObject().swap(Param);
        Param.insert("objectName", "PatternCount");
        Param.insert("count", numOfPattern);
        Param.insert("msg", "PATTERN START");
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(raw_data, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(raw_data);
        numOfPattern++;
        interlockPressPattern = true;
        patternTimer->start(10000);
    } else if (obj["objectName"].toString() == "ManualTest") {
        interlockPattern = false;
        QJsonDocument jsonDoc;
        QJsonObject Param;
        Param.insert("objectName", "ManualTest");
        jsonDoc.setObject(Param);
        QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        emit sendMessage(msgs, FPGA_address);
        qDebug() << "ManualTest ManualTest ManualTest ManualTest";
        //        updateMode("Manual");
    }
    // system
    else if (obj["objectName"].toString() == "SwVersion") {
        qDebug() << "VersionUpdate" << obj;
        QJsonDocument jsonDoc;
        QJsonObject Param, Param2;
        if (obj["HwName"].toString() == "OpenPLC") {
            OpenPLC_address = wClient;
            qDebug() << "OpenPLC->address:" << OpenPLC_address << " wClient:" << wClient;
            if (obj["SwVersion"].toString() == SwVersion) {               // SwVersion เอามาจาก database
                Param.insert("objectName", "SwVersion");                  //
                Param.insert("SwVersion", "already the newest version");  //
                jsonDoc.setObject(Param);
                QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                //                qDebug() << "raw_data:" << raw_data;
                //                QThread::msleep(100);
                emit sendMessage(raw_data, wClient);
            } else {
                Param.insert("objectName", "SwVersion");  // SwVersion เอามาจาก database
                Param.insert("SwVersion", "Not Update");
                jsonDoc.setObject(Param);
                QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                //                qDebug() << "raw_data:" << raw_data;
                //                QThread::msleep(100);
                emit sendMessage(raw_data, wClient);
            }
            sendInitial(wClient);
            Aux->printinfo();

            //            uint8_t reg[1] = {1}; //มีแค่ 12 output ต้องเก็บค่าให้ครบ
            //            reg[0] = Aux->fails;
            //            QJsonDocument jsonDoc2;
            //            QJsonObject Param2;
            //            QJsonArray Reg2;
            //            QString raw_datas2;
            //            Param2.insert("objectName","writeCoils");
            //            for(int i=0;i<sizeof(reg);i++){
            //                Reg2.append(reg[i]);
            //            }
            //            Param2.insert("register",Reg2);
            //            Param2.insert("index",800);
            //            jsonDoc2.setObject(Param2);
            //            raw_datas2 = QJsonDocument(Param2).toJson(QJsonDocument::Compact).toStdString().c_str();
            //            // OpenPLC_address
            //            if(OpenPLC_address->state() == QAbstractSocket::ConnectedState)
            //                emit sendMessage(raw_datas2, OpenPLC_address);
            //            else
            //                qDebug() << "OpenPLC_address:" << OpenPLC_address->state();

            //            Reg2 = QJsonArray();
            //            uint8_t reg2[1] = {1}; //มีแค่ 12 output ต้องเก็บค่าให้ครบ
            //            reg2[0] = Aux->operate;
            //            Param2.insert("objectName","writeCoils");
            //            for(int i=0;i<sizeof(reg2);i++){
            //                Reg2.append(reg2[i]);
            //            }
            //            Param2.insert("register",Reg2);
            //            Param2.insert("index",801);
            //            jsonDoc2.setObject(Param2);
            //            raw_datas2 = QJsonDocument(Param2).toJson(QJsonDocument::Compact).toStdString().c_str();
            //            // OpenPLC_address
            //            if(OpenPLC_address->state() == QAbstractSocket::ConnectedState)
            //                emit sendMessage(raw_datas2, OpenPLC_address);
            //            else
            //                qDebug() << "OpenPLC_address:" << OpenPLC_address->state();

        } else if (obj["HwName"].toString() == "INPUT_PLC") {
            INPUT_PLC_address = wClient;
            sendDelayToClient(INPUT_PLC_address);

            Param.insert("objectName", "START_SERVICE");  //
            jsonDoc.setObject(Param);
            QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
            qDebug() << "INPUT_PLC:" << raw_data;
            emit sendMessage(raw_data, wClient);

            Param.insert("objectName", "selectUser");
            Param.insert("userType", masterLFL);
            Param.insert("ip_master", masterIP);
            Param.insert("ip_slave", slaveIP);
            if (masterLFL == "MASTER") {
                Param.insert("RemoteTOMonitor", "REMOTE TO SLAVE");
            } else if (masterLFL == "SLAVE") {
                Param.insert("RemoteTOMonitor", "REMOTE TO MASTER");
            }

            raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();

            if (INPUT_PLC_address->state() == QAbstractSocket::ConnectedState) {
                emit sendMessage(raw_data, INPUT_PLC_address);
                qDebug() << "INPUT_PLC_address";
                // qWarning() << "INPUT_PLC_address raw_data:" << raw_data;
            } else
                qDebug() << "INPUT_PLC_address:" << INPUT_PLC_address->state();
        } else if (obj["HwName"].toString() == "PLCServer") {
            if (obj["SwVersion"].toString() == SwVersion) {               // SwVersion เอามาจาก database
                Param.insert("objectName", "SwVersion");                  //
                Param.insert("SwVersion", "already the newest version");  //
                jsonDoc.setObject(Param);
                QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                // qDebug() << "raw_data:" << raw_data;
                //                QThread::msleep(300);
                emit sendMessage(raw_data, wClient);
            } else {
                Param.insert("objectName", "SwVersion");  // SwVersion เอามาจาก database
                Param.insert("SwVersion", "Not Update");
                jsonDoc.setObject(Param);
                QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                // qDebug() << "raw_data:" << raw_data;
                //                QThread::msleep(300);
                emit sendMessage(raw_data, wClient);
            }
            PLCserver_address = wClient;
            sendInitial(wClient);
        } else if (obj["HwName"].toString() == "Monitor") {
            removeRawDataFiles();
            // qDebug() << "margins Monitors size:" << wClient << Monitor_address.size() << Monitor_address.isEmpty() << " ipaddress" << obj["ipaddress"].toString();
            // qDebug() << "old ip::" << ipaddress_monitor
            //          << " new ip::" << obj["ipaddress"].toString();
            if(Monitor_address.size() <= 0){
                Monitor_address.append(wClient);
                server->m_Monitor.append(wClient);
                // qWarning() << "if Monitor_address.size()" << Monitor_address.size();
            }
            else{
                QJsonDocument jsonDoc;
                QJsonObject Param;
                Param.insert("objectName", "Pop-up");
                if (masterLFL == "SLAVE") {
                    Param.insert("msg", "REMOTE FROM MASTER");
                } else if (masterLFL == "MASTER") {
                    Param.insert("msg", "REMOTE FROM SLAVE");
                }
                Param.insert("state", true);
                jsonDoc.setObject(Param);
                QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                Q_FOREACH (QWebSocket *pClient, Monitor_address) {
                    if (pClient->state() == QAbstractSocket::ConnectedState){
                        emit sendMessage(raw_data, pClient);
                        qDebug() << "Pop-up :: " << raw_data;
                    }
                    else
                        qDebug() << "Monitor_address:" << pClient->state();
                }

                Param.insert("objectName", "Pop-up");
                Param.insert("msg", "REMOTE TO " + masterLFL);
                Param.insert("state", false);
                jsonDoc.setObject(Param);
                raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                if (wClient->state() == QAbstractSocket::ConnectedState){
                    emit sendMessage(raw_data, wClient);
                }
                else
                    qDebug() << "Monitor_address:" << wClient->state();
                // loopLogout->start(1000 * 6 * 10 * 5);
                stopThread4 = false;
                stopThread5 = false;
                int retnew = pthread_create( & idThread4, NULL, ThreadFunc4, this);
                if (retnew == 0) {
                    qDebug() << ("ThreadFunc4 created successfully.\n");
                } else {
                    qDebug() << ("ThreadFunc4 not created.\n");
                }

                Monitor_address.append(wClient);
                server -> m_Monitor.append(wClient);
                // qWarning() << "else Monitor_address.size()" << Monitor_address.size();
            }

            // qWarning()<< "Monitor_address" << Monitor_address
            //           << " server -> m_Monitor" << server -> m_Monitor;

            // qWarning()<< "MonitorSecond_address" << MonitorSecond_address
            //           << " MonitorFirst_address" << MonitorFirst_address
            //           << " obj['ipaddress'].toString()" << obj["ipaddress"].toString();
            // qDebug() << "MonitorSecond_address" << (MonitorSecond_address == nullptr)
            //          << " MonitorFirst_address" << (MonitorFirst_address == nullptr);


            // if (MonitorFirst_address != nullptr && MonitorSecond_address == nullptr && obj["ipaddress"].toString() != ipaddress_monitor) {
            //     qDebug() << "REMOTE FROM NEW IPADDRESS old ip:" << ipaddress_monitor << " NEW IP:" << obj["ipaddress"].toString();
            //     QJsonDocument jsonDoc;
            //     QJsonObject Param;
            //     Param.insert("objectName", "Pop-up");
            //     if (masterLFL == "SLAVE") {
            //         Param.insert("msg", "REMOTE FROM MASTER");
            //     } else if (masterLFL == "MASTER") {
            //         Param.insert("msg", "REMOTE FROM SLAVE");
            //     }
            //     Param.insert("state", true);
            //     jsonDoc.setObject(Param);
            //     QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
            //     Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            //         if (pClient->state() == QAbstractSocket::ConnectedState){
            //             emit sendMessage(raw_data, pClient);
            //             qDebug() << "Pop-up :: " << raw_data;
            //         }
            //         else
            //             qDebug() << "Monitor_address:" << pClient->state();
            //     }

            //     Param.insert("objectName", "Pop-up");
            //     Param.insert("msg", "REMOTE TO " + masterLFL);
            //     Param.insert("state", false);
            //     jsonDoc.setObject(Param);
            //     raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
            //     if (wClient->state() == QAbstractSocket::ConnectedState){
            //         emit sendMessage(raw_data, wClient);
            //     }
            //     else
            //         qDebug() << "Monitor_address:" << wClient->state();
            //     // loopLogout->start(1000 * 6 * 10 * 5);
            //     int retnew = pthread_create( & idThread4, NULL, ThreadFunc4, this);
            //     if (retnew == 0) {
            //         qDebug() << ("ThreadFunc4 created successfully.\n");
            //     } else {
            //         qDebug() << ("ThreadFunc4 not created.\n");
            //     }

            //     if(obj["ipaddress"].toString() != ""){
            //         ipaddress_monitor = obj["ipaddress"].toString();
            //     }
            //     MonitorSecond_address = wClient;
            //     Monitor_address.append(wClient);
            //     server->m_Monitor.append(wClient);
            //      qWarning()<< "if";
            // }
            // else{
            //     qWarning()<< "else";
            //     // if(obj["ipaddress"].toString() != ipaddress_monitor){
            //         qDebug() << "ipaddress_monitor else :::";
            //         if(obj["ipaddress"].toString() != ""){
            //             ipaddress_monitor = obj["ipaddress"].toString();
            //         }
            //         // ipaddress_monitor = obj["ipaddress"].toString();
            //         Monitor_address.append(wClient);
            //         MonitorFirst_address = wClient;
            //         server->m_Monitor.append(wClient);
            //         MonitorSecond_address = nullptr;
            //     // }
            // }
            qDebug() << "ipaddress_monitor values :::" << ipaddress_monitor;
                //            if(obj["SwVersion"].toString() == SwVersion){            //SwVersion เอามาจาก database
                //                Param.insert("objectName","SwVersion");	             //
                //                Param.insert("SwVersion","already the newest version");	             //
                //                jsonDoc.setObject(Param);
                //                QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                //                qDebug() << "raw_data:" << raw_data;
                ////                QThread::msleep(300);
                //                emit sendMessage(raw_data,wClient);
                //            }
                //            else{
                //                Param.insert("objectName","SwVersion");	             //SwVersion เอามาจาก database
                //                Param.insert("SwVersion","Not Update");
                //                jsonDoc.setObject(Param);
                //                QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                //                qDebug() << "raw_data:" << raw_data;
                ////                QThread::msleep(300);
                //                emit sendMessage(raw_data,wClient);
                ////                sendUpdateToMonitor();
                //            }

                if (version->Monitor_version != obj["SwVersion"].toString()) {
                    Param.insert("objectName", "SwVersion");  // SwVersion เอามาจาก database
                    Param.insert("SwVersion", "Not Update");
                    Param.insert("Latest_version", version->Monitor_version);  //
                    jsonDoc.setObject(Param);
                    QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                    emit sendMessage(raw_data, wClient);
                    sendUpdateToMonitor();
                } else {
                    Param.insert("objectName", "SwVersion");                   //
                    Param.insert("SwVersion", "already the newest version");   //
                    Param.insert("Latest_version", version->Monitor_version);  //
                    jsonDoc.setObject(Param);
                    QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                    // qDebug() << "raw_data:" << raw_data;
                    emit sendMessage(raw_data, wClient);
                }

                emit clearMonitorUpdate();
                qDebug() << wClient << server->m_Monitor;
                emit GetSettingDisplaySignal();
                emit getThresholdSignal();
                emit getdatapatternDataDbSignal();
                emit updateMarginSignal();
                emit getMyTaggingPhaseASignal();
                emit getMyTaggingPhaseBSignal();
                emit getMyTaggingPhaseCSignal();
                emit RangeLFLSignal();
                emit getLFLSignal();
                lenghtFLF = myDatabase->lenghtFLF;
                emit getuserlogin("", "", wClient);

                //            myDatabase -> getLFL();
                //            myDatabase -> getrecipientEmail();
                emit getSettingInfoSignal();
                emit preiodicSetting();
                // qWarning() << "emit getEventAlarmHistory();";
                emit getEventAlarmHistory();
                day->times = myDatabase->times;
                day->Monday = myDatabase->Monday;
                day->Tuesday = myDatabase->Tuesday;
                day->Wednesday = myDatabase->Wednesday;
                day->Thursday = myDatabase->Thursday;
                day->Friday = myDatabase->Friday;
                day->Saturday = myDatabase->Saturday;
                day->Sunday = myDatabase->Sunday;

                *lastStateday = *day;

                sendInitial(wClient);
                Aux->fails = myDatabase->LFL_Fail;
                Aux->operate = myDatabase->LFL_Operate;
                //            getLFL_Fail = myDatabase->LFL_Fail;
                //            getLFL_Operate = myDatabase->LFL_Operate;

                QJsonObject().swap(Param);
                Param.insert("objectName", "statusFails");  // SwVersion เอามาจาก database
                Param.insert("LFLFAIL", Aux->fails);
                jsonDoc.setObject(Param);
                QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                emit sendMessage(raw_data, wClient);

                QJsonObject().swap(Param);
                Param.insert("objectName", "statusOperates");  // SwVersion เอามาจาก database
                Param.insert("LFLOPERATE", Aux->operate);
                jsonDoc.setObject(Param);
                QString raw_data2 = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                emit sendMessage(raw_data2, wClient);

                QJsonObject().swap(Param);
                Param.insert("objectName", "TOWER_NO_INIT");  // SwVersion เอามาจาก database
                Param.insert("TransmissionLine", TransmissionLine);
                Param.insert("FullDistance", myDatabase->FullDistance);
                Param.insert("selectPatterName", myDatabase -> selectPatterName);
                jsonDoc.setObject(Param);
                QString raw_data3 = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                emit sendMessage(raw_data3, wClient);

                QJsonObject().swap(Param);
                Param.insert("objectName", "LineFails");  // SwVersion เอามาจาก database
                Param.insert("rangeoflfl", lenghtFLF);
                jsonDoc.setObject(Param);
                QString raw_data4 = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                emit sendMessage(raw_data4, wClient);

                // myDatabase->selectMasterMode();
                emit selectMasterModeSignal();
                // qWarning() << "initMaster DEBUG" << masterLFL << " " << wClient << " Monitor";
                initMaster();
                qDebug() << "send to P.win";
                // RecalculateWithMarginManual(wClient);
                // qWarning() << "send to P.win" << myDatabase -> selectPatterName << myDatabase -> datetimefile;
                getCsvFile(myDatabase -> selectPatterName, "Pattern", myDatabase -> datetimefile);

                QJsonObject().swap(Param);
                Param.insert("objectName", "TOWER_NO");
                Param.insert("TransmissionLine", towerA);
                Param.insert("FullDistance", distanceA / 1000);
                Param.insert("phase", "A");
                jsonDoc.setObject(Param);
                QString raw_phaseA = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                emit sendMessage(raw_phaseA, wClient);

                QJsonObject().swap(Param);
                Param.insert("objectName", "TOWER_NO");
                Param.insert("TransmissionLine", towerB);
                Param.insert("FullDistance", distanceB / 1000);
                Param.insert("phase", "B");
                jsonDoc.setObject(Param);
                QString raw_phaseB = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                emit sendMessage(raw_phaseB, wClient);

                QJsonObject().swap(Param);
                Param.insert("objectName", "TOWER_NO");
                Param.insert("TransmissionLine", towerC);
                Param.insert("FullDistance", distanceC / 1000);
                Param.insert("phase", "C");
                jsonDoc.setObject(Param);
                QString raw_phaseC = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                emit sendMessage(raw_phaseC, wClient);

                emit sendMessage(testNoneSmoothA, wClient);
                emit sendMessage(testNoneSmoothB, wClient);
                emit sendMessage(testNoneSmoothC, wClient);
                myDatabase->fetchTaggingData();
        } else if (obj["HwName"].toString() == "MonitorVNC") {
            removeRawDataFiles();
            qDebug() << "VNCmargin Monitors" << Monitor_address.size();
            //            if(!Monitor_address.isEmpty()){
            //                QJsonDocument jsonDoc;
            //                QJsonObject Param;
            //                Param.insert("objectName","Pop-up");
            //                Param.insert("msg","REMOTE FROM VNC");
            //                Param.insert("state",true);
            //                jsonDoc.setObject(Param);
            //                QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
            //                Q_FOREACH (QWebSocket *pClient, Monitor_address)
            //                {
            //                    if(pClient->state() == QAbstractSocket::ConnectedState)
            //                        emit sendMessage(raw_data, pClient);
            //                    else
            //                        qDebug() << "Monitor_address:" << pClient->state();
            //                }

            //                Param.insert("objectName","Pop-up");
            //                Param.insert("msg","REMOTE TO "+masterLFL);
            //                Param.insert("state",false);
            //                jsonDoc.setObject(Param);
            //                raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
            //                if(wClient->state() == QAbstractSocket::ConnectedState)
            //                    emit sendMessage(raw_data, wClient);
            //                else
            //                    qDebug() << "Monitor_address:" << wClient->state();
            //            }
            // Monitor_address.append(wClient);
            server->m_VNC.append(wClient);
            vnc_address = wClient;
            // if(!Monitor_address.isEmpty()){
                QJsonDocument jsonDoc;
                QJsonObject Param;
                Param.insert("objectName", "Pop-up");
                Param.insert("msg", "REMOTE FROM VNC");
                Param.insert("state", true);
                jsonDoc.setObject(Param);
                QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();

                // Q_FOREACH (QWebSocket *pClient, Monitor_address) {
                //     if (vnc_address != pClient) {
                //         if (pClient->state() == QAbstractSocket::ConnectedState){
                //             emit sendMessage(raw_data, pClient);
                //             // qDebug() << "REMOTE FROM VNC" << raw_data;
                //         }
                //         else
                //             qDebug() << "Monitor_address:" << pClient->state();
                //     }
                // }

                Param.insert("objectName", "Pop-up");
                Param.insert("msg", "Logging out");
                Param.insert("state", true);
                jsonDoc.setObject(Param);
                raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                if (wClient->state() == QAbstractSocket::ConnectedState){
                    if(vnc_address == wClient){
                        emit sendMessage(raw_data, wClient);
                        qDebug() << "vnc_address TO " << raw_data;
                    }
                }
                else
                    qDebug() << "vnc_address:" << wClient->state();
                // int retnew = pthread_create( & idThread5, NULL, ThreadFunc5, this);
                // if (retnew == 0) {
                //     qDebug() << ("ThreadFunc5 created successfully.\n");
                // } else {
                //     qDebug() << ("ThreadFunc5 not created.\n");
                // }
                stopThread4 = false;
                stopThread5 = false;
            // }

            //            if(obj["SwVersion"].toString() == SwVersion){            //SwVersion เอามาจาก database
            //                Param.insert("objectName","SwVersion");	             //
            //                Param.insert("SwVersion","already the newest version");	             //
            //                jsonDoc.setObject(Param);
            //                QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
            //                qDebug() << "raw_data:" << raw_data;
            ////                QThread::msleep(300);
            //                emit sendMessage(raw_data,wClient);
            //            }
            //            else{
            //                Param.insert("objectName","SwVersion");	             //SwVersion เอามาจาก database
            //                Param.insert("SwVersion","Not Update");
            //                jsonDoc.setObject(Param);
            //                QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
            //                qDebug() << "raw_data:" << raw_data;
            ////                QThread::msleep(300);
            //                emit sendMessage(raw_data,wClient);
            ////                sendUpdateToMonitor();
            //            }

            if (version->Monitor_version != obj["SwVersion"].toString()) {
                Param.insert("objectName", "SwVersion");  // SwVersion เอามาจาก database
                Param.insert("SwVersion", "Not Update");
                Param.insert("Latest_version", version->Monitor_version);  //
                jsonDoc.setObject(Param);
                raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                emit sendMessage(raw_data, wClient);
                //                sendUpdateToMonitor();
            } else {
                Param.insert("objectName", "SwVersion");                   //
                Param.insert("SwVersion", "already the newest version");   //
                Param.insert("Latest_version", version->Monitor_version);  //
                jsonDoc.setObject(Param);
                raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                // qDebug() << "raw_data:" << raw_data;
                emit sendMessage(raw_data, wClient);
            }

            qDebug() << wClient << server->m_Monitor;
            emit GetSettingDisplaySignal();
            emit getThresholdSignal();
            emit getdatapatternDataDbSignal();
            emit updateMarginSignal();
            emit getMyTaggingPhaseASignal();
            emit getMyTaggingPhaseBSignal();
            emit getMyTaggingPhaseCSignal();
            emit RangeLFLSignal();
            lenghtFLF = myDatabase->lenghtFLF;
            emit getuserlogin("", "", wClient);
            emit getSettingInfoSignal();
            emit preiodicSetting();
            emit getLFLSignal();
            // qWarning() << "emit getEventAlarmHistory();";
            emit getEventAlarmHistory();
            day->times = myDatabase->times;
            day->Monday = myDatabase->Monday;
            day->Tuesday = myDatabase->Tuesday;
            day->Wednesday = myDatabase->Wednesday;
            day->Thursday = myDatabase->Thursday;
            day->Friday = myDatabase->Friday;
            day->Saturday = myDatabase->Saturday;
            day->Sunday = myDatabase->Sunday;

            *lastStateday = *day;

            sendInitial(wClient);
            Aux->fails = myDatabase->LFL_Fail;
            Aux->operate = myDatabase->LFL_Operate;
            //            getLFL_Fail = myDatabase->LFL_Fail;
            //            getLFL_Operate = myDatabase->LFL_Operate;

            QJsonObject().swap(Param);
            Param.insert("objectName", "statusFails");  // SwVersion เอามาจาก database
            Param.insert("LFLFAIL", Aux->fails);
            jsonDoc.setObject(Param);
            raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
            emit sendMessage(raw_data, wClient);

            QJsonObject().swap(Param);
            Param.insert("objectName", "statusOperates");  // SwVersion เอามาจาก database
            Param.insert("LFLOPERATE", Aux->operate);
            jsonDoc.setObject(Param);
            QString raw_data2 = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
            emit sendMessage(raw_data2, wClient);

            QJsonObject().swap(Param);
            Param.insert("objectName", "TOWER_NO_INIT");  // SwVersion เอามาจาก database
            Param.insert("TransmissionLine", TransmissionLine);
            Param.insert("FullDistance", QString::number(FullDistance));
            jsonDoc.setObject(Param);
            QString raw_data3 = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
            emit sendMessage(raw_data3, wClient);

            QJsonObject().swap(Param);
            Param.insert("objectName", "LineFails");  // SwVersion เอามาจาก database
            Param.insert("rangeoflfl", lenghtFLF);
            jsonDoc.setObject(Param);
            QString raw_data4 = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
            emit sendMessage(raw_data4, wClient);
            // myDatabase->selectMasterMode();
            emit selectMasterModeSignal();
            // qWarning() << "initMaster DEBUG" << masterLFL << " " << wClient << " VNC";
            initMaster();
            getCsvFile(myDatabase -> selectPatterName, "Pattern", myDatabase -> datetimefile);
            // RecalculateWithMarginManual(wClient);

            QJsonObject().swap(Param);
            Param.insert("objectName", "TOWER_NO");
            Param.insert("TransmissionLine", towerA);
            Param.insert("FullDistance", distanceA / 1000);
            Param.insert("phase", "A");
            jsonDoc.setObject(Param);
            QString raw_phaseA = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
            emit sendMessage(raw_phaseA, wClient);

            QJsonObject().swap(Param);
            Param.insert("objectName", "TOWER_NO");
            Param.insert("TransmissionLine", towerB);
            Param.insert("FullDistance", distanceB / 1000);
            Param.insert("phase", "B");
            jsonDoc.setObject(Param);
            QString raw_phaseB = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
            emit sendMessage(raw_phaseB, wClient);

            QJsonObject().swap(Param);
            Param.insert("objectName", "TOWER_NO");
            Param.insert("TransmissionLine", towerC);
            Param.insert("FullDistance", distanceC / 1000);
            Param.insert("phase", "C");
            jsonDoc.setObject(Param);
            QString raw_phaseC = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
            emit sendMessage(raw_phaseC, wClient);

            emit sendMessage(testNoneSmoothA, wClient);
            emit sendMessage(testNoneSmoothB, wClient);
            emit sendMessage(testNoneSmoothC, wClient);
            myDatabase->fetchTaggingData();
        } else if (obj["HwName"].toString() == "FPGA") {
            qDebug() << "HwName FPGA";
            FPGA_address = wClient;
            //            if(obj["SwVersion"].toString() == SwVersion){            //SwVersion เอามาจาก database
            //                Param.insert("objectName","SwVersion");	             //
            //                Param.insert("SwVersion","already the newest version");	             //
            //                jsonDoc.setObject(Param);
            //                QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
            //                qDebug() << "raw_data:" << raw_data;
            ////                QThread::msleep(300);
            //                emit sendMessage(raw_data,wClient);
            //            }
            //            else{
            //                Param.insert("objectName","SwVersion");	             //SwVersion เอามาจาก database
            //                Param.insert("SwVersion","Not Update");
            //                jsonDoc.setObject(Param);
            //                QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
            //                qDebug() << "raw_data:" << raw_data;
            ////                QThread::msleep(300);
            //                emit sendMessage(raw_data,wClient);
            ////                sendUpdateToFPGA();
            //            }

            qDebug() << "FPGA_version:" << version->FPGA_version << " SwVersion" << obj["SwVersion"].toString();

            if (version->FPGA_version != obj["SwVersion"].toString()) {
                Param.insert("objectName", "SwVersion");  // SwVersion เอามาจาก database
                Param.insert("SwVersion", "Not Update");
                Param.insert("Latest_version", version->FPGA_version);
                jsonDoc.setObject(Param);
                QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                emit sendMessage(raw_data, wClient);
                // sendUpdateToFPGA();
            } else {
                Param.insert("objectName", "SwVersion");                  //
                Param.insert("SwVersion", "already the newest version");  //
                Param.insert("Latest_version", version->FPGA_version);    //
                jsonDoc.setObject(Param);
                QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                // qDebug() << "raw_data:" << raw_data;
                emit sendMessage(raw_data, wClient);
            }

            emit clearFpgaUpdate();
            sendInitial(wClient);
            myDatabase->GetSag();
            QJsonObject().swap(Param);
            Param.insert("objectName", "plc_input_1");  // SwVersion เอามาจาก database
            Param.insert("value", delays->delay0);
            jsonDoc.setObject(Param);
            QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
            emit sendMessage(raw_data, FPGA_address);

            //            Param = QJsonObject();
            Param.insert("objectName", "plc_input_2");  // SwVersion เอามาจาก database
            Param.insert("value", delays->delay1);
            jsonDoc.setObject(Param);
            raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
            emit sendMessage(raw_data, FPGA_address);

            //            Param = QJsonObject();
            Param.insert("objectName", "plc_input_3");  // SwVersion เอามาจาก database
            Param.insert("value", delays->delay2);
            jsonDoc.setObject(Param);
            raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
            emit sendMessage(raw_data, FPGA_address);

            //            Param = QJsonObject();
            Param.insert("objectName", "plc_input_4");  // SwVersion เอามาจาก database
            Param.insert("value", delays->delay3);
            jsonDoc.setObject(Param);
            raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
            emit sendMessage(raw_data, FPGA_address);

            //            Param = QJsonObject();
            Param.insert("objectName", "plc_input_5");  // SwVersion เอามาจาก database
            Param.insert("value", delays->delay4);
            jsonDoc.setObject(Param);
            raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
            emit sendMessage(raw_data, FPGA_address);

            //            Param = QJsonObject();
            Param.insert("objectName", "plc_input_6");  // SwVersion เอามาจาก database
            Param.insert("value", delays->delay5);
            jsonDoc.setObject(Param);
            raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
            emit sendMessage(raw_data, FPGA_address);

            //            Param = QJsonObject();
            Param.insert("objectName", "plc_input_7");  // SwVersion เอามาจาก database
            Param.insert("value", delays->delay6);
            jsonDoc.setObject(Param);
            raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
            emit sendMessage(raw_data, FPGA_address);

            //            Param = QJsonObject();
            Param.insert("objectName", "plc_input_8");  // SwVersion เอามาจาก database
            Param.insert("value", delays->delay7);
            jsonDoc.setObject(Param);
            raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
            emit sendMessage(raw_data, FPGA_address);

        } else if (obj["HwName"].toString() == "snmp") {
            snmp_address = wClient;
            if (obj["SwVersion"].toString() == SwVersion) {               // SwVersion เอามาจาก database
                Param.insert("objectName", "SwVersion");                  //
                Param.insert("SwVersion", "already the newest version");  //
                jsonDoc.setObject(Param);
                QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                // qDebug() << "raw_data:" << raw_data;
                //                QThread::msleep(300);
                emit sendMessage(raw_data, wClient);
            } else {
                Param.insert("objectName", "SwVersion");  // SwVersion เอามาจาก database
                Param.insert("SwVersion", "Not Update");
                jsonDoc.setObject(Param);
                QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                // qDebug() << "raw_data:" << raw_data;
                //                QThread::msleep(300);
                emit sendMessage(raw_data, wClient);
            }
            sendInitial(wClient);
            myDatabase->getrecipientEmail();
            sendEmailParamToSNMP();
        }
    }
    // SNMP
    else if (obj["objectName"].toString() == "UpdateSnmpIP") {
        networks->ip_snmp = obj["ip"].toString();
        //        qDebug()<< "UpdateSmtpIP" << snmp_param->ipSnmpServer;
        updateSnmpipServer(networks->ip_snmp);
        updateSNMP();
        QJsonDocument jsonDoc;
        QJsonObject Param;
        QString raw_data;
        Param.insert("objectName", "Network");
        Param.insert("ip_address", networks->ip_address);
        Param.insert("ip_gateway", networks->ip_gateway);
        Param.insert("ip_snmp", networks->ip_snmp);
        Param.insert("ip_timeserver", networks->ip_timeserver);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(raw_data, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(raw_data);
    }
    // TrapsEnabling
    else if (obj["objectName"].toString() == "TrapsEnabling") {
        qDebug() << "enabling from monitor" << obj;
        Monitor_param->PLC_DO_ERROR = obj["PLC_DO_ERROR"].toBool();
        Monitor_param->PLC_DI_ERROR = obj["PLC_DI_ERROR"].toBool();
        Monitor_param->MODULE_HI_SPEED_PHASE_A_ERROR = obj["MODULE_HI_SPEED_PHASE_A_ERROR"].toBool();
        Monitor_param->MODULE_HI_SPEED_PHASE_B_ERROR = obj["MODULE_HI_SPEED_PHASE_B_ERROR"].toBool();
        Monitor_param->MODULE_HI_SPEED_PHASE_C_ERROR = obj["MODULE_HI_SPEED_PHASE_C_ERROR"].toBool();
        Monitor_param->INTERNAL_PHASE_A_ERROR = obj["INTERNAL_PHASE_A_ERROR"].toBool();
        Monitor_param->INTERNAL_PHASE_B_ERROR = obj["INTERNAL_PHASE_B_ERROR"].toBool();
        Monitor_param->INTERNAL_PHASE_C_ERROR = obj["INTERNAL_PHASE_C_ERROR"].toBool();
        Monitor_param->GPS_MODULE_FAIL = obj["GPS_MODULE_FAIL"].toBool();
        Monitor_param->SYSTEM_INITIAL = obj["SYSTEM_INITIAL"].toBool();
        Monitor_param->COMMUNICATION_ERROR = obj["COMMUNICATION_ERROR"].toBool();
        Monitor_param->RELAY_START_EVENT = obj["RELAY_START_EVENT"].toBool();
        Monitor_param->SURGE_START_EVENT = obj["SURGE_START_EVENT"].toBool();
        Monitor_param->PERIODIC_TEST_EVENT = obj["PERIODIC_TEST_EVENT"].toBool();
        Monitor_param->MANUAL_TEST_EVENT = obj["MANUAL_TEST_EVENT"].toBool();
        Monitor_param->LFL_FAIL = obj["LFL_FAIL"].toBool();
        Monitor_param->LFL_OPERATE = obj["LFL_OPERATE"].toBool();
        if (snmp_param != Monitor_param) {
            if (snmp_address->state() == QAbstractSocket::ConnectedState) {
                //                qDebug() << "send to snmp:";
                emit sendMessage(msgs, snmp_address);
            } else {
                //                qDebug() << "snmp_address:" << snmp_address->state();
                qDebug() << "not send to snmp:";
            }

            //            qDebug() << "snmp_address:" << snmp_address->state() << " OpenPLC_address:" << (OpenPLC_address->state() == QAbstractSocket::ConnectedState);
            //            emit sendMessage(msgs,OpenPLC_address);
            *snmp_param = *Monitor_param;
            updateSnmpSelectionState();
        }
    }
    // TrapsAlert
    else if (obj["TrapsAlert"].toString() == "PLC_DO_ERROR") {
        OpenPLC_param->PLC_DO_ERROR = obj["state"].toBool();
        //        if(OpenPLC_param->PLC_DO_ERROR == PLCserver_param->PLC_DO_ERROR){
        if (snmp_address->state() == QAbstractSocket::ConnectedState) {
            emit sendMessage(msgs, snmp_address);
        } else
            qDebug() << "snmp_address:" << snmp_address->state();
        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(msgs, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(msgs);
        //        }
        PLCserver_param->PLC_DO_ERROR = OpenPLC_param->PLC_DO_ERROR;
        sendSocketThreeDevice();
    } else if (obj["TrapsAlert"].toString() == "PLC_DI_ERROR") {
        OpenPLC_param->PLC_DI_ERROR = obj["state"].toBool();
        //        if(OpenPLC_param->PLC_DI_ERROR == PLCserver_param->PLC_DI_ERROR){
        if (snmp_address->state() == QAbstractSocket::ConnectedState) {
            emit sendMessage(msgs, snmp_address);
        } else
            qDebug() << "snmp_address:" << snmp_address->state();
        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(msgs, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(msgs);
        //        }
        PLCserver_param->PLC_DI_ERROR = OpenPLC_param->PLC_DI_ERROR;
        sendSocketThreeDevice();
    } else if (obj["TrapsAlert"].toString() == "MODULE_HI_SPEED_PHASE_A_ERROR") {
        OpenPLC_param->MODULE_HI_SPEED_PHASE_A_ERROR = obj["state"].toBool();
        //        if(OpenPLC_param->MODULE_HI_SPEED_PHASE_A_ERROR == PLCserver_param->MODULE_HI_SPEED_PHASE_A_ERROR){
        if (snmp_address->state() == QAbstractSocket::ConnectedState) {
            emit sendMessage(msgs, snmp_address);
        } else
            qDebug() << "snmp_address:" << snmp_address->state();
        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(msgs, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(msgs);
        //        }
        PLCserver_param->MODULE_HI_SPEED_PHASE_A_ERROR = OpenPLC_param->MODULE_HI_SPEED_PHASE_A_ERROR;
        sendSocketThreeDevice();
    } else if (obj["TrapsAlert"].toString() == "MODULE_HI_SPEED_PHASE_B_ERROR") {
        OpenPLC_param->MODULE_HI_SPEED_PHASE_B_ERROR = obj["state"].toBool();
        //        if(OpenPLC_param->MODULE_HI_SPEED_PHASE_B_ERROR == PLCserver_param->MODULE_HI_SPEED_PHASE_B_ERROR){
        if (snmp_address->state() == QAbstractSocket::ConnectedState) {
            emit sendMessage(msgs, snmp_address);
        } else
            qDebug() << "snmp_address:" << snmp_address->state();
        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(msgs, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(msgs);
        //        }
        PLCserver_param->MODULE_HI_SPEED_PHASE_B_ERROR = OpenPLC_param->MODULE_HI_SPEED_PHASE_B_ERROR;
        sendSocketThreeDevice();
    } else if (obj["TrapsAlert"].toString() == "MODULE_HI_SPEED_PHASE_C_ERROR") {
        OpenPLC_param->MODULE_HI_SPEED_PHASE_C_ERROR = obj["state"].toBool();
        //        if(OpenPLC_param->MODULE_HI_SPEED_PHASE_C_ERROR == PLCserver_param->MODULE_HI_SPEED_PHASE_C_ERROR){
        if (snmp_address->state() == QAbstractSocket::ConnectedState) {
            emit sendMessage(msgs, snmp_address);
        } else
            qDebug() << "snmp_address:" << snmp_address->state();
        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(msgs, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(msgs);
        //        }
        PLCserver_param->MODULE_HI_SPEED_PHASE_C_ERROR = OpenPLC_param->MODULE_HI_SPEED_PHASE_C_ERROR;
        sendSocketThreeDevice();
    } else if (obj["TrapsAlert"].toString() == "INTERNAL_PHASE_A_ERROR") {
        OpenPLC_param->INTERNAL_PHASE_A_ERROR = obj["state"].toBool();
        //        if(OpenPLC_param->INTERNAL_PHASE_A_ERROR == PLCserver_param->INTERNAL_PHASE_A_ERROR){
        if (snmp_address->state() == QAbstractSocket::ConnectedState) {
            emit sendMessage(msgs, snmp_address);
        } else
            qDebug() << "snmp_address:" << snmp_address->state();
        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(msgs, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(msgs);
        //        }
        PLCserver_param->INTERNAL_PHASE_A_ERROR = OpenPLC_param->INTERNAL_PHASE_A_ERROR;
        sendSocketThreeDevice();
    } else if (obj["TrapsAlert"].toString() == "INTERNAL_PHASE_B_ERROR") {
        OpenPLC_param->INTERNAL_PHASE_B_ERROR = obj["state"].toBool();
        //        if(OpenPLC_param->INTERNAL_PHASE_B_ERROR == PLCserver_param->INTERNAL_PHASE_B_ERROR){
        if (snmp_address->state() == QAbstractSocket::ConnectedState) {
            emit sendMessage(msgs, snmp_address);
        } else
            qDebug() << "snmp_address:" << snmp_address->state();
        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(msgs, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(msgs);
        //        }
        PLCserver_param->INTERNAL_PHASE_B_ERROR = OpenPLC_param->INTERNAL_PHASE_B_ERROR;
        sendSocketThreeDevice();
    } else if (obj["TrapsAlert"].toString() == "INTERNAL_PHASE_C_ERROR") {
        OpenPLC_param->INTERNAL_PHASE_C_ERROR = obj["state"].toBool();
        //        if(OpenPLC_param->INTERNAL_PHASE_C_ERROR == PLCserver_param->INTERNAL_PHASE_C_ERROR){
        if (snmp_address->state() == QAbstractSocket::ConnectedState) {
            emit sendMessage(msgs, snmp_address);
        } else
            qDebug() << "snmp_address:" << snmp_address->state();
        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(msgs, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(msgs);
        //        }
        PLCserver_param->INTERNAL_PHASE_C_ERROR = OpenPLC_param->INTERNAL_PHASE_C_ERROR;
        sendSocketThreeDevice();
    } else if (obj["TrapsAlert"].toString() == "GPS_MODULE_FAIL") {
        OpenPLC_param->GPS_MODULE_FAIL = obj["state"].toBool();
        //        if(OpenPLC_param->GPS_MODULE_FAIL == PLCserver_param->GPS_MODULE_FAIL){
        if (snmp_address->state() == QAbstractSocket::ConnectedState) {
            emit sendMessage(msgs, snmp_address);
        } else
            qDebug() << "snmp_address:" << snmp_address->state();
        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(msgs, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(msgs);
        //        }
        PLCserver_param->GPS_MODULE_FAIL = OpenPLC_param->GPS_MODULE_FAIL;
        sendSocketThreeDevice();
    } else if (obj["TrapsAlert"].toString() == "SYSTEM_INITIAL") {
        OpenPLC_param->SYSTEM_INITIAL = obj["state"].toBool();
        //        if(OpenPLC_param->SYSTEM_INITIAL == PLCserver_param->SYSTEM_INITIAL){
        if (snmp_address->state() == QAbstractSocket::ConnectedState) {
            emit sendMessage(msgs, snmp_address);
        } else
            qDebug() << "snmp_address:" << snmp_address->state();
        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(msgs, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(msgs);
        //        }
        PLCserver_param->SYSTEM_INITIAL = OpenPLC_param->SYSTEM_INITIAL;
        sendSocketThreeDevice();
    } else if (obj["TrapsAlert"].toString() == "COMMUNICATION_ERROR") {
        OpenPLC_param->COMMUNICATION_ERROR = obj["state"].toBool();
        //        if(OpenPLC_param->COMMUNICATION_ERROR == PLCserver_param->COMMUNICATION_ERROR){
        if (snmp_address->state() == QAbstractSocket::ConnectedState) {
            emit sendMessage(msgs, snmp_address);
        } else
            qDebug() << "snmp_address:" << snmp_address->state();
        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(msgs, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(msgs);
        //        }
        PLCserver_param->COMMUNICATION_ERROR = OpenPLC_param->COMMUNICATION_ERROR;
        sendSocketThreeDevice();
    } else if (obj["TrapsAlert"].toString() == "RELAY_START_EVENT") {
        OpenPLC_param->RELAY_START_EVENT = obj["state"].toBool();
        //        if(OpenPLC_param->RELAY_START_EVENT == PLCserver_param->RELAY_START_EVENT){
        if (snmp_address->state() == QAbstractSocket::ConnectedState) {
            emit sendMessage(msgs, snmp_address);
        } else
            qDebug() << "snmp_address:" << snmp_address->state();
        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(msgs, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(msgs);
        //        }
        PLCserver_param->RELAY_START_EVENT = OpenPLC_param->RELAY_START_EVENT;
    } else if (obj["TrapsAlert"].toString() == "SURGE_START_EVENT") {
        OpenPLC_param->SURGE_START_EVENT = obj["state"].toBool();
        //        if(OpenPLC_param->SURGE_START_EVENT == PLCserver_param->SURGE_START_EVENT){
        if (snmp_address->state() == QAbstractSocket::ConnectedState) {
            emit sendMessage(msgs, snmp_address);
        } else
            qDebug() << "snmp_address:" << snmp_address->state();
        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(msgs, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(msgs);
        //        }
        PLCserver_param->SURGE_START_EVENT = OpenPLC_param->SURGE_START_EVENT;
    } else if (obj["TrapsAlert"].toString() == "PERIODIC_TEST_EVENT") {
        OpenPLC_param->PERIODIC_TEST_EVENT = obj["state"].toBool();
        //        if(OpenPLC_param->PERIODIC_TEST_EVENT == PLCserver_param->PERIODIC_TEST_EVENT){
        if (snmp_address->state() == QAbstractSocket::ConnectedState) {
            emit sendMessage(msgs, snmp_address);
        } else
            qDebug() << "snmp_address:" << snmp_address->state();
        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(msgs, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(msgs);
        //        }
        PLCserver_param->PERIODIC_TEST_EVENT = OpenPLC_param->PERIODIC_TEST_EVENT;
    } else if (obj["TrapsAlert"].toString() == "MANUAL_TEST_EVENT") {
        // qWarning() << "DEBUG TrapsAlert " << msgs;
        OpenPLC_param->MANUAL_TEST_EVENT = obj["state"].toBool();
        //        if(OpenPLC_param->MANUAL_TEST_EVENT == PLCserver_param->MANUAL_TEST_EVENT){
        if (snmp_address->state() == QAbstractSocket::ConnectedState) {
            emit sendMessage(msgs, snmp_address);
        } else
            qDebug() << "snmp_address:" << snmp_address->state();
        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(msgs, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(msgs);
        //        }
        PLCserver_param->MANUAL_TEST_EVENT = OpenPLC_param->MANUAL_TEST_EVENT;
    } else if (obj["TrapsAlert"].toString() == "LFL_FAIL") {
        OpenPLC_param->LFL_FAIL = obj["state"].toBool();
        //        if(OpenPLC_param->LFL_FAIL == PLCserver_param->LFL_FAIL){
        if (snmp_address->state() == QAbstractSocket::ConnectedState) {
            emit sendMessage(msgs, snmp_address);
        } else
            qDebug() << "snmp_address:" << snmp_address->state();
        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(msgs, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(msgs);
        //        }
        PLCserver_param->LFL_FAIL = OpenPLC_param->LFL_FAIL;
    } else if (obj["TrapsAlert"].toString() == "LFL_OPERATE") {
        OpenPLC_param->LFL_OPERATE = obj["state"].toBool();
        //        if(OpenPLC_param->LFL_OPERATE == PLCserver_param->LFL_OPERATE){
        if (snmp_address->state() == QAbstractSocket::ConnectedState) {
            emit sendMessage(msgs, snmp_address);
        } else
            qDebug() << "snmp_address:" << snmp_address->state();
        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(msgs, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(msgs);
        //        }
        PLCserver_param->LFL_OPERATE = OpenPLC_param->LFL_OPERATE;
    } else if (obj["objectName"].toString() == "RequestTrapsEnabling") {
        //        qDebug() << "RequestTrapsEnabling";
        QJsonDocument jsonDoc;
        QJsonObject Param;
        Param.insert("objectName", "TrapsEnabling");
        Param.insert("PLC_DO_ERROR", snmp_param->PLC_DO_ERROR);
        Param.insert("PLC_DI_ERROR", snmp_param->PLC_DI_ERROR);
        Param.insert("MODULE_HI_SPEED_PHASE_A_ERROR", snmp_param->MODULE_HI_SPEED_PHASE_A_ERROR);
        Param.insert("MODULE_HI_SPEED_PHASE_B_ERROR", snmp_param->MODULE_HI_SPEED_PHASE_B_ERROR);
        Param.insert("MODULE_HI_SPEED_PHASE_C_ERROR", snmp_param->MODULE_HI_SPEED_PHASE_C_ERROR);
        Param.insert("INTERNAL_PHASE_A_ERROR", snmp_param->INTERNAL_PHASE_A_ERROR);
        Param.insert("INTERNAL_PHASE_B_ERROR", snmp_param->INTERNAL_PHASE_B_ERROR);
        Param.insert("INTERNAL_PHASE_C_ERROR", snmp_param->INTERNAL_PHASE_C_ERROR);
        Param.insert("GPS_MODULE_FAIL", snmp_param->GPS_MODULE_FAIL);
        Param.insert("SYSTEM_INITIAL", snmp_param->SYSTEM_INITIAL);
        Param.insert("COMMUNICATION_ERROR", snmp_param->COMMUNICATION_ERROR);
        Param.insert("RELAY_START_EVENT", snmp_param->RELAY_START_EVENT);
        Param.insert("SURGE_START_EVENT", snmp_param->SURGE_START_EVENT);
        Param.insert("PERIODIC_TEST_EVENT", snmp_param->PERIODIC_TEST_EVENT);
        Param.insert("MANUAL_TEST_EVENT", snmp_param->MANUAL_TEST_EVENT);
        Param.insert("LFL_FAIL", snmp_param->LFL_FAIL);
        Param.insert("LFL_OPERATE", snmp_param->LFL_OPERATE);
        jsonDoc.setObject(Param);
        QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        emit sendMessage(raw_data, wClient);
    } else if (obj["objectName"].toString() == "RequestTrapsAlert") {
        QString datetime = getChrrentDateTime();

        QJsonDocument jsonDoc;
        QJsonObject Param;
        QString raw_data;

        Param.insert("TrapsAlert", "PLC_DO_ERROR");  // Name
        Param.insert("state", PLCserver_param->PLC_DO_ERROR);
        Param.insert("time", datetime);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        qDebug() << "PLC_DO_ERROR raw_data:" << raw_data;
        emit sendMessage(raw_data, wClient);
        QJsonObject().swap(Param);

        Param.insert("TrapsAlert", "PLC_DI_ERROR");  // Name
        Param.insert("state", PLCserver_param->PLC_DI_ERROR);
        Param.insert("time", datetime);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        qDebug() << "PLC_DI_ERROR raw_data:" << raw_data;
        emit sendMessage(raw_data, wClient);
        QJsonObject().swap(Param);

        Param.insert("TrapsAlert", "MODULE_HI_SPEED_PHASE_A_ERROR");  // Name
        Param.insert("state", PLCserver_param->MODULE_HI_SPEED_PHASE_A_ERROR);
        Param.insert("time", datetime);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        qDebug() << "MODULE_HI_SPEED_PHASE_A_ERROR raw_data:" << raw_data;
        emit sendMessage(raw_data, wClient);
        QJsonObject().swap(Param);

        Param.insert("TrapsAlert", "MODULE_HI_SPEED_PHASE_B_ERROR");  // Name
        Param.insert("state", PLCserver_param->MODULE_HI_SPEED_PHASE_B_ERROR);
        Param.insert("time", datetime);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        qDebug() << "MODULE_HI_SPEED_PHASE_B_ERROR raw_data:" << raw_data;
        emit sendMessage(raw_data, wClient);
        QJsonObject().swap(Param);

        Param.insert("TrapsAlert", "MODULE_HI_SPEED_PHASE_C_ERROR");  // Name
        Param.insert("state", PLCserver_param->MODULE_HI_SPEED_PHASE_C_ERROR);
        Param.insert("time", datetime);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        qDebug() << "MODULE_HI_SPEED_PHASE_C_ERROR raw_data:" << raw_data;
        emit sendMessage(raw_data, wClient);
        QJsonObject().swap(Param);

        Param.insert("TrapsAlert", "INTERNAL_PHASE_A_ERROR");  // Name
        Param.insert("state", PLCserver_param->INTERNAL_PHASE_A_ERROR);
        Param.insert("time", datetime);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        qDebug() << "INTERNAL_PHASE_A_ERROR raw_data:" << raw_data;
        emit sendMessage(raw_data, wClient);
        QJsonObject().swap(Param);

        Param.insert("TrapsAlert", "INTERNAL_PHASE_B_ERROR");  // Name
        Param.insert("state", PLCserver_param->INTERNAL_PHASE_B_ERROR);
        Param.insert("time", datetime);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        qDebug() << "INTERNAL_PHASE_B_ERROR raw_data:" << raw_data;
        emit sendMessage(raw_data, wClient);
        QJsonObject().swap(Param);

        Param.insert("TrapsAlert", "INTERNAL_PHASE_C_ERROR");  // Name
        Param.insert("state", PLCserver_param->INTERNAL_PHASE_C_ERROR);
        Param.insert("time", datetime);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        qDebug() << "INTERNAL_PHASE_C_ERROR raw_data:" << raw_data;
        emit sendMessage(raw_data, wClient);
        QJsonObject().swap(Param);

        Param.insert("TrapsAlert", "GPS_MODULE_FAIL");  // Name
        Param.insert("state", PLCserver_param->GPS_MODULE_FAIL);
        Param.insert("time", datetime);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        qDebug() << "GPS_MODULE_FAIL raw_data:" << raw_data;
        emit sendMessage(raw_data, wClient);
        QJsonObject().swap(Param);

        Param.insert("TrapsAlert", "SYSTEM_INITIAL");  // Name
        Param.insert("state", PLCserver_param->SYSTEM_INITIAL);
        Param.insert("time", datetime);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        qDebug() << "SYSTEM_INITIAL raw_data:" << raw_data;
        emit sendMessage(raw_data, wClient);
        QJsonObject().swap(Param);

        Param.insert("TrapsAlert", "COMMUNICATION_ERROR");  // Name
        Param.insert("state", PLCserver_param->COMMUNICATION_ERROR);
        Param.insert("time", datetime);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        qDebug() << "COMMUNICATION_ERROR raw_data:" << raw_data;
        emit sendMessage(raw_data, wClient);
        QJsonObject().swap(Param);

        Param.insert("TrapsAlert", "RELAY_START_EVENT");  // Name
        Param.insert("state", PLCserver_param->RELAY_START_EVENT);
        Param.insert("time", datetime);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        qDebug() << "RELAY_START_EVENT raw_data:" << raw_data;
        emit sendMessage(raw_data, wClient);
        QJsonObject().swap(Param);

        Param.insert("TrapsAlert", "SURGE_START_EVENT");  // Name
        Param.insert("state", PLCserver_param->SURGE_START_EVENT);
        Param.insert("time", datetime);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        qDebug() << "SURGE_START_EVENT raw_data:" << raw_data;
        emit sendMessage(raw_data, wClient);
        QJsonObject().swap(Param);

        Param.insert("TrapsAlert", "PERIODIC_TEST_EVENT");  // Name
        Param.insert("state", PLCserver_param->PERIODIC_TEST_EVENT);
        Param.insert("time", datetime);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        qDebug() << "PERIODIC_TEST_EVENT raw_data:" << raw_data;
        emit sendMessage(raw_data, wClient);
        QJsonObject().swap(Param);

        Param.insert("TrapsAlert", "MANUAL_TEST_EVENT");  // Name
        Param.insert("state", PLCserver_param->MANUAL_TEST_EVENT);
        Param.insert("time", datetime);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        qDebug() << "MANUAL_TEST_EVENT raw_data:" << raw_data;
        emit sendMessage(raw_data, wClient);
        // qWarning() << "TrapsAlert" << raw_data;
        QJsonObject().swap(Param);

        Param.insert("TrapsAlert", "LFL_FAIL");  // Name
        Param.insert("state", PLCserver_param->LFL_FAIL);
        Param.insert("time", datetime);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        qDebug() << "LFL_FAIL raw_data:" << raw_data;
        emit sendMessage(raw_data, wClient);
        QJsonObject().swap(Param);

        Param.insert("TrapsAlert", "LFL_OPERATE");  // Name
        Param.insert("state", PLCserver_param->LFL_OPERATE);
        Param.insert("time", datetime);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        qDebug() << "LFL_OPERATE raw_data:" << raw_data;
        emit sendMessage(raw_data, wClient);
        QJsonObject().swap(Param);
    }

    // MONITOR
    else if (getCommand.contains("UserSelectM")) {
        QString userType = QJsonValue(command["userType"]).toString();
        bool userStatus = QJsonValue(command["userStatusMaster"]).toBool();
        QString selectMaster = QString(
                                   "{"
                                   "\"objectName\"  :\"UserSelectM\","
                                   "\"userType\"    :\"%1\","
                                   "\"userStatusMaster\"  :\"%2\""
                                   "}")
                                   .arg(userType)
                                   .arg(userStatus);
        //        qDebug() << "cppSubmitTextFiled UserM:" << selectMaster << userStatus << userType;
        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(selectMaster, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(selectMaster);

        emit updateUser(selectMaster);
    } else if (getCommand.contains("UserSelectS")) {
        QString userType = QJsonValue(command["userType"]).toString();
        bool userStatus = QJsonValue(command["userStatusSlave"]).toBool();
        QString selectSlave = QString(
                                  "{"
                                  "\"objectName\"  :\"UserSelectS\","
                                  "\"userType\"    :\"%1\","
                                  "\"userStatusSlave\"  :\"%2\""
                                  "}")
                                  .arg(userType)
                                  .arg(userStatus);
        //        qDebug() << "cppSubmitTextFiled UserS:" << selectSlave << userStatus << userType;
        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(selectSlave, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(selectSlave);
        emit updateUser(selectSlave);
    } else if (getCommand == ("statusOperate")) {
        //            bool activeStatus = QJsonValue(command["LFLOPERATE"]).toBool();
        //            QString auxiliary = QString("{"
        //                                          "\"objectName\"  :\"statusOperate\","
        //                                          "\"LFLOPERATE\"  :\"%1\""
        //                                          "}").arg(activeStatus);
        //            qDebug() << "auxiliary:" << auxiliary;
        qDebug() << "statusOperate:" << obj["LFLOPERATE"].toBool();
        //            uint8_t reg[1];
        //            reg[0] = obj["LFLOPERATE"].toBool();
        Aux->operate = obj["LFLOPERATE"].toBool();
        //            QJsonDocument jsonDoc;
        //            QJsonObject Param;
        //            QJsonArray Reg;
        //            QString raw_datas;
        //            Param.insert("objectName","writeCoils");
        //            for(int i=0;i<sizeof(reg);i++){
        //                Reg.append(reg[i]);
        //            }
        //            Param.insert("register",Reg);
        //            Param.insert("index",801);
        //            jsonDoc.setObject(Param);
        //            raw_datas = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        //            // OpenPLC_address
        //            if(OpenPLC_address->state() == QAbstractSocket::ConnectedState)
        //                emit sendMessage(raw_datas, OpenPLC_address);
        //            else
        //                qDebug() << "OpenPLC_address:" << OpenPLC_address->state();

        emit updateRelay(msgs);
    } else if (getCommand == ("statusFail")) {
        //            bool activeStatus = QJsonValue(command["LFLFAIL"]).toBool();
        //            QString auxiliary = QString("{"
        //                                          "\"objectName\"  :\"statusFail\","
        //                                          "\"LFLFAIL\"  :\"%1\""
        //                                          "}").arg(activeStatus);
        qDebug() << "statusFail:" << obj["LFLFAIL"].toBool();
        //            uint8_t reg[1] = {0}; //มีแค่ 12 output ต้องเก็บค่าให้ครบ
        //            reg[0] = obj["LFLFAIL"].toBool();
        Aux->fails = obj["LFLFAIL"].toBool();
        //            QJsonDocument jsonDoc;
        //            QJsonObject Param;
        //            QJsonArray Reg;
        //            Param.insert("objectName","writeCoils");
        //            for(int i=0;i<sizeof(reg);i++){
        //                Reg.append(reg[i]);
        //            }
        //            Param.insert("register",Reg);
        //            Param.insert("index",800);
        //            jsonDoc.setObject(Param);
        //            QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        //            if(OpenPLC_address->state() == QAbstractSocket::ConnectedState)
        //                emit sendMessage(raw_data, OpenPLC_address);
        //            else
        //                qDebug() << "snmp_address:" << OpenPLC_address->state();
        emit updateRelay(msgs);
    }

    //    else if(getCommand.contains("valueVoltage")){
    //            int valueVoltage = QJsonValue(command["Voltage"]).toInt();
    //            QString Voltage = QString("{"
    //                                          "\"objectName\"   :\"valueVoltage\","
    //                                          "\"Voltage\"      :\"%1\""
    //                                          "}").arg(valueVoltage);
    //            qDebug() << "cppSubmitTextFiled Voltage:" << Voltage << valueVoltage;
    //            Q_FOREACH (QWebSocket *pClient, Monitor_address)
    //            {
    //                if(pClient->state() == QAbstractSocket::ConnectedState)
    //                    emit sendMessage(Voltage, pClient);
    //                else
    //                    qDebug() << "Monitor_address:" << pClient->state();
    //            }
    //        }else if(getCommand.contains("valueSubstation")){
    //            QString valueSubstation = QJsonValue(command["Substation"]).toString();
    //            QString Substation = QString("{"
    //                                          "\"objectName\"   :\"valueSubstation\","
    //                                          "\"Substation\"      :\"%1\""
    //                                          "}").arg(valueSubstation);
    //            qDebug() << "cppSubmitTextFiled Substation:" << Substation << valueSubstation;
    //            Q_FOREACH (QWebSocket *pClient, Monitor_address)
    //            {
    //                if(pClient->state() == QAbstractSocket::ConnectedState)
    //                    emit sendMessage(Substation, pClient);
    //                else
    //                    qDebug() << "Monitor_address:" << pClient->state();
    //            }
    //        }else if(getCommand.contains("valueDirection")){
    //            QString valueDirection = QJsonValue(command["Direction"]).toString();
    //            QString Direction = QString("{"
    //                                          "\"objectName\"   :\"valueDirection\","
    //                                          "\"Direction\"      :\"%1\""
    //                                          "}").arg(valueDirection);
    //            qDebug() << "cppSubmitTextFiled Direction:" << Direction << valueDirection;
    //            Q_FOREACH (QWebSocket *pClient, Monitor_address)
    //            {
    //                if(pClient->state() == QAbstractSocket::ConnectedState)
    //                    emit sendMessage(Direction, pClient);
    //                else
    //                    qDebug() << "Monitor_address:" << pClient->state();
    //            }
    //        }else if(getCommand.contains("valueLineNo")){
    //            int valueLineNo = QJsonValue(command["LineNo"]).toInt();
    //            QString LineNo = QString("{"
    //                                          "\"objectName\"   :\"valueLineNo\","
    //                                          "\"LineNo\"      :\"%1\""
    //                                          "}").arg(valueLineNo);
    //            qDebug() << "cppSubmitTextFiled Direction:" << LineNo << valueLineNo;
    //            Q_FOREACH (QWebSocket *pClient, Monitor_address)
    //            {
    //                if(pClient->state() == QAbstractSocket::ConnectedState)
    //                    emit sendMessage(LineNo, pClient);
    //                else
    //                    qDebug() << "Monitor_address:" << pClient->state();
    //            }
    //        }
    else if (getCommand == "updateTime") {
        //            double timer = QJsonValue(command["Time"]).toDouble();
        //            QString timerupdate = QString("{"
        //                                          "\"objectName\"   :\"updateTime\","
        //                                          "\"Time\"      :%1"
        //                                          "}").arg(timer);
        //            qDebug() << "cppSubmitTextFiled Time:" << timerupdate << timer;
        emit updateTimer(msgs);
    } else if (getCommand.contains("distanceField")) {
        int distanceField = QJsonValue(command["distanceField"]).toInt();
        QString distance = QString(
                               "{"
                               "\"objectName\"   :\"distanceField\","
                               "\"distanceField\"      :\"%1\""
                               "}")
                               .arg(distanceField);
        qDebug() << "cppSubmitTextFiled distance:" << distance << distanceField;
        //        cppCommand(distance);
    } else if (getCommand.contains("detailField")) {
        double detailField = QJsonValue(command["detailField"]).toDouble();
        QString Details = QString(
                              "{"
                              "\"objectName\"   :\"detailField\","
                              "\"detailField\"      :\"%1\""
                              "}")
                              .arg(Details);
        qDebug() << "cppSubmitTextFiled Details:" << Details << detailField;
        //        cppCommand(Details);
    } else if (getCommand.contains("getDistanceDetailA")) {
        double distancecmd = QJsonValue(command["Distance"]).toDouble();
        QString detailcmd = QJsonValue(command["Detail"]).toString();
        QString phase = QJsonValue(command["PHASE"]).toString();
        QString DetailsAndDistance = QString(
                                         "{"
                                         "\"objectName\":\"getDistanceDetailA\","
                                         "\"Distance\":\%1,"
                                         "\"Detail\":\"%2\","
                                         "\"PHASE\":\"%3\""
                                         "}")
                                         .arg(distancecmd)
                                         .arg(detailcmd)
                                         .arg(phase);

        qDebug() << "cppSubmitTextFiled DetailsAndDistance:" << DetailsAndDistance << phase << distancecmd << detailcmd;
        emit getDistanceandDetailA(DetailsAndDistance);
    } else if (getCommand.contains("getDistanceDetailB")) {
        double distancecmd = QJsonValue(command["Distance"]).toDouble();
        QString detailcmd = QJsonValue(command["Detail"]).toString();
        QString phase = QJsonValue(command["PHASE"]).toString();
        QString DetailsAndDistance = QString(
                                         "{"
                                         "\"objectName\":\"getDistanceDetailB\","
                                         "\"Distance\":\%1,"
                                         "\"Detail\":\"%2\","
                                         "\"PHASE\":\"%3\""
                                         "}")
                                         .arg(distancecmd)
                                         .arg(detailcmd)
                                         .arg(phase);

        qDebug() << "cppSubmitTextFiled DetailsAndDistance:" << DetailsAndDistance << phase << distancecmd << detailcmd;
        emit getDistanceandDetailB(DetailsAndDistance);
    } else if (getCommand.contains("getDistanceDetailC")) {
        double distancecmd = QJsonValue(command["Distance"]).toDouble();
        QString detailcmd = QJsonValue(command["Detail"]).toString();
        QString phase = QJsonValue(command["PHASE"]).toString();
        QString DetailsAndDistance = QString(
                                         "{"
                                         "\"objectName\":\"getDistanceDetailC\","
                                         "\"Distance\":\%1,"
                                         "\"Detail\":\"%2\","
                                         "\"PHASE\":\"%3\""
                                         "}")
                                         .arg(distancecmd)
                                         .arg(detailcmd)
                                         .arg(phase);

        qDebug() << "cppSubmitTextFiled DetailsAndDistance:" << DetailsAndDistance << phase << distancecmd << detailcmd;
        emit getDistanceandDetailC(DetailsAndDistance);
    } else if (getCommand.contains("TaggingPhaseA")) {
        QString tableTaggingPhaseA = QJsonValue(command["tableTaggingPhaseA"]).toString();
        QString getTaggingPhaseA = QString(
                                       "{"
                                       "\"objectName\":\"TaggingPhaseA\","
                                       "\"tableTaggingPhaseA\":\"%1\""
                                       "}")
                                       .arg(tableTaggingPhaseA);

        qDebug() << "getTaggingPhaseA:" << getTaggingPhaseA;
        emit getTablePhaseA(getTaggingPhaseA);
    } else if (getCommand.contains("TaggingPhaseB")) {
        QString tableTaggingPhaseB = QJsonValue(command["tableTaggingPhaseB"]).toString();
        QString getTaggingPhaseB = QString(
                                       "{"
                                       "\"objectName\":\"TaggingPhaseB\","
                                       "\"tableTaggingPhaseB\":\"%1\""
                                       "}")
                                       .arg(tableTaggingPhaseB);

        qDebug() << "getTaggingPhaseB:" << tableTaggingPhaseB;
        emit getTablePhaseB(getTaggingPhaseB);
    } else if (getCommand.contains("TaggingPhaseC")) {
        QString tableTaggingPhaseC = QJsonValue(command["tableTaggingPhaseC"]).toString();
        QString getTaggingPhaseC = QString(
                                       "{"
                                       "\"objectName\":\"TaggingPhaseC\","
                                       "\"tableTaggingPhaseC\":\"%1\""
                                       "}")
                                       .arg(tableTaggingPhaseC);

        qDebug() << "getTaggingPhaseC:" << tableTaggingPhaseC;
        emit getTablePhaseC(getTaggingPhaseC);
    } else if (getCommand.contains("delectmysqlA")) {
        bool checkedStates = QJsonValue(command["checkedStates"]).toBool();
        int num_list = QJsonValue(command["num_listA"]).toInt();

        QString deletedtmySQLA = QString(
                                     "{"
                                     "\"objectName\":\"delectmysqlA\","
                                     "\"checkedStates\":\"%1\","
                                     "\"num_listA\":\"%2\""
                                     "}")
                                     .arg(checkedStates)
                                     .arg(num_list);

        qDebug() << "delectmySQLA:" << deletedtmySQLA;
        emit deletedMySQLA(deletedtmySQLA);
    } else if (getCommand.contains("delectmysqlB")) {
        bool checkedStates = QJsonValue(command["checkedStates"]).toBool();
        int num_list = QJsonValue(command["num_listB"]).toInt();

        QString deletedtmySQLB = QString(
                                     "{"
                                     "\"objectName\":\"delectmysqlB\","
                                     "\"checkedStates\":\"%1\","
                                     "\"num_listB\":\"%2\""
                                     "}")
                                     .arg(checkedStates)
                                     .arg(num_list);

        qDebug() << "delectmySQLB:" << deletedtmySQLB;
        emit deletedMySQLB(deletedtmySQLB);
    } else if (getCommand.contains("delectmysqlC")) {
        bool checkedStates = QJsonValue(command["checkedStates"]).toBool();
        int num_list = QJsonValue(command["num_listC"]).toInt();

        QString deletedtmySQLC = QString(
                                     "{"
                                     "\"objectName\":\"delectmysqlC\","
                                     "\"checkedStates\":\"%1\","
                                     "\"num_listC\":\"%2\""
                                     "}")
                                     .arg(checkedStates)
                                     .arg(num_list);

        qDebug() << "delectmySQLC:" << deletedtmySQLC;
        emit deletedMySQLC(deletedtmySQLC);
    } else if (getCommand.contains("UpdatePhaseA")) {
        QString updatetablePhaseA = QJsonValue(command["updatetablePhaseA"]).toString();
        QString getupdatePhaseA = QString(
                                      "{"
                                      "\"objectName\":\"UpdatePhaseA\","
                                      "\"updatetablePhaseA\":\"%1\""
                                      "}")
                                      .arg(updatetablePhaseA);

        qDebug() << "getupdatePhaseA:" << getupdatePhaseA;
        emit updateTablePhaseA(getupdatePhaseA);
    } else if (getCommand.contains("UpdatePhaseB")) {
        QString updatetablePhaseB = QJsonValue(command["updatetablePhaseB"]).toString();
        QString getupdatePhaseB = QString(
                                      "{"
                                      "\"objectName\":\"UpdatePhaseB\","
                                      "\"updatetablePhaseB\":\"%1\""
                                      "}")
                                      .arg(updatetablePhaseB);

        qDebug() << "getupdatePhaseB:" << getupdatePhaseB;
        emit updateTablePhaseA(getupdatePhaseB);
    } else if (getCommand.contains("UpdatePhaseC")) {
        QString updatetablePhaseC = QJsonValue(command["updatetablePhaseC"]).toString();
        QString getupdatePhaseC = QString(
                                      "{"
                                      "\"objectName\":\"UpdatePhaseC\","
                                      "\"updatetablePhaseC\":\"%1\""
                                      "}")
                                      .arg(updatetablePhaseC);

        qDebug() << "getupdatePhaseA:" << getupdatePhaseC;
        emit updateTablePhaseA(getupdatePhaseC);
    } else if (getCommand.contains("editDataPhaseA")) {
        double checkedStates = QJsonValue(command["checkedStates"]).toDouble();
        QString phase = QJsonValue(command["PHASE"]).toString();
        int IndexNum = QJsonValue(command["num_listA"]).toInt();
        QString EditDatalist = QString(
                                   "{"
                                   "\"objectName\":\"editDataPhaseA\","
                                   "\"IndexNum\":%1,"
                                   "\"PHASE\":\"%2\""
                                   "}")
                                   .arg(IndexNum)
                                   .arg(phase);

        qDebug() << "Debug editDataPhaseA:" << EditDatalist << phase << checkedStates;
        emit getEditDatafromMySQLA(EditDatalist);
    } else if (getCommand.trimmed() == "marginCountA" || getCommand.trimmed() == "valueMarginVoltageA") {
        int marginA = QJsonValue(command["valueMarginA"]).toInt();
        int valueVoltageA = QJsonValue(command["valueVoltageA"]).toInt();
        int focusIndex = QJsonValue(command["focusIndex"]).toInt();
        int listmarginA = QJsonValue(command["listMarginA"]).toInt();
        QString phase = QJsonValue(command["PHASE"]).toString();
        // qWarning() << "marginA:" << marginA << listmarginA << valueVoltageA << focusIndex << phase;
        if (getCommand.contains("marginCountA") && !getCommand.contains("valueMarginVoltageA")) {
            thelistNumOfMarginA = marginA;
            // qWarning() << "Updated thelistNumOfMargin to:" << thelistNumOfMarginA;
        } else if (getCommand.contains("valueMarginVoltageA")) {
            // qWarning() << "Received valueMarginVoltageA, skipping marginA update.";
        }

        if ((valueVoltageA != 0) && (marginA != 0)) {
            QString combinedData = QString(
                                       "{"
                                       "\"objectName\":\"combinedDataPhaseA\","
                                       "\"margin\":%1,"
                                       "\"valueVoltage\":%2,"
                                       "\"focusIndex\":%3,"
                                       "\"PHASE\":\"%4\""
                                       "}")
                                       .arg(thelistNumOfMarginA)
                                       .arg(valueVoltageA)
                                       .arg(focusIndex)
                                       .arg(phase);

            // qWarning() << "Combined Data for Phase A:" << combinedData;

            RecalculateWithMargin(combinedData);
            emit UpdateMarginSettingParameter(combinedData);
            emit updataListOfMarginA(combinedData);
        } else if (marginA != 0) {
            //            QString combinedData = QString("{"
            //                                            "\"objectName\":\"combinedDataPhaseA\","
            //                                            "\"marginA\":%1,"
            //                                            "\"PHASE\":\"%4\""
            //                                            "}").arg(thelistNumOfMarginA).arg(phase);

            //            qDebug() << "Margin for Phase A:" << combinedData;

            //            emit UpdateMarginSettingParameter(combinedData);
        } else if (marginA == 0) {
            marginA = listmarginA;
            QString combinedData = QString(
                                       "{"
                                       "\"objectName\":\"combinedDataPhaseA\","
                                       "\"margin\":%1,"
                                       "\"valueVoltage\":%2,"
                                       "\"focusIndex\":%3,"
                                       "\"PHASE\":\"%4\""
                                       "}")
                                       .arg(marginA)
                                       .arg(valueVoltageA)
                                       .arg(focusIndex)
                                       .arg(phase);

            // qWarning() << "new value of marginA:" << combinedData << marginA << listmarginA;
            //            sendMessage(qmlJson);

            RecalculateWithMargin(combinedData);
            emit UpdateMarginSettingParameter(combinedData);
            emit updataListOfMarginA(combinedData);
        } else {
            // qWarning() << "valueVoltageA is not greater than 0. Skipping RecalculateWithMargin.";
        }
        QString parameterA = QString(
                                 "{"
                                 "\"objectName\":\"marginCountA\","
                                 "\"marginA\":%1,"
                                 "\"PHASE\":\"%2\""
                                 "}")
                                 .arg(marginA)
                                 .arg(phase);

        // qWarning() << "Debug marginCountA:" << marginA << phase;
        emit parameterMarginA(parameterA);
    } else if (getCommand.contains("marginCountB") || getCommand.contains("valueMarginVoltageB")) {
        int marginB = QJsonValue(command["valueMarginB"]).toInt();
        int valueVoltageB = QJsonValue(command["valueVoltageB"]).toInt();
        int focusIndex = QJsonValue(command["focusIndex"]).toInt();
        int listmarginB = QJsonValue(command["listMarginB"]).toInt();
        QString phase = QJsonValue(command["PHASE"]).toString();

        // qWarning() << "marginB:" << marginB << listmarginB << valueVoltageB << focusIndex << phase;

        if (getCommand.contains("marginCountB") && !getCommand.contains("valueMarginVoltageB")) {
            thelistNumOfMarginB = marginB;
            // qWarning() << "Updated thelistNumOfMarginB to:" << thelistNumOfMarginB;
        } else if (getCommand.contains("valueMarginVoltageB")) {
            // qWarning() << "Received valueMarginVoltageB, skipping marginB update.";
        }

        if ((valueVoltageB != 0) && (marginB != 0)) {
            QString combinedData = QString(
                                       "{"
                                       "\"objectName\":\"combinedDataPhaseB\","
                                       "\"margin\":%1,"
                                       "\"valueVoltage\":%2,"
                                       "\"focusIndex\":%3,"
                                       "\"PHASE\":\"%4\""
                                       "}")
                                       .arg(thelistNumOfMarginB)
                                       .arg(valueVoltageB)
                                       .arg(focusIndex)
                                       .arg(phase);

            // qWarning() << "Combined Data for Phase B:" << combinedData;

            RecalculateWithMargin(combinedData);
            emit UpdateMarginSettingParameter(combinedData);
            emit updataListOfMarginB(combinedData);
        } else if (marginB != 0) {
            //            QString combinedData = QString("{"
            //                                           "\"objectName\":\"combinedDataPhaseB\","
            //                                           "\"marginB\":%1,"
            //                                           "\"PHASE\":\"%2\""
            //                                           "}").arg(thelistNumOfMarginB).arg(phase);

            //            qDebug() << "Combined Data for Phase B:" << combinedData;

            //            emit UpdateMarginSettingParameter(combinedData);
        } else if (marginB == 0) {
            marginB = listmarginB;
            QString combinedData = QString(
                                       "{"
                                       "\"objectName\":\"combinedDataPhaseB\","
                                       "\"margin\":%1,"
                                       "\"valueVoltage\":%2,"
                                       "\"focusIndex\":%3,"
                                       "\"PHASE\":\"%4\""
                                       "}")
                                       .arg(marginB)
                                       .arg(valueVoltageB)
                                       .arg(focusIndex)
                                       .arg(phase);

            // qWarning() << "New value of marginB:" << combinedData << marginB << listmarginB;

            RecalculateWithMargin(combinedData);
            emit UpdateMarginSettingParameter(combinedData);
            emit updataListOfMarginB(combinedData);
        } else {
            // qWarning() << "valueVoltageB is not greater than 0. Skipping RecalculateWithMargin.";
        }

        QString parameterB = QString(
                                 "{"
                                 "\"objectName\":\"marginCountB\","
                                 "\"marginB\":%1,"
                                 "\"PHASE\":\"%2\""
                                 "}")
                                 .arg(marginB)
                                 .arg(phase);

        // qWarning() << "Debug marginCountB:" << marginB << phase;
        emit parameterMarginB(parameterB);

    } else if (getCommand.contains("marginCountC") || getCommand.contains("valueMarginVoltageC")) {
        int marginC = QJsonValue(command["valueMarginC"]).toInt();
        int valueVoltageC = QJsonValue(command["valueVoltageC"]).toInt();
        int focusIndex = QJsonValue(command["focusIndex"]).toInt();
        int listmarginC = QJsonValue(command["listMarginC"]).toInt();
        QString phase = QJsonValue(command["PHASE"]).toString();

        // qWarning() << "marginC:" << marginC << listmarginC << valueVoltageC << focusIndex << phase;

        if (getCommand.contains("marginCountC") && !getCommand.contains("valueMarginVoltageC")) {
            thelistNumOfMarginC = marginC;
            // qWarning() << "Updated thelistNumOfMarginC to:" << thelistNumOfMarginC;
        } else if (getCommand.contains("valueMarginVoltageC")) {
            // qWarning() << "Received valueMarginVoltageC, skipping marginC update.";
        }

        if ((valueVoltageC != 0) && (marginC != 0)) {
            QString combinedData = QString(
                                       "{"
                                       "\"objectName\":\"combinedDataPhaseC\","
                                       "\"margin\":%1,"
                                       "\"valueVoltage\":%2,"
                                       "\"focusIndex\":%3,"
                                       "\"PHASE\":\"%4\""
                                       "}")
                                       .arg(thelistNumOfMarginC)
                                       .arg(valueVoltageC)
                                       .arg(focusIndex)
                                       .arg(phase);

            // qWarning() << "Combined Data for Phase C:" << combinedData;

            RecalculateWithMargin(combinedData);
            emit UpdateMarginSettingParameter(combinedData);
            emit updataListOfMarginC(combinedData);
        } else if (marginC != 0) {
            //            QString combinedData = QString("{"
            //                                           "\"objectName\":\"combinedDataPhaseC\","
            //                                           "\"marginC\":%1,"
            //                                           "\"PHASE\":\"%2\""
            //                                           "}").arg(thelistNumOfMarginB).arg(phase);

            //            qDebug() << "Combined Data for Phase C:" << combinedData;

            //            emit UpdateMarginSettingParameter(combinedData);
        } else if (marginC == 0) {
            marginC = listmarginC;
            QString combinedData = QString(
                                       "{"
                                       "\"objectName\":\"combinedDataPhaseC\","
                                       "\"margin\":%1,"
                                       "\"valueVoltage\":%2,"
                                       "\"focusIndex\":%3,"
                                       "\"PHASE\":\"%4\""
                                       "}")
                                       .arg(marginC)
                                       .arg(valueVoltageC)
                                       .arg(focusIndex)
                                       .arg(phase);

            // qWarning() << "New value of marginC:" << combinedData << marginC << listmarginC;

            RecalculateWithMargin(combinedData);
            emit UpdateMarginSettingParameter(combinedData);
            emit updataListOfMarginC(combinedData);
        } else {
            // qWarning() << "valueVoltageC is not greater than 0. Skipping RecalculateWithMargin.";
        }

        QString parameterC = QString(
                                 "{"
                                 "\"objectName\":\"marginCountC\","
                                 "\"marginC\":%1,"
                                 "\"PHASE\":\"%2\""
                                 "}")
                                 .arg(marginC)
                                 .arg(phase);

        // qWarning() << "Debug marginCountC:" << marginC << phase;
        emit parameterMarginC(parameterC);
    } else if (getCommand == ("autoSetValueMarginA") || getCommand == ("autoSetValueMarginB") || getCommand == ("autoSetValueMarginC")) {
        RecalculateWithMargin(msgs);
        if (getCommand == ("autoSetValueMarginA")) {
            emit updataListOfMarginA(msgs);
        } else if (getCommand == ("autoSetValueMarginB")) {
            emit updataListOfMarginB(msgs);
        } else if (getCommand == ("autoSetValueMarginC")) {
            emit updataListOfMarginC(msgs);
        }

    } else if (getCommand == ("selectTaggingPoint")) {
        myDatabase->updateTaggingPoint(msgs);
    } else if (obj["objectName"].toString() == "thresholdA") {
        myDatabase->configParemeterThreshold(msgs);
    } else if (msgs == "updateTaggingList") {
        myDatabase->fetchTaggingData();
    } else if (obj["objectName"].toString() == "thresholdB") {
        myDatabase->configParemeterThreshold(msgs);
    } else if (obj["objectName"].toString() == "thresholdC") {
        myDatabase->configParemeterThreshold(msgs);
    } else if (msgs == "getCurrentThreshold") {
        emit getDataThreshold();
    } else if (msgs == "SettingGeneral") {
        emit settingGeneral();
    } else if (msgs == "getpreiodicInfo") {
        emit preiodicSetting();
    } else if (getCommand == "date") {
        //            if (command.contains("Monday")) {
        emit updateWeekly(msgs);
        //            } else if (command.contains("Tuesday")) {
        //                emit updateWeekly(msgs);
        //            } else if (command.contains("Wednesday")) {
        //                emit updateWeekly(msgs);
        //            } else if (command.contains("Thursday")) {
        //                emit updateWeekly(msgs);
        //            } else if (command.contains("Friday")) {
        //                emit updateWeekly(msgs);
        //            } else if (command.contains("Saturday")) {
        //                emit updateWeekly(msgs);
        //            } else if (command.contains("Sunday")) {
        //                emit updateWeekly(msgs);
        //            }
    } else if (getCommand.contains("distance") && (command.contains("distance"))) {
        double distance = QJsonValue(command["distance"]).toDouble();
        QString positionDistance = QString(
                                       "{"
                                       "\"objectName\"     :\"distance\","
                                       "\"distance\"         :\"%1\""
                                       "}")
                                       .arg(distance);
        qDebug() << "positionDistance:" << positionDistance << distance;
        emit cursorDistance(positionDistance);
    } else if (getCommand.contains("decreaseValue")) {
        double decreaseValue = QJsonValue(command["decreaseValue"]).toDouble();
        QString movetoleft = QString(
                                 "{"
                                 "\"objectName\"     :\"decreaseValue\","
                                 "\"decreaseValue\"         :%1"
                                 "}")
                                 .arg(decreaseValue);
        qDebug() << "movetoleft:" << movetoleft << decreaseValue;
        emit moveCursor(movetoleft);
    } else if (getCommand.contains("increaseValue")) {
        double increaseValue = QJsonValue(command["increaseValue"]).toDouble();
        QString movetoright = QString(
                                  "{"
                                  "\"objectName\"     :\"increaseValue\","
                                  "\"increaseValue\"         :%1"
                                  "}")
                                  .arg(increaseValue);
        qDebug() << "movetoleft:" << movetoright << increaseValue;
        emit moveCursor(movetoright);

    } else if (getCommand.contains("getDatabuttonPhaseA")) {
        //            QString rawdataA = QJsonValue(command["rawdataA"]).toString();
        //            QString getRawDataPhaseA = QString("{"
        //                                            "\"objectName\"     :\"getDatabuttonPhaseA\","
        //                                            "\"rawdataA\"         :\"%1\""
        //                                            "}").arg(rawdataA);
        //            qDebug() << "getpatternPhaseA:" << getRawDataPhaseA << rawdataA;
        //            emit rawdataPlot(getRawDataPhaseA);
        QJsonDocument jsonDoc;
        QJsonObject Param;
        QString raw_data;
        Param.insert("objectName", "replotDataA");  // Name
        //            Param.insert("threshold",1500); startPlotingDataPhaseA
        //            Param.insert("sagFactor",0.983);
        //            Param.insert("samplingRate",180);
        //            Param.insert("distanceToStart",0);
        //            Param.insert("distanceToShow",10);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(raw_data, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(raw_data);
    } else if (getCommand.contains("getDatabuttonPhaseB")) {
        //            QString rawdataB = QJsonValue(command["rawdataB"]).toString();
        //            QString getRawDataPhaseB = QString("{"
        //                                            "\"objectName\"     :\"getDatabuttonPhaseB\","
        //                                            "\"rawdataB\"         :\"%1\""
        //                                            "}").arg(rawdataB);
        //            qDebug() << "getpatternPhaseB:" << getRawDataPhaseB << rawdataB;
        //            emit rawdataPlot(getRawDataPhaseB);
        QJsonDocument jsonDoc;
        QJsonObject Param;
        QString raw_data;
        Param.insert("objectName", "startPlotingDataPhaseB");  // Name
        Param.insert("threshold", 1500);
        Param.insert("sagFactor", 0.983);
        Param.insert("samplingRate", 180);
        Param.insert("distanceToStart", 0);
        Param.insert("distanceToShow", 10);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(raw_data, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(raw_data);
    } else if (getCommand.contains("getDatabuttonPhaseC")) {
        //            QString rawdataC = QJsonValue(command["rawdataC"]).toString();
        //            QString getRawDataPhaseC = QString("{"
        //                                            "\"objectName\"     :\"getDatabuttonPhaseC\","
        //                                            "\"rawdataC\"         :\"%1\""
        //                                            "}").arg(rawdataC);
        //            qDebug() << "getpatternPhaseC:" << getRawDataPhaseC << rawdataC;
        //            emit rawdataPlot(getRawDataPhaseC);
        QJsonDocument jsonDoc;
        QJsonObject Param;
        QString raw_data;
        Param.insert("objectName", "startPlotingDataPhaseC");  // Name
        Param.insert("threshold", 1500);
        Param.insert("sagFactor", 0.983);
        Param.insert("samplingRate", 180);
        Param.insert("distanceToStart", 0);
        Param.insert("distanceToShow", 10);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(raw_data, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(raw_data);
    }

    else if (getCommand.contains("getpatternPhaseA")) {
        QString patternA = QJsonValue(command["patternA"]).toString();
        QString getpatternPhaseA = QString(
                                       "{"
                                       "\"objectName\"     :\"getpatternPhaseA\","
                                       "\"getpatternA\"         :\"%1\""
                                       "}")
                                       .arg(patternA);
        qDebug() << "getpatternPhaseA:" << getpatternPhaseA << patternA;
        emit rawdataPlot(getpatternPhaseA);
    } else if (getCommand.contains("getpatternPhaseB")) {
        QString patternB = QJsonValue(command["patternB"]).toString();
        QString getpatternPhaseB = QString(
                                       "{"
                                       "\"objectName\"     :\"getpatternPhaseB\","
                                       "\"getpatternB\"         :\"%1\""
                                       "}")
                                       .arg(patternB);
        qDebug() << "getpatternPhaseB:" << getpatternPhaseB << patternB;
        emit rawdataPlot(getpatternPhaseB);
    } else if (getCommand.contains("getpatternPhaseC")) {
        QString patternC = QJsonValue(command["patternC"]).toString();
        QString getpatternPhaseC = QString(
                                       "{"
                                       "\"objectName\"     :\"getpatternPhaseC\","
                                       "\"getpatternC\"         :\"%1\""
                                       "}")
                                       .arg(patternC);
        qDebug() << "getpatternPhaseC:" << getpatternPhaseC << patternC;
        emit rawdataPlot(getpatternPhaseC);
    } else if (getCommand.contains("clearpatternPhaseA")) {
        QString patternA = QJsonValue(command["patternA"]).toString();
        QString clearpatternPhaseA = QString(
                                         "{"
                                         "\"objectName\"     :\"clearpatternPhaseA\","
                                         "\"clearpatternA\"         :\"%1\""
                                         "}")
                                         .arg(patternA);
        qDebug() << "clearpatternPhaseA:" << clearpatternPhaseA << patternA;
        emit clearDisplay(clearpatternPhaseA);
    } else if (getCommand.contains("clearpatternPhaseB")) {
        QString patternB = QJsonValue(command["patternB"]).toString();
        QString clearpatternPhaseB = QString(
                                         "{"
                                         "\"objectName\"     :\"clearpatternPhaseB\","
                                         "\"clearpatternB\"         :\"%1\""
                                         "}")
                                         .arg(patternB);
        qDebug() << "clearpatternPhaseB:" << clearpatternPhaseB << patternB;
        emit clearDisplay(clearpatternPhaseB);
    } else if (getCommand.contains("clearpatternPhaseC")) {
        QString patternC = QJsonValue(command["patternC"]).toString();
        QString clearpatternPhaseC = QString(
                                         "{"
                                         "\"objectName\"     :\"clearpatternPhaseC\","
                                         "\"clearpatternC\"         :\"%1\""
                                         "}")
                                         .arg(patternC);
        qDebug() << "clearpatternPhaseC:" << clearpatternPhaseC << patternC;
        emit clearDisplay(clearpatternPhaseC);
    } else if (getCommand.contains("clearDatabuttonPhaseA")) {  // CLear Display Phase A
        QString dataA = QJsonValue(command["rawdataA"]).toString();
        QString cleardataA = QString(
                                 "{"
                                 "\"objectName\"     :\"clearDatabuttonPhaseA\","
                                 "\"cleardataA\"         :\"%1\""
                                 "}")
                                 .arg(dataA);
        qDebug() << "clearDatabuttonPhaseA:" << cleardataA << dataA;
        emit clearDisplay(cleardataA);
    } else if (getCommand.contains("clearDatabuttonPhaseB")) {  // CLear Display Phase B
        QString dataB = QJsonValue(command["rawdataB"]).toString();
        QString cleardataB = QString(
                                 "{"
                                 "\"objectName\"     :\"clearDatabuttonPhaseB\","
                                 "\"cleardataB\"         :\"%1\""
                                 "}")
                                 .arg(dataB);
        qDebug() << "clearDatabuttonPhaseB:" << cleardataB << dataB;
        emit clearDisplay(cleardataB);
    } else if (getCommand.contains("clearDatabuttonPhaseC")) {  // CLear Display Phase C
        QString dataC = QJsonValue(command["patternC"]).toString();
        QString cleardataC = QString(
                                 "{"
                                 "\"objectName\"     :\"clearDatabuttonPhaseC\","
                                 "\"cleardataC\"         :\"%1\""
                                 "}")
                                 .arg(dataC);
        qDebug() << "clearpatternPhaseC:" << cleardataC << dataC;
        emit clearDisplay(cleardataC);
    } else if (getCommand.contains("rangedistance")) {
        double range = QJsonValue(command["rangedistance"]).toDouble();
        QString rangeChange = QString(
                                  "{"
                                  "\"objectName\"     :\"rangedistance\","
                                  "\"rangedistance\"         :%1"
                                  "}")
                                  .arg(range);
        qDebug() << "rangeChange:" << rangeChange << range;
        emit changeDistanceRange(rangeChange);
    } else if (getCommand.contains("taggingdata")) {
        int tagging = QJsonValue(command["checklist"]).toInt();
        bool statuslist = QJsonValue(command["statuslist"]).toBool();

        QString taggingdata = QString(
                                  "{"
                                  "\"objectName\":\"taggingdata\","
                                  "\"checklist\":%1,"
                                  "\"statuslist\":%2"
                                  "}")
                                  .arg(tagging)
                                  .arg(statuslist ? "true" : "false");

        qDebug() << "taggingdata:" << taggingdata << "tagging:" << tagging << "statuslist:" << statuslist;
        emit taggingpoint(taggingdata);
    } else if (obj["objectName"].toString() == "displaySetting") {
        //            qDebug() << "toString" << QString::number(obj["distancetoshowText"].toInt()) << " toDouble"<< obj["distancetoshowText"].toDouble();
        //            QString displaySetting;
        //            double distancetoshowText,sagFactor,samplingRate,distancetostartText,fulldistanceText;
        //            if(QString::number(obj["distancetoshowText"].toInt()) != ""){
        //                distancetoshowText = obj["distancetoshowText"].toDouble();
        //                displaySetting = QString("{"
        //                    "\"objectName\"     :\"displaySetting\","
        //                    "\"distancetoshowText\"         :%1"
        //                    "}").arg(distancetoshowText);
        //            }
        //            else if(QString::number(obj["distancetostartText"].toInt()) != ""){
        //                distancetostartText = obj["distancetostartText"].toDouble();
        //                displaySetting = QString("{"
        //                    "\"objectName\"     :\"displaySetting\","
        //                    "\"distancetostartText\"         :%1"
        //                    "}").arg(distancetostartText);
        //            }
        //            else if(QString::number(obj["samplingRate"].toInt()) != ""){
        //                samplingRate = obj["samplingRate"].toDouble();
        //                displaySetting = QString("{"
        //                    "\"objectName\"     :\"displaySetting\","
        //                    "\"samplingRate\"         :%1"
        //                    "}").arg(samplingRate);
        //            }
        //            else if(QString::number(obj["sagFactor"].toInt()) != ""){
        //                sagFactor = obj["sagFactor"].toDouble();
        //                displaySetting = QString("{"
        //                    "\"objectName\"     :\"displaySetting\","
        //                    "\"sagFactor\"         :%1"
        //                    "}").arg(sagFactor);
        //            }
        //            else if(QString::number(obj["fulldistanceText"].toInt()) != ""){
        //                fulldistanceText = obj["fulldistanceText"].toDouble();
        //                displaySetting = QString("{"
        //                    "\"objectName\"     :\"displaySetting\","
        //                    "\"fulldistanceText\"         :%1"
        //                    "}").arg(fulldistanceText);
        //            }
        //            qDebug() << "displaySetting:" << displaySetting;
        emit settingdisplay(msgs);

        QJsonDocument jsonDoc;
        QJsonObject Param;
        QString raw_data;
        Param.insert("menuID", "updateEquipmentdata");
        Param.insert("SubstationName", SetupEquipment->SubstationName);
        Param.insert("Voltage", SetupEquipment->Voltage);
        Param.insert("TransmissionLineName", SetupEquipment->TransmissionLineName);
        Param.insert("Distance", SetupEquipment->Distance);
        Param.insert("IPaddress", SetupEquipment->IPaddress);
        Param.insert("Brand", SetupEquipment->Brand);
        Param.insert("Model", SetupEquipment->Model);
        Param.insert("SerialNo", SetupEquipment->SerialNo);
        Param.insert("ContractNumber", SetupEquipment->ContractNumber);
        Param.insert("Date", SetupEquipment->Date);
        Param.insert("LFLSerialNo", SetupEquipment->LFLSerialNo);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        if (snmp_address->state() == QAbstractSocket::ConnectedState) {
            emit sendMessage(raw_data, snmp_address);
            qDebug() << "updateEquipmentdata: SubstationName";
        } else
            qDebug() << "snmp_address:" << snmp_address->state();

        Q_FOREACH (QWebSocket *pClient, webapp_address) {
            emit sendMessage(raw_data, pClient);
        }
    }

    // NETWORK
    else if (obj["menuID"].toString() == "updateLocalNetwork") {
        networks->dhcpmethod = obj["dhcpmethod"].toString();
        networks->ip_address = obj["ipaddress"].toString();
        networks->subnet = obj["subnet"].toString();
        networks->ip_gateway = obj["gateway"].toString();
        networks->pridns = obj["pridns"].toString();
        networks->secdns = obj["secdns"].toString();
        SetupEquipment->IPaddress = networks->ip_address;
        qDebug() << "updateLocalNetwork:" << networks->dhcpmethod << networks->ip_address << networks->subnet << networks->ip_gateway << networks->pridns << networks->secdns;

        QJsonDocument jsonDoc;
        QJsonObject Param;
        QString raw_data;
        Param.insert("menuID", "network");  // Name
        Param.insert("ipaddress", networks->ip_address);
        Param.insert("gateway", networks->ip_gateway);
        Param.insert("subnet", networks->subnet);
        if (networks->dhcpmethod == 0)
            Param.insert("dhcpmethod", "on");
        else
            Param.insert("dhcpmethod", "off");
        Param.insert("pridns", networks->pridns);
        Param.insert("secdns", networks->secdns);
        Param.insert("phyNetworkName", networks->phyName);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        emit sendMessage(raw_data, wClient);
        QJsonObject().swap(Param);

        Param.insert("menuID", "snmp_server");  // Name
        Param.insert("snmp_ip", networks->ip_snmp);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        emit sendMessage(raw_data, wClient);
        QJsonObject().swap(Param);

        Param.insert("menuID", "smtp_server");
        Param.insert("sender_email", Email_Config->senderEmail);
        Param.insert("sender_name", Email_Config->senderName);
        Param.insert("password_smtp", Email_Config->password);
        Param.insert("recipient_email", Email_Config->recipientEmail);
        Param.insert("recipient_name", Email_Config->recipientName);
        Param.insert("smtp_server", Email_Config->smtpServer);
        Param.insert("smtp_port", Email_Config->smtpPort);
        Param.insert("DELAY_EVENT_MAIL", Email_Param->DELAY_EVENT_MAIL);
        Param.insert("DELAY_ALARM_MAIL", Email_Param->DELAY_ALARM_MAIL);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        emit sendMessage(raw_data, wClient);

        Param.insert("objectName", "Network");
        Param.insert("ip_address", networks->ip_address);
        Param.insert("ip_gateway", networks->ip_gateway);
        Param.insert("ip_snmp", networks->ip_snmp);
        Param.insert("ip_timeserver", networks->ip_timeserver);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        // qDebug() << "raw_data::" << raw_data;
        if (snmp_address->state() == QAbstractSocket::ConnectedState) {
            emit sendMessage(raw_data, snmp_address);
        } else
            qDebug() << "snmp_address:" << snmp_address->state();
        if (FPGA_address->state() == QAbstractSocket::ConnectedState) {
            emit sendMessage(raw_data, FPGA_address);
        } else
            qDebug() << "FPGA_address:" << FPGA_address->state();
        //        emit sendMessage(raw_data,snmp_address);
        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(raw_data, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(raw_data);
        emit broadcastMessage(raw_data);

        networks->printinfo();

        myDatabase->updateParameterEquipment(SetupEquipment->SubstationName, SetupEquipment->Voltage, SetupEquipment->TransmissionLineName, SetupEquipment->Distance, SetupEquipment->IPaddress, SetupEquipment->Brand, SetupEquipment->Model, SetupEquipment->SerialNo, SetupEquipment->ContractNumber, SetupEquipment->Date, SetupEquipment->LFLSerialNo);

        loopNetworkTimer->start(3000);
        QString forward = "sudo systemctl restart PLCForward.service";
        system(forward.toUtf8());
        //        updateNetwork(networks->dhcpmethod.toInt(), networks->ip_address, networks->subnet, networks->ip_gateway, networks->pridns, networks->secdns, "eth0");

        //        myDatabase->updateSettingNetwork(networks->ip_address,networks->ip_gateway,networks->ip_snmp,networks->ip_timeserver
        //                                         ,snmp_param->PLC_DO_ERROR,snmp_param->PLC_DI_ERROR
        //                                         ,snmp_param->MODULE_HI_SPEED_PHASE_A_ERROR,snmp_param->MODULE_HI_SPEED_PHASE_B_ERROR,snmp_param->MODULE_HI_SPEED_PHASE_C_ERROR
        //                                         ,snmp_param->INTERNAL_PHASE_A_ERROR,snmp_param->INTERNAL_PHASE_B_ERROR,snmp_param->INTERNAL_PHASE_C_ERROR
        //                                         ,snmp_param->GPS_MODULE_FAIL,snmp_param->SYSTEM_INITIAL,snmp_param->COMMUNICATION_ERROR
        //                                         ,snmp_param->RELAY_START_EVENT,snmp_param->SURGE_START_EVENT,snmp_param->PERIODIC_TEST_EVENT
        //                                         ,snmp_param->MANUAL_TEST_EVENT,snmp_param->LFL_FAIL,snmp_param->LFL_OPERATE);
    } else if (obj["menuID"].toString() == "setMode") {
        masterLFL = obj["mode"].toString();
        masterIP = obj["ip_master"].toString();
        slaveIP = obj["ip_slave"].toString();
        qDebug() << "masterLFL:" << masterLFL << " masterIP:" << masterIP << " slaveIP:" << slaveIP;
        emit updateMasterMode(masterLFL, masterIP, slaveIP);
        // initMaster();

        QJsonDocument jsonDoc;
        QJsonObject Param;
        Param.insert("objectName", "selectUser");
        Param.insert("userType", masterLFL);
        Param.insert("ip_master", masterIP);
        Param.insert("ip_slave", slaveIP);
        if (masterLFL == "MASTER") {
            Param.insert("RemoteTOMonitor", "REMOTE TO SLAVE");
        } else if (masterLFL == "SLAVE") {
            Param.insert("RemoteTOMonitor", "REMOTE TO MASTER");
        }

        QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();

        Q_FOREACH (QWebSocket *pClient, webapp_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(raw_data, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }

        if (INPUT_PLC_address->state() == QAbstractSocket::ConnectedState) {
            emit sendMessage(raw_data, INPUT_PLC_address);
            qDebug() << "INPUT_PLC_address";
        } else
            qDebug() << "INPUT_PLC_address:" << INPUT_PLC_address->state();

        jsonDoc = QJsonDocument();
        Param = QJsonObject();
        Param.insert("objectName", "selectUserChange");
        Param.insert("userType", masterLFL);
        Param.insert("ip_master", masterIP);
        Param.insert("ip_slave", slaveIP);
        if (masterLFL == "MASTER") {
            Param.insert("RemoteTOMonitor", "REMOTE TO SLAVE");
        } else if (masterLFL == "SLAVE") {
            Param.insert("RemoteTOMonitor", "REMOTE TO MASTER");
        }

        QString raw_data2 = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();

        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState) {
                emit sendMessage(raw_data2, pClient);
                qDebug() << "sendtomonitors";
            } else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(raw_data2);
        emit sendToSocketPLC(raw_data2);
        // qWarning() << "sendtomonitors" << raw_data2;
        // if (OpenPLC_address->state() == QAbstractSocket::ConnectedState && OpenPLC_address != nullptr) {
        //     emit sendMessage(raw_data2, OpenPLC_address);
        //     qDebug() << "sendtomonitors";
        // } else
        //     qDebug() << "Monitor_address:" << OpenPLC_address->state();


        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState) {
                Monitor_address.removeAll(pClient);
                qDebug() << "sendtomonitors";
            } else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        // Monitor_address.removeAll(MonitorFirst_address);
        // Monitor_address.removeAll(MonitorSecond_address);
        // MonitorFirst_address = nullptr;
        // MonitorSecond_address = nullptr;
        changeIPAddress = true;
        selectUserFromWeb();
        plc_client->m_webSocket.close();
    }
    // NTP
    else if (obj["menuID"].toString() == "updateNTPServer") {
        client->disconnectFromServer();
        networks->ip_timeserver = obj["ntpServer"].toString();
        qDebug() << "updateNTPServer:" << networks->ip_timeserver;
        updateNTP();
        networking->setNTPServer(networks->ip_timeserver);
        emit setNTPServer(networks->ip_timeserver);
        serverAddress = networks -> ip_timeserver;
        qDebug() << "serverAddress:" << serverAddress << " serverPort:" << serverPort;

        QJsonDocument jsonDoc;
        QJsonObject Param;
        QString raw_data;
        Param.insert("objectName", "Network");
        Param.insert("ip_address", networks->ip_address);
        Param.insert("ip_gateway", networks->ip_gateway);
        Param.insert("ip_snmp", networks->ip_snmp);
        Param.insert("ip_timeserver", networks->ip_timeserver);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(raw_data, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(raw_data);

        // Param = QJsonObject();
        // raw_data = "";
        // Param.insert("menuID", "system");  // Name
        // Param.insert("SwVersion", SwVersion);
        // Param.insert("HwVersion", HwVersion);
        // if (networks->ip_timeserver != "" && networks->location_snmp != "") {
        //     Param.insert("dateTimeMethod", 1);
        // } else
        //     Param.insert("dateTimeMethod", 0);
        // Param.insert("ntpServer", networks->ip_timeserver);
        // Param.insert("location", networks->location_snmp);
        // raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        // Q_FOREACH (QWebSocket *pClient, Monitor_address) {
        //     if (pClient->state() == QAbstractSocket::ConnectedState)
        //         emit sendMessage(raw_data, pClient);
        //     else
        //         qDebug() << "Monitor_address:" << pClient->state();
        // }
        // emit sendToVNC(raw_data);

    } else if (obj["menuID"].toString() == "getNetworkPage") {
        if (webapp_address.isEmpty()) {
            webapp_address.append(wClient);
        } else {
            Q_FOREACH (QWebSocket *pClient, webapp_address) {
                if (pClient == wClient) {
                    ;
                } else {
                    webapp_address.append(wClient);
                }
            }
        }
        QJsonDocument jsonDoc;
        QJsonObject Param;
        QString raw_data;
        Param.insert("menuID", "network");  // Name
        Param.insert("ipaddress", networks->ip_address);
        Param.insert("gateway", networks->ip_gateway);
        Param.insert("subnet", networks->subnet);
        if (networks->dhcpmethod == 0)
            Param.insert("dhcpmethod", "on");
        else
            Param.insert("dhcpmethod", "off");
        Param.insert("pridns", networks->pridns);
        Param.insert("secdns", networks->secdns);
        Param.insert("phyNetworkName", networks->phyName);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        emit sendMessage(raw_data, wClient);
        QJsonObject().swap(Param);

        Param.insert("menuID", "snmp_server");  // Name
        Param.insert("snmp_ip", networks->ip_snmp);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        emit sendMessage(raw_data, wClient);
        QJsonObject().swap(Param);

        Param.insert("menuID", "smtp_server");
        Param.insert("sender_email", Email_Config->senderEmail);
        Param.insert("sender_name", Email_Config->senderName);
        Param.insert("password_smtp", Email_Config->password);
        Param.insert("recipient_email", Email_Config->recipientEmail);
        Param.insert("recipient_name", Email_Config->recipientName);
        Param.insert("smtp_server", Email_Config->smtpServer);
        Param.insert("smtp_port", Email_Config->smtpPort);
        Param.insert("DELAY_EVENT_MAIL", Email_Param->DELAY_EVENT_MAIL);
        Param.insert("DELAY_ALARM_MAIL", Email_Param->DELAY_ALARM_MAIL);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        emit sendMessage(raw_data, wClient);

        initMaster();
        //        sendEmailParamToWeb();
    } else if (obj["objectName"].toString() == "getEquipmentdata") {
        if (webapp_address.isEmpty()) {
            webapp_address.append(wClient);
        } else {
            Q_FOREACH (QWebSocket *pClient, webapp_address) {
                if (pClient == wClient) {
                    ;
                } else {
                    webapp_address.append(wClient);
                }
            }
        }
        QJsonDocument jsonDoc;
        QJsonObject Param;
        QString raw_data;
        Param.insert("menuID", "updateEquipmentdata");
        Param.insert("SubstationName", SetupEquipment->SubstationName);
        Param.insert("Voltage", SetupEquipment->Voltage);
        Param.insert("TransmissionLineName", SetupEquipment->TransmissionLineName);
        Param.insert("Distance", SetupEquipment->Distance);
        Param.insert("IPaddress", SetupEquipment->IPaddress);
        Param.insert("Brand", SetupEquipment->Brand);
        Param.insert("Model", SetupEquipment->Model);
        Param.insert("SerialNo", SetupEquipment->SerialNo);
        Param.insert("ContractNumber", SetupEquipment->ContractNumber);
        Param.insert("Date", SetupEquipment->Date);
        Param.insert("LFLSerialNo", SetupEquipment->LFLSerialNo);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        emit sendMessage(raw_data, wClient);
        //        qDebug()<<"sendmsgupdateEquipmentdata";
    } else if (obj["objectName"].toString() == "SetupEquipment") {
        //        qDebug() << "SetupEquipment:" << SetupEquipment->Distance
        //                 << " OBJ:" << obj["Distance"].toString();
        SetupEquipment->SubstationName = obj["SubstationName"].toString();
        SetupEquipment->Voltage = obj["Voltage"].toInt();
        SetupEquipment->TransmissionLineName = obj["TransmissionLineName"].toString();
        SetupEquipment->Distance = obj["Distance"].toString();
        SetupEquipment->IPaddress = obj["IPaddress"].toString();
        SetupEquipment->Brand = obj["Brand"].toString();
        SetupEquipment->Model = obj["Model"].toString();
        SetupEquipment->SerialNo = obj["SerialNo"].toString();
        SetupEquipment->ContractNumber = obj["ContractNumber"].toString();
        SetupEquipment->Date = obj["Date"].toString();
        SetupEquipment->LFLSerialNo = obj["LFLSerialNo"].toString();
        networks->ip_address = SetupEquipment->IPaddress;
        emit updateSetupEquipment(SetupEquipment);
        //        qDebug()<< "Sub SetupEquipment" << "SubstationName" <<SetupEquipment->SubstationName << "Voltage"<<SetupEquipment->Voltage<<
        //                   "TransmissionLineName"<<SetupEquipment->TransmissionLineName<< "Distance" << SetupEquipment->Distance << "IPaddress"
        //                << SetupEquipment->IPaddress << "Brand "<<SetupEquipment->Brand << "Model" << SetupEquipment->Model << "SerialNo"<<
        //                   SetupEquipment->SerialNo << "ContractNumber"<< SetupEquipment->ContractNumber << "Date"<<SetupEquipment->Date
        //                <<"LFLSerialNo"<<SetupEquipment->LFLSerialNo;
        QJsonDocument jsonDoc;
        QJsonObject Param;
        QString raw_data;
        Param.insert("menuID", "updateEquipmentdata");
        Param.insert("SubstationName", SetupEquipment->SubstationName);
        Param.insert("Voltage", SetupEquipment->Voltage);
        Param.insert("TransmissionLineName", SetupEquipment->TransmissionLineName);
        Param.insert("Distance", SetupEquipment->Distance);
        Param.insert("IPaddress", SetupEquipment->IPaddress);
        Param.insert("Brand", SetupEquipment->Brand);
        Param.insert("Model", SetupEquipment->Model);
        Param.insert("SerialNo", SetupEquipment->SerialNo);
        Param.insert("ContractNumber", SetupEquipment->ContractNumber);
        Param.insert("Date", SetupEquipment->Date);
        Param.insert("LFLSerialNo", SetupEquipment->LFLSerialNo);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        if (snmp_address->state() == QAbstractSocket::ConnectedState) {
            emit sendMessage(raw_data, snmp_address);
            qDebug() << "updateEquipmentdata: SubstationName";
        } else
            qDebug() << "snmp_address:" << snmp_address->state();

        Q_FOREACH (QWebSocket *pClient, webapp_address) {
            emit sendMessage(raw_data, pClient);
        }

        // qWarning() << "updateEquipmentdata " << raw_data;

        myDatabase->updateParameterEquipment(SetupEquipment->SubstationName, SetupEquipment->Voltage, SetupEquipment->TransmissionLineName, SetupEquipment->Distance, SetupEquipment->IPaddress, SetupEquipment->Brand, SetupEquipment->Model, SetupEquipment->SerialNo, SetupEquipment->ContractNumber, SetupEquipment->Date, SetupEquipment->LFLSerialNo);

        QJsonObject param;
        param.insert("objectName", "UpdateSettingGeneralInfo");
        param.insert("valueVoltage", SetupEquipment->Voltage);
        param.insert("Substation", SetupEquipment->SubstationName);
        param.insert("Direction", SetupEquipment->TransmissionLineName);      // แก้ชื่อ member ให้ตรงของจริง
        param.insert("LineNo", SetupEquipment->LFLSerialNo);

        const QString raw_datas =
                QString::fromUtf8(QJsonDocument(param).toJson(QJsonDocument::Compact));

        qDebug() << "[sendUpdateSettingGeneralInfo]" << raw_datas;

        myDatabase->updateSettingGeneralInfo(raw_datas);

        Param = QJsonObject();
        Param.insert("objectName", "displaySetting");
        Param.insert("fulldistanceText", SetupEquipment->Distance.toDouble());
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        emit settingdisplay(raw_data);

    } else if (obj["objectName"].toString() == "updateSettingNetwork") {
        // qWarning() << "before updateSettingNetwork DEBUG:" << obj;
        Monitor_param->printinfo();
        //            qDebug() << Monitor_param->PLC_DO_ERROR << QString(obj["PLC_DO_ERROR"].toString()).toInt();
        //        qDebug() << "MANUAL_TEST_EVENT DEBUG:" << obj["MANUAL_TEST_EVENT"].toBool() << QString(obj["MANUAL_TEST_EVENT"].toString()).toInt();
        //        qDebug() << "LFL_FAIL DEBUG:" << obj["LFL_FAIL"].toBool() << QString(obj["LFL_FAIL"].toString()).toInt();
        //        qDebug() << "LFL_OPERATE DEBUG:" << obj["LFL_OPERATE"].toBool() << QString(obj["LFL_OPERATE"].toString()).toInt();
        QJsonDocument jsonDoc;
        QJsonObject Param;
        QString raw_data;

        if (obj.contains("PLC_DO_ERROR") && !obj["PLC_DO_ERROR"].isNull()) {
            snmp_param->PLC_DO_ERROR = obj["PLC_DO_ERROR"].toBool();
        }
        if (obj.contains("PLC_DI_ERROR") && !obj["PLC_DI_ERROR"].isNull()) {
            snmp_param->PLC_DI_ERROR = obj["PLC_DI_ERROR"].toBool();
        }
        if (obj.contains("MODULE_HI_SPEED_PHASE_A_ERROR") && !obj["MODULE_HI_SPEED_PHASE_A_ERROR"].isNull()) {
            snmp_param->MODULE_HI_SPEED_PHASE_A_ERROR = obj["MODULE_HI_SPEED_PHASE_A_ERROR"].toBool();
        }
        if (obj.contains("MODULE_HI_SPEED_PHASE_B_ERROR") && !obj["MODULE_HI_SPEED_PHASE_B_ERROR"].isNull()) {
            snmp_param->MODULE_HI_SPEED_PHASE_B_ERROR = obj["MODULE_HI_SPEED_PHASE_B_ERROR"].toBool();
        }
        if (obj.contains("MODULE_HI_SPEED_PHASE_C_ERROR") && !obj["MODULE_HI_SPEED_PHASE_C_ERROR"].isNull()) {
            snmp_param->MODULE_HI_SPEED_PHASE_C_ERROR = obj["MODULE_HI_SPEED_PHASE_C_ERROR"].toBool();
        }
        if (obj.contains("INTERNAL_PHASE_A_ERROR") && !obj["INTERNAL_PHASE_A_ERROR"].isNull()) {
            snmp_param->INTERNAL_PHASE_A_ERROR = obj["INTERNAL_PHASE_A_ERROR"].toBool();
        }
        if (obj.contains("INTERNAL_PHASE_B_ERROR") && !obj["INTERNAL_PHASE_B_ERROR"].isNull()) {
            snmp_param->INTERNAL_PHASE_B_ERROR = obj["INTERNAL_PHASE_B_ERROR"].toBool();
        }
        if (obj.contains("INTERNAL_PHASE_C_ERROR") && !obj["INTERNAL_PHASE_C_ERROR"].isNull()) {
            snmp_param->INTERNAL_PHASE_C_ERROR = obj["INTERNAL_PHASE_C_ERROR"].toBool();
        }
        if (obj.contains("GPS_MODULE_FAIL") && !obj["GPS_MODULE_FAIL"].isNull()) {
            snmp_param->GPS_MODULE_FAIL = obj["GPS_MODULE_FAIL"].toBool();
        }
        if (obj.contains("SYSTEM_INITIAL") && !obj["SYSTEM_INITIAL"].isNull()) {
            snmp_param->SYSTEM_INITIAL = obj["SYSTEM_INITIAL"].toBool();
        }
        if (obj.contains("COMMUNICATION_ERROR") && !obj["COMMUNICATION_ERROR"].isNull()) {
            snmp_param->COMMUNICATION_ERROR = obj["COMMUNICATION_ERROR"].toBool();
        }
        if (obj.contains("RELAY_START_EVENT") && !obj["RELAY_START_EVENT"].isNull()) {
            snmp_param->RELAY_START_EVENT = obj["RELAY_START_EVENT"].toBool();
        }
        if (obj.contains("SURGE_START_EVENT") && !obj["SURGE_START_EVENT"].isNull()) {
            snmp_param->SURGE_START_EVENT = obj["SURGE_START_EVENT"].toBool();
        }
        if (obj.contains("PERIODIC_TEST_EVENT") && !obj["PERIODIC_TEST_EVENT"].isNull()) {
            snmp_param->PERIODIC_TEST_EVENT = obj["PERIODIC_TEST_EVENT"].toBool();
        }
        if (obj.contains("MANUAL_TEST_EVENT") && !obj["MANUAL_TEST_EVENT"].isNull()) {
            snmp_param->MANUAL_TEST_EVENT = obj["MANUAL_TEST_EVENT"].toBool();
        }
        if (obj.contains("LFL_FAIL") && !obj["LFL_FAIL"].isNull()) {
            snmp_param->LFL_FAIL = obj["LFL_FAIL"].toBool();
        }
        if (obj.contains("LFL_OPERATE") && !obj["LFL_OPERATE"].isNull()) {
            snmp_param->LFL_OPERATE = obj["LFL_OPERATE"].toBool();
        }
        qDebug() << "after updateSettingNetwork DEBUG:" << obj;
        snmp_param->printinfo();

        qDebug() << "before networks->ip_address:" << networks->ip_address << "networks->ip_gateway:" << networks->ip_gateway << "obj['ipaddress'].toString():" << obj["ip_address"].toString() << "obj['gateway'].toString()" << obj["ip_gateway"].toString();

        if (obj["ip_address"].toString() != "" && obj["ip_gateway"].toString() != "") {
            networks->ip_address = obj["ip_address"].toString();
            networks->ip_gateway = obj["ip_gateway"].toString();
            SetupEquipment->IPaddress = networks->ip_address;
            qDebug() << "in the middle networks->ip_address:" << networks->ip_address << "networks->ip_gateway:" << networks->ip_gateway << "obj['ipaddress'].toString():" << obj["ip_address"].toString() << "obj['gateway'].toString()" << obj["ip_gateway"].toString();
            Param.insert("objectName", "Network");
            Param.insert("ip_address", networks->ip_address);
            Param.insert("ip_gateway", networks->ip_gateway);
            Param.insert("ip_snmp", networks->ip_snmp);
            Param.insert("ip_timeserver", networks->ip_timeserver);
            jsonDoc.setObject(Param);
            raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
            // qDebug() << "raw_data::" << raw_data;
            if (snmp_address->state() == QAbstractSocket::ConnectedState) {
                emit sendMessage(raw_data, snmp_address);
            } else
                qDebug() << "snmp_address:" << snmp_address->state();
            if (FPGA_address->state() == QAbstractSocket::ConnectedState) {
                emit sendMessage(raw_data, FPGA_address);
            } else
                qDebug() << "FPGA_address:" << FPGA_address->state();
            //        emit sendMessage(raw_data,snmp_address);
            Q_FOREACH (QWebSocket *pClient, Monitor_address) {
                if (pClient->state() == QAbstractSocket::ConnectedState)
                    emit sendMessage(raw_data, pClient);
                else
                    qDebug() << "Monitor_address:" << pClient->state();
            }
            emit sendToVNC(raw_data);
            emit broadcastMessage(raw_data);
            myDatabase->updateParameterEquipment(SetupEquipment->SubstationName, SetupEquipment->Voltage, SetupEquipment->TransmissionLineName, SetupEquipment->Distance, SetupEquipment->IPaddress, SetupEquipment->Brand, SetupEquipment->Model, SetupEquipment->SerialNo, SetupEquipment->ContractNumber, SetupEquipment->Date, SetupEquipment->LFLSerialNo);
            QString forward = "sudo systemctl restart PLCForward.service";
            system(forward.toUtf8());
            //           updateNetwork();
        }
        //       qDebug() << "after networks->ip_address:" << networks->ip_address
        //                << "networks->ip_gateway:" << networks->ip_gateway
        //                << "obj['ipaddress'].toString():" << obj["ipaddress"].toString()
        //                << "obj['gateway'].toString()" << obj["gateway"].toString();
        //       if(obj["gateway"].toString() != ""){
        //           networks->ip_gateway = obj["gateway"].toString();
        //       }

        //       if(obj["ip_address"].toString() != "" && obj["ip_gateway"].toString() != ""){
        //           updateNetwork(networks->dhcpmethod.toInt(), networks->ip_address, networks->subnet, networks->ip_gateway, networks->pridns, networks->secdns, "eth0");
        //       }

        if (obj["ip_snmp"].toString() != "") {
            networks->ip_snmp = obj["ip_snmp"].toString();
            qDebug() << "ip_snmp:" << obj["ip_snmp"].toString();
            updateSnmpipServer(networks->ip_snmp);
            updateSNMP();
            Param.insert("menuID", "snmp_server");
            Param.insert("snmp_ip", networks->ip_snmp);
            jsonDoc.setObject(Param);
            raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
            Q_FOREACH (QWebSocket *pClient, webapp_address) {
                if (pClient->state() == QAbstractSocket::ConnectedState)
                    emit sendMessage(raw_data, pClient);
                else
                    qDebug() << "webapp_address:" << pClient->state();
            }
        }
        qDebug() << "ntpServer" << networks->ip_timeserver << " change to" <<  obj["ntpServer"].toString();
        if (obj["ntpServer"].toString() != "") {
            qDebug() << "ntpServer" << networks->ip_timeserver << " change to" <<  obj["ntpServer"].toString();
            networks->ip_timeserver = obj["ntpServer"].toString();
            networking->setNTPServer(networks->ip_timeserver);
            loopNetworkTimer->start(3000);
            updateNTP();
        }
        *Monitor_param = *snmp_param;

        //       QJsonObject().swap(Param);
        Param.insert("objectName", "TrapsEnabling");  // Name
        Param.insert("ipaddress", networks->ip_address);
        Param.insert("gateway", networks->ip_gateway);
        Param.insert("ip_snmp", networks->ip_snmp);
        Param.insert("ntpServer", networks->ip_timeserver);
        Param.insert("PLC_DO_ERROR", Monitor_param->PLC_DO_ERROR);
        Param.insert("PLC_DI_ERROR", Monitor_param->PLC_DI_ERROR);
        Param.insert("MODULE_HI_SPEED_PHASE_A_ERROR", Monitor_param->MODULE_HI_SPEED_PHASE_A_ERROR);
        Param.insert("MODULE_HI_SPEED_PHASE_B_ERROR", Monitor_param->MODULE_HI_SPEED_PHASE_B_ERROR);
        Param.insert("MODULE_HI_SPEED_PHASE_C_ERROR", Monitor_param->MODULE_HI_SPEED_PHASE_C_ERROR);
        Param.insert("INTERNAL_PHASE_A_ERROR", Monitor_param->INTERNAL_PHASE_A_ERROR);
        Param.insert("INTERNAL_PHASE_B_ERROR", Monitor_param->INTERNAL_PHASE_B_ERROR);
        Param.insert("INTERNAL_PHASE_C_ERROR", Monitor_param->INTERNAL_PHASE_C_ERROR);
        Param.insert("GPS_MODULE_FAIL", Monitor_param->GPS_MODULE_FAIL);
        Param.insert("SYSTEM_INITIAL", Monitor_param->SYSTEM_INITIAL);
        Param.insert("COMMUNICATION_ERROR", Monitor_param->COMMUNICATION_ERROR);
        Param.insert("RELAY_START_EVENT", Monitor_param->RELAY_START_EVENT);
        Param.insert("SURGE_START_EVENT", Monitor_param->SURGE_START_EVENT);
        Param.insert("PERIODIC_TEST_EVENT", Monitor_param->PERIODIC_TEST_EVENT);
        Param.insert("MANUAL_TEST_EVENT", Monitor_param->MANUAL_TEST_EVENT);
        Param.insert("LFL_FAIL", Monitor_param->LFL_FAIL);
        Param.insert("LFL_OPERATE", Monitor_param->LFL_OPERATE);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        qDebug() << "PLC_DO_ERROR raw_data:" << raw_data;
        if (snmp_address->state() == QAbstractSocket::ConnectedState) {
            emit sendMessage(raw_data, snmp_address);
        } else
            qDebug() << "snmp_address:" << snmp_address->state();
        //        emit sendMessage(raw_data,snmp_address);
        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(raw_data, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(raw_data);
        loopNetworkTimer->start(3000);
        //        QString ip,QString gateway, QString snmp, QString ntp, bool PlcDoError, bool PlcDiError, bool ModuleHispeedPhaseAError, bool ModuleHispeedPhaseBError, bool ModuleHispeedPhaseCError, bool modbusPhaseAError, bool modbusPhaseBError, bool modbusPhaseCError, bool GpsModuleFail, bool SystemInital, bool CommunicationError, bool RelayStartEvent, bool surgeStartEvent, bool ReriodicStartEvent, bool ManualTestEvent, bool LFLFail, bool LFLOperate){

    } else if (obj["menuID"].toString() == "getSystemPage") {
        if (webapp_address.isEmpty()) {
            webapp_address.append(wClient);
        } else {
            Q_FOREACH (QWebSocket *pClient, webapp_address) {
                if (pClient == wClient) {
                    ;
                } else {
                    webapp_address.append(wClient);
                }
            }
        }
        QJsonDocument jsonDoc;
        QJsonObject Param;
        QString raw_data;
        Param.insert("menuID", "system");  // Name
        Param.insert("SwVersion", SwVersion);
        Param.insert("HwVersion", HwVersion);
        if (networks->ip_timeserver != "" && networks->location_snmp != "") {
            Param.insert("dateTimeMethod", 1);
        } else
            Param.insert("dateTimeMethod", 0);
        Param.insert("ntpServer", networks->ip_timeserver);
        Param.insert("location", networks->location_snmp);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        emit sendMessage(raw_data, wClient);

        Param.insert("menuID", "program");
        Param.insert("msg", language);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        emit sendMessage(raw_data, wClient);
    } else if (obj["menuID"].toString() == "getEmailPage") {
        if (webapp_address.isEmpty()) {
            webapp_address.append(wClient);
        } else {
            Q_FOREACH (QWebSocket *pClient, webapp_address) {
                if (pClient == wClient) {
                    ;
                } else {
                    webapp_address.append(wClient);
                }
            }
        }
        qDebug() << "getEmailPage send data to website";
        sendEmailParamToWeb();
    } else if (obj["menuID"].toString() == "getFTPPage") {
        if (webapp_address.isEmpty()) {
            webapp_address.append(wClient);
        } else {
            Q_FOREACH (QWebSocket *pClient, webapp_address) {
                if (pClient == wClient) {
                    ;
                } else {
                    webapp_address.append(wClient);
                }
            }
        }
        sendFTPParam();
    } else if (obj["menuID"].toString() == "setLocation") {
        //        QString location = obj["location"].toString();
        networks->location_snmp = obj["location"].toString();
        networking->setNTPServer(networks->ip_timeserver);
        updateLocation();
    } else if (obj["menuID"].toString() == "updateNTPServerss") {
        // //        QString ntpServer = obj["ntpServer"].toString();
        // networks->ip_timeserver = obj["ntpServer"].toString();
        // networking->setNTPServer(networks->ip_timeserver);
        // updateNTP();
        // QJsonDocument jsonDoc;
        // QJsonObject Param;
        // QString raw_data;
        // Param.insert("objectName", "Network");
        // Param.insert("ip_address", networks->ip_address);
        // Param.insert("ip_gateway", networks->ip_gateway);
        // Param.insert("ip_snmp", networks->ip_snmp);
        // Param.insert("ip_timeserver", networks->ip_timeserver);
        // jsonDoc.setObject(Param);
        // raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        // // qDebug() << "raw_data::" << raw_data;
        // if (snmp_address->state() == QAbstractSocket::ConnectedState) {
        //     emit sendMessage(raw_data, snmp_address);
        // } else
        //     qDebug() << "snmp_address:" << snmp_address->state();
        // if (FPGA_address->state() == QAbstractSocket::ConnectedState) {
        //     emit sendMessage(raw_data, FPGA_address);
        // } else
        //     qDebug() << "FPGA_address:" << FPGA_address->state();
        // //        emit sendMessage(raw_data,snmp_address);
        // Q_FOREACH (QWebSocket *pClient, Monitor_address) {
        //     if (pClient->state() == QAbstractSocket::ConnectedState)
        //         emit sendMessage(raw_data, pClient);
        //     else
        //         qDebug() << "Monitor_address:" << pClient->state();
        // }

    } else if (obj["menuID"].toString() == "startdate") {
        QString dateTime = obj["dateTime"].toString();
    } else if (obj["menuID"].toString() == "rebootSystem") {
        QString reboot = QString("reboot");
        system(reboot.toStdString().c_str());
    } else if (obj["menuID"].toString() == "updateFirmware") {
        qDebug() << "updateFirmware";
        QThread::msleep(100);
        updateFirmware();
    } else if (obj["menuID"].toString() == "broadcastLocalTime") {
        if (obj["HwName"].toString() == "OpenPLC") {
            plc_connect = 5;
            qDebug() << "plc_connect assign 3";
        }
    } else if (obj["objectName"].toString() == "gpiotest") {
        if (OpenPLC_address->state() == QAbstractSocket::ConnectedState)
            emit sendMessage(msgs, OpenPLC_address);
        else
            qDebug() << "OpenPLC_address:" << OpenPLC_address->state();
    } else if (obj["objectName"].toString() == "getNanoSec") {
        qDebug() << "getNanoSec:" << obj;
        nanoPhaseASlave = obj["nanoPhaseA"].toString();
        nanoPhaseBSlave = obj["nanoPhaseB"].toString();
        nanoPhaseCSlave = obj["nanoPhaseC"].toString();
    }
    // FPGA
    else if (obj["objectName"].toString() == "adcRawData") {
        // qDebug() << "adcRawData::" << obj;
        // qWarning() << "adcRawData::" << obj;

        testModeRec = "";
        modeName = "";
        QString adc_chanel = QString::number(obj["adc_channel"].toInt());
        QString name = obj["filename"].toString();
        QString url = obj["url"].toString();
        QString date = obj["date"].toString();
        QString timefile = obj["time"].toString();
        QString timestamp = obj["timestamp"].toString();
        QString testMode = obj["testMode"].toString();
        nanosecRec = obj["nanosec"].toString();
        testModeRec = testMode;

        if (testModeRec == "ManualTest") {
            modeName = "Manual";
            interlockPressPattern = false;
        } else if (testModeRec == "Surge") {
            interlockPressPattern = false;
            modeName = "Surge";
        } else if (testModeRec == "RelayTest") {
            interlockPressPattern = false;
            modeName = "Relay";
        } else if (testModeRec == "Periodic") {
            interlockPressPattern = false;
            modeName = "Periodic";
        } else if (testModeRec == "PatternTest") {
            modeName = "Pattern";
        }

        if(modeName == "Manual"){
           // fullstate = true;
        }

        if(sendDIO == false && modeName == "Relay"){
            // qWarning() << "sendDIO:" << sendDIO <<  " modeName:" << modeName;
            return;
        }

        if (adc_chanel == "0") {
            chanelRec = "A";
            nanoPhaseA = obj["nanosec"].toString();
            nanosecRec = obj["nanosec"].toString();
            nanoSecCount++;
        } else if (adc_chanel == "1") {
            nanosecRec = obj["nanosec"].toString();
            nanoPhaseB = obj["nanosec"].toString();
            chanelRec = "B";
            nanoSecCount++;
        } else if (adc_chanel == "2") {
            nanosecRec = obj["nanosec"].toString();
            nanoPhaseC = obj["nanosec"].toString();
            chanelRec = "C";
            nanoSecCount++;
        }
        if (nanoSecCount == 3) {
            QJsonDocument jsonDoc;
            QJsonObject Param;
            QString raw_data;
            Param.insert("objectName", "getNanoSec");  // Name
            Param.insert("nanoPhaseA", nanoPhaseA);
            Param.insert("nanoPhaseB", nanoPhaseB);
            Param.insert("nanoPhaseC", nanoPhaseC);
            jsonDoc.setObject(Param);
            raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
            emit sendToSocketPLC(raw_data);
            nanoSecCount = 0;
        }

        updateEvent = true;
        updateEventCount = 0;
        qDebug() << "adcRawData testModeRec = testMode" << testModeRec << testMode;

        if(testMode == "Surge"){
            if(surge_event_check){
                getRawDataADC(name, url, adc_chanel, timefile, date, timestamp);
            }
            else {
                temp_adc_chanel = adc_chanel;
                temp_name = name;
                temp_url = url;
                temp_date = date;
                temp_timefile = timefile;
                temp_timestamp = timestamp;
                adc_event_check = true;
            }
        }
        else{
          getRawDataADC(name, url, adc_chanel, timefile, date, timestamp);
        }
        // qWarning() << "adcRawData adc_event_check:" << adc_event_check
        //          << " surge_event_check:" << surge_event_check;
        // qWarning() << "temp_url:" << temp_url;
        if (testMode == "Surge") {
            // if(temp_url != ""){
                timeRec = obj["time"].toString();
                dateRec = obj["date"].toString();
                //            nanosecRec = QString::number(obj["nanosec"].toInt());

                QDate date = QDate::fromString(dateRec, "yyyyMMdd");
                QTime time = QTime::fromString(timeRec, "hhmmss");
                QString currentTime = time.toString("hh:mm:ss");
                QString currentDate = date.toString("dd/MM/yyyy");
                OpenPLC_param->SURGE_START_EVENT = true;
                QJsonDocument jsonDoc;
                QJsonObject Param;
                Param.insert("TrapsAlert", "SURGE_START_EVENT_" + chanelRec);
                Param.insert("state", OpenPLC_param->SURGE_START_EVENT);
                Param.insert("time", currentDate + " " + currentTime + "." + nanosecRec);
                jsonDoc.setObject(Param);
                QString raw_datas = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                // qDebug() << "raw_datas:" << raw_datas;
                Q_FOREACH (QWebSocket *pClient, Monitor_address) {
                    if (pClient->state() == QAbstractSocket::ConnectedState)
                        emit sendMessage(raw_datas, pClient);
                    else
                        qDebug() << "Monitor_address:" << pClient->state();
                }
                emit sendToVNC(raw_datas);
                if (snmp_address->state() == QAbstractSocket::ConnectedState)
                    emit sendMessage(raw_datas, snmp_address);
                else
                    qDebug() << "snmp_address:" << snmp_address->state();

                QString currentTimes = time.toString("hh-mm-ss");
                QString currentDates = date.toString("yyyy-MM-dd");
                DateKept = currentDates;
                TimeKept = currentTimes;

                // Surge compatibility fallback.  A later explicit eventRecord
                // will overwrite this with the authoritative event timestamp.
                eventBaseDateSnapshot = currentDates;
                eventBaseTimeSnapshot = currentTimes;
                eventBaseModeSnapshot = QStringLiteral("Surge");

                eventHistory->setEvent("SURGE_START_EVENT_" + chanelRec, currentDate + " " + currentTime + "." + nanosecRec, OpenPLC_param->SURGE_START_EVENT);
                QJsonObject ParamPop;
                ParamPop.insert("objectName", "eventRecord");
                ParamPop.insert("testMode", "Surge");
                QString msg_pop = QJsonDocument(ParamPop).toJson(QJsonDocument::Compact).toStdString().c_str();
                emit sendToVNC(msg_pop);
                Q_FOREACH (QWebSocket *pClient, Monitor_address) {
                    if (pClient->state() == QAbstractSocket::ConnectedState)
                        emit sendMessage(msg_pop, pClient);
                    else
                        qDebug() << "Monitor_address:" << pClient->state();
                }
            // }
            //            qDebug() << "SURGE_START_EVENT_"+chanelRec << "updateEvent" << updateEvent << " updateEventCount" << updateEventCount;
        }
    } else if (obj["objectName"].toString() == "surgeEventDetect") {
        surge_event_check = true;
        interlockPressPattern = false;
        temp_url = obj["remote_url"].toString();
        PositionFromLocal = 0;
        PositionFromRemote = 0;
        local_nanosec = 0;
        remote_nanosec = 0;
        // remote_link = "";
        PositionFromLocal = static_cast<int>(obj["PositionFromLocal"].toDouble());
        PositionFromRemote = static_cast<int>(obj["PositionFromRemote"].toDouble());
        local_nanosec = obj["local_nanosec"].toString().toInt();
        remote_nanosec = obj["remote_nanosec"].toString().toInt();
        remote_link = obj["remote_url"].toString() == remote_link ? "" : obj["remote_url"].toString();
        qDebug() << "surgeEventDetect remote_url:" << obj["remote_url"].toString() << " remote_link" << remote_link;
        qDebug() << "adcRawData_surgeEventDetect:" << PositionFromLocal << PositionFromRemote << local_nanosec << remote_nanosec << obj["local_nanosec"].toString().toInt() << obj["remote_nanosec"].toString().toInt() << obj["remote_nanosec"].toString() << obj["local_nanosec"].toString();
        qWarning() << "adcRawData adc_event_check:" << obj;
        if(adc_event_check){
            getRawDataADC(temp_name, temp_url, temp_adc_chanel, temp_timefile, temp_date, temp_timestamp);
        }

        QJsonObject ParamPop;
        ParamPop.insert("objectName", "eventRecord");
        ParamPop.insert("testMode", "Surge");
        QString msg_pop = QJsonDocument(ParamPop).toJson(QJsonDocument::Compact).toStdString().c_str();
        emit sendToVNC(msg_pop);
        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(msg_pop, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        // qWarning() << "surgeEventDetect adc_event_check:" << adc_event_check
        //          << " surge_event_check:" << surge_event_check;
        qDebug() << "temp_url:" << temp_url;
    }

    else if (obj["objectName"].toString() == "keepAlive") {
        if (obj["HwName"].toString() == "OpenPLC") {
            OpenPLC_count = 0;
        }
        if (obj["HwName"].toString() == "FPGA") {
            FPGA_count = 0;
            // qWarning() << "FPGA_count == 0";
        }
        if (obj["HwName"].toString() == "snmp") {
            snmp_count = 0;
        }
        if (obj["HwName"].toString() == "INPUT_PLC") {
            input_count = 0;
        }
        if (obj["HwName"].toString() == "OpenPLCSever") {
            plcServer_count = 0;
            qDebug() << "receive from slave";
        }
    } else if (obj["objectName"].toString() == "graphPlot") {
        QString url = obj["url"].toString();
        QString filename = obj["filename"].toString();
        QString time = obj["time"].toString();
        getPicturePlot(filename, url, time);
    } else if (obj["objectName"].toString() == "START_TEST") {
        QString cmd = "systemctl start testgpio.service";
        system(cmd.toUtf8());
    } else if (obj["objectName"].toString() == "TERMINATE_TEST") {
        QString cmd = "systemctl stop testgpio.service";
        system(cmd.toUtf8());
    } else if (obj["objectName"].toString() == "START_INPUT_PULSE_READER") {
        QString cmd = "systemctl start gpio_pulse_reader.service";
        system(cmd.toUtf8());
    } else if (obj["objectName"].toString() == "TERMINATE_INPUT_PULSE_READER") {
        QString cmd = "systemctl stop gpio_pulse_reader.service";
        system(cmd.toUtf8());
    } else if (obj["objectName"].toString() == "inputDelayUpdate") {
        delays->delay0 = obj["DELAY0"].toInt();
        delays->delay1 = obj["DELAY1"].toInt();
        delays->delay2 = obj["DELAY2"].toInt();
        delays->delay3 = obj["DELAY3"].toInt();
        delays->delay4 = obj["DELAY4"].toInt();
        delays->delay5 = obj["DELAY5"].toInt();
        delays->delay6 = obj["DELAY6"].toInt();
        delays->delay7 = obj["DELAY7"].toInt();

        if (*temp_delays != *delays) {
            updateDelay();
            *temp_delays = *delays;
            sendDelayToClient(INPUT_PLC_address);
        }

    } else if (obj["objectName"].toString() == "requestInputDelay") {
        sendDelayToClient(wClient);
    } else if (obj["objectName"].toString() == "plc_input_1") {
        delays->delay0 = obj["value"].toInt();

        if (*temp_delays != *delays) {
            updateDelay();
            *temp_delays = *delays;
            sendDelayToClient(INPUT_PLC_address);
        }

    } else if (obj["objectName"].toString() == "plc_input_2") {
        delays->delay1 = obj["value"].toInt();

        if (*temp_delays != *delays) {
            updateDelay();
            *temp_delays = *delays;
            sendDelayToClient(INPUT_PLC_address);
        }
    } else if (obj["objectName"].toString() == "plc_input_3") {
        delays->delay2 = obj["value"].toInt();

        if (*temp_delays != *delays) {
            updateDelay();
            *temp_delays = *delays;
            sendDelayToClient(INPUT_PLC_address);
        }
    } else if (obj["objectName"].toString() == "plc_input_4") {
        delays->delay3 = obj["value"].toInt();

        if (*temp_delays != *delays) {
            updateDelay();
            *temp_delays = *delays;
            sendDelayToClient(INPUT_PLC_address);
        }
    } else if (obj["objectName"].toString() == "plc_input_5") {
        delays->delay4 = obj["value"].toInt();

        if (*temp_delays != *delays) {
            updateDelay();
            *temp_delays = *delays;
            sendDelayToClient(INPUT_PLC_address);
        }
    } else if (obj["objectName"].toString() == "plc_input_6") {
        delays->delay5 = obj["value"].toInt();

        if (*temp_delays != *delays) {
            updateDelay();
            *temp_delays = *delays;
            sendDelayToClient(INPUT_PLC_address);
        }
    } else if (obj["objectName"].toString() == "plc_input_7") {
        delays->delay6 = obj["value"].toInt();

        if (*temp_delays != *delays) {
            updateDelay();
            *temp_delays = *delays;
            sendDelayToClient(INPUT_PLC_address);
        }
    } else if (obj["objectName"].toString() == "plc_input_8") {
        delays->delay7 = obj["value"].toInt();

        if (*temp_delays != *delays) {
            updateDelay();
            *temp_delays = *delays;
            sendDelayToClient(INPUT_PLC_address);
        }
    } else if (obj["objectName"].toString() == "eventRecord") {

        // qWarning() << "eventRecord testModeRec = testMode"
        //            << testModeRec
        //            << obj["testMode"].toString()
        //            << " obj::" << obj;

        timeRec     = obj["time"].toString().trimmed();       // 011147
        dateRec     = obj["date"].toString().trimmed();       // 20260521
        nanosecRec  = obj["nanosec"].toString().trimmed();    // 375965789
        testModeRec = obj["testMode"].toString().trimmed();
        eventText   = obj["event"].toString().trimmed();

        qDebug() << "[eventRecord]"
                 << "dateRec =" << dateRec
                 << "timeRec =" << timeRec
                 << "nanosecRec =" << nanosecRec
                 << "testModeRec =" << testModeRec
                 << "eventText =" << eventText;

        QDate date = QDate::fromString(dateRec, "yyyyMMdd");
        QTime time = QTime::fromString(timeRec, "hhmmss");

        if (!date.isValid() || !time.isValid()) {
            qWarning() << "[eventRecord] invalid date/time"
                       << "dateRec =" << dateRec
                       << "timeRec =" << timeRec;
            return;
        }

        /*
         * รูปแบบที่ต้องการ:
         * date = 21/05/2026
         * time = 01:11:47.375965789
         */
        QString currentDate = date.toString("dd/MM/yyyy");
        QString currentTime = time.toString("hh:mm:ss");

        // ใช้สำหรับ FTP path เท่านั้น
        QString ftpDate = date.toString("yyyy-MM-dd");
        QString ftpTime = time.toString("hh-mm-ss");

        if (nanosecRec.isEmpty()) {
            nanosecRec = "000000000";
        }

        DateKept = ftpDate;
        TimeKept = ftpTime;

        if (testModeRec == "ManualTest") {
            interlockPressPattern = false;
            modeName = "Manual";
        } else if (testModeRec == "Surge") {
            interlockPressPattern = false;
            modeName = "Surge";
        } else if (testModeRec == "RelayTest") {
            interlockPressPattern = false;
            modeName = "Relay";
        } else if (testModeRec == "Periodic") {
            interlockPressPattern = false;
            modeName = "Periodic";
        } else if (testModeRec == "PatternTest") {
            modeName = "Pattern";
        }

        // eventRecord is the authoritative owner of the EVENT base folder.
        // Keep this immutable snapshot separate from Picture/ADC timestamps.
        eventBaseDateSnapshot = ftpDate;
        eventBaseTimeSnapshot = ftpTime;
        eventBaseModeSnapshot = modeName;

        // A new event owns a fresh, single Picture slot.  Clear every verified
        // transaction snapshot here (not on every ScreenPicture) so a late or
        // duplicate ScreenPicture can never erase the already-verified bundle
        // of the current event.
        verifiedEventBundleReady = false;
        verifiedEventCsvLocalPathSnapshot.clear();
        verifiedPictureLocalPathSnapshot.clear();
        verifiedPatternLocalPathSnapshot.clear();
        verifiedFtpEventCsvRemotePath.clear();
        verifiedFtpPictureRemotePath.clear();
        verifiedFtpPatternRemotePath.clear();
        fullPicPATH.clear();
        fullPathPattern.clear();

        // A new event also owns one mail publication slot.  This is separate
        // from FTP/SYNC ownership because verified local files are sufficient
        // for the mail payload, while SYNC still requires verified remote FTP
        // paths.
        eventMailPublished = false;
        eventMailPublishedEventId.clear();

        QString pictureOwnerNanosec = nanosecRec;
        if (pictureOwnerNanosec.isEmpty())
            pictureOwnerNanosec = QStringLiteral("000000000");
        const QString pictureEventId =
            QStringLiteral("%1_%2_%3_%4")
                .arg(modeName, date.toString(QStringLiteral("yyyyMMdd")),
                     time.toString(QStringLiteral("hhmmss")), pictureOwnerNanosec);
        resetEventPictureOwnership(pictureEventId, modeName, ftpDate, ftpTime);

        qDebug() << "[EVENT-BASE][CAPTURE]"
                 << "mode=" << eventBaseModeSnapshot
                 << "date=" << eventBaseDateSnapshot
                 << "time=" << eventBaseTimeSnapshot
                 << "pictureEventId=" << activePictureEventId
                 << "source=eventRecord";

        /*
         * ส่งเข้า setEvent เป็น:
         * "21/05/2026 01:11:47.375965789"
         *
         * เพราะ setEvent ของคุณ split ด้วยช่องว่าง:
         * words[0] = วันที่
         * words[1] = เวลา
         */
        QString timestamp = currentDate + " " + currentTime + "." + nanosecRec;

        eventHistory->setEvent(eventText, timestamp, true);

        qDebug() << "[eventRecord] timestamp =" << timestamp
                 << "eventText =" << eventText;

        qDebug() << "obj" << obj;
    }

    // slave master
    else if (obj["objectName"].toString() == "selectUsers") {
        // qWarning() << "obj receive -->" << obj;
        // qWarning() << "before selectUsers::" << obj["userType"].toString() << "masterLFL" << masterLFL << masterIP << slaveIP << standAlone;
        // QString temp_slave = slaveIP;
        // QString temp_master = masterIP;
        // if(!standAlone){
        //     if(masterLFL != obj["userType"].toString()){
        //         slaveIP = temp_master;
        //         masterIP = temp_slave;
        //     }
        //     //qWarning() << "swap IP each other";
        // }
        // else{
        //     if(masterLFLBkup != obj["userType"].toString()){
        //         slaveIP = temp_master;
        //         masterIP = temp_slave;
        //     }
        // }
        // masterLFL = obj["userType"].toString();
        // qWarning() << "after selectUsers::" << obj["userType"].toString() << "masterLFL" << masterLFL << masterIP << slaveIP << standAlone;
        // emit updateMasterMode(masterLFL, masterIP, slaveIP);
        // initMaster();

        // QJsonDocument jsonDoc;
        // QJsonObject Param;
        // Param.insert("objectName", "selectUserServer");
        // Param.insert("userType", masterLFL);
        // jsonDoc.setObject(Param);
        // QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        // emit sendToSocketPLC(raw_data);
        // standAlone = false;
        QString temp_slave = slaveIP;
        QString temp_master = masterIP;
        if(!standAlone){
            if(masterLFL != obj["userType"].toString()){
                slaveIP = temp_master;
                masterIP = temp_slave;
            }
            //qWarning() << "swap IP each other";
        }
        else{
            if(masterLFL != obj["userType"].toString()){
                if(masterLFLBkup != obj["userType"].toString()){
                    slaveIP = temp_master;
                    masterIP = temp_slave;
                }
            }
        }
        masterLFL = obj["userType"].toString();
        // qWarning() << "after selectUsers::" << obj["userType"].toString() << "masterLFL" << masterLFL << masterIP << slaveIP << standAlone;
        //qWarning() << "selectUserFromWeb::" << obj["userType"].toString() << "masterLFL" << masterLFL << masterIP << slaveIP << standAlone;
        emit updateMasterMode(masterLFL, masterIP, slaveIP);
        initMaster();
        QJsonDocument jsonDoc;
        QJsonObject Param;
        Param.insert("objectName", "selectUserServer");
        Param.insert("userType", masterLFL);
        Param.insert("ip_master", masterIP);
        Param.insert("ip_slave", slaveIP);
        jsonDoc.setObject(Param);
        QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        emit sendToSocketPLC(raw_data);
        // qWarning() << "send to another plc selectUserServer raw_data" << raw_data;
        standAlone = false;
    }
    // slave master
    else if (obj["objectName"].toString() == "selectUserServer") {
        // qWarning() << "selectUserServer::" << obj["userType"].toString() << "masterLFL" << masterLFL << masterIP << slaveIP;
        if (obj["userType"].toString() == "MASTER") {
            masterLFL = "SLAVE";
        }
        if (obj["userType"].toString() == "SLAVE") {
            masterLFL = "MASTER";
        }

        masterIP = obj["ip_master"].toString();
        slaveIP = obj["ip_slave"].toString();

        // QString temp_slave = slaveIP;
        // QString temp_master = masterIP;
        // qWarning() << "before swap IP slaveIP " << slaveIP << " masterIP " << masterIP
        //            << " temp_slave " << temp_slave << " temp_master " << temp_master;
        // if(!standAlone){
        //     if(masterLFL != obj["userType"].toString()){
        //         slaveIP = temp_master;
        //         masterIP = temp_slave;
        //     }
        //     //qWarning() << "swap IP each other";
        // }
        // else{
        //     if(masterLFLBkup != obj["userType"].toString()){
        //         slaveIP = temp_master;
        //         masterIP = temp_slave;
        //     }
        // }
        // qWarning() << "after swap IP slaveIP " << slaveIP << " masterIP " << masterIP
        //            << " temp_slave " << temp_slave << " temp_master " << temp_master;
        // qWarning() << "swap IP each other Mode-->"  << masterLFL;
        emit updateMasterMode(masterLFL, masterIP, slaveIP);
        initMaster();
        standAlone = false;
    } else if (obj["objectName"].toString() == "selectUserFromWeb") {
        //        if(obj["userType"].toString() == "MASTER"){
        //            masterLFL = "SLAVE";
        //        }
        //        if(obj["userType"].toString() == "SLAVE"){
        //            masterLFL = "MASTER";
        //        }
        slaveIP = obj["ip_slave"].toString();
        masterIP = obj["ip_master"].toString();
        QString temp_slave = slaveIP;
        QString temp_master = masterIP;
        if(!standAlone){
            if(masterLFL != obj["userType"].toString()){
                slaveIP = temp_master;
                masterIP = temp_slave;
            }
            //qWarning() << "swap IP each other";
        }
        else{
            if(masterLFL != obj["userType"].toString()){
                if(masterLFLBkup != obj["userType"].toString()){
                    slaveIP = temp_master;
                    masterIP = temp_slave;
                }
            }
        }
        masterLFL = obj["userType"].toString();
        //qWarning() << "selectUserFromWeb::" << obj["userType"].toString() << "masterLFL" << masterLFL << masterIP << slaveIP << standAlone;
        emit updateMasterMode(masterLFL, masterIP, slaveIP);
        initMaster();
        QJsonDocument jsonDoc;
        QJsonObject Param;
        Param.insert("objectName", "selectUserServer");
        Param.insert("userType", masterLFL);
        Param.insert("ip_master", masterIP);
        Param.insert("ip_slave", slaveIP);
        jsonDoc.setObject(Param);
        QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        emit sendToSocketPLC(raw_data);
        standAlone = false;
    }
    //---------------------------- DataStorage page ---------------------------------//
    else if (obj["objectName"].toString() == "SearchName") {
        // bool Sort = QJsonValue(command["Sort"]).toBool();
        QString Name = QJsonValue(command["text"]).toString();
        QString categories = QJsonValue(command["categories"]).toString();
        //        qDebug()<<"iiiiixxxxxNAME"<<Name<<categories;
        emit searchByName(Name, categories);

    } else if (obj["objectName"].toString() == "SearchDate") {
        // qDebug()<<"iiiiixxxxx";
        QString Date = QJsonValue(command["text"]).toString();
        QString categories = QJsonValue(command["categories"]).toString();
        //        qDebug()<<"iiiiixxxxxDate"<<Date<<categories;
        emit searchByDate(Date, categories);
    } else if (obj["objectName"].toString() == "sortnamePattern") {
        bool Sort = QJsonValue(command["Sort"]).toBool();
        QString categories = QJsonValue(command["categories"]).toString();
        //         qDebug()<<"sortnamePatterniiiii"<<Sort<<categories;
        emit sortnamePattern(Sort, categories);
    } else if (obj["objectName"].toString() == "sortdatePattern") {
        bool Sort = QJsonValue(command["Sort"]).toBool();
        QString categories = QJsonValue(command["categories"]).toString();
        //         qDebug()<<"sortdatePatterniiiii"<<Sort<<categories;
        emit sortdatePattern(Sort, categories);
    }

    else if (obj["objectName"].toString() == "ButtonPattern") {
                // qDebug()<<"testppopop_ButtonPattern";
        QString category = obj["category"].toString();
        QString onclicked = obj["Onclicked"].toString();
        QString filename = obj["filename"].toString();
        QString event_datetime = obj["event_datetime"].toString();
                // qDebug()<<"ButtonPatterntestpp"<<category<<onclicked<<filename<<event_datetime;
        if (onclicked == "NEW") {
                        qDebug()<<"NEW";
            emit NewPatternFile(category, filename);
        } else if (onclicked == "OPEN") {
            open_interlock = true;
                        // qWarning()<<"OPEN" << " category:" << category << " filename:" << filename;

            // QString combinedData = QString(
            //                            "{"
            //                            "\"objectName\":\"combinedDataPhaseA\","
            //                            "\"margin\":%1,"
            //                            "\"valueVoltage\":%2,"
            //                            "\"focusIndex\":%3,"
            //                            "\"PHASE\":\"%4\""
            //                            "}")
            //                            .arg(100)
            //                            .arg(0)
            //                            .arg(-1)
            //                            .arg("A");
            // emit updataListOfMarginA(combinedData);

            // combinedData = QString(
            //                    "{"
            //                    "\"objectName\":\"combinedDataPhaseB\","
            //                    "\"margin\":%1,"
            //                    "\"valueVoltage\":%2,"
            //                    "\"focusIndex\":%3,"
            //                    "\"PHASE\":\"%4\""
            //                    "}")
            //                    .arg(100)
            //                    .arg(0)
            //                    .arg(-1)
            //                    .arg("B");
            // emit updataListOfMarginB(combinedData);

            // combinedData = QString(
            //                    "{"
            //                    "\"objectName\":\"combinedDataPhaseC\","
            //                    "\"margin\":%1,"
            //                    "\"valueVoltage\":%2,"
            //                    "\"focusIndex\":%3,"
            //                    "\"PHASE\":\"%4\""
            //                    "}")
            //                    .arg(100)
            //                    .arg(0)
            //                    .arg(-1)
            //                    .arg("C");
            // emit updataListOfMarginC(combinedData);

            // combinedData = QString(
            //                    "{"
            //                    "\"objectName\":\"combinedDataPhaseA\","
            //                    "\"margin\":%1,"
            //                    "\"valueVoltage\":%2,"
            //                    "\"focusIndex\":%3,"
            //                    "\"PHASE\":\"%4\""
            //                    "}")
            //                    .arg(1)
            //                    .arg(0)
            //                    .arg(0)
            //                    .arg("A");

            // qDebug() << "Combined Data for Phase A:" << combinedData;
            // emit UpdateMarginSettingParameter(combinedData);


            // combinedData = QString(
            //                    "{"
            //                    "\"objectName\":\"combinedDataPhaseB\","
            //                    "\"margin\":%1,"
            //                    "\"valueVoltage\":%2,"
            //                    "\"focusIndex\":%3,"
            //                    "\"PHASE\":\"%4\""
            //                    "}")
            //                    .arg(1)
            //                    .arg(0)
            //                    .arg(0)
            //                    .arg("B");
            // emit UpdateMarginSettingParameter(combinedData);

            // combinedData = QString(
            //                    "{"
            //                    "\"objectName\":\"combinedDataPhaseC\","
            //                    "\"margin\":%1,"
            //                    "\"valueVoltage\":%2,"
            //                    "\"focusIndex\":%3,"
            //                    "\"PHASE\":\"%4\""
            //                    "}")
            //                    .arg(1)
            //                    .arg(0)
            //                    .arg(0)
            //                    .arg("C");
            // emit UpdateMarginSettingParameter(combinedData);

            emit getandSenddataFromCSV(filename, category, event_datetime);
        } else if (onclicked == "SAVE") {
                        qDebug()<<"SAVE";
            for (int i = 0; i < distanceArrayAPatternbkup.size(); i++) {
                myDatabase->distAPat.push_back(distanceArrayAPatternbkup[i]);
                myDatabase->voltAPat.push_back(voltageArrayAPatternbkup[i]);
            }

            for (int i = 0; i < distanceArrayBPatternbkup.size(); i++) {
                myDatabase->distBPat.push_back(distanceArrayBPatternbkup[i]);
                myDatabase->voltBPat.push_back(voltageArrayBPatternbkup[i]);
            }

            for (int i = 0; i < distanceArrayCPatternbkup.size(); i++) {
                myDatabase->distCPat.push_back(distanceArrayCPatternbkup[i]);
                myDatabase->voltCPat.push_back(voltageArrayCPatternbkup[i]);
            }
            qDebug() << "voltageArrayAPattern:" << voltageArrayAPattern.size()
                     << " voltageArrayBPattern:" << voltageArrayBPattern.size()
                     << " voltageArrayCPattern:" << voltageArrayCPattern.size();
            emit SavePatternFile(category, filename, event_datetime);

            // แปลง JSON เป็น QJsonDocument
            QJsonDocument doc = QJsonDocument::fromJson(msgs.toUtf8());
            if (!doc.isObject()) {
                qWarning() << "Invalid JSON";
            }


            QJsonObject root = doc.object();

            // qWarning() << "marginCountQJsonDocument:::" << root;
            QJsonArray valuesA,valuesB,valuesC;
            QString marginCountA,marginCountB,marginCountC;
            QString nameOfPattern;
            if (root.contains("marginAItems") && root["marginAItems"].isArray()) {
                valuesA = root["marginAItems"].toArray();
                marginCountA = root["countMarginA"].toString().trimmed().isEmpty() ? "1" : root["countMarginA"].toString().trimmed();
                // for (const QJsonValue &val : arr) {
                //     if (val.isObject()) {
                //         QJsonObject obj = val.toObject();
                //         int v = obj.value("valueMarginA").toInt();
                //         valuesA.append(v);
                //     }
                // }
            }
            if (root.contains("marginBItems") && root["marginBItems"].isArray()) {
                valuesB = root["marginBItems"].toArray();
                marginCountB = root["countMarginB"].toString().trimmed().isEmpty() ? "1" : root["countMarginB"].toString().trimmed();
                // for (const QJsonValue &val : arr) {
                //     if (val.isObject()) {
                //         QJsonObject obj = val.toObject();
                //         int v = obj.value("valueMarginB").toInt();
                //         valuesB.append(v);
                //     }
                // }
            }
            if (root.contains("marginCItems") && root["marginCItems"].isArray()) {
                valuesC = root["marginCItems"].toArray();
                marginCountC = root["countMarginC"].toString().trimmed().isEmpty() ? "1" : root["countMarginC"].toString().trimmed();

                // for (const QJsonValue &val : arr) {
                //     if (val.isObject()) {
                //         QJsonObject obj = val.toObject();
                //         int v = obj.value("valueMarginC").toInt();
                //         valuesC.append(v);
                //     }
                // }
            }

            if(root.contains("marginAItems") && root.contains("marginBItems") && root.contains("marginCItems")){
                nameOfPattern = root["filename"].toString();

                QString datefile = event_datetime.left(10);    // "2025-02-18"
                QString timefile = event_datetime.mid(11, 8);  // "03:30:17"
                timefile.replace(":", "-");                    // Replace ":" with "-" to match the folder format

                QString filePath = QString(EVENT_PATH + "%1/%2/%3/%4_Margin").arg("Pattern").arg(datefile).arg(timefile).arg(nameOfPattern);

                qDebug() << "filename" << nameOfPattern << "All valueMarginA =" << valuesA << " All valueMarginB =" << valuesB << " All valueMarginC =" << valuesC;
                myDatabase->writeMarginCSV(filePath,marginCountA,marginCountB,marginCountC,valuesA,valuesB,valuesC);
            }




        } else if (onclicked == "DELETE") {
            emit deleteCsvFileAndFolder(filename, category, event_datetime);
            //            qDebug()<<"DELETE";
        }

    } else if (obj["menuID"].toString() == "UpdateRecipientgmail") {
        QString Updategmail = obj["gmail"].toString();
        int id = obj["id"].toInt();
        //        qDebug()<<"UpdateRecipientgmail"<<Updategmail<<id;
        emit UpdateRecipientgmail(Updategmail, id);
    } else if (obj["menuID"].toString() == "RemoveRecipientgmail") {
        QString Removegmail = obj["gmail"].toString();
        emit RemoveRecipientgmail(Removegmail);
        //        qDebug()<<"RemoveRecipientgmail"<<Removegmail;
    } else if (obj["menuID"].toString() == "NewRecipientgmail") {
        QString Newgmail = obj["gmail"].toString();
        emit NewRecipientgmail(Newgmail);
        //        qDebug()<<"NewRecipientgmail"<<Newgmail;
    } else if (obj["menuID"].toString() == "UpdateuserNameandPassword") {
        QString UserName = obj["userName"].toString();
        QString Password = obj["password"].toString();
        int UserLevel = obj["UserLevel"].toInt();
        int id = obj["id"].toInt();
        qDebug() << "UpdateuserNameandPasswordoooo" << UserName << Password << UserLevel << id;
        emit UpdateuserNameandPassword(UserName, Password, UserLevel, id);
        //        qDebug()<<"UpdateRecipientgmail"<<Updategmail<<id;

    } else if (obj["menuID"].toString() == "RemoveuserNameandPassword") {
        QString UserName = obj["userName"].toString();
        QString Password = obj["password"].toString();
        int UserLevel = obj["UserLevel"].toInt();
        qDebug() << "RemoveuserNameandPasswordoooo" << UserName << Password << UserLevel;
        emit RemoveuserNameandPassword(UserName, Password, UserLevel);
        //        qDebug()<<"RemoveRecipientgmail"<<Removegmail;
    } else if (obj["menuID"].toString() == "NewuserNameandPassword") {
        QString UserName = obj["userName"].toString();
        QString Password = obj["password"].toString();
        int UserLevel = obj["UserLevel"].toInt();
        emit NewuserNameandPassword(UserName, Password, UserLevel);
        //        qDebug()<<"NewRecipientgmail"<<Newgmail;
    } else if (obj["objectName"].toString() == "ScreenPicture") {
        const QString incomingLink = obj["link"].toString().trimmed();
        const QString incomingFileName = QFileInfo(obj["fileName"].toString().trimmed()).fileName();
        const QString incomingModeRaw = obj["mode"].toString().trimmed();
        const QString senderAddress = wClient ? wClient->peerAddress().toString()
                                              : QStringLiteral("<unknown>");
        const quint16 senderPort = wClient ? wClient->peerPort() : 0;

        auto normalizedScreenMode = [](QString value) {
            value = value.trimmed();
            if (value.compare(QStringLiteral("ManualTest"), Qt::CaseInsensitive) == 0)
                return QStringLiteral("Manual");
            if (value.compare(QStringLiteral("RelayTest"), Qt::CaseInsensitive) == 0)
                return QStringLiteral("Relay");
            if (value.compare(QStringLiteral("Periodic"), Qt::CaseInsensitive) == 0)
                return QStringLiteral("Periodic");
            if (value.compare(QStringLiteral("Surge"), Qt::CaseInsensitive) == 0)
                return QStringLiteral("Surge");
            if (value.compare(QStringLiteral("PatternTest"), Qt::CaseInsensitive) == 0)
                return QStringLiteral("Pattern");
            return value;
        };

        const QString incomingMode = normalizedScreenMode(incomingModeRaw);
        const QString ownerMode = normalizedScreenMode(eventBaseModeSnapshot);

        qWarning() << "[SCREEN-RX]"
                   << "eventId=" << activePictureEventId
                   << "sender=" << senderAddress << senderPort
                   << "file=" << incomingFileName
                   << "mode=" << incomingMode
                   << "state=" << eventPictureStateName();

        // ScreenPicture is valid only while an eventRecord-owned Picture slot is
        // open.  This removes the old DateKept/TimeKept behavior where a late
        // picture could be attached to an already-completed event.
        if (!eventPictureActive || eventPictureState == EventPictureIdle ||
            eventPictureState == EventPictureClosed) {
            qWarning() << "[SCREEN-RX][DROP]"
                       << "reason=NO_ACTIVE_EVENT_OWNER"
                       << "eventId=" << activePictureEventId
                       << "sender=" << senderAddress << senderPort
                       << "file=" << incomingFileName;
            return;
        }

        if (incomingFileName.isEmpty() || incomingLink.isEmpty()) {
            qWarning() << "[SCREEN-RX][DROP]"
                       << "reason=EMPTY_LINK_OR_FILENAME"
                       << "eventId=" << activePictureEventId
                       << "sender=" << senderAddress << senderPort;
            return;
        }

        if (!incomingMode.isEmpty() && !ownerMode.isEmpty() &&
            incomingMode.compare(ownerMode, Qt::CaseInsensitive) != 0) {
            qWarning() << "[SCREEN-RX][DROP]"
                       << "reason=EVENT_MODE_MISMATCH"
                       << "eventId=" << activePictureEventId
                       << "ownerMode=" << ownerMode
                       << "incomingMode=" << incomingMode
                       << "file=" << incomingFileName;
            return;
        }

        if (eventPictureState != EventPictureWaiting) {
            const bool exactDuplicate =
                !acceptedPictureName.isEmpty() &&
                acceptedPictureName.compare(incomingFileName, Qt::CaseInsensitive) == 0;

            qWarning() << "[SCREEN-RX][DROP-DUPLICATE]"
                       << "eventId=" << activePictureEventId
                       << "sender=" << senderAddress << senderPort
                       << "incoming=" << incomingFileName
                       << "accepted=" << acceptedPictureName
                       << "state=" << eventPictureStateName()
                       << "exactDuplicate=" << exactDuplicate
                       << "reason=" << (exactDuplicate
                                                ? QStringLiteral("SAME_PICTURE_ALREADY_RESERVED_OR_COMMITTED")
                                                : QStringLiteral("EVENT_ALREADY_OWNS_ANOTHER_PICTURE"));

            QJsonObject duplicateDetail;
            duplicateDetail.insert(QStringLiteral("eventId"), activePictureEventId);
            duplicateDetail.insert(QStringLiteral("senderAddress"), senderAddress);
            duplicateDetail.insert(QStringLiteral("senderPort"), static_cast<int>(senderPort));
            duplicateDetail.insert(QStringLiteral("incomingFileName"), incomingFileName);
            duplicateDetail.insert(QStringLiteral("acceptedPictureName"), acceptedPictureName);
            duplicateDetail.insert(QStringLiteral("pictureState"), eventPictureStateName());
            duplicateDetail.insert(QStringLiteral("exactDuplicate"), exactDuplicate);
            auditEventStep(QStringLiteral("SCREEN_DUPLICATE_FILTER"),
                           QStringLiteral("Reject duplicate/late ScreenPicture"),
                           QStringLiteral("SKIP"),
                           QStringLiteral("Ignored ScreenPicture because this event already reserved or committed its canonical Picture"),
                           senderAddress,
                           acceptedPictureName,
                           duplicateDetail);
            return;
        }

        // First valid ScreenPicture wins the event Picture slot.  The 500 ms
        // timer is now a delay only, not a "latest wins" debounce: later
        // requests cannot overwrite this reservation.
        acceptedPictureName = incomingFileName;
        eventPictureState = EventPictureProcessing;

        pendingScreenLink = incomingLink;
        pendingScreenFileName = incomingFileName;
        pendingScreenMode = incomingModeRaw;
        pendingScreenSenderAddress = senderAddress;
        pendingScreenSenderPort = senderPort;

        ensureEventAuditContext(pendingScreenMode,
                                eventBaseDateSnapshot,
                                eventBaseTimeSnapshot,
                                QStringLiteral("SCREEN_PICTURE_EVENT"),
                                pendingScreenFileName);
        QJsonObject auditDetail;
        auditDetail.insert(QStringLiteral("eventId"), activePictureEventId);
        auditDetail.insert(QStringLiteral("link"), pendingScreenLink);
        auditDetail.insert(QStringLiteral("fileName"), pendingScreenFileName);
        auditDetail.insert(QStringLiteral("mode"), pendingScreenMode);
        auditDetail.insert(QStringLiteral("senderAddress"), pendingScreenSenderAddress);
        auditDetail.insert(QStringLiteral("senderPort"), static_cast<int>(pendingScreenSenderPort));
        auditDetail.insert(QStringLiteral("pictureState"), eventPictureStateName());
        auditDetail.insert(QStringLiteral("delayMs"), 500);
        auditEventStep(QStringLiteral("SCREEN_REQUEST_RECEIVED"),
                       QStringLiteral("Screen picture request received"),
                       QStringLiteral("PASS"),
                       QStringLiteral("Accepted the first ScreenPicture for this event and reserved the canonical Picture slot"),
                       pendingScreenLink,
                       QStringLiteral("PLCServer::getScreenPictureandSave"),
                       auditDetail);

        screenPictureDelayTimer->start(500);
        qDebug() << "ScreenPicture screenPictureDelayTimer"
                 << "eventId=" << activePictureEventId
                 << "accepted=" << acceptedPictureName;
    }
    //////////////////////// Email Param SNMP
    else if (obj["objectName"].toString() == "SNMP_EMAIL_ENABLE") {
        qDebug() << "SNMP_EMAIL_ENABLE DEBUG" << obj;
        Email_Param->PLC_DO_ERROR_MAIL = obj["PLC_DO_ACTIVE_MAIL"].toInt();
        Email_Param->PLC_DI_ERROR_MAIL = obj["PLC_DI_ACTIVE_MAIL"].toInt();
        Email_Param->MODULE_HI_SPEED_PHASE_A_ERROR_MAIL = obj["MODULE_HI_SPEED_PHASE_A_ERROR_MAIL"].toInt();
        Email_Param->MODULE_HI_SPEED_PHASE_B_ERROR_MAIL = obj["MODULE_HI_SPEED_PHASE_B_ERROR_MAIL"].toInt();
        Email_Param->MODULE_HI_SPEED_PHASE_C_ERROR_MAIL = obj["MODULE_HI_SPEED_PHASE_C_ERROR_MAIL"].toInt();
        Email_Param->INTERNAL_PHASE_A_ERROR_MAIL = obj["INTERNAL_PHASE_A_ERROR_MAIL"].toInt();
        Email_Param->INTERNAL_PHASE_B_ERROR_MAIL = obj["INTERNAL_PHASE_B_ERROR_MAIL"].toInt();
        Email_Param->INTERNAL_PHASE_C_ERROR_MAIL = obj["INTERNAL_PHASE_C_ERROR_MAIL"].toInt();
        Email_Param->GPS_MODULE_FAIL_MAIL = obj["GPS_MODULE_FAIL_MAIL"].toInt();
        Email_Param->SYSTEM_INITIAL_MAIL = obj["SYSTEM_INITIAL_MAIL"].toInt();
        Email_Param->COMMUNICATION_ERROR_MAIL = obj["COMMUNICATION_ERROR_MAIL"].toInt();
        Email_Param->RELAY_START_EVENT_MAIL = obj["RELAY_START_EVENT_MAIL"].toInt();
        Email_Param->SURGE_START_EVENT_MAIL = obj["SURGE_START_EVENT_MAIL"].toInt();
        Email_Param->PERIODIC_TEST_EVENT_MAIL = obj["PERIODIC_TEST_EVENT_MAIL"].toInt();
        Email_Param->MANUAL_TEST_EVENT_MAIL = obj["MANUAL_TEST_EVENT_MAIL"].toInt();
        Email_Param->LFL_FAIL_MAIL = obj["LFL_FAIL_MAIL"].toInt();
        Email_Param->LFL_OPERATE_MAIL = obj["LFL_OPERATE_MAIL"].toInt();
        //        Email_Param->DELAY_EVENT_MAIL = obj["DELAY_MAIL"].toInt();
        //        Email_Param->DELAY_ALARM_MAIL = obj["DELAY_MAIL"].toInt();
        updateEmailParam();
    } else if (obj["objectName"].toString() == "InforSettingVoltage") {
        qDebug() << "valueVoltageDEBUG";
        myDatabase->updateSettingInfo(msgs);
    } else if (obj["objectName"].toString() == "ValueSubstation") {
        qDebug() << "ValueSubstationDEBUG";
        myDatabase->updateSettingInfo(msgs);
    } else if (obj["objectName"].toString() == "valueDirection") {
        qDebug() << "valueDirectionDEBUG";
        myDatabase->updateSettingInfo(msgs);
    } else if (obj["objectName"].toString() == "valueLineNo") {
        qDebug() << "valueLineNoDEBUG";
        myDatabase->updateSettingInfo(msgs);
    } else if (obj["objectName"].toString() == "UpdateFTPParameter") {
        FTP_Param_->FTP_IP = obj["ftpIP"].toString();
        FTP_Param_->USERNAME = obj["username"].toString();
        FTP_Param_->PASSWORD = obj["password"].toString();
        FTP_Param_->PERIODIC_FILE = obj["periodic_file"].toString();
        FTP_Param_->RELAY_FILE = obj["relay_file"].toString();
        FTP_Param_->SURGE_FILE = obj["surge_file"].toString();
        FTP_Param_->MANUAL_FILE = obj["manual_file"].toString();
        FTP_Param_->PATTERN_FILE = obj["pattern_file"].toString();

        updateFTPServer();
        sendFTPParam();

        emit preiodicSetting();
        emit updateFTPParam(FTP_Param_);
    } else if (obj["objectName"].toString() == "login") {
        QString user = obj["username"].toString();
        QString password = obj["password"].toString();
        qDebug() << "loginjfodijfd" << user << password;
        // qWarning() << "loginjfodijfd" << user << password;
        emit getuserlogin(user, password, wClient);
        //        if (vnc_address == wClient) {
        //            QJsonDocument jsonDoc;
        //            QJsonObject Param;
        //            Param.insert("objectName", "Pop-up");
        //            Param.insert("msg", "REMOTE FROM VNC");
        //            Param.insert("state", true);
        //            jsonDoc.setObject(Param);
        //            QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        //            Q_FOREACH (QWebSocket *pClient, Monitor_address) {
        //                if (vnc_address != pClient) {
        //                    if (pClient->state() == QAbstractSocket::ConnectedState)
        //                        emit sendMessage(raw_data, pClient);
        //                    else
        //                        qDebug() << "Monitor_address:" << pClient->state();
        //                }
        //            }

        //            Param.insert("objectName", "Pop-up");
        //            Param.insert("msg", "REMOTE TO " + masterLFL);
        //            Param.insert("state", false);
        //            jsonDoc.setObject(Param);
        //            raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        //            if (wClient->state() == QAbstractSocket::ConnectedState)
        //                emit sendMessage(raw_data, wClient);
        //            else
        //                qDebug() << "Monitor_address:" << wClient->state();
        //            loopLogoutVNC->start(1000 * 6 * 10 * 5);
        //        }
    } else if (obj["objectName"].toString() == "logout") {
        if (vnc_address == wClient) {
            stopThread5 = true;
            stopThread4 = true;
            QJsonDocument jsonDoc;
            QJsonObject Param;
            Param.insert("objectName", "Pop-up");
            Param.insert("msg", "disable");
            Param.insert("state", false);
            jsonDoc.setObject(Param);
            QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
            Q_FOREACH (QWebSocket *pClient, Monitor_address) {
                if (vnc_address != pClient) {
                    if (pClient->state() == QAbstractSocket::ConnectedState)
                        emit sendMessage(raw_data, pClient);
                    else
                        qDebug() << "Monitor_address:" << pClient->state();
                }
            }

            Param.empty();
            Param.insert("objectName", "Pop-up");
            Param.insert("msg", "Logging out");
            Param.insert("state", true);
            jsonDoc.setObject(Param);
            raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
            emit sendToVNC(raw_data);
            Q_FOREACH (QWebSocket *pClient, Monitor_address) {
                if (vnc_address == pClient) {
                    if (pClient->state() == QAbstractSocket::ConnectedState)
                        emit sendMessage(raw_data, pClient);
                    else
                        qDebug() << "Monitor_address:" << pClient->state();
                }
            }
        }
    } else if (obj["objectName"].toString() == "VNClogin") {
        stopThread5 = false;
        // qWarning() << "stopThread5 is " << stopThread5;
        // qWarning() << "objectName : change alive" << obj;
        qDebug() << "objectName : alive";
        QJsonDocument jsonDoc;
        QJsonObject Param;
        // Param.insert("objectName", "Pop-up");
        // Param.insert("msg", "disable");
        // Param.insert("state", false);
        // jsonDoc.setObject(Param);
        QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        // if (wClient->state() == QAbstractSocket::ConnectedState)
        //     emit sendMessage(raw_data, wClient);
        // else
        //     qDebug() << "wClient:" << wClient->state();

        jsonDoc = QJsonDocument();
        Param = QJsonObject();
        raw_data = QString();
        Param.empty();
        Param.insert("objectName", "Pop-up");
        Param.insert("msg", "REMOTE FROM VNC");
        Param.insert("state", true);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (vnc_address != pClient) {
                if (pClient->state() == QAbstractSocket::ConnectedState){
                    emit sendMessage(raw_data, pClient);
                    qDebug() << "vnc_address != pClient" << raw_data << " stopThread5:" << stopThread5;
                }
                else
                    qDebug() << "Monitor_address:" << pClient->state();
            }
        }

        jsonDoc = QJsonDocument();
        Param = QJsonObject();
        raw_data = QString();
        Param.empty();
        Param.insert("objectName", "Pop-up");
        Param.insert("msg", "disable");
        Param.insert("state", false);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        emit sendToVNC(raw_data);
        // stopThread5 = true;
        int retnew = pthread_create( & idThread5, NULL, ThreadFunc5, this);
        if (retnew == 0) {
            qDebug() << ("ThreadFunc5 created successfully.\n");
        } else {
            qDebug() << ("ThreadFunc5 not created.\n");
        }
    }
    else if (obj["objectName"].toString() == "VNClogout") {
        stopThread5 = true;
        qDebug() << "VNClogout << " << stopThread5;
    }
    else if (obj["objectName"].toString() == "alive") {
        // qWarning() << "alive" << obj;
        stopThread4 = true;
        change_monitor = false;
        qDebug() << "alive << " << stopThread4;
    }
    else if (obj["menuID"].toString() == "program") {
        qDebug() << "language:" << language << obj;
        language = obj["msg"].toInt();
        selectProgram(language);
        updateProgram();
    } else if (obj["menuID"].toString() == "uploadCSVTower") {
        emit uploadCSVTower();
    } else if (obj["objectName"].toString() == "saveDataTaging") {
        int num_list = obj["num_listA"].toInt();
        float distance = static_cast<float>(obj["Distance"].toString().toDouble());
        QString detail = obj["Detail"].toString();
        //        qDebug()<< "saveDataTagingA_uihsfuihdf" << num_list << distance<< detail;
        emit updateTableDataTagging(num_list, distance, detail);
    } else if (obj["objectName"].toString() == "deleteMysql") {
        int num_list = obj["num_listA"].toInt();
        qDebug() << "deleteTableDataTagging---------" << num_list;
        emit deleteTableDataTagging(num_list);
    } else if (obj["objectName"].toString() == "selectUserChange") {
        standAlone = false;
        // qWarning() << "selectUserChange::" << obj << standAlone;
        QString mode = obj["userType"].toString();
        if(mode == "MASTER"){
            masterLFL = "SLAVE";
        }
        else{
            masterLFL = "MASTER";
        }
        QJsonDocument jsonDoc;
        QJsonObject Param;
        Param.insert("objectName", "selectUserChange");
        Param.insert("userType", masterLFL);
        Param.insert("ip_master", obj["ip_master"].toString());
        Param.insert("ip_slave", obj["ip_slave"].toString());
        if (masterLFL == "MASTER") {
            Param.insert("RemoteTOMonitor", "REMOTE TO SLAVE");
        } else if (masterLFL == "SLAVE") {
            Param.insert("RemoteTOMonitor", "REMOTE TO MASTER");
        }

        QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();

        qDebug() << "objectName selectUserChange sendtomonitors" << raw_data;
        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState && pClient == Monitor_address.at(0)) {
                emit sendMessage(raw_data, pClient);
                qDebug() << "selectUserChange sendtomonitors";
            } else
                qDebug() << "Monitor_address:" << pClient->state();
        }
    } else if (obj["objectName"].toString() == "reset_ip") {
        qDebug() << "reset_ip" << reset_ip;
        if (!reset_ip) {
            networks->dhcpmethod = "0";
            networks->ip_address = "192.168.1.2";
            networks->subnet = "255.255.255.0";
            networks->ip_gateway = "192.168.1.254";
            networks->pridns = "8.8.8.8";
            networks->secdns = "8.8.4.4";
            networks->ip_timeserver = "192.168.1.1";
            networking->setNTPServer(networks->ip_timeserver);
            updateNTP();
            masterLFL = "MASTER";
            masterIP = networks->ip_address;
            slaveIP = "192.168.1.5";

            SetupEquipment->IPaddress = networks->ip_address;
            emit updateSetupEquipment(SetupEquipment);
            //            QJsonDocument jsonDoc;
            //            QJsonObject Param;
            //            QString raw_data;
            //            Param.insert("menuID", "updateEquipmentdata");
            //            Param.insert("SubstationName", SetupEquipment->SubstationName);
            //            Param.insert("Voltage", SetupEquipment->Voltage);
            //            Param.insert("TransmissionLineName", SetupEquipment->TransmissionLineName);
            //            Param.insert("Distance", SetupEquipment->Distance);
            //            Param.insert("IPaddress", SetupEquipment->IPaddress);
            //            Param.insert("Brand", SetupEquipment->Brand);
            //            Param.insert("Model", SetupEquipment->Model);
            //            Param.insert("SerialNo", SetupEquipment->SerialNo);
            //            Param.insert("ContractNumber", SetupEquipment->ContractNumber);
            //            Param.insert("Date", SetupEquipment->Date);
            //            Param.insert("LFLSerialNo", SetupEquipment->LFLSerialNo);
            //            jsonDoc.setObject(Param);
            //            raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
            //            if (snmp_address->state() == QAbstractSocket::ConnectedState) {
            //                emit sendMessage(raw_data, snmp_address);
            //                qDebug() << "updateEquipmentdata: SubstationName";
            //            } else
            //                qDebug() << "snmp_address:" << snmp_address->state();

            //            Q_FOREACH (QWebSocket *pClient, webapp_address) {
            //                emit sendMessage(raw_data, pClient);
            //            }

            myDatabase->updateParameterEquipment(SetupEquipment->SubstationName, SetupEquipment->Voltage, SetupEquipment->TransmissionLineName, SetupEquipment->Distance, SetupEquipment->IPaddress, SetupEquipment->Brand, SetupEquipment->Model, SetupEquipment->SerialNo, SetupEquipment->ContractNumber, SetupEquipment->Date, SetupEquipment->LFLSerialNo);

            emit updateMasterMode(masterLFL, masterIP, slaveIP);
            loopNetworkTimer->start(3000);
            QString forward = "sudo systemctl restart PLCForward.service";
            system(forward.toUtf8());
            reset_ip = true;
        }

    } else if (obj["objectName"].toString() == "LineFail") {
        int str = obj["rangeoflfl"].toInt();
        //        bool ok;
        lenghtFLF = str;
        qDebug() << "str:" << str << " lenghtFLF:" << lenghtFLF << obj;
        myDatabase->updateRangeLFL(QString::number(str));
        QJsonDocument jsonDoc;
        QJsonObject Param;
        Param.insert("objectName", "LineFails");
        Param.insert("rangeoflfl", lenghtFLF);
        jsonDoc.setObject(Param);
        QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(raw_data, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(raw_data);
    }
    else if(obj["objectName"].toString().toUpper() == "STANDALONE") {
        plcServer_count = 0;
        masterLFLBkup = masterLFL;
        masterLFL = "STANDALONE";
        emit updateMasterMode(masterLFL, masterIP, slaveIP);
        initMaster();
        // QJsonDocument jsonDoc;
        // QJsonObject Param;
        // Param.insert("objectName", "LineFails");
        // Param.insert("rangeoflfl", lenghtFLF);
        // jsonDoc.setObject(Param);
        // QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        // Q_FOREACH (QWebSocket *pClient, webapp_address) {
        //     if (pClient->state() == QAbstractSocket::ConnectedState)
        //         emit sendMessage(raw_data, pClient);
        //     else
        //         qDebug() << "Monitor_address:" << pClient->state();
        // }
        standAlone = true;
        //qWarning() << "motor rec:" << obj["objectName"].toString().toLower() << masterLFL << masterIP << slaveIP << standAlone;
    } else if (obj["menuID"].toString() == "RelayTest") {
        // qWarning() << "menuID RelayTest";
        sendDIO = true;
    } else if(obj["menuID"].toString() == "RelayTestStop"){
        // qWarning() << "menuID RelayTestStop";
        sendDIO = false;
    }
    else {
        // qWarning() << "::::else::::" << msgs;
    }
}
