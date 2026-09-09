#include "PLCServer.h"

#include <algorithm>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QStorageInfo>
#include <QSysInfo>
#include <QTextStream>
#include <QThread>

namespace {

QString auditReadFirstLine(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    return QString::fromUtf8(file.readLine()).trimmed();
}

qint64 auditReadProcKb(const QByteArray &fileName, const QByteArray &key)
{
    QFile file(QString::fromLatin1(fileName));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return -1;

    while (!file.atEnd()) {
        const QByteArray line = file.readLine();
        if (!line.startsWith(key))
            continue;

        const QList<QByteArray> parts = line.simplified().split(' ');
        if (parts.size() >= 2)
            return parts.at(1).toLongLong();
    }

    return -1;
}

QString auditSafeToken(QString value, const QString &fallback)
{
    value = value.trimmed();
    value.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._#-]")), QStringLiteral("_"));
    while (value.contains(QStringLiteral("..")))
        value.replace(QStringLiteral(".."), QStringLiteral("_"));

    if (value.isEmpty() || value == QStringLiteral(".") || value == QStringLiteral(".."))
        value = fallback;

    return value.left(180);
}

QString socketStateText(const QWebSocket *socket)
{
    if (!socket)
        return QStringLiteral("NULL");

    switch (socket->state()) {
    case QAbstractSocket::UnconnectedState: return QStringLiteral("UNCONNECTED");
    case QAbstractSocket::HostLookupState:  return QStringLiteral("HOST_LOOKUP");
    case QAbstractSocket::ConnectingState:  return QStringLiteral("CONNECTING");
    case QAbstractSocket::ConnectedState:   return QStringLiteral("CONNECTED");
    case QAbstractSocket::BoundState:       return QStringLiteral("BOUND");
    case QAbstractSocket::ClosingState:     return QStringLiteral("CLOSING");
    case QAbstractSocket::ListeningState:   return QStringLiteral("LISTENING");
    }
    return QStringLiteral("UNKNOWN");
}

QJsonValue auditPassValue(const QString &status)
{
    const QString s = status.trimmed().toUpper();
    if (s == QStringLiteral("PASS") || s == QStringLiteral("SUCCESS"))
        return QJsonValue(true);
    if (s == QStringLiteral("FAIL") || s == QStringLiteral("BLOCK") || s == QStringLiteral("ABORTED"))
        return QJsonValue(false);
    return QJsonValue(QJsonValue::Null);
}

int expectedAuditOrder(const QString &code)
{
    static const QStringList order = {
        QStringLiteral("EVENT_START"),
        QStringLiteral("EVENT_MODE_RESOLVED"),
        QStringLiteral("DATA_PHASE_A"),
        QStringLiteral("DATA_PHASE_B"),
        QStringLiteral("DATA_PHASE_C"),
        QStringLiteral("CSV_LOCAL_READY"),
        QStringLiteral("SCREEN_REQUEST_RECEIVED"),
        QStringLiteral("SCREEN_DUPLICATE_FILTER"),
        QStringLiteral("PICTURE_DIRECTORY_READY"),
        QStringLiteral("PICTURE_DOWNLOAD"),
        QStringLiteral("PICTURE_RESOLVED"),
        QStringLiteral("CSV_RESOLVED"),
        QStringLiteral("VERIFY_STAGE1_PICTURE"),
        QStringLiteral("VERIFY_STAGE1_CSV"),
        QStringLiteral("VERIFY_STAGE1_GATE"),
        QStringLiteral("VERIFY_STAGE2_PICTURE"),
        QStringLiteral("VERIFY_STAGE2_CSV"),
        QStringLiteral("VERIFY_STAGE2_GATE"),
        QStringLiteral("PATTERN_OPTIONAL"),
        QStringLiteral("PRE_FTP_GATE"),
        QStringLiteral("FTP_REMOTE_DIR"),
        QStringLiteral("FTP_EVENT_CSV"),
        QStringLiteral("FTP_PICTURE"),
        QStringLiteral("FTP_PATTERN"),
        QStringLiteral("FTP_BUNDLE"),
        QStringLiteral("FINAL_LOCAL_VERIFY"),
        QStringLiteral("SENDMAIL_NOTIFICATION"),
        QStringLiteral("LOOP_WAIT_PIC"),
        QStringLiteral("SYNC_VERIFY"),
        QStringLiteral("SYNC_PUBLISH"),
        QStringLiteral("EVENT_COMPLETE")
    };

    const int idx = order.indexOf(code);
    return idx < 0 ? 999 : idx + 1;
}

bool expectedAuditRequired(const QString &code)
{
    // Pattern is intentionally optional. Unknown extension steps default to
    // optional so a future diagnostic step cannot accidentally become a
    // mandatory production gate merely because the web UI does not know it.
    if (code == QStringLiteral("PATTERN_OPTIONAL") ||
        code == QStringLiteral("FTP_PATTERN") ||
        code == QStringLiteral("SCREEN_DUPLICATE_FILTER")) {
        return false;
    }

    return expectedAuditOrder(code) != 999;
}

QString auditMessageTypeForCode(const QString &code)
{
    if (code == QStringLiteral("EVENT_START"))
        return QStringLiteral("EVENT_START");
    if (code == QStringLiteral("EVENT_COMPLETE"))
        return QStringLiteral("EVENT_COMPLETE");
    return QStringLiteral("STEP");
}

QJsonArray expectedAuditChecklist()
{
    struct Item { const char *code; const char *name; bool required; };
    static const Item items[] = {
        {"EVENT_START", "Event received/start", true},
        {"EVENT_MODE_RESOLVED", "Resolve event mode and identity", true},
        {"DATA_PHASE_A", "Process/store Phase A event data", true},
        {"DATA_PHASE_B", "Process/store Phase B event data", true},
        {"DATA_PHASE_C", "Process/store Phase C event data", true},
        {"CSV_LOCAL_READY", "Event CSV created and physically available", true},
        {"SCREEN_REQUEST_RECEIVED", "Screen picture request received", true},
        {"SCREEN_DUPLICATE_FILTER", "Reject duplicate/late ScreenPicture", false},
        {"PICTURE_DIRECTORY_READY", "Picture event directory created", true},
        {"PICTURE_DOWNLOAD", "Picture download/re-download and physical check", true},
        {"PICTURE_RESOLVED", "Resolve exact local Picture path", true},
        {"CSV_RESOLVED", "Resolve exact local Event CSV path", true},
        {"VERIFY_STAGE1_PICTURE", "Picture verification stage #1", true},
        {"VERIFY_STAGE1_CSV", "Event CSV verification stage #1", true},
        {"VERIFY_STAGE1_GATE", "Mandatory verification stage #1 gate", true},
        {"VERIFY_STAGE2_PICTURE", "Picture stability verification stage #2", true},
        {"VERIFY_STAGE2_CSV", "Event CSV stability verification stage #2", true},
        {"VERIFY_STAGE2_GATE", "Mandatory verification stage #2 gate", true},
        {"PATTERN_OPTIONAL", "Optional Pattern verification", false},
        {"PRE_FTP_GATE", "Final mandatory gate before FTP", true},
        {"FTP_REMOTE_DIR", "Prepare FTP remote event directory", true},
        {"FTP_EVENT_CSV", "Upload and download-back verify Event CSV", true},
        {"FTP_PICTURE", "Upload and download-back verify Picture", true},
        {"FTP_PATTERN", "Upload optional Pattern", false},
        {"FTP_BUNDLE", "Mandatory FTP bundle verification", true},
        {"FINAL_LOCAL_VERIFY", "Final local verification after FTP", true},
        {"SENDMAIL_NOTIFICATION", "Publish verified sendMail payload", true},
        {"LOOP_WAIT_PIC", "Prepare verified legacy SYNC path payload", true},
        {"SYNC_VERIFY", "Final SYNC verification", true},
        {"SYNC_PUBLISH", "Publish SYNC to monitor/VNC/SNMP", true},
        {"EVENT_COMPLETE", "Event processing complete", true}
    };

    QJsonArray array;
    for (int i = 0; i < int(sizeof(items) / sizeof(items[0])); ++i) {
        QJsonObject item;
        item.insert(QStringLiteral("order"), i + 1);
        item.insert(QStringLiteral("processCode"), QString::fromLatin1(items[i].code));
        item.insert(QStringLiteral("processName"), QString::fromLatin1(items[i].name));
        item.insert(QStringLiteral("mandatory"), items[i].required);
        item.insert(QStringLiteral("required"), items[i].required); // schema-v1 compatibility
        array.append(item);
    }
    return array;
}

} // namespace

void PLCServer::ensureEventAuditContext(const QString &mode,
                                        const QString &eventDate,
                                        const QString &eventTime,
                                        const QString &eventName,
                                        const QString &identityHint)
{
    if (eventAuditActive)
        return;

    startEventAudit(mode, eventDate, eventTime, eventName, identityHint);
}

void PLCServer::startEventAudit(const QString &mode,
                                const QString &eventDate,
                                const QString &eventTime,
                                const QString &eventName,
                                const QString &identityHint)
{
    if (eventAuditActive) {
        finishEventAudit(false,
                         QStringLiteral("New event started before previous audit completed; previous event marked ABORTED"));
    }

    QString normalizedMode = mode.trimmed();
    if (normalizedMode.isEmpty())
        normalizedMode = QStringLiteral("Unknown");

    QString date = eventDate.trimmed();
    QString time = eventTime.trimmed();
    date.replace(QLatin1Char('/'), QLatin1Char('-'));
    time.replace(QLatin1Char(':'), QLatin1Char('-'));

    const QDate parsedDate = QDate::fromString(date, QStringLiteral("yyyy-MM-dd"));
    const QTime parsedTime = QTime::fromString(time.left(8), QStringLiteral("HH-mm-ss"));
    if (!parsedDate.isValid())
        date = QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
    if (!parsedTime.isValid())
        time = QTime::currentTime().toString(QStringLiteral("HH-mm-ss"));
    else
        time = parsedTime.toString(QStringLiteral("HH-mm-ss"));

    QString hint = identityHint.trimmed();
    if (hint.isEmpty())
        hint = QString::number(QDateTime::currentMSecsSinceEpoch());

    eventAuditId = auditSafeToken(
        normalizedMode + QStringLiteral("_") +
        date.remove(QLatin1Char('-')) + QStringLiteral("_") +
        time.remove(QLatin1Char('-')) + QStringLiteral("_") + hint,
        QStringLiteral("event_%1").arg(QDateTime::currentMSecsSinceEpoch()));

    eventAuditMode = normalizedMode;
    eventAuditName = eventName.trimmed().isEmpty() ? normalizedMode : eventName.trimmed();
    eventAuditDate = parsedDate.isValid() ? parsedDate.toString(QStringLiteral("yyyy-MM-dd"))
                                          : QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
    eventAuditTime = parsedTime.isValid() ? parsedTime.toString(QStringLiteral("HH-mm-ss"))
                                          : QTime::currentTime().toString(QStringLiteral("HH-mm-ss"));
    eventAuditStartedMs = QDateTime::currentMSecsSinceEpoch();
    eventAuditSequence = 0;
    eventAuditPassCount = 0;
    eventAuditFailCount = 0;
    eventAuditRetryCount = 0;
    eventAuditBlockCount = 0;
    eventAuditInfoCount = 0;
    eventAuditSkipCount = 0;
    eventAuditStartCount = 0;
    eventAuditActive = true;
    eventAuditLatestSteps.clear();

    const QString auditDir = QDir(EVENT_PATH).filePath(
        QStringLiteral("Audit/") + eventAuditDate);
    QDir().mkpath(auditDir);
    eventAuditLogPath = QDir(auditDir).filePath(eventAuditId + QStringLiteral(".jsonl"));

    QJsonObject detail;
    detail.insert(QStringLiteral("identityHint"), identityHint);
    detail.insert(QStringLiteral("auditLogPath"), eventAuditLogPath);
    detail.insert(QStringLiteral("checklist"), expectedAuditChecklist());

    auditEventStep(QStringLiteral("EVENT_START"),
                   QStringLiteral("Event received/start"),
                   QStringLiteral("START"),
                   QStringLiteral("Created event audit context and checklist"),
                   QStringLiteral("PLC event detector"),
                   QStringLiteral("PLCServer event pipeline"),
                   detail);
}

QJsonObject PLCServer::eventAuditMachineSnapshot() const
{
    QJsonObject machine;
    machine.insert(QStringLiteral("hostName"), QHostInfo::localHostName());
    machine.insert(QStringLiteral("kernelType"), QSysInfo::kernelType());
    machine.insert(QStringLiteral("kernelVersion"), QSysInfo::kernelVersion());
    machine.insert(QStringLiteral("productType"), QSysInfo::productType());
    machine.insert(QStringLiteral("productVersion"), QSysInfo::productVersion());
    machine.insert(QStringLiteral("cpuArch"), QSysInfo::currentCpuArchitecture());
    machine.insert(QStringLiteral("appPid"), double(QCoreApplication::applicationPid()));
    machine.insert(QStringLiteral("softwareVersion"), QStringLiteral(SwVersion));
    machine.insert(QStringLiteral("hardwareVersion"), QStringLiteral(HwVersion));

    const qint64 vmRssKb = auditReadProcKb("/proc/self/status", "VmRSS:");
    const qint64 memTotalKb = auditReadProcKb("/proc/meminfo", "MemTotal:");
    const qint64 memAvailableKb = auditReadProcKb("/proc/meminfo", "MemAvailable:");
    if (vmRssKb >= 0) machine.insert(QStringLiteral("processRssKb"), double(vmRssKb));
    if (memTotalKb >= 0) machine.insert(QStringLiteral("memoryTotalKb"), double(memTotalKb));
    if (memAvailableKb >= 0) machine.insert(QStringLiteral("memoryAvailableKb"), double(memAvailableKb));

    const QString uptimeLine = auditReadFirstLine(QStringLiteral("/proc/uptime"));
    if (!uptimeLine.isEmpty()) {
        const QStringList parts = uptimeLine.split(QLatin1Char(' '), QString::SkipEmptyParts);
        if (!parts.isEmpty()) machine.insert(QStringLiteral("systemUptimeSec"), parts.first().toDouble());
    }
    const QString loadLine = auditReadFirstLine(QStringLiteral("/proc/loadavg"));
    if (!loadLine.isEmpty()) {
        const QStringList parts = loadLine.split(QLatin1Char(' '), QString::SkipEmptyParts);
        QJsonObject load;
        if (parts.size() > 0) load.insert(QStringLiteral("load1"), parts.at(0).toDouble());
        if (parts.size() > 1) load.insert(QStringLiteral("load5"), parts.at(1).toDouble());
        if (parts.size() > 2) load.insert(QStringLiteral("load15"), parts.at(2).toDouble());
        machine.insert(QStringLiteral("loadAverage"), load);
    }

    QJsonArray networkAddresses;
    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : interfaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp) || (iface.flags() & QNetworkInterface::IsLoopBack))
            continue;
        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            const QHostAddress address = entry.ip();
            if (address.isNull())
                continue;
            QJsonObject addr;
            addr.insert(QStringLiteral("interface"), iface.humanReadableName());
            addr.insert(QStringLiteral("address"), address.toString());
            addr.insert(QStringLiteral("prefixLength"), entry.prefixLength());
            networkAddresses.append(addr);
        }
    }
    machine.insert(QStringLiteral("networkAddresses"), networkAddresses);

    QStorageInfo storage(PIC_PATH);
    if (!storage.isValid() || !storage.isReady())
        storage = QStorageInfo(QStringLiteral("/mnt/sdcard"));
    if (storage.isValid() && storage.isReady()) {
        QJsonObject disk;
        disk.insert(QStringLiteral("rootPath"), storage.rootPath());
        disk.insert(QStringLiteral("device"), QString::fromUtf8(storage.device()));
        disk.insert(QStringLiteral("fileSystemType"), QString::fromUtf8(storage.fileSystemType()));
        disk.insert(QStringLiteral("bytesTotal"), double(storage.bytesTotal()));
        disk.insert(QStringLiteral("bytesAvailable"), double(storage.bytesAvailable()));
        disk.insert(QStringLiteral("readOnly"), storage.isReadOnly());
        machine.insert(QStringLiteral("eventStorage"), disk);
    }

    QJsonObject sockets;
    sockets.insert(QStringLiteral("webClients"), webapp_address.size());
    sockets.insert(QStringLiteral("monitorClients"), Monitor_address.size());
    sockets.insert(QStringLiteral("snmp"), socketStateText(snmp_address));
    sockets.insert(QStringLiteral("openPLC"), socketStateText(OpenPLC_address));
    sockets.insert(QStringLiteral("fpga"), socketStateText(FPGA_address));
    sockets.insert(QStringLiteral("inputPLC"), socketStateText(INPUT_PLC_address));
    machine.insert(QStringLiteral("connections"), sockets);

    if (SetupEquipment) {
        QJsonObject equipment;
        equipment.insert(QStringLiteral("substation"), SetupEquipment->SubstationName);
        equipment.insert(QStringLiteral("line"), SetupEquipment->TransmissionLineName);
        equipment.insert(QStringLiteral("deviceIp"), SetupEquipment->IPaddress);
        equipment.insert(QStringLiteral("brand"), SetupEquipment->Brand);
        equipment.insert(QStringLiteral("model"), SetupEquipment->Model);
        equipment.insert(QStringLiteral("serialNo"), SetupEquipment->SerialNo);
        machine.insert(QStringLiteral("equipment"), equipment);
    }

    return machine;
}

QJsonObject PLCServer::eventAuditRuntimeSnapshot() const
{
    // Lightweight per-step state.  Static machine/equipment/network identity is
    // published on EVENT_START and SNAPSHOT instead of being duplicated in
    // every WebSocket step.
    QJsonObject runtime;

    const qint64 vmRssKb = auditReadProcKb("/proc/self/status", "VmRSS:");
    const qint64 memAvailableKb = auditReadProcKb("/proc/meminfo", "MemAvailable:");
    if (vmRssKb >= 0)
        runtime.insert(QStringLiteral("processRssKb"), double(vmRssKb));
    if (memAvailableKb >= 0)
        runtime.insert(QStringLiteral("memoryAvailableKb"), double(memAvailableKb));

    const QString loadLine = auditReadFirstLine(QStringLiteral("/proc/loadavg"));
    if (!loadLine.isEmpty()) {
        const QStringList parts = loadLine.split(QLatin1Char(' '), QString::SkipEmptyParts);
        if (!parts.isEmpty())
            runtime.insert(QStringLiteral("load1"), parts.at(0).toDouble());
    }

    QStorageInfo storage(PIC_PATH);
    if (!storage.isValid() || !storage.isReady())
        storage = QStorageInfo(QStringLiteral("/mnt/sdcard"));
    if (storage.isValid() && storage.isReady())
        runtime.insert(QStringLiteral("eventStorageBytesAvailable"), double(storage.bytesAvailable()));

    runtime.insert(QStringLiteral("webClients"), webapp_address.size());
    runtime.insert(QStringLiteral("monitorClients"), Monitor_address.size());
    runtime.insert(QStringLiteral("snmp"), socketStateText(snmp_address));
    runtime.insert(QStringLiteral("openPLC"), socketStateText(OpenPLC_address));
    runtime.insert(QStringLiteral("fpga"), socketStateText(FPGA_address));
    runtime.insert(QStringLiteral("inputPLC"), socketStateText(INPUT_PLC_address));

    return runtime;
}

void PLCServer::appendEventAuditLocal(const QJsonObject &record)
{
    if (eventAuditLogPath.trimmed().isEmpty())
        return;

    QFile file(eventAuditLogPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qWarning() << "[EVENT-AUDIT] cannot append local audit log:" << eventAuditLogPath
                   << file.errorString();
        return;
    }

    file.write(QJsonDocument(record).toJson(QJsonDocument::Compact));
    file.write("\n");
    file.flush();
    file.close();
}

void PLCServer::sendEventAuditToWeb(const QJsonObject &record)
{
    // Never touch QWebSocket objects from a worker thread.  The audit system is
    // an observer only; publishing is queued to the PLCServer/QWebSocket owner
    // thread and never waits for a browser acknowledgement.
    if (QThread::currentThread() != thread()) {
        const QJsonObject copy = record;
        QMetaObject::invokeMethod(this, [this, copy]() {
            sendEventAuditToWeb(copy);
        }, Qt::QueuedConnection);
        return;
    }

    const QString raw = QString::fromUtf8(QJsonDocument(record).toJson(QJsonDocument::Compact));
    Q_FOREACH (QWebSocket *client, webapp_address) {
        if (client && client->state() == QAbstractSocket::ConnectedState)
            emit sendMessage(raw, client);
    }
}

void PLCServer::auditEventStep(const QString &processCode,
                               const QString &processName,
                               const QString &status,
                               const QString &description,
                               const QString &source,
                               const QString &destination,
                               const QJsonObject &details)
{
    if (!eventAuditActive) {
        startEventAudit(modeName,
                        pictureEventDateSnapshot.isEmpty() ? DateKept : pictureEventDateSnapshot,
                        pictureEventTimeSnapshot.isEmpty() ? TimeKept : pictureEventTimeSnapshot,
                        modeName,
                        QStringLiteral("auto_%1").arg(QDateTime::currentMSecsSinceEpoch()));
    }

    ++eventAuditSequence;

    QJsonObject step;
    step.insert(QStringLiteral("objectName"), QStringLiteral("EVENT_PROCESS_AUDIT"));
    step.insert(QStringLiteral("messageType"), auditMessageTypeForCode(processCode));
    step.insert(QStringLiteral("schemaVersion"), 2);
    step.insert(QStringLiteral("eventId"), eventAuditId);
    step.insert(QStringLiteral("eventMode"), eventAuditMode);
    step.insert(QStringLiteral("eventName"), eventAuditName);
    step.insert(QStringLiteral("eventDate"), eventAuditDate);
    step.insert(QStringLiteral("eventTime"), eventAuditTime);
    step.insert(QStringLiteral("active"), processCode == QStringLiteral("EVENT_COMPLETE") ? false : eventAuditActive);
    step.insert(QStringLiteral("auditLogPath"), eventAuditLogPath);
    step.insert(QStringLiteral("sequence"), eventAuditSequence);
    step.insert(QStringLiteral("expectedOrder"), expectedAuditOrder(processCode));
    step.insert(QStringLiteral("processCode"), processCode);
    step.insert(QStringLiteral("processName"), processName);
    step.insert(QStringLiteral("mandatory"), expectedAuditRequired(processCode));
    step.insert(QStringLiteral("status"), status.trimmed().toUpper());
    step.insert(QStringLiteral("passed"), auditPassValue(status));
    step.insert(QStringLiteral("description"), description);
    step.insert(QStringLiteral("source"), source);
    step.insert(QStringLiteral("destination"), destination);
    step.insert(QStringLiteral("timestamp"), QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    step.insert(QStringLiteral("elapsedMs"), double(QDateTime::currentMSecsSinceEpoch() - eventAuditStartedMs));
    step.insert(QStringLiteral("details"), details);
    step.insert(QStringLiteral("runtime"), eventAuditRuntimeSnapshot());
    if (processCode == QStringLiteral("EVENT_START"))
        step.insert(QStringLiteral("machine"), eventAuditMachineSnapshot());

    const QString normalizedStatus = status.trimmed().toUpper();
    if (normalizedStatus == QStringLiteral("PASS") || normalizedStatus == QStringLiteral("SUCCESS")) ++eventAuditPassCount;
    else if (normalizedStatus == QStringLiteral("FAIL")) ++eventAuditFailCount;
    else if (normalizedStatus == QStringLiteral("RETRY")) ++eventAuditRetryCount;
    else if (normalizedStatus == QStringLiteral("BLOCK") || normalizedStatus == QStringLiteral("ABORTED")) ++eventAuditBlockCount;
    else if (normalizedStatus == QStringLiteral("INFO")) ++eventAuditInfoCount;
    else if (normalizedStatus == QStringLiteral("SKIP")) ++eventAuditSkipCount;
    else if (normalizedStatus == QStringLiteral("START")) ++eventAuditStartCount;

    eventAuditLatestSteps.insert(processCode, step);
    appendEventAuditLocal(step);
    sendEventAuditToWeb(step);

    qDebug() << "[EVENT-AUDIT]"
               << eventAuditId
               << processCode
               << status.trimmed().toUpper()
               << description;
}

void PLCServer::auditEventDataPhase(const QString &phase,
                                    const QString &fileTimeStamp,
                                    const QJsonArray &distanceData,
                                    const QJsonArray &voltageData)
{
    // Pattern generation is not an Event CSV + Picture publish transaction and
    // therefore does not use this mandatory event checklist.
    if (modeName.compare(QStringLiteral("Pattern"), Qt::CaseInsensitive) == 0)
        return;

    ensureEventAuditContext(modeName, DateKept, TimeKept, modeName, fileTimeStamp);

    const QString normalized = phase.trimmed().toUpper();
    QString code = QStringLiteral("DATA_") + normalized;
    if (normalized == QStringLiteral("PHASEA")) code = QStringLiteral("DATA_PHASE_A");
    else if (normalized == QStringLiteral("PHASEB")) code = QStringLiteral("DATA_PHASE_B");
    else if (normalized == QStringLiteral("PHASEC")) code = QStringLiteral("DATA_PHASE_C");

    QJsonObject details;
    details.insert(QStringLiteral("phase"), phase);
    details.insert(QStringLiteral("fileTimeStamp"), fileTimeStamp);
    details.insert(QStringLiteral("distanceSampleCount"), distanceData.size());
    details.insert(QStringLiteral("voltageSampleCount"), voltageData.size());
    if (!distanceData.isEmpty()) {
        details.insert(QStringLiteral("distanceFirst"), distanceData.at(0));
        details.insert(QStringLiteral("distanceLast"), distanceData.at(distanceData.size() - 1));
    }
    if (!voltageData.isEmpty()) {
        details.insert(QStringLiteral("voltageFirst"), voltageData.at(0));
        details.insert(QStringLiteral("voltageLast"), voltageData.at(voltageData.size() - 1));
    }
    details.insert(QStringLiteral("fullArraysStreamed"), false);
    details.insert(QStringLiteral("note"), QStringLiteral("Full waveform arrays are intentionally not duplicated into audit WebSocket JSON to avoid memory/network overload; counts and boundary samples are logged."));

    auditEventStep(code,
                   QStringLiteral("Process/store %1 event data").arg(phase),
                   QStringLiteral("PASS"),
                   QStringLiteral("Forwarding processed phase event data to Database::SumNormalizationandUpdateDb"),
                   QStringLiteral("FPGA/PLC processed event data"),
                   QStringLiteral("Database::SumNormalizationandUpdateDb"),
                   details);
}

QJsonObject PLCServer::buildEventAuditSnapshot() const
{
    QJsonObject root;
    root.insert(QStringLiteral("objectName"), QStringLiteral("EVENT_PROCESS_AUDIT"));
    root.insert(QStringLiteral("messageType"), QStringLiteral("SNAPSHOT"));
    root.insert(QStringLiteral("schemaVersion"), 2);
    root.insert(QStringLiteral("active"), eventAuditActive);
    root.insert(QStringLiteral("eventId"), eventAuditId);
    root.insert(QStringLiteral("eventMode"), eventAuditMode);
    root.insert(QStringLiteral("eventName"), eventAuditName);
    root.insert(QStringLiteral("eventDate"), eventAuditDate);
    root.insert(QStringLiteral("eventTime"), eventAuditTime);
    root.insert(QStringLiteral("startedAtMs"), double(eventAuditStartedMs));
    root.insert(QStringLiteral("auditLogPath"), eventAuditLogPath);
    root.insert(QStringLiteral("checklistDefinition"), expectedAuditChecklist());

    QList<QJsonObject> ordered;
    for (auto it = eventAuditLatestSteps.constBegin(); it != eventAuditLatestSteps.constEnd(); ++it)
        ordered.append(it.value());
    std::sort(ordered.begin(), ordered.end(), [](const QJsonObject &a, const QJsonObject &b) {
        return a.value(QStringLiteral("expectedOrder")).toInt(999) <
               b.value(QStringLiteral("expectedOrder")).toInt(999);
    });

    QJsonArray steps;
    for (const QJsonObject &step : ordered)
        steps.append(step);
    root.insert(QStringLiteral("steps"), steps);
    root.insert(QStringLiteral("history"), eventAuditHistory);

    QJsonObject counters;
    counters.insert(QStringLiteral("pass"), eventAuditPassCount);
    counters.insert(QStringLiteral("fail"), eventAuditFailCount);
    counters.insert(QStringLiteral("retry"), eventAuditRetryCount);
    counters.insert(QStringLiteral("block"), eventAuditBlockCount);
    counters.insert(QStringLiteral("info"), eventAuditInfoCount);
    counters.insert(QStringLiteral("skip"), eventAuditSkipCount);
    counters.insert(QStringLiteral("start"), eventAuditStartCount);
    root.insert(QStringLiteral("summary"), counters);
    root.insert(QStringLiteral("runtime"), eventAuditRuntimeSnapshot());
    root.insert(QStringLiteral("machine"), eventAuditMachineSnapshot());
    return root;
}

void PLCServer::sendEventAuditSnapshot(QWebSocket *client)
{
    if (!client || client->state() != QAbstractSocket::ConnectedState)
        return;

    const QString raw = QString::fromUtf8(
        QJsonDocument(buildEventAuditSnapshot()).toJson(QJsonDocument::Compact));
    emit sendMessage(raw, client);
}

void PLCServer::finishEventAudit(bool pass, const QString &description)
{
    if (!eventAuditActive)
        return;

    QJsonObject details;
    details.insert(QStringLiteral("finalResult"), pass ? QStringLiteral("PASS") : QStringLiteral("FAIL"));
    details.insert(QStringLiteral("totalProcessUpdates"), eventAuditSequence + 1);
    details.insert(QStringLiteral("auditLogPath"), eventAuditLogPath);
    QJsonObject countersBeforeComplete;
    countersBeforeComplete.insert(QStringLiteral("pass"), eventAuditPassCount);
    countersBeforeComplete.insert(QStringLiteral("fail"), eventAuditFailCount);
    countersBeforeComplete.insert(QStringLiteral("retry"), eventAuditRetryCount);
    countersBeforeComplete.insert(QStringLiteral("block"), eventAuditBlockCount);
    countersBeforeComplete.insert(QStringLiteral("info"), eventAuditInfoCount);
    countersBeforeComplete.insert(QStringLiteral("skip"), eventAuditSkipCount);
    countersBeforeComplete.insert(QStringLiteral("start"), eventAuditStartCount);
    details.insert(QStringLiteral("summary"), countersBeforeComplete);

    auditEventStep(QStringLiteral("EVENT_COMPLETE"),
                   QStringLiteral("Event processing complete"),
                   pass ? QStringLiteral("PASS") : QStringLiteral("FAIL"),
                   description,
                   QStringLiteral("PLCServer event pipeline"),
                   QStringLiteral("Event closed"),
                   details);

    QJsonObject summary;
    summary.insert(QStringLiteral("eventId"), eventAuditId);
    summary.insert(QStringLiteral("eventMode"), eventAuditMode);
    summary.insert(QStringLiteral("eventName"), eventAuditName);
    summary.insert(QStringLiteral("eventDate"), eventAuditDate);
    summary.insert(QStringLiteral("eventTime"), eventAuditTime);
    summary.insert(QStringLiteral("result"), pass ? QStringLiteral("PASS") : QStringLiteral("FAIL"));
    summary.insert(QStringLiteral("description"), description);
    summary.insert(QStringLiteral("finishedAt"), QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    summary.insert(QStringLiteral("durationMs"), double(QDateTime::currentMSecsSinceEpoch() - eventAuditStartedMs));
    summary.insert(QStringLiteral("auditLogPath"), eventAuditLogPath);
    QJsonObject counters;
    counters.insert(QStringLiteral("pass"), eventAuditPassCount);
    counters.insert(QStringLiteral("fail"), eventAuditFailCount);
    counters.insert(QStringLiteral("retry"), eventAuditRetryCount);
    counters.insert(QStringLiteral("block"), eventAuditBlockCount);
    counters.insert(QStringLiteral("info"), eventAuditInfoCount);
    counters.insert(QStringLiteral("skip"), eventAuditSkipCount);
    counters.insert(QStringLiteral("start"), eventAuditStartCount);
    summary.insert(QStringLiteral("summary"), counters);

    eventAuditHistory.append(summary);
    while (eventAuditHistory.size() > 20)
        eventAuditHistory.removeAt(0);

    eventAuditActive = false;
}
