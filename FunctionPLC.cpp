#include "PLCServer.h"

namespace {

QString safeFileNameOnly(QString name, const QString &fallback = QStringLiteral("file"))
{
    name = QFileInfo(name.trimmed()).fileName();
    name.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._#-]")), QStringLiteral("_"));
    while (name.contains(QStringLiteral(".."))) {
        name.replace(QStringLiteral(".."), QStringLiteral("_"));
    }
    if (name.isEmpty() || name == QStringLiteral(".") || name == QStringLiteral("..")) {
        name = fallback;
    }
    return name.left(180);
}

QString safeFtpPart(QString value, const QString &fallback = QStringLiteral("item"))
{
    value = value.trimmed();
    value.replace(QRegularExpression(QStringLiteral("[^\\p{L}\\p{N}._#-]")), QStringLiteral("_"));
    while (value.contains(QStringLiteral(".."))) {
        value.replace(QStringLiteral(".."), QStringLiteral("_"));
    }
    value.remove(QStringLiteral("/"));
    value.remove(QStringLiteral("\\"));

    if (value.isEmpty() || value == QStringLiteral(".") || value == QStringLiteral("..")) {
        value = fallback;
    }

    return value.left(180);
}

QString safeFtpRootPath(QString value, const QString &fallback = QStringLiteral("event_record"))
{
    value = value.trimmed().replace(QStringLiteral("\\"), QStringLiteral("/"));
    const bool absolute = value.startsWith(QLatin1Char('/'));

    QStringList safeParts;
    const QStringList parts = value.split(QLatin1Char('/'), QString::SkipEmptyParts);
    for (QString part : parts) {
        part.replace(QRegularExpression(QStringLiteral("[^\\p{L}\\p{N}._#-]")), QStringLiteral("_"));
        while (part.contains(QStringLiteral(".."))) {
            part.replace(QStringLiteral(".."), QStringLiteral("_"));
        }
        if (!part.isEmpty() && part != QStringLiteral(".") && part != QStringLiteral("..")) {
            safeParts.append(part.left(180));
        }
    }

    if (safeParts.isEmpty()) {
        safeParts.append(safeFtpPart(fallback, QStringLiteral("event_record")));
    }

    QString result = safeParts.join(QStringLiteral("/"));
    if (absolute) {
        result.prepend(QLatin1Char('/'));
    }
    return result;
}

QString joinFtpPath(const QStringList &parts)
{
    QString result;
    for (QString part : parts) {
        part = part.trimmed();
        if (part.isEmpty()) {
            continue;
        }

        if (result.isEmpty()) {
            result = part;
            while (result.endsWith(QLatin1Char('/')) && result.size() > 1) {
                result.chop(1);
            }
            continue;
        }

        while (result.endsWith(QLatin1Char('/')) && result.size() > 1) {
            result.chop(1);
        }
        while (part.startsWith(QLatin1Char('/'))) {
            part.remove(0, 1);
        }
        result += QLatin1Char('/') + part;
    }
    return result;
}

QStringList incrementalFtpDirs(const QStringList &parts)
{
    QStringList dirs;
    QString current;
    for (const QString &part : parts) {
        current = joinFtpPath(QStringList{current, part});
        if (!current.isEmpty()) {
            dirs.append(current);
        }
    }
    return dirs;
}

QString lftpArg(QString value)
{
    value = value.trimmed();
    if (!value.contains(QRegularExpression(QStringLiteral("[\\s;'\\\"]")))) {
        return value;
    }
    value.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    value.replace(QStringLiteral("\""), QStringLiteral("\\\""));
    return QStringLiteral("\"%1\"").arg(value);
}

QString ensureCsvSuffix(QString fileName)
{
    if (!fileName.endsWith(QStringLiteral(".csv"), Qt::CaseInsensitive)) {
        fileName += QStringLiteral(".csv");
    }
    return fileName;
}

QString buildLegacyLftpCommand(const QString &username,
                               const QString &password,
                               const QString &host,
                               const QString &script)
{
    return QStringLiteral("lftp -u %1,%2 %3 -e '%4'")
        .arg(username, password, host, script);
}

struct FtpExecResult
{
    bool success = false;
    bool started = false;
    bool timedOut = false;
    int exitCode = -1;
    QString stdoutText;
    QString stderrText;
    QString errorText;
};

QString sanitizeFtpDiagnostic(QString text, const QString &password)
{
    if (!password.isEmpty()) {
        text.replace(password, QStringLiteral("******"), Qt::CaseSensitive);
    }
    // Keep WebSocket audit payloads bounded even when lftp prints a long trace.
    constexpr int kMaxDiagnosticChars = 3000;
    if (text.size() > kMaxDiagnosticChars) {
        text = text.right(kMaxDiagnosticChars);
        text.prepend(QStringLiteral("...[truncated] "));
    }
    return text.trimmed();
}

FtpExecResult runLftpDetailed(const QString &username,
                              const QString &password,
                              const QString &host,
                              const QString &script,
                              int timeoutMs)
{
    FtpExecResult result;

    const QString credential = username + QLatin1Char(',') + password;
    const QStringList args{
        QStringLiteral("-u"), credential,
        QStringLiteral("-e"), script,
        host
    };

    QProcess process;
    process.start(QStringLiteral("lftp"), args);

    if (!process.waitForStarted(3000)) {
        result.errorText = sanitizeFtpDiagnostic(process.errorString(), password);
        return result;
    }

    result.started = true;
    const int boundedTimeout = qBound(1000, timeoutMs, 8000);
    if (!process.waitForFinished(boundedTimeout)) {
        result.timedOut = true;
        process.kill();
        process.waitForFinished(2000);
    }

    result.exitCode = process.exitCode();
    result.stdoutText = sanitizeFtpDiagnostic(
        QString::fromLocal8Bit(process.readAllStandardOutput()), password);
    result.stderrText = sanitizeFtpDiagnostic(
        QString::fromLocal8Bit(process.readAllStandardError()), password);

    if (result.errorText.isEmpty() && process.error() != QProcess::UnknownError) {
        result.errorText = sanitizeFtpDiagnostic(process.errorString(), password);
    }

    result.success = !result.timedOut &&
                     process.exitStatus() == QProcess::NormalExit &&
                     result.exitCode == 0;
    return result;
}

QJsonObject ftpExecResultJson(const FtpExecResult &result)
{
    QJsonObject d;
    d.insert(QStringLiteral("success"), result.success);
    d.insert(QStringLiteral("started"), result.started);
    d.insert(QStringLiteral("timedOut"), result.timedOut);
    d.insert(QStringLiteral("exitCode"), result.exitCode);
    if (!result.stderrText.isEmpty())
        d.insert(QStringLiteral("stderr"), result.stderrText);
    if (!result.stdoutText.isEmpty())
        d.insert(QStringLiteral("stdout"), result.stdoutText);
    if (!result.errorText.isEmpty())
        d.insert(QStringLiteral("processError"), result.errorText);
    return d;
}

QString classifyFtpFailure(const FtpExecResult &result)
{
    if (result.success)
        return QStringLiteral("NONE");
    if (result.timedOut)
        return QStringLiteral("TIMEOUT");
    if (!result.started)
        return QStringLiteral("PROCESS_START_FAILED");

    const QString text = (result.stderrText + QLatin1Char(' ') + result.errorText).toLower();
    if (text.contains(QStringLiteral("login failed")) ||
        text.contains(QStringLiteral("authentication")) ||
        text.contains(QStringLiteral("530 ")) ||
        text.contains(QStringLiteral("530-")))
        return QStringLiteral("AUTHENTICATION_FAILED");
    if (text.contains(QStringLiteral("permission denied")) ||
        text.contains(QStringLiteral("550 permission")))
        return QStringLiteral("PERMISSION_DENIED");
    if (text.contains(QStringLiteral("no route")) ||
        text.contains(QStringLiteral("connection refused")) ||
        text.contains(QStringLiteral("not connected")) ||
        text.contains(QStringLiteral("host is unreachable")))
        return QStringLiteral("CONNECTION_FAILED");
    if (text.contains(QStringLiteral("not found")) ||
        text.contains(QStringLiteral("no such file")) ||
        text.contains(QStringLiteral("550 ")))
        return QStringLiteral("REMOTE_PATH_REJECTED");
    return QStringLiteral("FTP_COMMAND_FAILED");
}

QString eventFtpSetupScript(const QString &mainCommand, int timeoutSeconds = 5)
{
    return QStringLiteral(
        "set net:timeout %1; "
        "set net:reconnect-interval-base 1; "
        "set net:max-retries 1; %2; bye")
        .arg(timeoutSeconds)
        .arg(mainCommand);
}

bool ensureDirExists(const QString &path)
{
    return QDir().mkpath(QDir::cleanPath(path));
}

bool runProcessChecked(const QString &program,
                       const QStringList &arguments,
                       int timeoutMs = 30000)
{
    QProcess process;
    process.start(program, arguments);

    if (!process.waitForStarted(5000)) {
        qWarning() << "[SafeProcess] failed to start" << program << arguments
                   << process.errorString();
        return false;
    }

    if (!process.waitForFinished(timeoutMs)) {
        qWarning() << "[SafeProcess] timeout" << program << arguments;
        process.kill();
        process.waitForFinished(3000);
        return false;
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        qWarning() << "[SafeProcess] failed" << program << arguments
                   << "exitCode=" << process.exitCode()
                   << "stderr=" << QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
        return false;
    }

    return true;
}

bool downloadUrlToFile(const QString &urlString, const QString &destinationPath)
{
    const QUrl url(urlString.trimmed());
    if (!url.isValid() || url.scheme().isEmpty()) {
        qWarning() << "[Download] invalid URL:" << urlString;
        return false;
    }

    const QString scheme = url.scheme().toLower();
    if (scheme != QStringLiteral("http") &&
        scheme != QStringLiteral("https") &&
        scheme != QStringLiteral("ftp")) {
        qWarning() << "[Download] blocked URL scheme:" << scheme;
        return false;
    }

    const QFileInfo destInfo(QDir::cleanPath(destinationPath));
    if (!ensureDirExists(destInfo.absolutePath())) {
        qWarning() << "[Download] cannot create directory:" << destInfo.absolutePath();
        return false;
    }

    const QString tempPath = destInfo.absoluteFilePath() + QStringLiteral(".download");
    QFile::remove(tempPath);

    if (!runProcessChecked(QStringLiteral("wget"),
                           QStringList{QStringLiteral("--timeout=3"),
                                       QStringLiteral("-O"),
                                       tempPath,
                                       url.toString()},
                           20000)) {
        QFile::remove(tempPath);
        return false;
    }

    if (!QFileInfo::exists(tempPath) || QFileInfo(tempPath).size() <= 0) {
        qWarning() << "[Download] downloaded file is missing or empty:" << tempPath;
        QFile::remove(tempPath);
        return false;
    }

    QFile::remove(destInfo.absoluteFilePath());
    if (!QFile::rename(tempPath, destInfo.absoluteFilePath())) {
        qWarning() << "[Download] cannot move temp file to destination:"
                   << tempPath << "->" << destInfo.absoluteFilePath();
        QFile::remove(tempPath);
        return false;
    }

    return true;
}

bool isVerifiedDownloadedPictureFile(const QString &path, qint64 *size = nullptr)
{
    const QString cleanPath = QDir::cleanPath(path.trimmed());
    const QFileInfo info(cleanPath);
    if (!info.exists() || !info.isFile() || info.isSymLink() || info.size() <= 0) {
        return false;
    }

    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[PIC-DOWNLOAD] file exists but cannot be opened:"
                   << info.absoluteFilePath() << file.errorString();
        return false;
    }

    const QByteArray header = file.read(16);
    file.close();

    // Detect a common failure where wget succeeds but an HTML/error payload is
    // saved with an image filename.  Validate known image signatures without
    // introducing a QtGui dependency (this project builds with QT -= gui).
    const QString suffix = info.suffix().toLower();
    bool formatOk = true;
    if (suffix == QStringLiteral("png")) {
        formatOk = header.startsWith(QByteArray::fromHex("89504E470D0A1A0A"));
    } else if (suffix == QStringLiteral("jpg") || suffix == QStringLiteral("jpeg")) {
        formatOk = header.startsWith(QByteArray::fromHex("FFD8FF"));
    } else if (suffix == QStringLiteral("bmp")) {
        formatOk = header.startsWith(QByteArray("BM", 2));
    } else if (suffix == QStringLiteral("webp")) {
        formatOk = header.size() >= 12 &&
                   header.left(4) == QByteArray("RIFF", 4) &&
                   header.mid(8, 4) == QByteArray("WEBP", 4);
    }

    if (!formatOk) {
        qWarning() << "[PIC-DOWNLOAD] invalid image signature:"
                   << info.absoluteFilePath()
                   << "suffix=" << suffix
                   << "size=" << info.size();
        return false;
    }

    if (size) {
        *size = info.size();
    }
    return true;
}

bool isValidAdcChannel(const QString &channel)
{
    return channel == QStringLiteral("0") ||
           channel == QStringLiteral("1") ||
           channel == QStringLiteral("2");
}

bool isValidRemoteAdcChannel(const QString &channel)
{
    return channel == QStringLiteral("data0") ||
           channel == QStringLiteral("data1") ||
           channel == QStringLiteral("data2");
}

bool safeRemoveFile(const QString &path)
{
    const QString cleanPath = QDir::cleanPath(path);
    const QFileInfo info(cleanPath);

    if (!info.exists()) {
        return true;
    }

    if (!info.isFile() || info.isSymLink()) {
        qWarning() << "[SafeFile] blocked remove; not a regular file:" << cleanPath;
        return false;
    }

    if (!QFile::remove(cleanPath)) {
        qWarning() << "[SafeFile] remove failed:" << cleanPath;
        return false;
    }

    return true;
}

bool safeCopyFileReplace(const QString &srcPath, const QString &destPath)
{
    const QFileInfo srcInfo(QDir::cleanPath(srcPath));
    const QFileInfo destInfo(QDir::cleanPath(destPath));

    if (!srcInfo.exists() || !srcInfo.isFile() || srcInfo.isSymLink()) {
        qWarning() << "[SafeFile] blocked copy source:" << srcInfo.absoluteFilePath();
        return false;
    }

    if (!ensureDirExists(destInfo.absolutePath())) {
        qWarning() << "[SafeFile] cannot create destination directory:" << destInfo.absolutePath();
        return false;
    }

    QFile::remove(destInfo.absoluteFilePath());
    if (!QFile::copy(srcInfo.absoluteFilePath(), destInfo.absoluteFilePath())) {
        qWarning() << "[SafeFile] copy failed:"
                   << srcInfo.absoluteFilePath() << "->" << destInfo.absoluteFilePath();
        return false;
    }

    return true;
}

bool safeMoveFileReplace(const QString &srcPath, const QString &destPath)
{
    const QFileInfo srcInfo(QDir::cleanPath(srcPath));
    const QFileInfo destInfo(QDir::cleanPath(destPath));

    if (!srcInfo.exists() || !srcInfo.isFile() || srcInfo.isSymLink()) {
        qWarning() << "[SafeFile] blocked move source:" << srcInfo.absoluteFilePath();
        return false;
    }

    if (!ensureDirExists(destInfo.absolutePath())) {
        qWarning() << "[SafeFile] cannot create destination directory:" << destInfo.absolutePath();
        return false;
    }

    QFile::remove(destInfo.absoluteFilePath());
    if (!QFile::rename(srcInfo.absoluteFilePath(), destInfo.absoluteFilePath())) {
        if (!safeCopyFileReplace(srcInfo.absoluteFilePath(), destInfo.absoluteFilePath())) {
            return false;
        }
        QFile::remove(srcInfo.absoluteFilePath());
    }

    return true;
}


QString canonicalRecoveryMode(QString mode)
{
    mode = mode.trimmed();
    if (mode.compare(QStringLiteral("Manual"), Qt::CaseInsensitive) == 0 ||
        mode.compare(QStringLiteral("ManualTest"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Manual");
    }
    if (mode.compare(QStringLiteral("Relay"), Qt::CaseInsensitive) == 0 ||
        mode.compare(QStringLiteral("RelayTest"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Relay");
    }
    if (mode.compare(QStringLiteral("Surge"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Surge");
    }
    if (mode.compare(QStringLiteral("Periodic"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Periodic");
    }
    if (mode.compare(QStringLiteral("Pattern"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Pattern");
    }
    return QString();
}

QString inferRecoveryModeFromPath(const QString &path)
{
    const QString normalized = QDir::fromNativeSeparators(QDir::cleanPath(path));
    const QStringList modes{QStringLiteral("Manual"), QStringLiteral("Relay"),
                            QStringLiteral("Surge"), QStringLiteral("Periodic"),
                            QStringLiteral("Pattern")};
    for (const QString &mode : modes) {
        const QString token = QStringLiteral("/%1/").arg(mode);
        if (normalized.contains(token, Qt::CaseInsensitive) ||
            normalized.endsWith(QStringLiteral("/%1").arg(mode), Qt::CaseInsensitive)) {
            return mode;
        }
    }
    return QString();
}

bool normalizeEventDateTime(QString &date, QString &time)
{
    date = date.trimmed();
    time = time.trimmed();

    QDate d;
    if (date.contains(QLatin1Char('-'))) {
        d = QDate::fromString(date, QStringLiteral("yyyy-MM-dd"));
    } else {
        d = QDate::fromString(date, QStringLiteral("yyyyMMdd"));
    }

    QTime t;
    if (time.contains(QLatin1Char('-'))) {
        t = QTime::fromString(time, QStringLiteral("HH-mm-ss"));
    } else if (time.contains(QLatin1Char(':'))) {
        t = QTime::fromString(time, QStringLiteral("HH:mm:ss"));
    } else {
        t = QTime::fromString(time, QStringLiteral("HHmmss"));
    }

    if (!d.isValid() || !t.isValid()) {
        return false;
    }

    date = d.toString(QStringLiteral("yyyy-MM-dd"));
    time = t.toString(QStringLiteral("HH-mm-ss"));
    return true;
}

bool extractEventDateTimeFromText(const QString &text, QString &date, QString &time)
{
    if (text.trimmed().isEmpty()) {
        return false;
    }

    const QString normalized = QDir::fromNativeSeparators(text);

    // Preferred format used by local event directories:
    // .../<yyyy-MM-dd>/<HH-mm-ss>/...
    QRegularExpression pathRe(QStringLiteral("(?:^|/)(\\d{4}-\\d{2}-\\d{2})/(\\d{2}-\\d{2}-\\d{2})(?:/|$)"));
    QRegularExpressionMatch match = pathRe.match(normalized);
    if (match.hasMatch()) {
        QString d = match.captured(1);
        QString t = match.captured(2);
        if (normalizeEventDateTime(d, t)) {
            date = d;
            time = t;
            return true;
        }
    }

    // Compact timestamps used by event/picture names, e.g.
    // 20260819_100217_630.png or ...M20260819_094818
    QRegularExpression compactRe(QStringLiteral("(20\\d{6})[_-]?(\\d{6})"));
    match = compactRe.match(QFileInfo(normalized).fileName());
    if (!match.hasMatch()) {
        match = compactRe.match(normalized);
    }
    if (match.hasMatch()) {
        QString d = match.captured(1);
        QString t = match.captured(2);
        if (normalizeEventDateTime(d, t)) {
            date = d;
            time = t;
            return true;
        }
    }

    return false;
}

bool pathMatchesEventDateTime(const QString &path,
                              const QString &eventDate,
                              const QString &eventTime)
{
    if (eventDate.isEmpty() || eventTime.isEmpty()) {
        return true;
    }
    const QString normalized = QDir::fromNativeSeparators(QDir::cleanPath(path));
    const QString token = QStringLiteral("/%1/%2/").arg(eventDate, eventTime);
    return normalized.contains(token) || normalized.endsWith(token.left(token.size() - 1));
}


bool eventDateTimeWithinSeconds(const QString &dateA,
                                const QString &timeA,
                                const QString &dateB,
                                const QString &timeB,
                                int toleranceSeconds = 120)
{
    QString da = dateA;
    QString ta = timeA;
    QString db = dateB;
    QString tb = timeB;
    if (!normalizeEventDateTime(da, ta) || !normalizeEventDateTime(db, tb)) {
        return false;
    }

    const QDateTime a(QDate::fromString(da, QStringLiteral("yyyy-MM-dd")),
                      QTime::fromString(ta, QStringLiteral("HH-mm-ss")));
    const QDateTime b(QDate::fromString(db, QStringLiteral("yyyy-MM-dd")),
                      QTime::fromString(tb, QStringLiteral("HH-mm-ss")));
    if (!a.isValid() || !b.isValid()) {
        return false;
    }
    return qAbs(a.secsTo(b)) <= toleranceSeconds;
}

bool isPathInsideBase(const QString &path, const QString &basePath)
{
    const QFileInfo pathInfo(QDir::cleanPath(path));
    const QFileInfo baseInfo(QDir::cleanPath(basePath));

    QString resolvedPath = pathInfo.canonicalFilePath();
    if (resolvedPath.isEmpty()) {
        resolvedPath = pathInfo.absoluteFilePath();
    }

    QString resolvedBase = baseInfo.canonicalFilePath();
    if (resolvedBase.isEmpty()) {
        resolvedBase = baseInfo.absoluteFilePath();
    }

    resolvedPath = QDir::fromNativeSeparators(QDir::cleanPath(resolvedPath));
    resolvedBase = QDir::fromNativeSeparators(QDir::cleanPath(resolvedBase));
    if (!resolvedBase.endsWith(QLatin1Char('/'))) {
        resolvedBase += QLatin1Char('/');
    }

    return resolvedPath.startsWith(resolvedBase, Qt::CaseSensitive);
}

struct FileVerifySnapshot
{
    QString path;
    QString fileName;
    qint64 size = -1;
    qint64 lastModifiedMs = -1;
    bool valid = false;
};

bool isForbiddenEventCsvCandidate(const QString &pathOrName)
{
    if (pathOrName.trimmed().isEmpty()) {
        return false;
    }

    const QString normalized = QDir::fromNativeSeparators(QDir::cleanPath(pathOrName.trimmed()));
    const QString fileName = QFileInfo(normalized).fileName();

    // Hard contract:
    // 1) Event CSV must never be a *_PATTERN* file.
    // 2) Event CSV must never live below /Pattern/.
    return fileName.contains(QStringLiteral("_PATTERN"), Qt::CaseInsensitive) ||
           normalized.contains(QStringLiteral("/Pattern/"), Qt::CaseInsensitive) ||
           normalized.endsWith(QStringLiteral("/Pattern"), Qt::CaseInsensitive);
}

bool captureRegularReadableSnapshot(const QString &path, FileVerifySnapshot *snapshot)
{
    if (snapshot) {
        *snapshot = FileVerifySnapshot();
    }

    const QFileInfo info(QDir::cleanPath(path.trimmed()));
    if (!info.exists() || !info.isFile() || info.isDir() || info.isSymLink() || info.size() <= 0) {
        return false;
    }

    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const bool readable = !file.atEnd() || info.size() > 0;
    file.close();
    if (!readable) {
        return false;
    }

    if (snapshot) {
        snapshot->path = info.absoluteFilePath();
        snapshot->fileName = info.fileName();
        snapshot->size = info.size();
        snapshot->lastModifiedMs = info.lastModified().toMSecsSinceEpoch();
        snapshot->valid = true;
    }
    return true;
}

bool captureVerifiedPictureSnapshot(const QString &path,
                                    const QString &picRoot,
                                    const QString &eventDate,
                                    const QString &eventTime,
                                    FileVerifySnapshot *snapshot)
{
    qint64 checkedSize = 0;
    if (!isVerifiedDownloadedPictureFile(path, &checkedSize) ||
        !isPathInsideBase(path, picRoot)) {
        return false;
    }

    QString candidateDate;
    QString candidateTime;
    if (!eventDate.isEmpty() && !eventTime.isEmpty()) {
        if (!extractEventDateTimeFromText(path, candidateDate, candidateTime) ||
            !eventDateTimeWithinSeconds(eventDate, eventTime, candidateDate, candidateTime)) {
            qWarning() << "[EVENT-VERIFY][PIC][REJECT] event timestamp mismatch:"
                       << "path=" << path
                       << "expected=" << eventDate << eventTime
                       << "actual=" << candidateDate << candidateTime;
            return false;
        }
    }

    if (!captureRegularReadableSnapshot(path, snapshot)) {
        return false;
    }
    if (snapshot && snapshot->size != checkedSize) {
        qWarning() << "[EVENT-VERIFY][PIC][REJECT] size changed during verification:"
                   << path << "signatureSize=" << checkedSize << "snapshotSize=" << snapshot->size;
        return false;
    }
    return true;
}

bool captureVerifiedEventCsvSnapshot(const QString &path,
                                     const QString &eventRoot,
                                     const QString &mode,
                                     const QString &eventDate,
                                     const QString &eventTime,
                                     FileVerifySnapshot *snapshot)
{
    if (isForbiddenEventCsvCandidate(path)) {
        qWarning() << "[EVENT-VERIFY][CSV][REJECT] reason=PATTERN_NOT_ALLOWED candidate=" << path;
        return false;
    }

    if (!isPathInsideBase(path, eventRoot)) {
        qWarning() << "[EVENT-VERIFY][CSV][REJECT] outside EVENT_PATH:" << path;
        return false;
    }

    const QString expectedMode = canonicalRecoveryMode(mode);
    if (expectedMode != QStringLiteral("Manual") &&
        expectedMode != QStringLiteral("Relay") &&
        expectedMode != QStringLiteral("Surge") &&
        expectedMode != QStringLiteral("Periodic")) {
        qWarning() << "[EVENT-VERIFY][CSV][REJECT] invalid event mode:" << mode << path;
        return false;
    }

    const QString actualMode = inferRecoveryModeFromPath(path);
    if (actualMode.compare(expectedMode, Qt::CaseInsensitive) != 0) {
        qWarning() << "[EVENT-VERIFY][CSV][REJECT] mode mismatch:"
                   << "path=" << path << "expected=" << expectedMode << "actual=" << actualMode;
        return false;
    }

    QString candidateDate;
    QString candidateTime;
    if (!eventDate.isEmpty() && !eventTime.isEmpty()) {
        if (!extractEventDateTimeFromText(path, candidateDate, candidateTime) ||
            !eventDateTimeWithinSeconds(eventDate, eventTime, candidateDate, candidateTime)) {
            qWarning() << "[EVENT-VERIFY][CSV][REJECT] event timestamp mismatch:"
                       << "path=" << path
                       << "expected=" << eventDate << eventTime
                       << "actual=" << candidateDate << candidateTime;
            return false;
        }
    }

    return captureRegularReadableSnapshot(path, snapshot);
}

bool captureOptionalPatternSnapshot(const QString &path,
                                    const QString &patternRoot,
                                    FileVerifySnapshot *snapshot)
{
    if (path.trimmed().isEmpty()) {
        return false;
    }
    if (!isPathInsideBase(path, patternRoot)) {
        qWarning() << "[EVENT-VERIFY][PATTERN] optional pattern path rejected; outside Pattern root:"
                   << path;
        return false;
    }
    return captureRegularReadableSnapshot(path, snapshot);
}

bool snapshotsAreStable(const FileVerifySnapshot &first,
                        const FileVerifySnapshot &second)
{
    return first.valid && second.valid &&
           first.path == second.path &&
           first.fileName == second.fileName &&
           first.size == second.size &&
           first.lastModifiedMs == second.lastModifiedMs;
}

QStringList exactFileCandidates(const QString &name)
{
    QStringList names;
    const QString cleanName = safeFileNameOnly(name, QString());
    if (!cleanName.isEmpty()) {
        names << cleanName;
        if (!cleanName.endsWith(QStringLiteral(".csv"), Qt::CaseInsensitive)) {
            names << cleanName + QStringLiteral(".csv");
        }
    }
    names.removeDuplicates();
    return names;
}

void removeRawDataFiles()
{
    QDir dir(QStringLiteral("/home/pi/Rawdata"));
    if (!dir.exists()) {
        return;
    }

    const QStringList files = dir.entryList(QStringList{QStringLiteral("data*.raw"),
                                                       QStringLiteral("data*.download")},
                                           QDir::Files | QDir::NoSymLinks);
    for (const QString &file : files) {
        safeRemoveFile(dir.absoluteFilePath(file));
    }
}

} // namespace

bool PLCServer::isVerifiedLocalFile(const QString &path, qint64 *size) const
{
    const QString cleanPath = QDir::cleanPath(path.trimmed());
    const QFileInfo info(cleanPath);
    if (!info.exists() || !info.isFile() || info.isSymLink() || info.size() <= 0) {
        return false;
    }

    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[PATH-CHECK] file exists but cannot be opened for read:"
                   << info.absoluteFilePath() << file.errorString();
        return false;
    }
    file.close();

    if (size) {
        *size = info.size();
    }
    return true;
}

QString PLCServer::resolveExistingPicturePath(const QString &storedPath,
                                              const QString &picName,
                                              const QString &eventDate,
                                              const QString &eventTime) const
{
    QString date = eventDate;
    QString time = eventTime;
    normalizeEventDateTime(date, time);

    QString recoveredName = safeFileNameOnly(picName, QString());
    if (recoveredName.isEmpty() && !storedPath.trimmed().isEmpty()) {
        recoveredName = safeFileNameOnly(QFileInfo(storedPath).fileName(), QString());
    }

    if (!recoveredName.isEmpty() && !date.isEmpty() && !time.isEmpty()) {
        QString nameDate;
        QString nameTime;
        if (extractEventDateTimeFromText(recoveredName, nameDate, nameTime) &&
            !eventDateTimeWithinSeconds(date, time, nameDate, nameTime)) {
            qWarning() << "[PATH-CHECK][PIC] picture name belongs to another event; ignore stale name:"
                       << recoveredName
                       << "current=" << date << time
                       << "nameEvent=" << nameDate << nameTime;
            recoveredName.clear();
        }
    }

    const QString cleanStored = QDir::cleanPath(storedPath.trimmed());
    if (!cleanStored.isEmpty() && isVerifiedLocalFile(cleanStored) &&
        isPathInsideBase(cleanStored, PIC_PATH)) {
        // Existing legacy flat /Pic/<file> paths remain valid for backward
        // compatibility. New hierarchical paths must match the event timestamp.
        const QString normalized = QDir::fromNativeSeparators(cleanStored);
        const bool isHierarchical = normalized.contains(QRegularExpression(
            QStringLiteral("/\\d{4}-\\d{2}-\\d{2}/\\d{2}-\\d{2}-\\d{2}/")));
        if (!isHierarchical || pathMatchesEventDateTime(cleanStored, date, time)) {
            qDebug() << "[PATH-CHECK][PIC] VERIFIED stored path:"
                     << cleanStored << "size=" << QFileInfo(cleanStored).size();
            return QFileInfo(cleanStored).absoluteFilePath();
        }
        qWarning() << "[PATH-CHECK][PIC] stored file exists but belongs to another event:"
                   << cleanStored << "expectedDate=" << date << "expectedTime=" << time;
    }

    if (!date.isEmpty() && !time.isEmpty() && !recoveredName.isEmpty()) {
        const QString expected = QDir(QDir(PIC_PATH).filePath(date + QStringLiteral("/") + time))
                                     .absoluteFilePath(recoveredName);
        if (isVerifiedLocalFile(expected)) {
            qWarning() << "[PATH-RECOVERY][PIC] RECOVERED canonical path:"
                       << "stored=" << storedPath
                       << "expected=" << expected
                       << "size=" << QFileInfo(expected).size();
            return QFileInfo(expected).absoluteFilePath();
        }
    }

    // Recover a lost picture name only inside the exact event folder.  If
    // multiple files exist, require an unambiguous timestamp match.
    if (!date.isEmpty() && !time.isEmpty() && recoveredName.isEmpty()) {
        const QString eventDirPath = QDir(PIC_PATH).filePath(date + QStringLiteral("/") + time);
        QDir eventDir(eventDirPath);
        if (eventDir.exists()) {
            QStringList valid;
            const QFileInfoList entries = eventDir.entryInfoList(QDir::Files | QDir::NoSymLinks,
                                                                 QDir::Name);
            for (const QFileInfo &entry : entries) {
                if (isVerifiedLocalFile(entry.absoluteFilePath())) {
                    valid << entry.absoluteFilePath();
                }
            }
            if (valid.size() == 1) {
                qWarning() << "[PATH-RECOVERY][PIC] name was empty; recovered unique event picture:"
                           << valid.first();
                return valid.first();
            }
            if (valid.size() > 1) {
                QStringList timestampMatches;
                for (const QString &candidate : valid) {
                    QString candidateDate;
                    QString candidateTime;
                    if (extractEventDateTimeFromText(candidate, candidateDate, candidateTime) &&
                        eventDateTimeWithinSeconds(date, time, candidateDate, candidateTime)) {
                        timestampMatches << candidate;
                    }
                }
                if (timestampMatches.size() == 1) {
                    qWarning() << "[PATH-RECOVERY][PIC] recovered unique timestamp-matching picture:"
                               << timestampMatches.first();
                    return timestampMatches.first();
                }
                qWarning() << "[PATH-RECOVERY][PIC] multiple pictures in exact event folder; refuse ambiguity:"
                           << eventDirPath;
            }
        }

        // Backward compatibility for pictures created before the date/time
        // hierarchy existed: /Pic/20260819_HHmmss_*.png
        QString compactDate = date;
        compactDate.remove(QLatin1Char('-'));
        QString compactTime = time;
        compactTime.remove(QLatin1Char('-'));
        const QString legacyPrefix = compactDate + QStringLiteral("_") + compactTime;
        QDir legacyRoot(PIC_PATH);
        if (legacyRoot.exists()) {
            const QFileInfoList legacy = legacyRoot.entryInfoList(
                QStringList{legacyPrefix + QStringLiteral("*")},
                QDir::Files | QDir::NoSymLinks,
                QDir::Name);
            QStringList validLegacy;
            for (const QFileInfo &entry : legacy) {
                if (isVerifiedLocalFile(entry.absoluteFilePath())) {
                    validLegacy << entry.absoluteFilePath();
                }
            }
            if (validLegacy.size() == 1) {
                qWarning() << "[PATH-RECOVERY][PIC] recovered legacy flat picture:"
                           << validLegacy.first();
                return validLegacy.first();
            }
        }
    }

    // Bounded recovery: only search inside the event date directory and only
    // for the exact picture name. Never scan the whole SD card.
    if (!date.isEmpty() && !recoveredName.isEmpty()) {
        const QString dateDirPath = QDir(PIC_PATH).filePath(date);
        QDir dateDir(dateDirPath);
        if (dateDir.exists()) {
            QStringList found;
            QDirIterator it(dateDir.absolutePath(), QStringList{recoveredName},
                            QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
            int inspected = 0;
            while (it.hasNext() && inspected < 2000) {
                const QString candidate = it.next();
                ++inspected;
                if (isVerifiedLocalFile(candidate)) {
                    found << QFileInfo(candidate).absoluteFilePath();
                    if (found.size() > 1) {
                        break;
                    }
                }
            }
            if (found.size() == 1) {
                qWarning() << "[PATH-RECOVERY][PIC] RECOVERED by date search:"
                           << found.first();
                return found.first();
            }
            if (found.size() > 1) {
                qWarning() << "[PATH-RECOVERY][PIC] ambiguous matches; refuse recovery:"
                           << recoveredName << "date=" << date;
            }
        }
    }

    qWarning() << "[PATH-CHECK][PIC][FAILED]"
               << "stored=" << storedPath
               << "name=" << recoveredName
               << "date=" << date
               << "time=" << time;
    return QString();
}

QString PLCServer::resolveExistingEventCsvPath(const QString &storedPath,
                                               const QString &csvName,
                                               const QString &mode,
                                               const QString &eventDate,
                                               const QString &eventTime) const
{
    QString resolvedMode = canonicalRecoveryMode(mode);
    if (resolvedMode.isEmpty()) {
        resolvedMode = inferRecoveryModeFromPath(storedPath);
    }

    if (resolvedMode == QStringLiteral("Pattern")) {
        qDebug() << "[PATH-CHECK][CSV] Pattern is intentionally excluded from event CSV recovery:"
                 << storedPath;
        return QString();
    }

    if (resolvedMode != QStringLiteral("Manual") &&
        resolvedMode != QStringLiteral("Relay") &&
        resolvedMode != QStringLiteral("Surge") &&
        resolvedMode != QStringLiteral("Periodic")) {
        qWarning() << "[PATH-CHECK][CSV] unsupported/unknown event mode:"
                   << mode << storedPath;
        return QString();
    }

    QString date = eventDate;
    QString time = eventTime;
    if (!normalizeEventDateTime(date, time)) {
        if (!extractEventDateTimeFromText(storedPath, date, time)) {
            extractEventDateTimeFromText(csvName, date, time);
        }
    }

    QString recoveredName = safeFileNameOnly(csvName, QString());
    if (recoveredName.isEmpty() && !storedPath.trimmed().isEmpty()) {
        recoveredName = safeFileNameOnly(QFileInfo(storedPath).fileName(), QString());
    }

    if (isForbiddenEventCsvCandidate(recoveredName)) {
        qWarning() << "[PATH-CHECK][CSV][REJECT] Event CSV name is Pattern; ignore and recover only a real event file:"
                   << recoveredName;
        recoveredName.clear();
    }

    if (!recoveredName.isEmpty() && !date.isEmpty() && !time.isEmpty()) {
        QString nameDate;
        QString nameTime;
        if (extractEventDateTimeFromText(recoveredName, nameDate, nameTime) &&
            !eventDateTimeWithinSeconds(date, time, nameDate, nameTime)) {
            qWarning() << "[PATH-CHECK][CSV] CSV name belongs to another event; ignore stale name:"
                       << recoveredName
                       << "current=" << date << time
                       << "nameEvent=" << nameDate << nameTime;
            recoveredName.clear();
        }
    }

    const QString cleanStored = QDir::cleanPath(storedPath.trimmed());
    if (!cleanStored.isEmpty() && isForbiddenEventCsvCandidate(cleanStored)) {
        qWarning() << "[PATH-CHECK][CSV][REJECT] stored Event CSV candidate is Pattern:"
                   << cleanStored;
    }
    if (!cleanStored.isEmpty() && !isForbiddenEventCsvCandidate(cleanStored) &&
        isVerifiedLocalFile(cleanStored) && isPathInsideBase(cleanStored, EVENT_PATH)) {
        const QString storedMode = inferRecoveryModeFromPath(cleanStored);
        const bool modeMatches = !storedMode.isEmpty() &&
                                 storedMode.compare(resolvedMode, Qt::CaseInsensitive) == 0;
        QString storedDate;
        QString storedTime;
        const bool hasStoredTime = extractEventDateTimeFromText(cleanStored, storedDate, storedTime);
        const bool timeMatches = date.isEmpty() || time.isEmpty() ||
                                 (hasStoredTime && eventDateTimeWithinSeconds(date, time, storedDate, storedTime));
        if (modeMatches && timeMatches) {
            qDebug() << "[PATH-CHECK][CSV] VERIFIED stored event path:"
                     << cleanStored << "mode=" << resolvedMode
                     << "size=" << QFileInfo(cleanStored).size();
            return QFileInfo(cleanStored).absoluteFilePath();
        }
        qWarning() << "[PATH-CHECK][CSV] stored file exists but event context mismatches:"
                   << cleanStored
                   << "storedMode=" << storedMode
                   << "expectedMode=" << resolvedMode
                   << "expectedDate=" << date
                   << "expectedTime=" << time;
    }

    const QString eventDir = (!date.isEmpty() && !time.isEmpty())
        ? QDir(EVENT_PATH).filePath(resolvedMode + QStringLiteral("/") + date + QStringLiteral("/") + time)
        : QString();

    if (!eventDir.isEmpty() && !recoveredName.isEmpty()) {
        const QStringList names = exactFileCandidates(recoveredName);
        for (const QString &name : names) {
            const QString expected = QDir(eventDir).absoluteFilePath(name);
            if (isForbiddenEventCsvCandidate(expected)) {
                qWarning() << "[PATH-CHECK][CSV][REJECT] canonical candidate is Pattern:" << expected;
                continue;
            }
            if (isVerifiedLocalFile(expected)) {
                qWarning() << "[PATH-RECOVERY][CSV] RECOVERED canonical path:"
                           << "stored=" << storedPath
                           << "expected=" << expected
                           << "mode=" << resolvedMode;
                return QFileInfo(expected).absoluteFilePath();
            }
        }
    }

    // If the name itself was lost, recover only when the exact event folder
    // has one unambiguous non-margin file. This avoids selecting a stale event.
    if (!eventDir.isEmpty() && recoveredName.isEmpty()) {
        QDir dir(eventDir);
        if (dir.exists()) {
            QStringList valid;
            const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::NoSymLinks,
                                                            QDir::Name);
            const QString compactStamp = date;
            QString compactDate = compactStamp;
            compactDate.remove(QLatin1Char('-'));
            QString compactTime = time;
            compactTime.remove(QLatin1Char('-'));
            const QString stamp = compactDate + QStringLiteral("_") + compactTime;

            for (const QFileInfo &entry : entries) {
                if (!isVerifiedLocalFile(entry.absoluteFilePath())) {
                    continue;
                }
                if (entry.fileName().contains(QStringLiteral("Margin"), Qt::CaseInsensitive)) {
                    continue;
                }
                if (isForbiddenEventCsvCandidate(entry.absoluteFilePath())) {
                    qWarning() << "[PATH-CHECK][CSV][REJECT] Pattern file found in Event directory; never use as Event CSV:"
                               << entry.absoluteFilePath();
                    continue;
                }
                valid << entry.absoluteFilePath();
            }

            if (valid.size() > 1) {
                QStringList timestampMatches;
                for (const QString &candidate : valid) {
                    if (QFileInfo(candidate).fileName().contains(stamp)) {
                        timestampMatches << candidate;
                    }
                }
                if (timestampMatches.size() == 1) {
                    valid = timestampMatches;
                }
            }

            if (valid.size() == 1) {
                qWarning() << "[PATH-RECOVERY][CSV] name was empty; recovered unique event file:"
                           << valid.first();
                return valid.first();
            }
            if (valid.size() > 1) {
                qWarning() << "[PATH-RECOVERY][CSV] multiple files in event folder; refuse ambiguous recovery:"
                           << eventDir;
            }
        }
    }

    // If both stored path and CSV name were lost/stale, search only the
    // current mode/date and accept a file only when its embedded/path timestamp
    // is close to the current picture event. Multiple matches are ambiguous.
    if (!date.isEmpty() && !time.isEmpty() && recoveredName.isEmpty()) {
        const QString dateDirPath = QDir(EVENT_PATH).filePath(resolvedMode + QStringLiteral("/") + date);
        QDir dateDir(dateDirPath);
        if (dateDir.exists()) {
            QStringList nearMatches;
            QDirIterator it(dateDir.absolutePath(), QDir::Files | QDir::NoSymLinks,
                            QDirIterator::Subdirectories);
            int inspected = 0;
            while (it.hasNext() && inspected < 4000) {
                const QString candidate = it.next();
                ++inspected;
                const QFileInfo info(candidate);
                if (!isVerifiedLocalFile(candidate) ||
                    info.fileName().contains(QStringLiteral("Margin"), Qt::CaseInsensitive) ||
                    isForbiddenEventCsvCandidate(candidate)) {
                    if (isForbiddenEventCsvCandidate(candidate)) {
                        qWarning() << "[PATH-CHECK][CSV][REJECT] nearby candidate is Pattern:" << candidate;
                    }
                    continue;
                }

                QString candidateDate;
                QString candidateTime;
                if (extractEventDateTimeFromText(candidate, candidateDate, candidateTime) &&
                    eventDateTimeWithinSeconds(date, time, candidateDate, candidateTime)) {
                    nearMatches << info.absoluteFilePath();
                    if (nearMatches.size() > 1) {
                        break;
                    }
                }
            }

            if (nearMatches.size() == 1) {
                qWarning() << "[PATH-RECOVERY][CSV] path/name were lost; recovered nearby event file:"
                           << nearMatches.first();
                return nearMatches.first();
            }
            if (nearMatches.size() > 1) {
                qWarning() << "[PATH-RECOVERY][CSV] multiple nearby event files; refuse ambiguous recovery:"
                           << "mode=" << resolvedMode << "date=" << date << "time=" << time;
            }
        }
    }

    // Last bounded fallback: exact-name search inside mode/date only.
    if (!date.isEmpty() && !recoveredName.isEmpty()) {
        const QString dateDirPath = QDir(EVENT_PATH).filePath(resolvedMode + QStringLiteral("/") + date);
        QDir dateDir(dateDirPath);
        if (dateDir.exists()) {
            QStringList found;
            const QStringList names = exactFileCandidates(recoveredName);
            QDirIterator it(dateDir.absolutePath(), names,
                            QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
            int inspected = 0;
            while (it.hasNext() && inspected < 4000) {
                const QString candidate = it.next();
                ++inspected;
                if (isVerifiedLocalFile(candidate) && !isForbiddenEventCsvCandidate(candidate)) {
                    found << QFileInfo(candidate).absoluteFilePath();
                    if (found.size() > 1) {
                        break;
                    }
                }
            }
            if (found.size() == 1) {
                qWarning() << "[PATH-RECOVERY][CSV] RECOVERED by mode/date search:"
                           << found.first();
                return found.first();
            }
            if (found.size() > 1) {
                qWarning() << "[PATH-RECOVERY][CSV] ambiguous exact-name matches; refuse recovery:"
                           << recoveredName << "mode=" << resolvedMode << "date=" << date;
            }
        }
    }

    qWarning() << "[PATH-CHECK][CSV][FAILED]"
               << "stored=" << storedPath
               << "name=" << recoveredName
               << "mode=" << resolvedMode
               << "date=" << date
               << "time=" << time;
    return QString();
}

void PLCServer::getRawDataADCRemote(QString url)
{
    qDebug() << "getRawDataADCRemote:" << url;

    const QString urlFileName = safeFileNameOnly(QFileInfo(QUrl(url).path()).fileName(),
                                                 QStringLiteral("remote.raw"));
    const QString chanels = urlFileName.section('_', -1).section('.', 0, 0);

    if (!isValidRemoteAdcChannel(chanels)) {
        qWarning() << "[getRawDataADCRemote] invalid channel from URL:"
                   << chanels << "fileName=" << urlFileName << "url=" << url;
        return;
    }

    const QString destPath = QStringLiteral("/home/pi/Rawdata/%1.raw").arg(chanels);
    if (!downloadUrlToFile(url, destPath)) {
        qWarning() << "[getRawDataADCRemote] download failed:" << url << "->" << destPath;
        return;
    }

    runProcessChecked(QStringLiteral("sync"), QStringList{}, 10000);

    if (chanels == QStringLiteral("data0")) {
        phaseSurge = "A";
        qDebug() << "phaseSurgeA:" << phaseSurge << chanels;
        surgeEventRemote(sagFactor, PositionFromLocal, PositionFromRemote, samplingrate, local_nanosec, remote_nanosec, phaseSurge);
    } else if (chanels == QStringLiteral("data1")) {
        phaseSurge = "B";
        qDebug() << "phaseSurgeB:" << phaseSurge << chanels;
        surgeEventRemote(sagFactor, PositionFromLocal, PositionFromRemote, samplingrate, local_nanosec, remote_nanosec, phaseSurge);
    } else if (chanels == QStringLiteral("data2")) {
        phaseSurge = "C";
        qDebug() << "phaseSurgeC:" << phaseSurge << chanels;
        surgeEventRemote(sagFactor, PositionFromLocal, PositionFromRemote, samplingrate, local_nanosec, remote_nanosec, phaseSurge);
    }
}

void PLCServer::getRawDataADC(QString filename, QString url, QString chanels, QString time, QString date, QString timestamp)
{
    qDebug() << "filename:" << filename << " url:" << url << " chanels:" << chanels << "time" << time << "date" << date << "timestamp" << timestamp;

    FileTimeStamp = timestamp;
    chanels = chanels.trimmed();

    if (!isValidAdcChannel(chanels)) {
        qWarning() << "[getRawDataADC] invalid ADC channel:" << chanels;
        return;
    }

    const QString destPath = QStringLiteral("/home/pi/Rawdata/data%1.raw").arg(chanels);
    if (!downloadUrlToFile(url, destPath)) {
        qWarning() << "[getRawDataADC] download failed:" << url << "->" << destPath;
        return;
    }

    runProcessChecked(QStringLiteral("sync"), QStringList{}, 10000);

    // modeName = "Surge";

    qDebug() << "interlockPattern:" << interlockPattern << " numOfPattern:" << numOfPattern << " ::modeName::" << modeName;
    if (!interlockPattern) {
        qDebug() << "getRawDataADCinterlockPattern:" << interlockPattern << modeName;
        if (modeName == "Surge") {
            SurgeMode = true;
            QThread::msleep(500);
            if ((chanels == "0")) {
                phaseSurge = "A";
                qDebug() << "phaseSurgeA:" << phaseSurge << chanels;
                surgeEvent(sagFactor, PositionFromLocal, PositionFromRemote, samplingrate, local_nanosec, remote_nanosec, phaseSurge);
                //                resultMaxListSurgeA.clear();
            } else if ((chanels == "1")) {
                phaseSurge = "B";
                qDebug() << "phaseSurgeB:" << phaseSurge << chanels;
                surgeEvent(sagFactor, PositionFromLocal, PositionFromRemote, samplingrate, local_nanosec, remote_nanosec, phaseSurge);
                //                resultMaxListSurgeB.clear();
            } else if ((chanels == "2")) {
                phaseSurge = "C";
                qDebug() << "phaseSurgeC:" << phaseSurge << chanels;
                surgeEvent(sagFactor, PositionFromLocal, PositionFromRemote, samplingrate, local_nanosec, remote_nanosec, phaseSurge);
                //                resultMaxListSurgeC.clear();
            }
        } else {
            if(modeName.contains("Pattern")){
                QString combinedData = QString(
                                               "{"
                                               "\"objectName\":\"combinedDataPhaseA\","
                                               "\"margin\":%1,"
                                               "\"valueVoltage\":%2,"
                                               "\"focusIndex\":%3,"
                                               "\"PHASE\":\"%4\""
                                               "}")
                                               .arg(100)
                                               .arg(0)
                                               .arg(-1)
                                               .arg("A");
                emit updataListOfMarginA(combinedData);

                combinedData = QString(
                                               "{"
                                               "\"objectName\":\"combinedDataPhaseB\","
                                               "\"margin\":%1,"
                                               "\"valueVoltage\":%2,"
                                               "\"focusIndex\":%3,"
                                               "\"PHASE\":\"%4\""
                                               "}")
                                               .arg(100)
                                               .arg(0)
                                               .arg(-1)
                                               .arg("B");
                emit updataListOfMarginB(combinedData);

                combinedData = QString(
                                               "{"
                                               "\"objectName\":\"combinedDataPhaseC\","
                                               "\"margin\":%1,"
                                               "\"valueVoltage\":%2,"
                                               "\"focusIndex\":%3,"
                                               "\"PHASE\":\"%4\""
                                               "}")
                                               .arg(100)
                                               .arg(0)
                                               .arg(-1)
                                               .arg("C");
                emit updataListOfMarginC(combinedData);

                combinedData = QString(
                                           "{"
                                           "\"objectName\":\"combinedDataPhaseA\","
                                           "\"margin\":%1,"
                                           "\"valueVoltage\":%2,"
                                           "\"focusIndex\":%3,"
                                           "\"PHASE\":\"%4\""
                                           "}")
                                           .arg(1)
                                           .arg(0)
                                           .arg(0)
                                           .arg("A");

                qDebug() << "Combined Data for Phase A:" << combinedData;
                emit UpdateMarginSettingParameter(combinedData);


                combinedData = QString(
                                       "{"
                                       "\"objectName\":\"combinedDataPhaseB\","
                                       "\"margin\":%1,"
                                       "\"valueVoltage\":%2,"
                                       "\"focusIndex\":%3,"
                                       "\"PHASE\":\"%4\""
                                       "}")
                                       .arg(1)
                                       .arg(0)
                                       .arg(0)
                                       .arg("B");
                emit UpdateMarginSettingParameter(combinedData);

                combinedData = QString(
                                       "{"
                                       "\"objectName\":\"combinedDataPhaseC\","
                                       "\"margin\":%1,"
                                       "\"valueVoltage\":%2,"
                                       "\"focusIndex\":%3,"
                                       "\"PHASE\":\"%4\""
                                       "}")
                                       .arg(1)
                                       .arg(0)
                                       .arg(0)
                                       .arg("C");
                emit UpdateMarginSettingParameter(combinedData);

                QString singleMarginDataA = QString(
                                               "{\"objectName\":\"marginlistCountA\", "
                                               "\"no\":%1, "
                                               "\"marginNo\":\"%2\", "
                                               "\"valueOfMargin\":%3, "
                                               "\"maxmargin\":%4, "
                                               "\"unit\":\"%5\"}")
                                               .arg(1)
                                               .arg("Margin"+QString::number(1))
                                               .arg(0)
                                                .arg(1)
                                               .arg("mV");

                QString singleMarginDataB = QString(
                                               "{\"objectName\":\"marginlistCountB\", "
                                               "\"no\":%1, "
                                               "\"marginNo\":\"%2\", "
                                               "\"valueOfMargin\":%3, "
                                               "\"maxmargin\":%4, "
                                               "\"unit\":\"%5\"}")
                                               .arg(1)
                                               .arg("Margin"+QString::number(1))
                                               .arg(0)
                                               .arg(1)
                                               .arg("mV");

                QString singleMarginDataC = QString(
                                               "{\"objectName\":\"marginlistCountC\", "
                                               "\"no\":%1, "
                                               "\"marginNo\":\"%2\", "
                                               "\"valueOfMargin\":%3, "
                                               "\"maxmargin\":%4, "
                                               "\"unit\":\"%5\"}")
                                               .arg(1)
                                               .arg("Margin"+QString::number(1))
                                               .arg(0)
                                               .arg(1)
                                               .arg("mV");

                QString autoMarginDataA = QString(
                                               "{\"objectName\":\"valueMarginVoltageAauto\", "
                                               "\"valueVoltageA\":0}");

                QString autoMarginDataB = QString(
                                               "{\"objectName\":\"valueMarginVoltageBauto\", "
                                               "\"valueVoltageB\":0}");

                QString autoMarginDataC = QString(
                                               "{\"objectName\":\"valueMarginVoltageCauto\", "
                                               "\"valueVoltageC\":0}");

                emit selectMarginSettingParameterSignal();
                if(interlockPressPattern == true){
                    myDatabase->deleteSelectPattern();
                }
                Q_FOREACH(QWebSocket * pClient, Monitor_address) {
                    if (pClient -> state() == QAbstractSocket::ConnectedState){
                        qDebug() << "sendMessage(singleMarginDataA, pClient)";
                        emit sendMessage(singleMarginDataA, pClient);
                        emit sendMessage(autoMarginDataA, pClient);
                        emit sendMessage(singleMarginDataB, pClient);
                        emit sendMessage(autoMarginDataB, pClient);
                        emit sendMessage(singleMarginDataC, pClient);
                        emit sendMessage(autoMarginDataC, pClient);
                    }
                    else
                        qDebug() << "Monitor_address:" << pClient -> state();
                }
                emit sendToVNC(singleMarginDataA);
                emit sendToVNC(autoMarginDataA);
                emit sendToVNC(singleMarginDataB);
                emit sendToVNC(autoMarginDataB);
                emit sendToVNC(singleMarginDataC);
                emit sendToVNC(autoMarginDataC);
            }
            qDebug() << "getRawDataADCinterlockPattern else :" << interlockPattern << modeName;
            if (chanels == "0") {
                qDebug() << "plotGraphA";
                plotGraphA(sagFactor, samplingRate, distanceToStart, distanceToShow, fulldistance, thresholdA);
            } else if (chanels == "1") {
                qDebug() << "plotGraphB";
                plotGraphB(sagFactor, samplingRate, distanceToStart, distanceToShow, fulldistance, thresholdB);
            } else if (chanels == "2") {
                qDebug() << "plotGraphC";
                plotGraphC(sagFactor, samplingRate, distanceToStart, distanceToShow, fulldistance, thresholdC);
            }
        }
    } else {
        if(modeName.contains("Pattern")){
            QString combinedData = QString(
                                           "{"
                                           "\"objectName\":\"combinedDataPhaseA\","
                                           "\"margin\":%1,"
                                           "\"valueVoltage\":%2,"
                                           "\"focusIndex\":%3,"
                                           "\"PHASE\":\"%4\""
                                           "}")
                                           .arg(100)
                                           .arg(0)
                                           .arg(-1)
                                           .arg("A");
            emit updataListOfMarginA(combinedData);

            combinedData = QString(
                                           "{"
                                           "\"objectName\":\"combinedDataPhaseB\","
                                           "\"margin\":%1,"
                                           "\"valueVoltage\":%2,"
                                           "\"focusIndex\":%3,"
                                           "\"PHASE\":\"%4\""
                                           "}")
                                           .arg(100)
                                           .arg(0)
                                           .arg(-1)
                                           .arg("B");
            emit updataListOfMarginB(combinedData);

            combinedData = QString(
                                           "{"
                                           "\"objectName\":\"combinedDataPhaseC\","
                                           "\"margin\":%1,"
                                           "\"valueVoltage\":%2,"
                                           "\"focusIndex\":%3,"
                                           "\"PHASE\":\"%4\""
                                           "}")
                                           .arg(100)
                                           .arg(0)
                                           .arg(-1)
                                           .arg("C");
            emit updataListOfMarginC(combinedData);

            combinedData = QString(
                                       "{"
                                       "\"objectName\":\"combinedDataPhaseA\","
                                       "\"margin\":%1,"
                                       "\"valueVoltage\":%2,"
                                       "\"focusIndex\":%3,"
                                       "\"PHASE\":\"%4\""
                                       "}")
                                       .arg(1)
                                       .arg(0)
                                       .arg(0)
                                       .arg("A");

            qDebug() << "Combined Data for Phase A:" << combinedData;
            emit UpdateMarginSettingParameter(combinedData);


            combinedData = QString(
                                   "{"
                                   "\"objectName\":\"combinedDataPhaseB\","
                                   "\"margin\":%1,"
                                   "\"valueVoltage\":%2,"
                                   "\"focusIndex\":%3,"
                                   "\"PHASE\":\"%4\""
                                   "}")
                                   .arg(1)
                                   .arg(0)
                                   .arg(0)
                                   .arg("B");
            emit UpdateMarginSettingParameter(combinedData);

            combinedData = QString(
                                   "{"
                                   "\"objectName\":\"combinedDataPhaseC\","
                                   "\"margin\":%1,"
                                   "\"valueVoltage\":%2,"
                                   "\"focusIndex\":%3,"
                                   "\"PHASE\":\"%4\""
                                   "}")
                                   .arg(1)
                                   .arg(0)
                                   .arg(0)
                                   .arg("C");
            emit UpdateMarginSettingParameter(combinedData);

            QString singleMarginDataA = QString(
                                           "{\"objectName\":\"marginlistCountA\", "
                                           "\"no\":%1, "
                                           "\"marginNo\":\"%2\", "
                                           "\"valueOfMargin\":%3, "
                                           "\"maxmargin\":%4, "
                                           "\"unit\":\"%5\"}")
                                           .arg(1)
                                           .arg("Margin"+QString::number(1))
                                           .arg(0)
                                            .arg(1)
                                           .arg("mV");

            QString singleMarginDataB = QString(
                                           "{\"objectName\":\"marginlistCountB\", "
                                           "\"no\":%1, "
                                           "\"marginNo\":\"%2\", "
                                           "\"valueOfMargin\":%3, "
                                           "\"maxmargin\":%4, "
                                           "\"unit\":\"%5\"}")
                                           .arg(1)
                                           .arg("Margin"+QString::number(1))
                                           .arg(0)
                                           .arg(1)
                                           .arg("mV");

            QString singleMarginDataC = QString(
                                           "{\"objectName\":\"marginlistCountC\", "
                                           "\"no\":%1, "
                                           "\"marginNo\":\"%2\", "
                                           "\"valueOfMargin\":%3, "
                                           "\"maxmargin\":%4, "
                                           "\"unit\":\"%5\"}")
                                           .arg(1)
                                           .arg("Margin"+QString::number(1))
                                           .arg(0)
                                           .arg(1)
                                           .arg("mV");

            QString autoMarginDataA = QString(
                                           "{\"objectName\":\"valueMarginVoltageAauto\", "
                                           "\"valueVoltageA\":0}");

            QString autoMarginDataB = QString(
                                           "{\"objectName\":\"valueMarginVoltageBauto\", "
                                           "\"valueVoltageB\":0}");

            QString autoMarginDataC = QString(
                                           "{\"objectName\":\"valueMarginVoltageCauto\", "
                                           "\"valueVoltageC\":0}");

            emit selectMarginSettingParameterSignal();
            if(interlockPressPattern == true){
                myDatabase->deleteSelectPattern();
            }
            Q_FOREACH(QWebSocket * pClient, Monitor_address) {
                if (pClient -> state() == QAbstractSocket::ConnectedState){
                    qDebug() << "sendMessage(singleMarginDataA, pClient)";
                    emit sendMessage(singleMarginDataA, pClient);
                    emit sendMessage(autoMarginDataA, pClient);
                    emit sendMessage(singleMarginDataB, pClient);
                    emit sendMessage(autoMarginDataB, pClient);
                    emit sendMessage(singleMarginDataC, pClient);
                    emit sendMessage(autoMarginDataC, pClient);
                }
                else
                    qDebug() << "Monitor_address:" << pClient -> state();
            }
            emit sendToVNC(singleMarginDataA);
            emit sendToVNC(autoMarginDataA);
            emit sendToVNC(singleMarginDataB);
            emit sendToVNC(autoMarginDataB);
            emit sendToVNC(singleMarginDataC);
            emit sendToVNC(autoMarginDataC);
        }
        if (chanels == "0") {
            plotGraphA(sagFactor, samplingRate, distanceToStart, distanceToShow, fulldistance, thresholdA);
            if (voltageListA.size() < 0 || kmListA.size() < 0 || maxVoltageListA.size() < 0 || maxKmListA.size() < 0 || resultMaxListA.size() < 0) {
                return;
            }
            if (numOfPattern == maxNumOfPattern) {
                qDebug() << "yes it is pattern" << maxNumOfPattern;
                if (findMaxEachIndex("A",voltageListA, kmListA, maxVoltageListA, maxKmListA, resultMaxListA)) {
                    resultMaxListA.clear();
                    voltageListA.clear();
                    maxVoltageListA.clear();
                    kmListA.clear();
                    maxKmListA.clear();
                    qDebug() << "reSamplingNormalizationA pattern A numOfPattern:" << numOfPattern << interlockPattern;

                }
            }
        } else if (chanels == "1") {
            plotGraphB(sagFactor, samplingRate, distanceToStart, distanceToShow, fulldistance, thresholdB);
            if (voltageListB.size() < 0 || kmListB.size() < 0 || maxVoltageListB.size() < 0 || maxKmListB.size() < 0 || resultMaxListB.size() < 0) {
                return;
            }
            if (numOfPattern == maxNumOfPattern) {
                if (findMaxEachIndex("B",voltageListB, kmListB, maxVoltageListB, maxKmListB, resultMaxListB)) {
                    resultMaxListB.clear();
                    voltageListB.clear();
                    maxVoltageListB.clear();
                    kmListB.clear();
                    maxKmListB.clear();
                }
            }
        } else if (chanels == "2") {
            plotGraphC(sagFactor, samplingRate, distanceToStart, distanceToShow, fulldistance, thresholdC);
            if (voltageListC.size() < 0 || kmListC.size() < 0 || maxVoltageListC.size() < 0 || maxKmListC.size() < 0 || resultMaxListC.size() < 0) {
                return;
            }
            if (numOfPattern == maxNumOfPattern) {
                if (findMaxEachIndex("C",voltageListC, kmListC, maxVoltageListC, maxKmListC, resultMaxListC)) {
                    resultMaxListC.clear();
                    voltageListC.clear();
                    maxVoltageListC.clear();
                    kmListC.clear();
                    maxKmListC.clear();
                }
            }
        }
    }
    safeRemoveFile(destPath);
}

void PLCServer::getScreenPictureandSave(QString link, QString fileName) {
    const QString requestedOwnerName = QFileInfo(fileName.trimmed()).fileName();

    // The ScreenPicture must already own the current event's reserved Picture
    // slot.  Never let a direct/late call start a second transaction or erase
    // the verified paths of the canonical Picture.
    if (!eventPictureActive || eventPictureState != EventPictureProcessing ||
        acceptedPictureName.compare(requestedOwnerName, Qt::CaseInsensitive) != 0) {
        qWarning() << "[PIC-OWNER][BLOCK-TRANSACTION]"
                   << "eventId=" << activePictureEventId
                   << "state=" << eventPictureStateName()
                   << "accepted=" << acceptedPictureName
                   << "requested=" << requestedOwnerName;
        return;
    }

    // Snapshot all event-sensitive values before doing any filesystem/FTP work.
    // New events may update mutable members later, but this transaction must keep
    // one coherent event context until the final publish gate.
    const QString storedCsvPathSnapshot = fullpathCSV;
    const QString storedCsvNameSnapshot = fullnameCSV;

    QString eventModeSnapshot = canonicalRecoveryMode(pendingScreenMode);
    if (eventModeSnapshot.isEmpty()) {
        eventModeSnapshot = canonicalRecoveryMode(modeName);
    }
    if (eventModeSnapshot.isEmpty()) {
        eventModeSnapshot = inferRecoveryModeFromPath(storedCsvPathSnapshot);
    }

    if (eventModeSnapshot != QStringLiteral("Manual") &&
        eventModeSnapshot != QStringLiteral("Relay") &&
        eventModeSnapshot != QStringLiteral("Surge") &&
        eventModeSnapshot != QStringLiteral("Periodic")) {
        qWarning() << "[EVENT-GATE][BLOCK] invalid/unsupported event mode before picture transaction:"
                   << eventModeSnapshot << "pending=" << pendingScreenMode << "modeName=" << modeName;
        QJsonObject d;
        d.insert(QStringLiteral("resolvedMode"), eventModeSnapshot);
        d.insert(QStringLiteral("pendingScreenMode"), pendingScreenMode);
        d.insert(QStringLiteral("modeName"), modeName);
        auditEventStep(QStringLiteral("EVENT_MODE_RESOLVED"),
                       QStringLiteral("Resolve event mode and identity"),
                       QStringLiteral("FAIL"),
                       QStringLiteral("Picture transaction blocked because event mode is invalid or unsupported"),
                       pendingScreenMode, eventModeSnapshot, d);
        finishEventAudit(false, QStringLiteral("Event blocked: invalid or unsupported event mode"));
        releaseEventPictureReservationForRetry(requestedOwnerName,
                                               QStringLiteral("invalid/unsupported event mode"));
        return;
    }

    fileNamePic = safeFileNameOnly(fileName, QStringLiteral("screen.png"));
    const QString picNameSnapshot = fileNamePic;
    if (picNameSnapshot.isEmpty()) {
        qWarning() << "[EVENT-GATE][BLOCK] picture filename is empty, link=" << link;
        QJsonObject d;
        d.insert(QStringLiteral("link"), link);
        d.insert(QStringLiteral("reportedFileName"), fileName);
        auditEventStep(QStringLiteral("PICTURE_DOWNLOAD"),
                       QStringLiteral("Picture download/re-download and physical check"),
                       QStringLiteral("FAIL"),
                       QStringLiteral("Picture transaction cannot start because filename is empty"),
                       link, PIC_PATH, d);
        finishEventAudit(false, QStringLiteral("Event blocked: Picture filename is empty"));
        releaseEventPictureReservationForRetry(requestedOwnerName,
                                               QStringLiteral("empty Picture filename"));
        return;
    }

    QString eventDate = eventBaseDateSnapshot.trimmed();
    QString eventTime = eventBaseTimeSnapshot.trimmed();
    const QString baseMode = canonicalRecoveryMode(eventBaseModeSnapshot);

    // Production contract: eventRecord is the only authority allowed to open a
    // Picture transaction.  DateKept/TimeKept and embedded Picture/CSV times are
    // correlation data only and must never resurrect a completed/old event.
    bool eventBaseOk = normalizeEventDateTime(eventDate, eventTime);
    if (eventBaseOk && !baseMode.isEmpty() &&
        baseMode.compare(eventModeSnapshot, Qt::CaseInsensitive) != 0) {
        qWarning() << "[EVENT-BASE][STALE] mode mismatch; block picture transaction:"
                   << "baseMode=" << baseMode << "eventMode=" << eventModeSnapshot
                   << "date=" << eventDate << "time=" << eventTime;
        eventBaseOk = false;
    }

    if (!eventBaseOk) {
        qWarning() << "[EVENT-BASE][BLOCK] no valid active eventRecord base;"
                   << "DateKept/TimeKept fallback is intentionally disabled for ScreenPicture:"
                   << "eventId=" << activePictureEventId
                   << "picture=" << picNameSnapshot
                   << "base=" << eventBaseDateSnapshot << eventBaseTimeSnapshot;
        QJsonObject d;
        d.insert(QStringLiteral("eventId"), activePictureEventId);
        d.insert(QStringLiteral("pictureName"), picNameSnapshot);
        d.insert(QStringLiteral("eventBaseDate"), eventBaseDateSnapshot);
        d.insert(QStringLiteral("eventBaseTime"), eventBaseTimeSnapshot);
        d.insert(QStringLiteral("eventBaseMode"), eventBaseModeSnapshot);
        auditEventStep(QStringLiteral("EVENT_MODE_RESOLVED"),
                       QStringLiteral("Resolve event mode and identity"),
                       QStringLiteral("FAIL"),
                       QStringLiteral("ScreenPicture has no active authoritative eventRecord base; stale DateKept/TimeKept fallback is disabled"),
                       picNameSnapshot, QStringLiteral("Picture transaction blocked"), d);
        finishEventAudit(false, QStringLiteral("Event blocked: no active eventRecord base for Picture"));
        releaseEventPictureReservationForRetry(picNameSnapshot,
                                               QStringLiteral("no authoritative eventRecord base"));
        return;
    }

    qDebug() << "[EVENT-BASE][USE]"
             << "eventId=" << activePictureEventId
             << "mode=" << eventModeSnapshot
             << "date=" << eventDate
             << "time=" << eventTime
             << "pictureCaptureName=" << picNameSnapshot;

    // Reject a stale Event base before creating any Picture directory.  The
    // Picture capture may legitimately be a few seconds later, but it must stay
    // inside the same event correlation window.
    QString captureDateCheck;
    QString captureTimeCheck;
    if (extractEventDateTimeFromText(picNameSnapshot, captureDateCheck, captureTimeCheck) &&
        !eventDateTimeWithinSeconds(eventDate, eventTime, captureDateCheck, captureTimeCheck)) {
        qWarning() << "[EVENT-BASE][BLOCK] Picture does not correlate with authoritative Event base:"
                   << "eventBase=" << eventDate << eventTime
                   << "picture=" << captureDateCheck << captureTimeCheck
                   << "file=" << picNameSnapshot;
        QJsonObject d;
        d.insert(QStringLiteral("eventDate"), eventDate);
        d.insert(QStringLiteral("eventTime"), eventTime);
        d.insert(QStringLiteral("pictureDate"), captureDateCheck);
        d.insert(QStringLiteral("pictureTime"), captureTimeCheck);
        auditEventStep(QStringLiteral("EVENT_MODE_RESOLVED"),
                       QStringLiteral("Resolve event mode and identity"),
                       QStringLiteral("FAIL"),
                       QStringLiteral("Picture timestamp is outside the authoritative Event base correlation window"),
                       picNameSnapshot, QStringLiteral("Picture transaction blocked"), d);
        finishEventAudit(false, QStringLiteral("Event blocked: stale/mismatched Event base timestamp"));
        releaseEventPictureReservationForRetry(picNameSnapshot,
                                               QStringLiteral("Picture timestamp outside Event correlation window"));
        return;
    }

    pictureEventDateSnapshot = eventDate;
    pictureEventTimeSnapshot = eventTime;
    pictureEventModeSnapshot = eventModeSnapshot;

    ensureEventAuditContext(eventModeSnapshot,
                            eventDate,
                            eventTime,
                            QStringLiteral("%1_EVENT").arg(eventModeSnapshot.toUpper()),
                            picNameSnapshot);
    QJsonObject contextDetail;
    contextDetail.insert(QStringLiteral("pictureName"), picNameSnapshot);
    contextDetail.insert(QStringLiteral("storedCsvPathSnapshot"), storedCsvPathSnapshot);
    contextDetail.insert(QStringLiteral("storedCsvNameSnapshot"), storedCsvNameSnapshot);
    contextDetail.insert(QStringLiteral("pendingScreenMode"), pendingScreenMode);
    contextDetail.insert(QStringLiteral("resolvedMode"), eventModeSnapshot);
    contextDetail.insert(QStringLiteral("eventDate"), eventDate);
    contextDetail.insert(QStringLiteral("eventTime"), eventTime);
    auditEventStep(QStringLiteral("EVENT_MODE_RESOLVED"),
                   QStringLiteral("Resolve event mode and identity"),
                   QStringLiteral("PASS"),
                   QStringLiteral("Frozen event mode/date/time and CSV state for the picture transaction"),
                   QStringLiteral("mutable PLC event state"),
                   QStringLiteral("immutable picture transaction snapshot"),
                   contextDetail);

    // New local picture layout:
    // /event_records/Pic/<yyyy-MM-dd>/<HH-mm-ss>/<picture>
    const QString pictureDir = QDir(PIC_PATH).filePath(eventDate + QStringLiteral("/") + eventTime);
    if (!ensureDirExists(pictureDir)) {
        qWarning() << "[EVENT-GATE][BLOCK] cannot create event picture directory:" << pictureDir;
        QJsonObject d;
        d.insert(QStringLiteral("pictureDir"), pictureDir);
        auditEventStep(QStringLiteral("PICTURE_DIRECTORY_READY"),
                       QStringLiteral("Picture event directory created"),
                       QStringLiteral("FAIL"),
                       QStringLiteral("Cannot create/access the event-specific Picture directory"),
                       PIC_PATH, pictureDir, d);
        finishEventAudit(false, QStringLiteral("Event blocked: cannot create/access Picture event directory"));
        releaseEventPictureReservationForRetry(picNameSnapshot,
                                               QStringLiteral("cannot create/access Picture event directory"));
        return;
    }

    const QString requestedPicPath = QDir(pictureDir).absoluteFilePath(picNameSnapshot);

    // Filesystem idempotency guard: a different valid image already present in
    // this exact event folder is treated as the canonical Picture.  Never add a
    // second filename to the same event folder.  The same requested filename is
    // allowed so a failed FTP/notification transaction can retry safely.
    QStringList otherValidPictures;
    {
        QDir existingDir(pictureDir);
        const QFileInfoList existingFiles = existingDir.entryInfoList(
            QDir::Files | QDir::NoSymLinks, QDir::Name);
        for (const QFileInfo &entry : existingFiles) {
            if (entry.fileName().endsWith(QStringLiteral(".download"), Qt::CaseInsensitive))
                continue;
            if (!isVerifiedDownloadedPictureFile(entry.absoluteFilePath()))
                continue;
            if (entry.fileName().compare(picNameSnapshot, Qt::CaseInsensitive) != 0)
                otherValidPictures << entry.absoluteFilePath();
        }
    }

    if (!otherValidPictures.isEmpty()) {
        qWarning() << "[PIC-OWNER][FS-DUPLICATE][BLOCK]"
                   << "eventId=" << activePictureEventId
                   << "requested=" << requestedPicPath
                   << "existingCanonical=" << otherValidPictures;
        QJsonObject d;
        d.insert(QStringLiteral("eventId"), activePictureEventId);
        d.insert(QStringLiteral("requestedPicture"), requestedPicPath);
        d.insert(QStringLiteral("existingCanonicalPicture"), otherValidPictures.first());
        d.insert(QStringLiteral("existingCount"), otherValidPictures.size());
        auditEventStep(QStringLiteral("SCREEN_DUPLICATE_FILTER"),
                       QStringLiteral("Reject duplicate/late ScreenPicture"),
                       QStringLiteral("BLOCK"),
                       QStringLiteral("A different valid Picture already exists in this event folder; refusing to create a second Picture"),
                       requestedPicPath, otherValidPictures.first(), d);
        finishEventAudit(false, QStringLiteral("Event Picture blocked: event folder already contains another canonical Picture"));
        closeEventPictureOwnership(QStringLiteral("filesystem already contains another Picture for this event"));
        return;
    }

    {
        QJsonObject d;
        d.insert(QStringLiteral("pictureDir"), pictureDir);
        d.insert(QStringLiteral("requestedPicPath"), requestedPicPath);
        auditEventStep(QStringLiteral("PICTURE_DIRECTORY_READY"),
                       QStringLiteral("Picture event directory created"),
                       QStringLiteral("PASS"),
                       QStringLiteral("Event-specific Picture directory is ready"),
                       PIC_PATH, requestedPicPath, d);
    }

    // Download once + two re-downloads. Every attempt is accepted only when the
    // physical file exists, is readable, has size > 0 and has a valid image signature.
    const int maxDownloadAttempts = 3;
    bool pictureDownloadedAndVerified = false;
    qint64 downloadedPicSize = 0;

    for (int attempt = 1; attempt <= maxDownloadAttempts; ++attempt) {
        if (!safeRemoveFile(requestedPicPath + QStringLiteral(".download"))) {
            qWarning() << "[PIC-DOWNLOAD][FAILED] cannot clear interrupted temp download:"
                       << requestedPicPath + QStringLiteral(".download");
            QJsonObject d;
            d.insert(QStringLiteral("attempt"), attempt);
            d.insert(QStringLiteral("tempPath"), requestedPicPath + QStringLiteral(".download"));
            auditEventStep(QStringLiteral("PICTURE_DOWNLOAD"),
                           QStringLiteral("Picture download/re-download and physical check"),
                           QStringLiteral("FAIL"),
                           QStringLiteral("Cannot remove interrupted temporary download before retry"),
                           link, requestedPicPath, d);
            finishEventAudit(false, QStringLiteral("Event blocked: cannot clear interrupted Picture download file"));
            releaseEventPictureReservationForRetry(picNameSnapshot,
                                                   QStringLiteral("cannot clear interrupted Picture download file"));
            return;
        }

        qDebug() << "[PIC-DOWNLOAD] attempt" << attempt << "/" << maxDownloadAttempts
                 << "url=" << link << "path=" << requestedPicPath;

        const bool downloadOk = downloadUrlToFile(link, requestedPicPath);
        downloadedPicSize = 0;
        const bool physicalFileOk = downloadOk &&
                                    isVerifiedDownloadedPictureFile(requestedPicPath,
                                                                    &downloadedPicSize);
        if (physicalFileOk) {
            pictureDownloadedAndVerified = true;
            qDebug() << "[PIC-DOWNLOAD] VERIFIED"
                     << "attempt=" << attempt
                     << "path=" << requestedPicPath
                     << "size=" << downloadedPicSize;
            QJsonObject d;
            d.insert(QStringLiteral("attempt"), attempt);
            d.insert(QStringLiteral("maxAttempts"), maxDownloadAttempts);
            d.insert(QStringLiteral("downloadOk"), downloadOk);
            d.insert(QStringLiteral("physicalFileOk"), physicalFileOk);
            d.insert(QStringLiteral("size"), double(downloadedPicSize));
            auditEventStep(QStringLiteral("PICTURE_DOWNLOAD"),
                           QStringLiteral("Picture download/re-download and physical check"),
                           QStringLiteral("PASS"),
                           QStringLiteral("Picture downloaded and verified as a readable physical image file"),
                           link, requestedPicPath, d);
            break;
        }

        const QFileInfo failedInfo(requestedPicPath);
        qWarning() << "[PIC-DOWNLOAD] verification failed"
                   << "attempt=" << attempt
                   << "downloadOk=" << downloadOk
                   << "exists=" << failedInfo.exists()
                   << "isFile=" << failedInfo.isFile()
                   << "size=" << (failedInfo.exists() ? failedInfo.size() : 0)
                   << "path=" << requestedPicPath;

        // If wget succeeded but the content verification failed, remove only that
        // newly-published bad file. If wget failed, keep any previous good file.
        if (downloadOk) {
            safeRemoveFile(requestedPicPath);
        }
        safeRemoveFile(requestedPicPath + QStringLiteral(".download"));

        {
            QJsonObject d;
            d.insert(QStringLiteral("attempt"), attempt);
            d.insert(QStringLiteral("maxAttempts"), maxDownloadAttempts);
            d.insert(QStringLiteral("downloadOk"), downloadOk);
            d.insert(QStringLiteral("exists"), failedInfo.exists());
            d.insert(QStringLiteral("isFile"), failedInfo.isFile());
            d.insert(QStringLiteral("size"), double(failedInfo.exists() ? failedInfo.size() : 0));
            auditEventStep(QStringLiteral("PICTURE_DOWNLOAD"),
                           QStringLiteral("Picture download/re-download and physical check"),
                           attempt < maxDownloadAttempts ? QStringLiteral("RETRY") : QStringLiteral("FAIL"),
                           attempt < maxDownloadAttempts
                               ? QStringLiteral("Download or physical image verification failed; re-download scheduled")
                               : QStringLiteral("Download or physical image verification failed on final attempt"),
                           link, requestedPicPath, d);
        }

        if (attempt < maxDownloadAttempts) {
            qWarning() << "[PIC-DOWNLOAD] re-download scheduled"
                       << "nextAttempt=" << (attempt + 1)
                       << "path=" << requestedPicPath;
            QThread::msleep(500);
        }
    }

    if (!pictureDownloadedAndVerified) {
        qWarning() << "[EVENT-GATE][BLOCK] picture download/physical verification failed after"
                   << maxDownloadAttempts << "attempts:" << link << "->" << requestedPicPath;
        QJsonObject d;
        d.insert(QStringLiteral("attempts"), maxDownloadAttempts);
        d.insert(QStringLiteral("link"), link);
        d.insert(QStringLiteral("path"), requestedPicPath);
        auditEventStep(QStringLiteral("PICTURE_DOWNLOAD"),
                       QStringLiteral("Picture download/re-download and physical check"),
                       QStringLiteral("FAIL"),
                       QStringLiteral("Picture remains unavailable/invalid after initial download plus two re-download attempts"),
                       link, requestedPicPath, d);
        finishEventAudit(false, QStringLiteral("Event blocked: Picture download/verification failed after all attempts"));
        releaseEventPictureReservationForRetry(picNameSnapshot,
                                               QStringLiteral("Picture download/verification failed after all attempts"));
        return;
    }

    // Resolve both mandatory files from the real filesystem. Event CSV recovery
    // is type-aware and explicitly rejects *_PATTERN* and /Pattern/ candidates.
    const QString verifiedPicPath = resolveExistingPicturePath(requestedPicPath,
                                                               picNameSnapshot,
                                                               eventDate,
                                                               eventTime);
    if (verifiedPicPath.isEmpty()) {
        qWarning() << "[EVENT-GATE][BLOCK] Picture resolver failed after download:"
                   << requestedPicPath;
        auditEventStep(QStringLiteral("PICTURE_RESOLVED"),
                       QStringLiteral("Resolve exact local Picture path"),
                       QStringLiteral("FAIL"),
                       QStringLiteral("Filesystem resolver could not prove the exact Picture file for this event"),
                       requestedPicPath, QStringLiteral("verified picture path"));
        finishEventAudit(false, QStringLiteral("Event blocked: exact physical Picture path could not be resolved"));
        releaseEventPictureReservationForRetry(picNameSnapshot,
                                               QStringLiteral("exact physical Picture path could not be resolved"));
        return;
    }
    auditEventStep(QStringLiteral("PICTURE_RESOLVED"),
                   QStringLiteral("Resolve exact local Picture path"),
                   QStringLiteral("PASS"),
                   QStringLiteral("Resolved the exact physical Picture path for this event"),
                   requestedPicPath, verifiedPicPath);

    QString verifiedCsvPath = resolveExistingEventCsvPath(storedCsvPathSnapshot,
                                                           storedCsvNameSnapshot,
                                                           eventModeSnapshot,
                                                           eventDate,
                                                           eventTime);
    if (verifiedCsvPath.isEmpty()) {
        qWarning() << "[EVENT-GATE][BLOCK] Event CSV resolver failed:"
                   << "storedCSVPATH=" << storedCsvPathSnapshot
                   << "storedCSVname=" << storedCsvNameSnapshot
                   << "mode=" << eventModeSnapshot
                   << "date=" << eventDate
                   << "time=" << eventTime
                   << "pic=" << verifiedPicPath;
        QJsonObject d;
        d.insert(QStringLiteral("storedCSVPATH"), storedCsvPathSnapshot);
        d.insert(QStringLiteral("storedCSVname"), storedCsvNameSnapshot);
        d.insert(QStringLiteral("mode"), eventModeSnapshot);
        d.insert(QStringLiteral("date"), eventDate);
        d.insert(QStringLiteral("time"), eventTime);
        d.insert(QStringLiteral("patternForbidden"), true);
        auditEventStep(QStringLiteral("CSV_RESOLVED"),
                       QStringLiteral("Resolve exact local Event CSV path"),
                       QStringLiteral("FAIL"),
                       QStringLiteral("Could not resolve a physical Event CSV for the same event; Pattern files are forbidden as Event CSV fallback"),
                       storedCsvPathSnapshot, QStringLiteral("verified Event CSV path"), d);
        finishEventAudit(false, QStringLiteral("Event blocked: exact non-Pattern Event CSV could not be resolved"));
        releaseEventPictureReservationForRetry(picNameSnapshot,
                                               QStringLiteral("exact Event CSV could not be resolved"));
        return;
    }
    auditEventStep(QStringLiteral("CSV_RESOLVED"),
                   QStringLiteral("Resolve exact local Event CSV path"),
                   QStringLiteral("PASS"),
                   QStringLiteral("Resolved the exact physical non-Pattern Event CSV path for this event"),
                   storedCsvPathSnapshot, verifiedCsvPath);

    // ------------------------------
    // Mandatory Verification Stage #1
    // ------------------------------
    FileVerifySnapshot picStage1;
    FileVerifySnapshot csvStage1;
    const bool picStage1Ok = captureVerifiedPictureSnapshot(verifiedPicPath,
                                                             PIC_PATH,
                                                             eventDate,
                                                             eventTime,
                                                             &picStage1);
    const bool csvStage1Ok = captureVerifiedEventCsvSnapshot(verifiedCsvPath,
                                                              EVENT_PATH,
                                                              eventModeSnapshot,
                                                              eventDate,
                                                              eventTime,
                                                              &csvStage1);

    {
        QJsonObject d;
        d.insert(QStringLiteral("path"), verifiedPicPath);
        d.insert(QStringLiteral("size"), double(picStage1.size));
        d.insert(QStringLiteral("mtimeMs"), double(picStage1.lastModifiedMs));
        auditEventStep(QStringLiteral("VERIFY_STAGE1_PICTURE"),
                       QStringLiteral("Picture verification stage #1"),
                       picStage1Ok ? QStringLiteral("PASS") : QStringLiteral("FAIL"),
                       picStage1Ok
                           ? QStringLiteral("Picture passed physical/type/event ownership verification stage #1")
                           : QStringLiteral("Picture failed physical/type/event ownership verification stage #1"),
                       verifiedPicPath, QStringLiteral("stage1 picture snapshot"), d);
    }
    {
        QJsonObject d;
        d.insert(QStringLiteral("path"), verifiedCsvPath);
        d.insert(QStringLiteral("size"), double(csvStage1.size));
        d.insert(QStringLiteral("mtimeMs"), double(csvStage1.lastModifiedMs));
        d.insert(QStringLiteral("patternForbidden"), true);
        auditEventStep(QStringLiteral("VERIFY_STAGE1_CSV"),
                       QStringLiteral("Event CSV verification stage #1"),
                       csvStage1Ok ? QStringLiteral("PASS") : QStringLiteral("FAIL"),
                       csvStage1Ok
                           ? QStringLiteral("Event CSV passed physical/type/event ownership verification stage #1")
                           : QStringLiteral("Event CSV failed stage #1 or was rejected as Pattern/wrong event"),
                       verifiedCsvPath, QStringLiteral("stage1 CSV snapshot"), d);
    }

    if (!picStage1Ok || !csvStage1Ok) {
        qWarning() << "[EVENT-GATE][BLOCK][STAGE-1] mandatory verification failed:"
                   << "pictureOk=" << picStage1Ok
                   << "eventCsvOk=" << csvStage1Ok
                   << "pic=" << verifiedPicPath
                   << "csv=" << verifiedCsvPath;
        QJsonObject d;
        d.insert(QStringLiteral("pictureOk"), picStage1Ok);
        d.insert(QStringLiteral("eventCsvOk"), csvStage1Ok);
        auditEventStep(QStringLiteral("VERIFY_STAGE1_GATE"),
                       QStringLiteral("Mandatory verification stage #1 gate"),
                       QStringLiteral("FAIL"),
                       QStringLiteral("Mandatory gate blocked: Picture and Event CSV must both pass stage #1"),
                       verifiedPicPath + QStringLiteral(" | ") + verifiedCsvPath,
                       QStringLiteral("stage #2 verification"), d);
        finishEventAudit(false, QStringLiteral("Event blocked at mandatory verification stage #1"));
        releaseEventPictureReservationForRetry(picNameSnapshot,
                                               QStringLiteral("mandatory verification stage #1 failed"));
        return;
    }

    auditEventStep(QStringLiteral("VERIFY_STAGE1_GATE"),
                   QStringLiteral("Mandatory verification stage #1 gate"),
                   QStringLiteral("PASS"),
                   QStringLiteral("Picture and Event CSV both passed mandatory stage #1"),
                   verifiedPicPath + QStringLiteral(" | ") + verifiedCsvPath,
                   QStringLiteral("stage #2 stability verification"));

    qDebug() << "[EVENT-VERIFY][PIC][PASS-1]"
             << "path=" << picStage1.path << "size=" << picStage1.size
             << "mtimeMs=" << picStage1.lastModifiedMs;
    qDebug() << "[EVENT-VERIFY][CSV][PASS-1]"
             << "path=" << csvStage1.path << "size=" << csvStage1.size
             << "mtimeMs=" << csvStage1.lastModifiedMs;

    // Pattern is OPTIONAL. Snapshot it if valid, otherwise force empty values.
    const QString selectedPatternPath = myDatabase ? myDatabase->selectPatterPath : QString();
    const QString selectedPatternName = myDatabase ? myDatabase->selectPatterName : QString();
    FileVerifySnapshot patternStage1;
    const bool patternStage1Ok = captureOptionalPatternSnapshot(selectedPatternPath,
                                                                 PATTERN_PATH,
                                                                 &patternStage1);
    {
        QJsonObject d;
        d.insert(QStringLiteral("selectedPath"), selectedPatternPath);
        d.insert(QStringLiteral("selectedName"), selectedPatternName);
        d.insert(QStringLiteral("verifiedPath"), patternStage1.path);
        d.insert(QStringLiteral("size"), double(patternStage1.size));
        d.insert(QStringLiteral("required"), false);
        auditEventStep(QStringLiteral("PATTERN_OPTIONAL"),
                       QStringLiteral("Optional Pattern verification"),
                       patternStage1Ok ? QStringLiteral("PASS") : QStringLiteral("SKIP"),
                       patternStage1Ok
                           ? QStringLiteral("Optional Pattern is physically valid and will be rechecked before FTP")
                           : QStringLiteral("Pattern is absent/invalid; optional step skipped without blocking the event"),
                       selectedPatternPath, patternStage1.path, d);
    }

    // The second pass proves that both mandatory files are still present and no
    // writer is still changing size/mtime. This protects against partial CSV writes
    // and cleanup/race windows between file creation and FTP.
    QThread::msleep(400);

    // ------------------------------
    // Mandatory Verification Stage #2
    // ------------------------------
    FileVerifySnapshot picStage2;
    FileVerifySnapshot csvStage2;
    const bool picStage2Ok = captureVerifiedPictureSnapshot(picStage1.path,
                                                             PIC_PATH,
                                                             eventDate,
                                                             eventTime,
                                                             &picStage2);
    const bool csvStage2Ok = captureVerifiedEventCsvSnapshot(csvStage1.path,
                                                              EVENT_PATH,
                                                              eventModeSnapshot,
                                                              eventDate,
                                                              eventTime,
                                                              &csvStage2);

    const bool picStable = picStage2Ok && snapshotsAreStable(picStage1, picStage2);
    const bool csvStable = csvStage2Ok && snapshotsAreStable(csvStage1, csvStage2);

    {
        QJsonObject d;
        d.insert(QStringLiteral("path"), picStage2.path);
        d.insert(QStringLiteral("stage1Size"), double(picStage1.size));
        d.insert(QStringLiteral("stage2Size"), double(picStage2.size));
        d.insert(QStringLiteral("stage1MtimeMs"), double(picStage1.lastModifiedMs));
        d.insert(QStringLiteral("stage2MtimeMs"), double(picStage2.lastModifiedMs));
        d.insert(QStringLiteral("stable"), picStable);
        auditEventStep(QStringLiteral("VERIFY_STAGE2_PICTURE"),
                       QStringLiteral("Picture stability verification stage #2"),
                       picStable ? QStringLiteral("PASS") : QStringLiteral("FAIL"),
                       picStable
                           ? QStringLiteral("Picture is unchanged and stable between verification stages")
                           : QStringLiteral("Picture disappeared, changed, or is no longer valid between verification stages"),
                       picStage1.path, picStage2.path, d);
    }
    {
        QJsonObject d;
        d.insert(QStringLiteral("path"), csvStage2.path);
        d.insert(QStringLiteral("stage1Size"), double(csvStage1.size));
        d.insert(QStringLiteral("stage2Size"), double(csvStage2.size));
        d.insert(QStringLiteral("stage1MtimeMs"), double(csvStage1.lastModifiedMs));
        d.insert(QStringLiteral("stage2MtimeMs"), double(csvStage2.lastModifiedMs));
        d.insert(QStringLiteral("stable"), csvStable);
        auditEventStep(QStringLiteral("VERIFY_STAGE2_CSV"),
                       QStringLiteral("Event CSV stability verification stage #2"),
                       csvStable ? QStringLiteral("PASS") : QStringLiteral("FAIL"),
                       csvStable
                           ? QStringLiteral("Event CSV is unchanged and stable between verification stages")
                           : QStringLiteral("Event CSV disappeared, changed, was still being written, or became invalid"),
                       csvStage1.path, csvStage2.path, d);
    }

    if (!picStable || !csvStable) {
        qWarning() << "[EVENT-GATE][BLOCK][STAGE-2] mandatory file missing/changed/not stable:"
                   << "pictureStable=" << picStable
                   << "eventCsvStable=" << csvStable
                   << "picSize1=" << picStage1.size << "picSize2=" << picStage2.size
                   << "csvSize1=" << csvStage1.size << "csvSize2=" << csvStage2.size
                   << "picMtime1=" << picStage1.lastModifiedMs << "picMtime2=" << picStage2.lastModifiedMs
                   << "csvMtime1=" << csvStage1.lastModifiedMs << "csvMtime2=" << csvStage2.lastModifiedMs;
        QJsonObject d;
        d.insert(QStringLiteral("pictureStable"), picStable);
        d.insert(QStringLiteral("eventCsvStable"), csvStable);
        auditEventStep(QStringLiteral("VERIFY_STAGE2_GATE"),
                       QStringLiteral("Mandatory verification stage #2 gate"),
                       QStringLiteral("FAIL"),
                       QStringLiteral("Mandatory stability gate blocked before FTP"),
                       picStage1.path + QStringLiteral(" | ") + csvStage1.path,
                       QStringLiteral("FTP blocked"), d);
        finishEventAudit(false, QStringLiteral("Event blocked at mandatory stability verification stage #2"));
        releaseEventPictureReservationForRetry(picNameSnapshot,
                                               QStringLiteral("mandatory stability verification stage #2 failed"));
        return;
    }

    auditEventStep(QStringLiteral("VERIFY_STAGE2_GATE"),
                   QStringLiteral("Mandatory verification stage #2 gate"),
                   QStringLiteral("PASS"),
                   QStringLiteral("Picture and Event CSV are both stable and eligible for FTP"),
                   picStage2.path + QStringLiteral(" | ") + csvStage2.path,
                   QStringLiteral("pre-FTP gate"));

    qDebug() << "[EVENT-VERIFY][PIC][PASS-2] stable=true"
             << "path=" << picStage2.path << "size=" << picStage2.size;
    qDebug() << "[EVENT-VERIFY][CSV][PASS-2] stable=true"
             << "path=" << csvStage2.path << "size=" << csvStage2.size;

    QString finalPatternPath;
    QString finalPatternName;
    FileVerifySnapshot patternStage2;
    if (patternStage1Ok &&
        captureOptionalPatternSnapshot(patternStage1.path, PATTERN_PATH, &patternStage2) &&
        snapshotsAreStable(patternStage1, patternStage2)) {
        finalPatternPath = patternStage2.path;
        finalPatternName = QFileInfo(patternStage2.path).fileName();
        if (!selectedPatternName.trimmed().isEmpty() &&
            safeFileNameOnly(selectedPatternName, QString()).compare(finalPatternName, Qt::CaseInsensitive) != 0) {
            qWarning() << "[EVENT-VERIFY][PATTERN] selected name differs from physical file; use physical name:"
                       << selectedPatternName << "->" << finalPatternName;
        }
        qDebug() << "[EVENT-VERIFY][PATTERN][OPTIONAL-PASS]"
                 << "path=" << finalPatternPath << "size=" << patternStage2.size;
    } else {
        if (!selectedPatternPath.trimmed().isEmpty()) {
            qWarning() << "[EVENT-VERIFY][PATTERN][OPTIONAL-MISSING] continue without Pattern:"
                       << selectedPatternPath;
        }
        finalPatternPath.clear();
        finalPatternName.clear();
    }

    // Freeze final verified paths. From this point FTP/JSON must never go back to
    // mutable fileNamePic/fullpathCSV/fullnameCSV/DateKept/TimeKept as authorities.
    const QString finalPicPath = picStage2.path;
    const QString finalPicName = QFileInfo(finalPicPath).fileName();
    const QString finalCsvPath = csvStage2.path;
    const QString finalCsvName = QFileInfo(finalCsvPath).fileName();

    fullpathCSV = finalCsvPath;  // compatibility only, after strict verification
    fullnameCSV = finalCsvName;

    qDebug() << "[EVENT-GATE][PASS][PRE-FTP]"
             << "mode=" << eventModeSnapshot
             << "date=" << eventDate
             << "time=" << eventTime
             << "picture=" << finalPicPath
             << "eventCsv=" << finalCsvPath
             << "pattern=" << (finalPatternPath.isEmpty() ? QStringLiteral("<optional-empty>") : finalPatternPath);
    {
        QJsonObject d;
        d.insert(QStringLiteral("mode"), eventModeSnapshot);
        d.insert(QStringLiteral("date"), eventDate);
        d.insert(QStringLiteral("time"), eventTime);
        d.insert(QStringLiteral("picture"), finalPicPath);
        d.insert(QStringLiteral("eventCsv"), finalCsvPath);
        d.insert(QStringLiteral("pattern"), finalPatternPath);
        d.insert(QStringLiteral("patternRequired"), false);
        auditEventStep(QStringLiteral("PRE_FTP_GATE"),
                       QStringLiteral("Final mandatory gate before FTP"),
                       QStringLiteral("PASS"),
                       QStringLiteral("Frozen verified local paths; FTP may start only from these exact files"),
                       finalCsvPath + QStringLiteral(" | ") + finalPicPath,
                       FTP_Param_ ? FTP_Param_->FTP_IP : QStringLiteral("FTP unavailable"), d);
    }

    // ------------------------------------------------------------------
    // Mail publisher (LOCAL verified files only)
    // ------------------------------------------------------------------
    // Mail and SYNC have different dependencies:
    //   - sendMail attachments use the verified LOCAL Event CSV + Picture.
    //   - SYNC publishes verified REMOTE FTP paths and therefore still depends
    //     on the FTP bundle.
    // Keeping them separate prevents an FTP outage from suppressing an email
    // even though the mandatory local evidence is complete and readable.
    auto publishVerifiedSendMail = [&](const QString &mailCsvPath,
                                       const QString &mailPicPath,
                                       const QString &mailPatternPath,
                                       const QString &mailPatternName,
                                       bool picFtpUploaded,
                                       const QString &publishContext) -> bool {
        const QString mailCsvName = QFileInfo(mailCsvPath).fileName();
        const QString mailPicName = QFileInfo(mailPicPath).fileName();

        if (mailCsvPath.trimmed().isEmpty() || mailPicPath.trimmed().isEmpty() ||
            mailCsvName.isEmpty() || mailPicName.isEmpty() ||
            !isVerifiedLocalFile(mailCsvPath) ||
            !isVerifiedDownloadedPictureFile(mailPicPath) ||
            isForbiddenEventCsvCandidate(mailCsvPath)) {
            qWarning() << "[MAIL-PUBLISH][BLOCK] local mandatory files are not valid at mail publish time:"
                       << "csv=" << mailCsvPath << "pic=" << mailPicPath;
            QJsonObject d;
            d.insert(QStringLiteral("context"), publishContext);
            d.insert(QStringLiteral("CSVPATH"), mailCsvPath);
            d.insert(QStringLiteral("PicPATH"), mailPicPath);
            d.insert(QStringLiteral("PicFtpUploaded"), picFtpUploaded);
            auditEventStep(QStringLiteral("SENDMAIL_NOTIFICATION"),
                           QStringLiteral("Publish verified sendMail payload"),
                           QStringLiteral("BLOCK"),
                           QStringLiteral("sendMail payload was blocked because the mandatory local Event CSV/Picture are no longer valid"),
                           QStringLiteral("PLCServer local verified files"),
                           QStringLiteral("Mail notification blocked"), d);
            return false;
        }

        if (eventMailPublished &&
            !activePictureEventId.isEmpty() &&
            eventMailPublishedEventId == activePictureEventId) {
            qWarning() << "[MAIL-PUBLISH][DUPLICATE-SUPPRESSED]"
                       << "eventId=" << activePictureEventId
                       << "csv=" << mailCsvPath
                       << "pic=" << mailPicPath;
            return true;
        }

        int monitorConnected = 0;
        for (QWebSocket *client : qAsConst(Monitor_address)) {
            if (client && client->state() == QAbstractSocket::ConnectedState)
                ++monitorConnected;
        }

        int vncConnected = 0;
        if (server) {
            for (QWebSocket *client : qAsConst(server->m_VNC)) {
                if (client && client->state() == QAbstractSocket::ConnectedState)
                    ++vncConnected;
            }
        }

        int webConnected = 0;
        for (QWebSocket *client : qAsConst(webapp_address)) {
            if (client && client->state() == QAbstractSocket::ConnectedState)
                ++webConnected;
        }

        const bool snmpConnected =
            snmp_address && snmp_address->state() == QAbstractSocket::ConnectedState;
        const int connectedTargets =
            monitorConnected + vncConnected + webConnected + (snmpConnected ? 1 : 0);

        QJsonObject Param;
        Param.insert(QStringLiteral("objectName"), QStringLiteral("sendMail"));
        Param.insert(QStringLiteral("CSVPATH"), mailCsvPath);
        Param.insert(QStringLiteral("CSVname"), mailCsvName);
        Param.insert(QStringLiteral("PicPATH"), mailPicPath);
        Param.insert(QStringLiteral("Picname"), mailPicName);
        Param.insert(QStringLiteral("PicFtpUploaded"), picFtpUploaded);
        Param.insert(QStringLiteral("CSVPatternPATH"), mailPatternPath);
        Param.insert(QStringLiteral("CSVPatternname"), mailPatternName);

        const QString rawMail = QString::fromUtf8(
            QJsonDocument(Param).toJson(QJsonDocument::Compact));

        qDebug() << "[PIC][SENDMAIL] verified local event payload:"
                 << rawMail
                 << "context=" << publishContext
                 << "connectedTargets=" << connectedTargets
                 << "snmpConnected=" << snmpConnected;

        if (connectedTargets <= 0) {
            qWarning() << "[MAIL-PUBLISH][WARN] sendMail payload is valid but no WebSocket notification consumer is connected;"
                       << "eventId=" << activePictureEventId;
        }
        if (!snmpConnected) {
            qWarning() << "[MAIL-PUBLISH][WARN] snmp service is not connected; if SNMP is the mail consumer, email delivery cannot occur until it reconnects";
        }

        // Preserve the existing downstream contract.  Publishing is asynchronous;
        // the PLC event pipeline never waits for an email/SNMP acknowledgement.
        emit sendToMonitor(rawMail);
        emit sendToVNC(rawMail);

        if (snmpConnected)
            emit sendMessage(rawMail, snmp_address);

        Q_FOREACH (QWebSocket *pClient, webapp_address) {
            if (pClient && pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(rawMail, pClient);
        }

        // Only lock the per-event mail slot when there was at least one live
        // downstream consumer.  With no connected consumer we keep it retryable
        // instead of falsely claiming that an email notification was delivered.
        if (connectedTargets > 0) {
            eventMailPublished = true;
            eventMailPublishedEventId = activePictureEventId;
        }

        QJsonObject d;
        d.insert(QStringLiteral("context"), publishContext);
        d.insert(QStringLiteral("CSVPATH"), mailCsvPath);
        d.insert(QStringLiteral("CSVname"), mailCsvName);
        d.insert(QStringLiteral("PicPATH"), mailPicPath);
        d.insert(QStringLiteral("Picname"), mailPicName);
        d.insert(QStringLiteral("CSVPatternPATH"), mailPatternPath);
        d.insert(QStringLiteral("CSVPatternname"), mailPatternName);
        d.insert(QStringLiteral("PicFtpUploaded"), picFtpUploaded);
        d.insert(QStringLiteral("monitorConnected"), monitorConnected);
        d.insert(QStringLiteral("vncConnected"), vncConnected);
        d.insert(QStringLiteral("webConnected"), webConnected);
        d.insert(QStringLiteral("snmpConnected"), snmpConnected);
        d.insert(QStringLiteral("connectedTargets"), connectedTargets);

        auditEventStep(QStringLiteral("SENDMAIL_NOTIFICATION"),
                       QStringLiteral("Publish verified sendMail payload"),
                       connectedTargets > 0 ? QStringLiteral("PASS") : QStringLiteral("BLOCK"),
                       connectedTargets > 0
                           ? QStringLiteral("Verified local Event CSV/Picture sendMail payload was published independently of FTP status")
                           : QStringLiteral("Verified sendMail payload was built, but no notification consumer was connected"),
                       mailCsvPath + QStringLiteral(" | ") + mailPicPath,
                       QStringLiteral("Monitor/VNC/SNMP/WebSocket"), d);
        return connectedTargets > 0;
    };

    // All event FTP is centralized here. DataStorage no longer uploads Event CSV
    // or Pattern before the mandatory Picture+Event CSV gate has passed.
    const bool ftpBundleOk = uplouploadFileWithtoftpServer(eventModeSnapshot,
                                                            finalCsvPath,
                                                            finalPicPath,
                                                            finalPicName,
                                                            finalPatternPath,
                                                            finalPatternName);
    if (!ftpBundleOk) {
        qWarning() << "[EVENT-GATE][FTP-FAIL] FTP event bundle was not fully verified;"
                   << "mail may still publish from re-verified local files, but SYNC remains blocked:"
                   << "csv=" << finalCsvPath << "pic=" << finalPicPath;
        QJsonObject d;
        d.insert(QStringLiteral("eventCsv"), finalCsvPath);
        d.insert(QStringLiteral("picture"), finalPicPath);
        d.insert(QStringLiteral("ftpHost"), FTP_Param_ ? FTP_Param_->FTP_IP : QString());
        auditEventStep(QStringLiteral("FTP_BUNDLE"),
                       QStringLiteral("Mandatory FTP bundle verification"),
                       QStringLiteral("FAIL"),
                       QStringLiteral("Event CSV and Picture FTP bundle was not fully upload/download-back verified; remote SYNC is blocked while mail may use verified local files"),
                       finalCsvPath + QStringLiteral(" | ") + finalPicPath,
                       FTP_Param_ ? FTP_Param_->FTP_IP : QStringLiteral("FTP"), d);

        QJsonObject blocked;
        blocked.insert(QStringLiteral("reason"), QStringLiteral("MANDATORY_FTP_BUNDLE_FAILED"));

        // FTP failure must not suppress email when the two mandatory LOCAL
        // files are still the same verified evidence.  Re-check them after the
        // failed FTP attempt before building the mail payload.
        FileVerifySnapshot picMailFallback;
        FileVerifySnapshot csvMailFallback;
        const bool picMailFallbackOk =
            captureVerifiedPictureSnapshot(finalPicPath,
                                           PIC_PATH,
                                           eventDate,
                                           eventTime,
                                           &picMailFallback) &&
            snapshotsAreStable(picStage2, picMailFallback);
        const bool csvMailFallbackOk =
            captureVerifiedEventCsvSnapshot(finalCsvPath,
                                            EVENT_PATH,
                                            eventModeSnapshot,
                                            eventDate,
                                            eventTime,
                                            &csvMailFallback) &&
            snapshotsAreStable(csvStage2, csvMailFallback);

        QString mailPatternPath = finalPatternPath;
        QString mailPatternName = finalPatternName;
        if (!mailPatternPath.isEmpty()) {
            FileVerifySnapshot patternMailFallback;
            if (!captureOptionalPatternSnapshot(mailPatternPath,
                                                PATTERN_PATH,
                                                &patternMailFallback) ||
                !snapshotsAreStable(patternStage2, patternMailFallback)) {
                qWarning() << "[MAIL-PUBLISH][PATTERN] optional Pattern changed/disappeared after FTP failure; send mail without Pattern:"
                           << mailPatternPath;
                mailPatternPath.clear();
                mailPatternName.clear();
            } else {
                mailPatternPath = patternMailFallback.path;
                mailPatternName = QFileInfo(patternMailFallback.path).fileName();
            }
        }

        if (picMailFallbackOk && csvMailFallbackOk) {
            QJsonObject localVerify;
            localVerify.insert(QStringLiteral("pictureOk"), true);
            localVerify.insert(QStringLiteral("eventCsvOk"), true);
            localVerify.insert(QStringLiteral("ftpBundleOk"), false);
            localVerify.insert(QStringLiteral("mailFallback"), true);
            localVerify.insert(QStringLiteral("pictureSize"), double(picMailFallback.size));
            localVerify.insert(QStringLiteral("eventCsvSize"), double(csvMailFallback.size));
            auditEventStep(QStringLiteral("FINAL_LOCAL_VERIFY"),
                           QStringLiteral("Final local verification after FTP"),
                           QStringLiteral("PASS"),
                           QStringLiteral("FTP failed, but mandatory local Picture/Event CSV remain stable and are safe for mail publication"),
                           csvMailFallback.path + QStringLiteral(" | ") + picMailFallback.path,
                           QStringLiteral("sendMail local fallback"), localVerify);

            publishVerifiedSendMail(csvMailFallback.path,
                                    picMailFallback.path,
                                    mailPatternPath,
                                    mailPatternName,
                                    false,
                                    QStringLiteral("FTP bundle failed; mail published from re-verified local mandatory files"));
        } else {
            QJsonObject localVerify;
            localVerify.insert(QStringLiteral("pictureOk"), picMailFallbackOk);
            localVerify.insert(QStringLiteral("eventCsvOk"), csvMailFallbackOk);
            localVerify.insert(QStringLiteral("ftpBundleOk"), false);
            auditEventStep(QStringLiteral("FINAL_LOCAL_VERIFY"),
                           QStringLiteral("Final local verification after FTP"),
                           QStringLiteral("FAIL"),
                           QStringLiteral("FTP failed and mandatory local files also failed the final re-verification; mail publication is blocked"),
                           finalCsvPath + QStringLiteral(" | ") + finalPicPath,
                           QStringLiteral("Mail/SYNC blocked"), localVerify);
            auditEventStep(QStringLiteral("SENDMAIL_NOTIFICATION"),
                           QStringLiteral("Publish verified sendMail payload"),
                           QStringLiteral("BLOCK"),
                           QStringLiteral("sendMail publication is blocked because mandatory local files failed final verification"),
                           QStringLiteral("PLCServer"), QStringLiteral("Mail notification blocked"), localVerify);
        }

        auditEventStep(QStringLiteral("LOOP_WAIT_PIC"),
                       QStringLiteral("Prepare verified legacy SYNC path payload"),
                       QStringLiteral("BLOCK"),
                       QStringLiteral("Legacy SYNC path preparation is blocked because mandatory FTP verification failed"),
                       QStringLiteral("PLCServer"), QStringLiteral("SYNC blocked"), blocked);
        auditEventStep(QStringLiteral("SYNC_VERIFY"),
                       QStringLiteral("Final SYNC verification"),
                       QStringLiteral("BLOCK"),
                       QStringLiteral("Final SYNC verification is blocked because mandatory FTP verification failed"),
                       QStringLiteral("PLCServer"), QStringLiteral("SYNC blocked"), blocked);
        auditEventStep(QStringLiteral("SYNC_PUBLISH"),
                       QStringLiteral("Publish SYNC to monitor/VNC/SNMP"),
                       QStringLiteral("BLOCK"),
                       QStringLiteral("SYNC publication is blocked because mandatory FTP verification failed"),
                       QStringLiteral("PLCServer"), QStringLiteral("Monitor/VNC/SNMP blocked"), blocked);

        finishEventAudit(false, QStringLiteral("Event blocked: mandatory FTP bundle verification failed"));
        releaseEventPictureReservationForRetry(picNameSnapshot,
                                               QStringLiteral("mandatory FTP bundle verification failed"));
        return;
    }
    {
        QJsonObject d;
        d.insert(QStringLiteral("ftpCsv"), verifiedFtpEventCsvRemotePath);
        d.insert(QStringLiteral("ftpPicture"), verifiedFtpPictureRemotePath);
        d.insert(QStringLiteral("ftpPattern"), verifiedFtpPatternRemotePath);
        auditEventStep(QStringLiteral("FTP_BUNDLE"),
                       QStringLiteral("Mandatory FTP bundle verification"),
                       QStringLiteral("PASS"),
                       QStringLiteral("Mandatory Event CSV and Picture were uploaded and verified by download-back checks"),
                       finalCsvPath + QStringLiteral(" | ") + finalPicPath,
                       verifiedFtpEventCsvRemotePath + QStringLiteral(" | ") + verifiedFtpPictureRemotePath, d);
    }

    // ------------------------------
    // Final mandatory publish gate
    // ------------------------------
    // Even after FTP, prove both local mandatory files are still the exact stable
    // files that passed Stage #2. If either changed/disappeared, publish nothing.
    FileVerifySnapshot picFinal;
    FileVerifySnapshot csvFinal;
    const bool picFinalOk = captureVerifiedPictureSnapshot(finalPicPath,
                                                            PIC_PATH,
                                                            eventDate,
                                                            eventTime,
                                                            &picFinal) &&
                            snapshotsAreStable(picStage2, picFinal);
    const bool csvFinalOk = captureVerifiedEventCsvSnapshot(finalCsvPath,
                                                             EVENT_PATH,
                                                             eventModeSnapshot,
                                                             eventDate,
                                                             eventTime,
                                                             &csvFinal) &&
                            snapshotsAreStable(csvStage2, csvFinal);

    if (!picFinalOk || !csvFinalOk) {
        qWarning() << "[EVENT-PUBLISH][BLOCK] final mandatory verification failed after FTP:"
                   << "pictureOk=" << picFinalOk
                   << "eventCsvOk=" << csvFinalOk
                   << "pic=" << finalPicPath
                   << "csv=" << finalCsvPath;
        QJsonObject d;
        d.insert(QStringLiteral("pictureOk"), picFinalOk);
        d.insert(QStringLiteral("eventCsvOk"), csvFinalOk);
        auditEventStep(QStringLiteral("FINAL_LOCAL_VERIFY"),
                       QStringLiteral("Final local verification after FTP"),
                       QStringLiteral("FAIL"),
                       QStringLiteral("Mandatory local files changed/disappeared after FTP; publish blocked"),
                       finalCsvPath + QStringLiteral(" | ") + finalPicPath,
                       QStringLiteral("notification blocked"), d);
        finishEventAudit(false, QStringLiteral("Event blocked: final local verification failed after FTP"));
        releaseEventPictureReservationForRetry(picNameSnapshot,
                                               QStringLiteral("final local verification failed after FTP"));
        return;
    }
    {
        QJsonObject d;
        d.insert(QStringLiteral("pictureSize"), double(picFinal.size));
        d.insert(QStringLiteral("eventCsvSize"), double(csvFinal.size));
        auditEventStep(QStringLiteral("FINAL_LOCAL_VERIFY"),
                       QStringLiteral("Final local verification after FTP"),
                       QStringLiteral("PASS"),
                       QStringLiteral("Mandatory local Picture and Event CSV are still the exact stable files after FTP"),
                       finalCsvPath + QStringLiteral(" | ") + finalPicPath,
                       QStringLiteral("publish gate"), d);
    }

    // Pattern remains optional even at publish time. If it disappeared, clear it
    // instead of substituting Event CSV or another path.
    if (!finalPatternPath.isEmpty()) {
        FileVerifySnapshot patternFinal;
        if (!captureOptionalPatternSnapshot(finalPatternPath, PATTERN_PATH, &patternFinal) ||
            !snapshotsAreStable(patternStage2, patternFinal)) {
            qWarning() << "[EVENT-PUBLISH][PATTERN] optional Pattern disappeared/changed; publish empty Pattern fields:"
                       << finalPatternPath;
            finalPatternPath.clear();
            finalPatternName.clear();
            verifiedFtpPatternRemotePath.clear();
        }
    }

    // Mail uses the verified LOCAL files and is intentionally independent of
    // the remote SYNC path gate.  At this point FTP succeeded and the local
    // mandatory files also passed the final post-FTP verification.
    publishVerifiedSendMail(csvFinal.path,
                            picFinal.path,
                            finalPatternPath,
                            finalPatternName,
                            true,
                            QStringLiteral("FTP bundle verified; mail published from final verified local files"));

    // The FTP function has already verified both mandatory remote files.  Do
    // not mark this event ready unless their exact remote paths were captured.
    if (verifiedFtpEventCsvRemotePath.isEmpty() || verifiedFtpPictureRemotePath.isEmpty()) {
        qWarning() << "[EVENT-PUBLISH][BLOCK] verified FTP remote mandatory paths are missing:"
                   << "csvRemote=" << verifiedFtpEventCsvRemotePath
                   << "picRemote=" << verifiedFtpPictureRemotePath;
        QJsonObject d;
        d.insert(QStringLiteral("csvRemote"), verifiedFtpEventCsvRemotePath);
        d.insert(QStringLiteral("picRemote"), verifiedFtpPictureRemotePath);
        auditEventStep(QStringLiteral("FTP_BUNDLE"),
                       QStringLiteral("Mandatory FTP bundle verification"),
                       QStringLiteral("FAIL"),
                       QStringLiteral("FTP reported success but mandatory verified remote paths are missing; publish blocked"),
                       finalCsvPath + QStringLiteral(" | ") + finalPicPath,
                       QStringLiteral("verified remote path snapshots"), d);
        finishEventAudit(false, QStringLiteral("Event blocked: verified FTP remote mandatory paths are missing"));
        releaseEventPictureReservationForRetry(picNameSnapshot,
                                               QStringLiteral("verified FTP remote mandatory paths are missing"));
        return;
    }

    verifiedEventCsvLocalPathSnapshot = csvFinal.path;
    verifiedPictureLocalPathSnapshot = picFinal.path;
    // Pattern is optional, but when present keep the exact verified source path
    // on this PLC.  SYNC uses this local filesystem path instead of the FTP
    // copy so downstream consumers can refer to the real Pattern source file.
    verifiedPatternLocalPathSnapshot = finalPatternPath;
    verifiedEventBundleReady = true;

    // The event now owns one verified canonical Picture.  Any later
    // ScreenPicture command for this event is duplicate/late and must be
    // ignored.  The ownership remains active until the final SYNC closes it.
    eventPictureState = EventPictureVerified;
    acceptedPictureName = QFileInfo(picFinal.path).fileName();
    qWarning() << "[PIC-OWNER][VERIFIED]"
               << "eventId=" << activePictureEventId
               << "picture=" << acceptedPictureName
               << "path=" << picFinal.path;

    qDebug() << "[EVENT-PUBLISH][FINAL-CHECK][PASS]"
             << "picture=" << picFinal.path
             << "eventCsv=" << csvFinal.path
             << "pattern=" << (finalPatternPath.isEmpty() ? QStringLiteral("<optional-empty>") : finalPatternPath)
             << "ftpCsv=" << verifiedFtpEventCsvRemotePath
             << "ftpPic=" << verifiedFtpPictureRemotePath
             << "patternLocal=" << (verifiedPatternLocalPathSnapshot.isEmpty()
                                        ? QStringLiteral("<optional-empty>")
                                        : verifiedPatternLocalPathSnapshot)
             << "ftpPattern=" << (verifiedFtpPatternRemotePath.isEmpty()
                                      ? QStringLiteral("<optional-empty>")
                                      : verifiedFtpPatternRemotePath);

}

QString PLCServer::canonicalRemoteEventCsvName(const QString &localCsvName,
                                                    const QString &eventDate,
                                                    const QString &eventTime) const
{
    QString safeName = ensureCsvSuffix(
        safeFileNameOnly(localCsvName, QStringLiteral("event.csv")));

    QString date = eventDate;
    QString time = eventTime;
    if (!normalizeEventDateTime(date, time)) {
        return safeName;
    }

    QString compactDate = date;
    compactDate.remove(QLatin1Char('-'));
    QString compactTime = time;
    compactTime.remove(QLatin1Char('-'));
    const QString canonicalStamp = compactDate + QStringLiteral("_") + compactTime;

    // Event filenames end with a compact yyyyMMdd_HHmmss timestamp, e.g.
    // KLM_..._M20260820_101501.csv.  Preserve the complete prefix/mode marker
    // and replace only that final timestamp for the REMOTE FTP object.
    const QRegularExpression stampRe(
        QStringLiteral("(20\\d{6})[_-](\\d{6})(?=\\.csv$)"),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = stampRe.match(safeName);
    if (!match.hasMatch()) {
        qWarning() << "[EVENT-FTP][CSV-NAME] no terminal event timestamp found; keep local name:"
                   << safeName << "event=" << date << time;
        return safeName;
    }

    safeName.replace(match.capturedStart(0), match.capturedLength(0), canonicalStamp);
    return safeName;
}

bool PLCServer::uplouploadFileWithtoftpServer(QString mode,
                                                QString eventCsvPath,
                                                QString picturePath,
                                                QString pictureName,
                                                QString patternPath,
                                                QString patternName) {
    // Never leave remote-path snapshots from an older FTP transaction around.
    // They are populated again only after the corresponding upload is verified.
    verifiedFtpEventCsvRemotePath.clear();
    verifiedFtpPictureRemotePath.clear();
    verifiedFtpPatternRemotePath.clear();

    {
        QJsonObject d;
        d.insert(QStringLiteral("mode"), mode);
        d.insert(QStringLiteral("eventCsvPath"), eventCsvPath);
        d.insert(QStringLiteral("picturePath"), picturePath);
        d.insert(QStringLiteral("pictureName"), pictureName);
        d.insert(QStringLiteral("patternPath"), patternPath);
        d.insert(QStringLiteral("patternName"), patternName);
        auditEventStep(QStringLiteral("FTP_BUNDLE"),
                       QStringLiteral("Mandatory FTP bundle verification"),
                       QStringLiteral("START"),
                       QStringLiteral("Starting centralized event FTP transaction after mandatory local verification"),
                       eventCsvPath + QStringLiteral(" | ") + picturePath,
                       FTP_Param_ ? FTP_Param_->FTP_IP : QStringLiteral("FTP"), d);
    }

    // This function is the single EVENT FTP owner. It must only be called after
    // Picture + Event CSV passed both local verification stages.
    QString safeMode = canonicalRecoveryMode(mode);
    if (safeMode != QStringLiteral("Manual") &&
        safeMode != QStringLiteral("Relay") &&
        safeMode != QStringLiteral("Surge") &&
        safeMode != QStringLiteral("Periodic")) {
        qWarning() << "[EVENT-FTP][BLOCK] invalid event mode:" << mode;
        QJsonObject d; d.insert(QStringLiteral("mode"), mode);
        auditEventStep(QStringLiteral("FTP_BUNDLE"), QStringLiteral("Mandatory FTP bundle verification"),
                       QStringLiteral("FAIL"), QStringLiteral("FTP blocked because event mode is invalid"),
                       eventCsvPath + QStringLiteral(" | ") + picturePath, QStringLiteral("FTP blocked"), d);
        return false;
    }

    const QFileInfo localPicInfo(QDir::cleanPath(picturePath));
    const QFileInfo localCsvInfo(QDir::cleanPath(eventCsvPath));

    if (!localPicInfo.exists() || !localPicInfo.isFile() || localPicInfo.isDir() ||
        localPicInfo.isSymLink() || localPicInfo.size() <= 0 ||
        !isPathInsideBase(localPicInfo.absoluteFilePath(), PIC_PATH) ||
        !isVerifiedDownloadedPictureFile(localPicInfo.absoluteFilePath())) {
        qWarning() << "[EVENT-FTP][BLOCK] mandatory Picture invalid before FTP:"
                   << localPicInfo.absoluteFilePath();
        auditEventStep(QStringLiteral("FTP_PICTURE"), QStringLiteral("Upload and download-back verify Picture"),
                       QStringLiteral("FAIL"), QStringLiteral("Mandatory local Picture became invalid before FTP"),
                       localPicInfo.absoluteFilePath(), QStringLiteral("FTP blocked"));
        return false;
    }

    if (!localCsvInfo.exists() || !localCsvInfo.isFile() || localCsvInfo.isDir() ||
        localCsvInfo.isSymLink() || localCsvInfo.size() <= 0 ||
        !isPathInsideBase(localCsvInfo.absoluteFilePath(), EVENT_PATH) ||
        isForbiddenEventCsvCandidate(localCsvInfo.absoluteFilePath())) {
        qWarning() << "[EVENT-FTP][BLOCK] mandatory Event CSV invalid/Pattern before FTP:"
                   << localCsvInfo.absoluteFilePath();
        auditEventStep(QStringLiteral("FTP_EVENT_CSV"), QStringLiteral("Upload and download-back verify Event CSV"),
                       QStringLiteral("FAIL"), QStringLiteral("Mandatory local Event CSV became invalid or was identified as Pattern before FTP"),
                       localCsvInfo.absoluteFilePath(), QStringLiteral("FTP blocked"));
        return false;
    }

    if (!FTP_Param_) {
        qWarning() << "[EVENT-FTP][BLOCK] FTP_Param_ is null";
        auditEventStep(QStringLiteral("FTP_BUNDLE"), QStringLiteral("Mandatory FTP bundle verification"),
                       QStringLiteral("FAIL"), QStringLiteral("FTP configuration object is unavailable"),
                       eventCsvPath + QStringLiteral(" | ") + picturePath, QStringLiteral("FTP configuration"));
        return false;
    }

    const QString username = FTP_Param_->USERNAME.trimmed();
    const QString password = FTP_Param_->PASSWORD;
    const QString host = FTP_Param_->FTP_IP.trimmed();
    if (username.isEmpty() || host.isEmpty()) {
        qWarning() << "[EVENT-FTP][BLOCK] FTP username/host is empty";
        QJsonObject d; d.insert(QStringLiteral("host"), host); d.insert(QStringLiteral("usernamePresent"), !username.isEmpty());
        auditEventStep(QStringLiteral("FTP_BUNDLE"), QStringLiteral("Mandatory FTP bundle verification"),
                       QStringLiteral("FAIL"), QStringLiteral("FTP host or username is empty; credentials are not included in audit logs"),
                       eventCsvPath + QStringLiteral(" | ") + picturePath, host, d);
        return false;
    }

    // EVENT base date/time is authoritative from objectName=eventRecord.
    // Picture and local CSV timestamps are correlation evidence only.
    QString ftpEventDate = eventBaseDateSnapshot.trimmed();
    QString ftpEventTime = eventBaseTimeSnapshot.trimmed();
    const QString ftpBaseMode = canonicalRecoveryMode(eventBaseModeSnapshot);

    if (!normalizeEventDateTime(ftpEventDate, ftpEventTime) ||
        (!ftpBaseMode.isEmpty() &&
         ftpBaseMode.compare(safeMode, Qt::CaseInsensitive) != 0)) {
        // Compatibility fallback for legacy paths: use the transaction snapshot
        // captured at getScreenPictureandSave(), which itself prefers eventRecord.
        ftpEventDate = pictureEventDateSnapshot;
        ftpEventTime = pictureEventTimeSnapshot;
        if (!normalizeEventDateTime(ftpEventDate, ftpEventTime)) {
            qWarning() << "[EVENT-FTP][BLOCK] authoritative Event base timestamp unavailable:"
                       << "base=" << eventBaseDateSnapshot << eventBaseTimeSnapshot
                       << "transaction=" << pictureEventDateSnapshot << pictureEventTimeSnapshot;
            auditEventStep(QStringLiteral("FTP_BUNDLE"), QStringLiteral("Mandatory FTP bundle verification"),
                           QStringLiteral("FAIL"), QStringLiteral("Authoritative Event base timestamp is unavailable"),
                           localPicInfo.absoluteFilePath(), QStringLiteral("FTP blocked"));
            return false;
        }
    }

    const QString actualLocalPicName = safeFileNameOnly(localPicInfo.fileName(), QStringLiteral("picture.png"));
    const QString requestedPicName = safeFileNameOnly(pictureName, QString());
    if (!requestedPicName.isEmpty() &&
        requestedPicName.compare(actualLocalPicName, Qt::CaseSensitive) != 0) {
        qWarning() << "[EVENT-FTP][LAYOUT] pictureName argument differs from physical file; use physical filename:"
                   << requestedPicName << "->" << actualLocalPicName;
    }

    // Correlate Picture capture timestamp with the Event base, but never use it
    // to change the remote folder.  A few seconds of capture delay is expected.
    QString picDate;
    QString picTime;
    if (extractEventDateTimeFromText(actualLocalPicName, picDate, picTime) &&
        !eventDateTimeWithinSeconds(ftpEventDate, ftpEventTime, picDate, picTime)) {
        qWarning() << "[EVENT-FTP][BLOCK] Picture timestamp is outside Event correlation window:"
                   << "eventBase=" << ftpEventDate << ftpEventTime
                   << "picture=" << picDate << picTime
                   << "file=" << actualLocalPicName;
        QJsonObject d;
        d.insert(QStringLiteral("eventDate"), ftpEventDate);
        d.insert(QStringLiteral("eventTime"), ftpEventTime);
        d.insert(QStringLiteral("pictureDate"), picDate);
        d.insert(QStringLiteral("pictureTime"), picTime);
        auditEventStep(QStringLiteral("FTP_BUNDLE"), QStringLiteral("Mandatory FTP bundle verification"),
                       QStringLiteral("FAIL"), QStringLiteral("Picture timestamp is outside the Event correlation window"),
                       localPicInfo.absoluteFilePath(), QStringLiteral("FTP blocked"), d);
        return false;
    }

    // Correlate local Event CSV timestamp with the same Event base.
    QString csvDate;
    QString csvTime;
    if (extractEventDateTimeFromText(localCsvInfo.absoluteFilePath(), csvDate, csvTime) &&
        !eventDateTimeWithinSeconds(ftpEventDate, ftpEventTime, csvDate, csvTime)) {
        qWarning() << "[EVENT-FTP][BLOCK] Event CSV timestamp is outside Event correlation window:"
                   << "eventBase=" << ftpEventDate << ftpEventTime
                   << "csv=" << csvDate << csvTime
                   << "file=" << localCsvInfo.absoluteFilePath();
        QJsonObject d;
        d.insert(QStringLiteral("eventDate"), ftpEventDate);
        d.insert(QStringLiteral("eventTime"), ftpEventTime);
        d.insert(QStringLiteral("csvDate"), csvDate);
        d.insert(QStringLiteral("csvTime"), csvTime);
        auditEventStep(QStringLiteral("FTP_BUNDLE"), QStringLiteral("Mandatory FTP bundle verification"),
                       QStringLiteral("FAIL"), QStringLiteral("Event CSV timestamp is outside the Event correlation window"),
                       localCsvInfo.absoluteFilePath(), QStringLiteral("FTP blocked"), d);
        return false;
    }

    qDebug() << "[EVENT-FTP][EVENT-BASE]"
             << "mode=" << safeMode
             << "date=" << ftpEventDate
             << "time=" << ftpEventTime
             << "pictureCapture=" << picDate << picTime
             << "csvLocalTime=" << csvDate << csvTime;

    FileVerifySnapshot csvFtpCheck;
    FileVerifySnapshot picFtpCheck;
    if (!captureVerifiedEventCsvSnapshot(localCsvInfo.absoluteFilePath(),
                                         EVENT_PATH,
                                         safeMode,
                                         ftpEventDate,
                                         ftpEventTime,
                                         &csvFtpCheck) ||
        !captureVerifiedPictureSnapshot(localPicInfo.absoluteFilePath(),
                                        PIC_PATH,
                                        ftpEventDate,
                                        ftpEventTime,
                                        &picFtpCheck)) {
        qWarning() << "[EVENT-FTP][BLOCK] mandatory files failed strict pre-network verification:"
                   << "csv=" << localCsvInfo.absoluteFilePath()
                   << "pic=" << localPicInfo.absoluteFilePath();
        auditEventStep(QStringLiteral("PRE_FTP_GATE"), QStringLiteral("Final mandatory gate before FTP"),
                       QStringLiteral("FAIL"), QStringLiteral("Mandatory Event CSV/Picture failed strict pre-network verification immediately before FTP"),
                       localCsvInfo.absoluteFilePath() + QStringLiteral(" | ") + localPicInfo.absoluteFilePath(),
                       QStringLiteral("FTP blocked"));
        return false;
    }

    const QString ftpSubstation = safeFtpPart(SetupEquipment ? SetupEquipment->SubstationName : QString(),
                                              QStringLiteral("Substation"));
    const QString ftpLine = safeFtpPart(SetupEquipment ? SetupEquipment->TransmissionLineName : QString(),
                                       QStringLiteral("Line"));
    const QString ftpDate = safeFtpPart(ftpEventDate, QStringLiteral("Date"));
    const QString ftpTime = safeFtpPart(ftpEventTime, QStringLiteral("Time"));

    QString configuredFtpRoot;
    QString ftpRootFallback;
    if (safeMode == QStringLiteral("Periodic")) {
        configuredFtpRoot = FTP_Param_->PERIODIC_FILE;
        ftpRootFallback = QStringLiteral("Periodic");
    } else if (safeMode == QStringLiteral("Relay")) {
        configuredFtpRoot = FTP_Param_->RELAY_FILE;
        ftpRootFallback = QStringLiteral("Relay");
    } else if (safeMode == QStringLiteral("Surge")) {
        configuredFtpRoot = FTP_Param_->SURGE_FILE;
        ftpRootFallback = QStringLiteral("Surge");
    } else {
        configuredFtpRoot = FTP_Param_->MANUAL_FILE;
        ftpRootFallback = QStringLiteral("Manual");
    }
    const QString ftpRoot = safeFtpRootPath(configuredFtpRoot, ftpRootFallback);

    const QStringList remoteParts{ftpRoot, ftpSubstation, ftpLine, safeMode, ftpDate, ftpTime};
    const QString remoteDir = joinFtpPath(remoteParts);

    const QString remoteCsvName = canonicalRemoteEventCsvName(
        localCsvInfo.fileName(), ftpEventDate, ftpEventTime);
    if (remoteCsvName.compare(localCsvInfo.fileName(), Qt::CaseInsensitive) != 0) {
        qDebug() << "[EVENT-FTP][CSV-NAME] canonicalized remote filename to Event base:"
                 << localCsvInfo.fileName() << "->" << remoteCsvName
                 << "base=" << ftpEventDate << ftpEventTime;
    }
    const QString remotePicName = actualLocalPicName;
    const QString remoteCsvPath = joinFtpPath(QStringList{remoteDir, remoteCsvName});
    const QString remotePicPath = joinFtpPath(QStringList{remoteDir, remotePicName});

    qDebug() << "[EVENT-FTP][LAYOUT] verified bundle mapping:"
             << "event=" << safeMode << ftpDate << ftpTime
             << "csvLocal=" << localCsvInfo.absoluteFilePath()
             << "csvRemote=" << remoteCsvPath
             << "picLocal=" << localPicInfo.absoluteFilePath()
             << "picRemote=" << remotePicPath;
    {
        QJsonObject d;
        d.insert(QStringLiteral("ftpHost"), host);
        d.insert(QStringLiteral("configuredRoot"), configuredFtpRoot);
        d.insert(QStringLiteral("sanitizedRoot"), ftpRoot);
        d.insert(QStringLiteral("remoteDir"), remoteDir);
        d.insert(QStringLiteral("csvLocal"), localCsvInfo.absoluteFilePath());
        d.insert(QStringLiteral("csvRemote"), remoteCsvPath);
        d.insert(QStringLiteral("picLocal"), localPicInfo.absoluteFilePath());
        d.insert(QStringLiteral("picRemote"), remotePicPath);
        d.insert(QStringLiteral("eventMode"), safeMode);
        d.insert(QStringLiteral("eventDate"), ftpDate);
        d.insert(QStringLiteral("eventTime"), ftpTime);
        auditEventStep(QStringLiteral("FTP_REMOTE_DIR"),
                       QStringLiteral("Prepare FTP remote event directory"),
                       QStringLiteral("START"),
                       QStringLiteral("Resolved local-to-remote FTP layout for this event"),
                       localCsvInfo.absolutePath(), remoteDir, d);
    }

    // Prepare the remote hierarchy one directory at a time.  This is intentionally
    // more verbose than the old mkdir-only script because Event Audit must be able
    // to identify the exact failing directory and FTP server response.
    const QStringList dirsToCreate = incrementalFtpDirs(remoteParts);
    QJsonArray directoryChecks;
    bool remoteDirReady = true;
    QString failedDir;
    QString failedOperation;
    FtpExecResult failedResult;

    for (const QString &dir : dirsToCreate) {
        QJsonObject check;
        check.insert(QStringLiteral("directory"), dir);

        const FtpExecResult cdBefore = runLftpDetailed(
            username, password, host,
            eventFtpSetupScript(QStringLiteral("cd %1").arg(lftpArg(dir))),
            8000);
        check.insert(QStringLiteral("checkBefore"), ftpExecResultJson(cdBefore));

        if (cdBefore.success) {
            check.insert(QStringLiteral("result"), QStringLiteral("EXISTS"));
            directoryChecks.append(check);
            continue;
        }

        const FtpExecResult mkdirResult = runLftpDetailed(
            username, password, host,
            eventFtpSetupScript(QStringLiteral("mkdir %1").arg(lftpArg(dir))),
            8000);
        check.insert(QStringLiteral("mkdir"), ftpExecResultJson(mkdirResult));

        // Always verify with cd after mkdir. Some FTP servers return a failing
        // status even though the directory was created successfully.
        const FtpExecResult cdAfter = runLftpDetailed(
            username, password, host,
            eventFtpSetupScript(QStringLiteral("cd %1").arg(lftpArg(dir))),
            8000);
        check.insert(QStringLiteral("checkAfter"), ftpExecResultJson(cdAfter));

        if (cdAfter.success) {
            check.insert(QStringLiteral("result"),
                         mkdirResult.success ? QStringLiteral("CREATED")
                                             : QStringLiteral("EXISTS_AFTER_MKDIR_ERROR"));
            directoryChecks.append(check);
            continue;
        }

        check.insert(QStringLiteral("result"), QStringLiteral("FAIL"));
        directoryChecks.append(check);
        remoteDirReady = false;
        failedDir = dir;
        failedOperation = QStringLiteral("CD/MKDIR/CD_VERIFY");
        failedResult = cdAfter;
        if (failedResult.stderrText.isEmpty() && !mkdirResult.stderrText.isEmpty())
            failedResult = mkdirResult;
        break;
    }

    if (!remoteDirReady) {
        qWarning() << "[EVENT-FTP][BLOCK] remote directory prepare failed:"
                   << "failedDir=" << failedDir
                   << "operation=" << failedOperation
                   << "exitCode=" << failedResult.exitCode
                   << "timeout=" << failedResult.timedOut
                   << "stderr=" << failedResult.stderrText;

        QJsonObject d;
        d.insert(QStringLiteral("ftpHost"), host);
        d.insert(QStringLiteral("configuredRoot"), configuredFtpRoot);
        d.insert(QStringLiteral("sanitizedRoot"), ftpRoot);
        d.insert(QStringLiteral("remoteDir"), remoteDir);
        d.insert(QStringLiteral("failedDir"), failedDir);
        d.insert(QStringLiteral("operation"), failedOperation);
        d.insert(QStringLiteral("result"), ftpExecResultJson(failedResult));
        d.insert(QStringLiteral("failureClass"), classifyFtpFailure(failedResult));
        d.insert(QStringLiteral("directoryChecks"), directoryChecks);
        auditEventStep(QStringLiteral("FTP_REMOTE_DIR"),
                       QStringLiteral("Prepare FTP remote event directory"),
                       QStringLiteral("FAIL"),
                       QStringLiteral("FTP remote directory preparation failed; exact directory and server diagnostics are available in details"),
                       QStringLiteral("PLCServer"), failedDir, d);

        QJsonObject blocked;
        blocked.insert(QStringLiteral("reason"), QStringLiteral("FTP_REMOTE_DIR_FAILED"));
        blocked.insert(QStringLiteral("failedDir"), failedDir);
        auditEventStep(QStringLiteral("FTP_EVENT_CSV"),
                       QStringLiteral("Upload and download-back verify Event CSV"),
                       QStringLiteral("BLOCK"),
                       QStringLiteral("Event CSV upload was not started because FTP remote directory preparation failed"),
                       eventCsvPath, remoteCsvPath, blocked);
        auditEventStep(QStringLiteral("FTP_PICTURE"),
                       QStringLiteral("Upload and download-back verify Picture"),
                       QStringLiteral("BLOCK"),
                       QStringLiteral("Picture upload was not started because FTP remote directory preparation failed"),
                       picturePath, remotePicPath, blocked);
        auditEventStep(QStringLiteral("FTP_PATTERN"),
                       QStringLiteral("Upload optional Pattern"),
                       QStringLiteral("SKIP"),
                       QStringLiteral("Optional Pattern upload was skipped because the mandatory FTP directory stage failed"),
                       patternPath, remoteDir, blocked);
        return false;
    }

    {
        QJsonObject d;
        d.insert(QStringLiteral("ftpHost"), host);
        d.insert(QStringLiteral("configuredRoot"), configuredFtpRoot);
        d.insert(QStringLiteral("sanitizedRoot"), ftpRoot);
        d.insert(QStringLiteral("remoteDir"), remoteDir);
        d.insert(QStringLiteral("directoryChecks"), directoryChecks);
        auditEventStep(QStringLiteral("FTP_REMOTE_DIR"),
                       QStringLiteral("Prepare FTP remote event directory"),
                       QStringLiteral("PASS"),
                       QStringLiteral("FTP remote event directory hierarchy exists and was verified level-by-level"),
                       QStringLiteral("PLCServer"), remoteDir, d);
    }

    auto uploadAndVerify = [&](const QString &localPath,
                               const QString &remoteName,
                               const QString &label,
                               bool pictureType) -> bool {
        const QString auditCode =
            (label == QStringLiteral("CSV")) ? QStringLiteral("FTP_EVENT_CSV") :
            (label == QStringLiteral("PIC")) ? QStringLiteral("FTP_PICTURE") :
                                                QStringLiteral("FTP_PATTERN");
        const QString auditName =
            (label == QStringLiteral("CSV")) ? QStringLiteral("Upload and download-back verify Event CSV") :
            (label == QStringLiteral("PIC")) ? QStringLiteral("Upload and download-back verify Picture") :
                                                QStringLiteral("Upload optional Pattern");
        const QFileInfo initialInfo(QDir::cleanPath(localPath));
        if (!initialInfo.exists() || !initialInfo.isFile() || initialInfo.isDir() ||
            initialInfo.isSymLink() || initialInfo.size() <= 0) {
            qWarning() << "[EVENT-FTP][" << label << "][BLOCK] local source invalid:" << localPath;
            auditEventStep(auditCode, auditName, QStringLiteral("FAIL"),
                           QStringLiteral("Local source file is invalid before FTP upload"),
                           localPath, joinFtpPath(QStringList{remoteDir, remoteName}));
            return false;
        }

        const QString verifyTempPath = QDir::temp().absoluteFilePath(
            QStringLiteral("plc_event_verify_%1_%2_%3")
                .arg(QCoreApplication::applicationPid())
                .arg(label)
                .arg(safeFileNameOnly(remoteName, QStringLiteral("verify.tmp"))));

        bool verified = false;
        for (int attempt = 1; attempt <= 2; ++attempt) {
            QFile::remove(verifyTempPath);

            const QString remoteFullPath = joinFtpPath(QStringList{remoteDir, remoteName});
            QJsonObject attemptDetail;
            attemptDetail.insert(QStringLiteral("attempt"), attempt);
            attemptDetail.insert(QStringLiteral("maxAttempts"), 2);
            attemptDetail.insert(QStringLiteral("ftpHost"), host);
            attemptDetail.insert(QStringLiteral("localPath"), localPath);
            attemptDetail.insert(QStringLiteral("remotePath"), remoteFullPath);
            attemptDetail.insert(QStringLiteral("verifyTempPath"), verifyTempPath);
            auditEventStep(auditCode, auditName,
                           attempt == 1 ? QStringLiteral("START") : QStringLiteral("RETRY"),
                           attempt == 1
                               ? QStringLiteral("Starting FTP upload and download-back verification")
                               : QStringLiteral("Retrying FTP upload and download-back verification"),
                           localPath, remoteFullPath, attemptDetail);

            const QFileInfo currentInfo(QDir::cleanPath(localPath));
            if (!currentInfo.exists() || !currentInfo.isFile() || currentInfo.isDir() ||
                currentInfo.isSymLink() || currentInfo.size() != initialInfo.size()) {
                qWarning() << "[EVENT-FTP][" << label << "][BLOCK] local file disappeared/changed before upload:"
                           << localPath;
                QJsonObject d = attemptDetail;
                d.insert(QStringLiteral("initialSize"), double(initialInfo.size()));
                d.insert(QStringLiteral("currentSize"), double(currentInfo.exists() ? currentInfo.size() : 0));
                auditEventStep(auditCode, auditName, QStringLiteral("FAIL"),
                               QStringLiteral("Local file disappeared or changed before FTP upload"),
                               localPath, remoteFullPath, d);
                break;
            }

            const QString putScript = eventFtpSetupScript(
                QStringLiteral("cd %1; put %2 -o %3")
                    .arg(lftpArg(remoteDir),
                         lftpArg(currentInfo.absoluteFilePath()),
                         lftpArg(remoteName)),
                8);
            const FtpExecResult putResult = runLftpDetailed(
                username, password, host, putScript, 8000);
            if (!putResult.success) {
                qWarning() << "[EVENT-FTP][" << label << "] upload failed attempt" << attempt
                           << currentInfo.absoluteFilePath()
                           << "->" << remoteFullPath
                           << "exitCode=" << putResult.exitCode
                           << "timeout=" << putResult.timedOut
                           << "stderr=" << putResult.stderrText;
                QJsonObject d = attemptDetail;
                d.insert(QStringLiteral("operation"), QStringLiteral("PUT"));
                d.insert(QStringLiteral("failureClass"), classifyFtpFailure(putResult));
                d.insert(QStringLiteral("ftpResult"), ftpExecResultJson(putResult));
                auditEventStep(auditCode, auditName,
                               attempt < 2 ? QStringLiteral("RETRY") : QStringLiteral("FAIL"),
                               QStringLiteral("FTP put failed; server/process diagnostics are available in details"),
                               currentInfo.absoluteFilePath(), remoteFullPath, d);
                QThread::msleep(500);
                continue;
            }

            const QString verifyScript = eventFtpSetupScript(
                QStringLiteral("cd %1; get %2 -o %3")
                    .arg(lftpArg(remoteDir),
                         lftpArg(remoteName),
                         lftpArg(verifyTempPath)),
                8);
            const FtpExecResult verifyResult = runLftpDetailed(
                username, password, host, verifyScript, 8000);
            if (!verifyResult.success) {
                qWarning() << "[EVENT-FTP][" << label << "] verify download failed attempt" << attempt
                           << remoteFullPath
                           << "exitCode=" << verifyResult.exitCode
                           << "timeout=" << verifyResult.timedOut
                           << "stderr=" << verifyResult.stderrText;
                QJsonObject d = attemptDetail;
                d.insert(QStringLiteral("operation"), QStringLiteral("GET_VERIFY"));
                d.insert(QStringLiteral("failureClass"), classifyFtpFailure(verifyResult));
                d.insert(QStringLiteral("ftpResult"), ftpExecResultJson(verifyResult));
                auditEventStep(auditCode, auditName,
                               attempt < 2 ? QStringLiteral("RETRY") : QStringLiteral("FAIL"),
                               QStringLiteral("FTP upload completed but download-back verification command failed; diagnostics are available in details"),
                               remoteFullPath, verifyTempPath, d);
                QThread::msleep(500);
                continue;
            }

            qint64 verifySize = 0;
            const bool verifyFileOk = pictureType
                ? isVerifiedDownloadedPictureFile(verifyTempPath, &verifySize)
                : (captureRegularReadableSnapshot(verifyTempPath, nullptr) &&
                   ((verifySize = QFileInfo(verifyTempPath).size()) > 0));

            if (verifyFileOk && verifySize == currentInfo.size()) {
                verified = true;
                qDebug() << "[EVENT-FTP][" << label << "][VERIFIED]"
                         << currentInfo.absoluteFilePath()
                         << "->" << joinFtpPath(QStringList{remoteDir, remoteName})
                         << "size=" << currentInfo.size()
                         << "attempt=" << attempt;
                QJsonObject d = attemptDetail;
                d.insert(QStringLiteral("localSize"), double(currentInfo.size()));
                d.insert(QStringLiteral("verifySize"), double(verifySize));
                d.insert(QStringLiteral("downloadBackVerified"), true);
                auditEventStep(auditCode, auditName, QStringLiteral("PASS"),
                               QStringLiteral("FTP upload succeeded and remote file passed download-back size/type verification"),
                               currentInfo.absoluteFilePath(), remoteFullPath, d);
                break;
            }

            qWarning() << "[EVENT-FTP][" << label << "] verify mismatch attempt" << attempt
                       << "localSize=" << currentInfo.size()
                       << "verifySize=" << verifySize
                       << "remote=" << joinFtpPath(QStringList{remoteDir, remoteName});
            QJsonObject d = attemptDetail;
            d.insert(QStringLiteral("localSize"), double(currentInfo.size()));
            d.insert(QStringLiteral("verifySize"), double(verifySize));
            auditEventStep(auditCode, auditName,
                           attempt < 2 ? QStringLiteral("RETRY") : QStringLiteral("FAIL"),
                           QStringLiteral("Downloaded remote verification file does not match local source size/type"),
                           localPath, remoteFullPath, d);
            QThread::msleep(500);
        }

        QFile::remove(verifyTempPath);
        return verified;
    };

    // Both mandatory local files already passed the gate. FTP success is also
    // required for both before any socket/mail notification can be published.
    if (!uploadAndVerify(localCsvInfo.absoluteFilePath(), remoteCsvName,
                         QStringLiteral("CSV"), false)) {
        qWarning() << "[EVENT-FTP][BLOCK] Event CSV upload was not verified; stop bundle:" << eventCsvPath;
        QJsonObject blocked;
        blocked.insert(QStringLiteral("reason"), QStringLiteral("FTP_EVENT_CSV_FAILED"));
        auditEventStep(QStringLiteral("FTP_PICTURE"),
                       QStringLiteral("Upload and download-back verify Picture"),
                       QStringLiteral("BLOCK"),
                       QStringLiteral("Picture FTP was not started because mandatory Event CSV FTP failed"),
                       picturePath, remotePicPath, blocked);
        auditEventStep(QStringLiteral("FTP_PATTERN"),
                       QStringLiteral("Upload optional Pattern"),
                       QStringLiteral("SKIP"),
                       QStringLiteral("Optional Pattern FTP was skipped because mandatory Event CSV FTP failed"),
                       patternPath, remoteDir, blocked);
        return false;
    }

    if (!uploadAndVerify(localPicInfo.absoluteFilePath(), remotePicName,
                         QStringLiteral("PIC"), true)) {
        qWarning() << "[EVENT-FTP][BLOCK] Picture upload was not verified; stop bundle:" << picturePath;
        QJsonObject blocked;
        blocked.insert(QStringLiteral("reason"), QStringLiteral("FTP_PICTURE_FAILED"));
        auditEventStep(QStringLiteral("FTP_PATTERN"),
                       QStringLiteral("Upload optional Pattern"),
                       QStringLiteral("SKIP"),
                       QStringLiteral("Optional Pattern FTP was skipped because mandatory Picture FTP failed"),
                       patternPath, remoteDir, blocked);
        return false;
    }

    // Mandatory remote paths are authoritative only after BOTH mandatory
    // uploads have been verified by download-back size/type checks.
    verifiedFtpEventCsvRemotePath = remoteCsvPath;
    verifiedFtpPictureRemotePath = remotePicPath;

    // Pattern is optional. It is uploaded only when it is a verified physical
    // file under PATTERN_PATH. Failure never substitutes another path and never
    // invalidates the already-valid mandatory event bundle.
    FileVerifySnapshot patternSnapshot;
    if (captureOptionalPatternSnapshot(patternPath, PATTERN_PATH, &patternSnapshot)) {
        QString physicalPatternName = QFileInfo(patternSnapshot.path).fileName();
        const QString requestedPatternName = safeFileNameOnly(patternName, QString());
        if (!requestedPatternName.isEmpty() &&
            requestedPatternName.compare(physicalPatternName, Qt::CaseInsensitive) != 0) {
            qWarning() << "[EVENT-FTP][PATTERN] requested name differs from physical file; use physical name:"
                       << requestedPatternName << "->" << physicalPatternName;
        }
        physicalPatternName = ensureCsvSuffix(
            safeFileNameOnly(physicalPatternName, QStringLiteral("pattern.csv")));
        if (!uploadAndVerify(patternSnapshot.path, physicalPatternName,
                             QStringLiteral("PATTERN"), false)) {
            qWarning() << "[EVENT-FTP][PATTERN] optional Pattern upload failed; continue without requiring Pattern:"
                       << patternSnapshot.path;
            verifiedFtpPatternRemotePath.clear();
        } else {
            verifiedFtpPatternRemotePath = joinFtpPath(QStringList{remoteDir, physicalPatternName});
        }
    } else {
        QJsonObject d;
        d.insert(QStringLiteral("patternOptional"), true);
        if (!patternPath.trimmed().isEmpty()) {
            qWarning() << "[EVENT-FTP][PATTERN] optional Pattern invalid/missing; skip:" << patternPath;
            d.insert(QStringLiteral("requestedPatternPath"), patternPath);
            auditEventStep(QStringLiteral("FTP_PATTERN"),
                           QStringLiteral("Upload optional Pattern"),
                           QStringLiteral("SKIP"),
                           QStringLiteral("Optional Pattern is missing/invalid; Event remains valid because Pattern is not mandatory"),
                           patternPath, remoteDir, d);
        } else {
            auditEventStep(QStringLiteral("FTP_PATTERN"),
                           QStringLiteral("Upload optional Pattern"),
                           QStringLiteral("SKIP"),
                           QStringLiteral("No optional Pattern was provided for this Event"),
                           QString(), remoteDir, d);
        }
    }

    qDebug() << "[EVENT-FTP][PASS] mandatory Event CSV + Picture uploaded and verified:"
             << "csv=" << eventCsvPath << "pic=" << picturePath;
    {
        QJsonObject d;
        d.insert(QStringLiteral("csvRemote"), verifiedFtpEventCsvRemotePath);
        d.insert(QStringLiteral("picRemote"), verifiedFtpPictureRemotePath);
        d.insert(QStringLiteral("patternRemote"), verifiedFtpPatternRemotePath);
        d.insert(QStringLiteral("patternOptional"), true);
        auditEventStep(QStringLiteral("FTP_BUNDLE"), QStringLiteral("Mandatory FTP bundle verification"),
                       QStringLiteral("PASS"),
                       QStringLiteral("FTP transaction completed: mandatory Event CSV and Picture are verified; Pattern remains optional"),
                       eventCsvPath + QStringLiteral(" | ") + picturePath,
                       verifiedFtpEventCsvRemotePath + QStringLiteral(" | ") + verifiedFtpPictureRemotePath, d);
    }
    return true;
}

void PLCServer::uploadFTP(QString msg)
{
    qDebug() << "uploadFTP" << msg.left(4) << "command=" << (msg.isEmpty() ? QStringLiteral("<empty>") : QStringLiteral("lftp ..."));
    uploadsFTP = msg;
    stopThread3 = false;
    int ret = pthread_create(&idThread3, NULL, ThreadFunc3, this);
    if (ret == 0) {
        qDebug() << ("Thread3 created successfully.\n");
    } else {
        qDebug() << ("Thread3 not created.\n");
    }
}

void PLCServer::uploadFileWithtoftpServer(QString filePath, QString ftpUrl, QString username, QString password)
{
    const QFileInfo fileInfo(QDir::cleanPath(filePath));
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        qWarning() << "[uploadFileWithtoftpServer] source file not found:" << fileInfo.absoluteFilePath();
        return;
    }

    QUrl url(ftpUrl.trimmed());
    const QString scheme = url.scheme().toLower();
    if (!url.isValid() || (scheme != QStringLiteral("ftp") && scheme != QStringLiteral("ftps"))) {
        qWarning() << "[uploadFileWithtoftpServer] blocked URL:" << ftpUrl;
        return;
    }

    url.setUserName(username);
    url.setPassword(password);

    qDebug() << "[uploadFileWithtoftpServer] upload"
             << fileInfo.absoluteFilePath()
             << "->"
             << url.toString(QUrl::RemovePassword);

    QFile *file = new QFile(fileInfo.absoluteFilePath());
    if (!file->open(QIODevice::ReadOnly)) {
        qWarning() << "[uploadFileWithtoftpServer] cannot open file for reading:"
                   << fileInfo.absoluteFilePath() << file->errorString();
        delete file;
        return;
    }

    QNetworkRequest request(url);
    QNetworkReply *reply = manager->put(request, file);
    file->setParent(reply);

    QObject::connect(reply, &QNetworkReply::finished, [reply, file]() {
        file->close();

        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "[uploadFileWithtoftpServer] upload error:" << reply->errorString();
        } else {
            qDebug() << "[uploadFileWithtoftpServer] upload successful";
        }

        reply->deleteLater();
    });
}

void PLCServer::getPicturePlot(QString filename, QString url, QString time)
{
    const QString pictureDir = QStringLiteral("/var/www/html/Picture");
    if (!ensureDirExists(pictureDir)) {
        qWarning() << "[getPicturePlot] cannot create picture directory:" << pictureDir;
        return;
    }

    QDir dir(pictureDir);
    const int fileCount = dir.entryList(QDir::Files | QDir::NoDotAndDotDot).size();
    const QString safeMode = safeFileNameOnly(modeName, QStringLiteral("mode"));
    const QString safeTime = safeFileNameOnly(time, QStringLiteral("time"));
    const QString safeInputName = safeFileNameOnly(filename, QStringLiteral("picture.bin"));
    const QString path = dir.filePath(QStringLiteral("%1_%2_%3_%4")
                                      .arg(safeMode)
                                      .arg(safeTime)
                                      .arg(fileCount + 1)
                                      .arg(safeInputName));

    if (!downloadUrlToFile(url, path)) {
        qWarning() << "[getPicturePlot] download failed:" << url << "->" << path;
        return;
    }

    runProcessChecked(QStringLiteral("curl"),
                      QStringList{QStringLiteral("-u"),
                                  QStringLiteral("pi:11111"),
                                  QStringLiteral("-T"),
                                  path,
                                  QStringLiteral("ftp://192.168.10.192/upload/")},
                      30000);
}

void PLCServer::updateMode(QString name) {
    modeName = name;
    qDebug() << "1 updateMode:" << modeName;
    if (modeName == "Manual") {
        OpenPLC_param->MANUAL_TEST_EVENT = true;
        PLCserver_param->MANUAL_TEST_EVENT = true;
        //        QJsonDocument jsonDoc;
        //        QJsonObject Param;
        //        QString raw_datas;
        //        Param.insert("TrapsAlert","MANUAL_TEST_EVENT");
        //        Param.insert("state",OpenPLC_param->MANUAL_TEST_EVENT);
        //        jsonDoc.setObject(Param);
        //        raw_datas = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        //        if(FPGA_address->state() == QAbstractSocket::ConnectedState)
        //            emit sendMessage(raw_datas, FPGA_address);
        //        else
        //            qDebug() << "FPGA_address:" << FPGA_address->state();
    } else if (modeName == "Surge") {
        OpenPLC_param->SURGE_START_EVENT = true;
        PLCserver_param->SURGE_START_EVENT = true;
        //        QJsonDocument jsonDoc;
        //        QJsonObject Param;
        //        QString raw_datas;
        //        Param.insert("TrapsAlert","SURGE_START_EVENT");
        //        Param.insert("state",OpenPLC_param->SURGE_START_EVENT);
        //        jsonDoc.setObject(Param);
        //        raw_datas = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        //        if(FPGA_address->state() == QAbstractSocket::ConnectedState)
        //            emit sendMessage(raw_datas, FPGA_address);
        //        else
        //            qDebug() << "FPGA_address:" << FPGA_address->state();
    } else if (modeName == "Relay") {
        OpenPLC_param->RELAY_START_EVENT = true;
        PLCserver_param->RELAY_START_EVENT = true;
        OpenPLC_param->PLC_DI_ERROR = true;
        OpenPLC_param->PLC_DO_ERROR = true;
        //        QJsonDocument jsonDoc;
        //        QJsonObject Param;
        //        QString raw_datas;
        //        Param.insert("TrapsAlert","RELAY_START_EVENT");
        //        Param.insert("state",OpenPLC_param->RELAY_START_EVENT);
        //        jsonDoc.setObject(Param);
        //        raw_datas = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        //        if(FPGA_address->state() == QAbstractSocket::ConnectedState)
        //            emit sendMessage(raw_datas, FPGA_address);
        //        else
        //            qDebug() << "FPGA_address:" << FPGA_address->state();
    } else if (modeName == "Periodic") {
        OpenPLC_param->PERIODIC_TEST_EVENT = true;
        PLCserver_param->PERIODIC_TEST_EVENT = true;
        //        QJsonDocument jsonDoc;
        //        QJsonObject Param;
        //        QString raw_datas;
        //        Param.insert("TrapsAlert","PERIODIC_TEST_EVENT");
        //        Param.insert("state",OpenPLC_param->PERIODIC_TEST_EVENT);
        //        jsonDoc.setObject(Param);
        //        raw_datas = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        //        if(FPGA_address->state() == QAbstractSocket::ConnectedState)
        //            emit sendMessage(raw_datas, FPGA_address);
        //        else
        //            qDebug() << "FPGA_address:" << FPGA_address->state();
    } else if (modeName == "Pattern") {
        interlockPattern = true;
        OpenPLC_param->MANUAL_TEST_EVENT = true;
        PLCserver_param->MANUAL_TEST_EVENT = true;
        //        QJsonDocument jsonDoc;
        //        QJsonObject Param;
        //        QString raw_datas;
        //        Param.insert("TrapsAlert","MANUAL_TEST_EVENT");
        //        Param.insert("state",OpenPLC_param->MANUAL_TEST_EVENT);
        //        jsonDoc.setObject(Param);
        //        raw_datas = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        //        if(FPGA_address->state() == QAbstractSocket::ConnectedState)
        //            emit sendMessage(raw_datas, FPGA_address);
        //        else
        //            qDebug() << "FPGA_address:" << FPGA_address->state();
    }
    qDebug() << "updateMode" << OpenPLC_param->RELAY_START_EVENT << modeName;
    updateEvent = true;
}

void PLCServer::updateProgram() {
    qDebug() << "getSetting";
    QSettings* settings;
    const QString cfgfile = FILESETTING;
    qDebug() << "Loading configuration from:" << cfgfile;
    if (QDir::isAbsolutePath(cfgfile)) {
        settings = new QSettings(cfgfile, QSettings::IniFormat);
        settings->setValue(QString("%1/LANGUAGE").arg("PROGRAM"), language);
    } else {
        qDebug() << "Loading configuration from:" << cfgfile << " FILE NOT FOUND!";
    }
    qDebug() << "Loading configuration completed";
    delete settings;
}

void PLCServer::getSetting() {
    qDebug() << "getSetting";
    QSettings* settings;
    const QString cfgfile = FILESETTING;
    qDebug() << "Loading configuration from:" << cfgfile;
    if (QDir::isAbsolutePath(cfgfile)) {
        qDebug() << "isAbsolutePath";
        settings = new QSettings(cfgfile, QSettings::IniFormat);
        relayOperateTime = settings->value(QString("%1/DELAY").arg(RELAY_OPERATE), 300000).toInt();
        qWarning() << " relayOperateTime val -->" << relayOperateTime;
        networks->dhcpmethod = settings->value(QString("%1/DHCP").arg(NETWORK_SERVER), 0).toInt();
        networks->ip_address = settings->value(QString("%1/IP_ADDRESS").arg(NETWORK_SERVER), "").toString();
        networks->subnet = settings->value(QString("%1/NETMASK").arg(NETWORK_SERVER), "").toString();
        networks->ip_gateway = settings->value(QString("%1/IP_GATEWAY").arg(NETWORK_SERVER), "").toString();
        networks->pridns = settings->value(QString("%1/PRIDNS").arg(NETWORK_SERVER), "").toString();
        networks->secdns = settings->value(QString("%1/SECDNS").arg(NETWORK_SERVER), "").toString();
        networks->phyName = settings->value(QString("%1/PHYNAME").arg(NETWORK_SERVER), "").toString();

        networks->ip_snmp = settings->value(QString("%1/IP_ADDRESS").arg(SNMP_SERVER), "0.0.0.0").toString();

        Email_Config->senderEmail = settings->value(QString("%1/SENDER_EMAIL").arg(SMTP_SERVER), "").toString();
        Email_Config->senderName = settings->value(QString("%1/SENDER_NAME").arg(SMTP_SERVER), "").toString();
        Email_Config->password = settings->value(QString("%1/PASSWORD").arg(SMTP_SERVER), "").toString();
        Email_Config->recipientEmail = settings->value(QString("%1/RECIPIENT_EMAIL").arg(SMTP_SERVER), "").toString();
        Email_Config->recipientName = settings->value(QString("%1/RECIPIENT_NAME").arg(SMTP_SERVER), "").toString();
        Email_Config->smtpServer = settings->value(QString("%1/SERVER").arg(SMTP_SERVER), "").toString();
        Email_Config->smtpPort = settings->value(QString("%1/PORT").arg(SMTP_SERVER), "").toInt();

        delays->delay0 = settings->value(QString("%1/DELAY0").arg(DELAYS), "").toInt();
        delays->delay1 = settings->value(QString("%1/DELAY1").arg(DELAYS), "").toInt();
        delays->delay2 = settings->value(QString("%1/DELAY2").arg(DELAYS), "").toInt();
        delays->delay3 = settings->value(QString("%1/DELAY3").arg(DELAYS), "").toInt();
        delays->delay4 = settings->value(QString("%1/DELAY4").arg(DELAYS), "").toInt();
        delays->delay5 = settings->value(QString("%1/DELAY5").arg(DELAYS), "").toInt();
        delays->delay6 = settings->value(QString("%1/DELAY6").arg(DELAYS), "").toInt();
        delays->delay7 = settings->value(QString("%1/DELAY7").arg(DELAYS), "").toInt();

        Email_Param->PLC_DO_ERROR_MAIL = settings->value(QString("%1/PLC_DO_ERROR_MAIL").arg(EMAIL_PATH), "").toBool();
        Email_Param->PLC_DI_ERROR_MAIL = settings->value(QString("%1/PLC_DI_ERROR_MAIL").arg(EMAIL_PATH), "").toBool();
        Email_Param->MODULE_HI_SPEED_PHASE_A_ERROR_MAIL = settings->value(QString("%1/MODULE_HI_SPEED_PHASE_A_ERROR_MAIL").arg(EMAIL_PATH), "").toBool();
        Email_Param->MODULE_HI_SPEED_PHASE_B_ERROR_MAIL = settings->value(QString("%1/MODULE_HI_SPEED_PHASE_B_ERROR_MAIL").arg(EMAIL_PATH), "").toBool();
        Email_Param->MODULE_HI_SPEED_PHASE_C_ERROR_MAIL = settings->value(QString("%1/MODULE_HI_SPEED_PHASE_C_ERROR_MAIL").arg(EMAIL_PATH), "").toBool();
        Email_Param->INTERNAL_PHASE_A_ERROR_MAIL = settings->value(QString("%1/INTERNAL_PHASE_A_ERROR_MAIL").arg(EMAIL_PATH), "").toBool();
        Email_Param->INTERNAL_PHASE_B_ERROR_MAIL = settings->value(QString("%1/INTERNAL_PHASE_B_ERROR_MAIL").arg(EMAIL_PATH), "").toBool();
        Email_Param->INTERNAL_PHASE_C_ERROR_MAIL = settings->value(QString("%1/INTERNAL_PHASE_C_ERROR_MAIL").arg(EMAIL_PATH), "").toBool();
        Email_Param->GPS_MODULE_FAIL_MAIL = settings->value(QString("%1/GPS_MODULE_FAIL_MAIL").arg(EMAIL_PATH), "").toBool();
        Email_Param->SYSTEM_INITIAL_MAIL = settings->value(QString("%1/SYSTEM_INITIAL_MAIL").arg(EMAIL_PATH), "").toBool();
        Email_Param->COMMUNICATION_ERROR_MAIL = settings->value(QString("%1/COMMUNICATION_ERROR_MAIL").arg(EMAIL_PATH), "").toBool();
        Email_Param->RELAY_START_EVENT_MAIL = settings->value(QString("%1/RELAY_START_EVENT_MAIL").arg(EMAIL_PATH), "").toBool();
        Email_Param->SURGE_START_EVENT_MAIL = settings->value(QString("%1/SURGE_START_EVENT_MAIL").arg(EMAIL_PATH), "").toBool();
        Email_Param->PERIODIC_TEST_EVENT_MAIL = settings->value(QString("%1/PERIODIC_TEST_EVENT_MAIL").arg(EMAIL_PATH), "").toBool();
        Email_Param->MANUAL_TEST_EVENT_MAIL = settings->value(QString("%1/MANUAL_TEST_EVENT_MAIL").arg(EMAIL_PATH), "").toBool();
        Email_Param->LFL_FAIL_MAIL = settings->value(QString("%1/LFL_FAIL_MAIL").arg(EMAIL_PATH), "").toBool();
        Email_Param->LFL_OPERATE_MAIL = settings->value(QString("%1/LFL_OPERATE_MAIL").arg(EMAIL_PATH), "").toBool();
        Email_Param->DELAY_EVENT_MAIL = settings->value(QString("%1/DELAY_EVENT_MAIL").arg(EMAIL_PATH), "").toInt();
        Email_Param->DELAY_ALARM_MAIL = settings->value(QString("%1/DELAY_ALARM_MAIL").arg(EMAIL_PATH), "").toInt();

        //        masterLFL = settings->value(QString("%1/USER").arg("USER"),"").toString();
        //        masterIP = settings->value(QString("%1/IP_MASTER").arg("USER"),"").toString();
        //        slaveIP = settings->value(QString("%1/IP_SLAVE").arg("USER"),"").toString();

        FTP_Param_->FTP_IP = settings->value(QString("%1/FTP_SERVER").arg(FTP_PATH), "").toString();
        FTP_Param_->USERNAME = settings->value(QString("%1/USERNAME").arg(FTP_PATH), "").toString();
        FTP_Param_->PASSWORD = settings->value(QString("%1/PASSWORD").arg(FTP_PATH), "").toString();
        FTP_Param_->PERIODIC_FILE = settings->value(QString("%1/PATH_PERIODIC_FILE").arg(FTP_PATH), "").toString();
        FTP_Param_->RELAY_FILE = settings->value(QString("%1/PATH_RELAY_FILE").arg(FTP_PATH), "").toString();
        FTP_Param_->SURGE_FILE = settings->value(QString("%1/PATH_SURGE_FILE").arg(FTP_PATH), "").toString();
        FTP_Param_->MANUAL_FILE = settings->value(QString("%1/PATH_MANUAL_FILE").arg(FTP_PATH), "").toString();
        FTP_Param_->PATTERN_FILE = settings->value(QString("%1/PATH_PATTERN_FILE").arg(FTP_PATH), "").toString();

        qDebug() << "[FTP-CONFIG]"
                 << "host=" << FTP_Param_->FTP_IP
                 << "usernamePresent=" << !FTP_Param_->USERNAME.trimmed().isEmpty()
                 << "PeriodicRoot=" << FTP_Param_->PERIODIC_FILE
                 << "RelayRoot=" << FTP_Param_->RELAY_FILE
                 << "SurgeRoot=" << FTP_Param_->SURGE_FILE
                 << "ManualRoot=" << FTP_Param_->MANUAL_FILE
                 << "PatternRoot=" << FTP_Param_->PATTERN_FILE;

        networks->ip_timeserver = settings->value(QString("%1/IP_ADDRESS").arg(TIME_SERVER), "0.0.0.0").toString();
        networks->location_snmp = settings->value(QString("%1/LOCATION").arg(TIME_SERVER), "").toString();

        language = settings->value(QString("%1/LANGUAGE").arg("PROGRAM"), 0).toInt();

        version->FPGA_version = settings->value(QString("%1/FPGA").arg("DEVICE"), "").toString();
        version->Monitor_version = settings->value(QString("%1/Monitor").arg("DEVICE"), "").toString();
    } else {
        qDebug() << "Loading configuration from:" << cfgfile << " FILE NOT FOUND!";
    }
    qDebug() << "Loading configuration completed" << " language:" << language;
    networks->printinfo();
    *networksTemp = *networks;
    networks->printinfo();
    *Email_ParamTemp = *Email_Param;
    FTP_Param_->printinfo();
    //    *versionInterLock = *version;
    delete settings;
}

void PLCServer::selectMasterMode(QString user, QString ipMaster, QString ipSlave) {
    // master slave
    if(user == "STANDALINE"){
       standAlone = true;
    }
    masterLFL = user;
    masterIP = ipMaster;
    slaveIP = ipSlave;
    // qWarning() << "masterLFL::" << masterLFL << " masterIP::" <<  masterIP << " slaveIP::" << slaveIP;
}

void PLCServer::updateFTPServer() {
    qDebug() << "updateFTPServer";

    const QString cfgfile = FILESETTING;               // e.g. "/etc/egatserver/settings.ini"
    QFileInfo finfo(cfgfile);

    // 1) ต้องเป็น absolute path
    if (!finfo.isAbsolute()) {
        qWarning() << "QSettings: FILESETTING is not an absolute path:" << cfgfile;
        return;
    }

    // 2) ให้แน่ใจว่าโฟลเดอร์ปลายทางมีอยู่ และ writable
    QDir dir = finfo.dir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qWarning() << "Cannot create directory for settings:" << dir.absolutePath();
            return;
        }
    }
    // หมายเหตุ: เขียนไฟล์ต้องมีสิทธิ์เขียนทั้ง "ไฟล์" (ถ้ามีอยู่แล้ว) และ "โฟลเดอร์"
    if (finfo.exists() && !finfo.isWritable()) {
        qWarning() << "Settings file is not writable:" << cfgfile;
        return;
    }
    QFileInfo dirInfo(dir.absolutePath());
    if (!dirInfo.isWritable()) {
        qWarning() << "Settings directory is not writable:" << dir.absolutePath();
        return;
    }

    // 3) เตรียม group ให้สะอาด (ไม่มี / นำหน้า/ปิดท้าย)
    QString group = FTP_PATH;           // เช่น "FTP" หรือ "SYSTEM/FTP"
    if (group.startsWith('/')) group.remove(0, 1);
    if (group.endsWith('/'))  group.chop(1);

    // 4) เขียนค่าด้วย QSettings (ใช้บนสแตก ไม่ต้อง new/delete)
    QSettings settings(cfgfile, QSettings::IniFormat);
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    settings.setIniCodec("UTF-8");
#endif

    qDebug() << "updateFTPServer Loading configuration from:" << cfgfile
             << " -> actual file:" << settings.fileName();

    FTP_Param_->printinfo();

    settings.beginGroup(group);
    settings.setValue("FTP_SERVER",        FTP_Param_->FTP_IP);
    settings.setValue("USERNAME",          FTP_Param_->USERNAME);
    settings.setValue("PASSWORD",          FTP_Param_->PASSWORD);
    settings.setValue("PATH_PERIODIC_FILE",FTP_Param_->PERIODIC_FILE);
    settings.setValue("PATH_RELAY_FILE",   FTP_Param_->RELAY_FILE);
    settings.setValue("PATH_SURGE_FILE",   FTP_Param_->SURGE_FILE);
    settings.setValue("PATH_MANUAL_FILE",  FTP_Param_->MANUAL_FILE);
    settings.setValue("PATH_PATTERN_FILE", FTP_Param_->PATTERN_FILE);
    settings.endGroup();

    settings.sync();  // 5) บังคับ flush ลงดิสก์ทันที

    if (settings.status() != QSettings::NoError) {
        qWarning() << "QSettings sync error:" << settings.status();
        return;
    }

    qDebug() << "Loading configuration completed";
    FTP_Param_->printinfo();
}

void PLCServer::updateEmailParam() {
    qDebug() << "updateEmailParam";
    Email_Param->printinfo();
    QSettings* settings;
    const QString cfgfile = FILESETTING;
    qDebug() << "Loading configuration from:" << cfgfile;
    if (QDir::isAbsolutePath(cfgfile)) {
        settings = new QSettings(cfgfile, QSettings::IniFormat);
        settings->setValue(QString("%1/PLC_DO_ERROR_MAIL").arg(EMAIL_PATH), Email_Param->PLC_DO_ERROR_MAIL);
        settings->setValue(QString("%1/PLC_DI_ERROR_MAIL").arg(EMAIL_PATH), Email_Param->PLC_DI_ERROR_MAIL);
        settings->setValue(QString("%1/MODULE_HI_SPEED_PHASE_A_ERROR_MAIL").arg(EMAIL_PATH), Email_Param->MODULE_HI_SPEED_PHASE_A_ERROR_MAIL);
        settings->setValue(QString("%1/MODULE_HI_SPEED_PHASE_B_ERROR_MAIL").arg(EMAIL_PATH), Email_Param->MODULE_HI_SPEED_PHASE_B_ERROR_MAIL);
        settings->setValue(QString("%1/MODULE_HI_SPEED_PHASE_C_ERROR_MAIL").arg(EMAIL_PATH), Email_Param->MODULE_HI_SPEED_PHASE_C_ERROR_MAIL);
        settings->setValue(QString("%1/INTERNAL_PHASE_A_ERROR_MAIL").arg(EMAIL_PATH), Email_Param->INTERNAL_PHASE_A_ERROR_MAIL);
        settings->setValue(QString("%1/INTERNAL_PHASE_B_ERROR_MAIL").arg(EMAIL_PATH), Email_Param->INTERNAL_PHASE_B_ERROR_MAIL);
        settings->setValue(QString("%1/INTERNAL_PHASE_C_ERROR_MAIL").arg(EMAIL_PATH), Email_Param->INTERNAL_PHASE_C_ERROR_MAIL);
        settings->setValue(QString("%1/GPS_MODULE_FAIL_MAIL").arg(EMAIL_PATH), Email_Param->GPS_MODULE_FAIL_MAIL);
        settings->setValue(QString("%1/SYSTEM_INITIAL_MAIL").arg(EMAIL_PATH), Email_Param->SYSTEM_INITIAL_MAIL);
        settings->setValue(QString("%1/COMMUNICATION_ERROR_MAIL").arg(EMAIL_PATH), Email_Param->COMMUNICATION_ERROR_MAIL);
        settings->setValue(QString("%1/RELAY_START_EVENT_MAIL").arg(EMAIL_PATH), Email_Param->RELAY_START_EVENT_MAIL);
        settings->setValue(QString("%1/SURGE_START_EVENT_MAIL").arg(EMAIL_PATH), Email_Param->SURGE_START_EVENT_MAIL);
        settings->setValue(QString("%1/PERIODIC_TEST_EVENT_MAIL").arg(EMAIL_PATH), Email_Param->PERIODIC_TEST_EVENT_MAIL);
        settings->setValue(QString("%1/MANUAL_TEST_EVENT_MAIL").arg(EMAIL_PATH), Email_Param->MANUAL_TEST_EVENT_MAIL);
        settings->setValue(QString("%1/LFL_FAIL_MAIL").arg(EMAIL_PATH), Email_Param->LFL_FAIL_MAIL);
        settings->setValue(QString("%1/LFL_OPERATE_MAIL").arg(EMAIL_PATH), Email_Param->LFL_OPERATE_MAIL);
        //        settings->setValue(QString("%1/DELAY_EVENT_MAIL").arg(EMAIL_PATH),Email_Param->DELAY_EVENT_MAIL);
        //        settings->setValue(QString("%1/DELAY_ALARM_MAIL").arg(EMAIL_PATH),Email_Param->DELAY_ALARM_MAIL);
    } else {
        qDebug() << "Loading configuration from:" << cfgfile << " FILE NOT FOUND!";
    }
    qDebug() << "Loading configuration completed";
    delete settings;
    *Email_ParamTemp = *Email_Param;
    sendEmailParamToWeb();
    sendEmailParamToSNMP();
}

void PLCServer::updateEmailDelay() {
    qDebug() << "updateEmailParam";
    Email_Param->printinfo();
    QSettings* settings;
    const QString cfgfile = FILESETTING;
    qDebug() << "Loading configuration from:" << cfgfile;
    if (QDir::isAbsolutePath(cfgfile)) {
        settings = new QSettings(cfgfile, QSettings::IniFormat);
        settings->setValue(QString("%1/DELAY_EVENT_MAIL").arg(EMAIL_PATH), Email_Param->DELAY_EVENT_MAIL);
        settings->setValue(QString("%1/DELAY_ALARM_MAIL").arg(EMAIL_PATH), Email_Param->DELAY_ALARM_MAIL);
    } else {
        qDebug() << "Loading configuration from:" << cfgfile << " FILE NOT FOUND!";
    }
    qDebug() << "Loading configuration completed";
    *Email_ParamTemp = *Email_Param;
    sendEmailParamToWeb();
    sendEmailParamToSNMP();
    delete settings;
}

void PLCServer::updateNetwork() {
    qDebug() << "updateNetwork";
    QSettings* settings;
    const QString cfgfile = FILESETTING;
    qDebug() << "Loading configuration from:" << cfgfile;
    if (QDir::isAbsolutePath(cfgfile)) {
        settings = new QSettings(cfgfile, QSettings::IniFormat);
        settings->setValue(QString("%1/DHCP").arg(NETWORK_SERVER), networks->dhcpmethod);
        settings->setValue(QString("%1/IP_ADDRESS").arg(NETWORK_SERVER), networks->ip_address);
        settings->setValue(QString("%1/IP_GATEWAY").arg(NETWORK_SERVER), networks->ip_gateway);
        settings->setValue(QString("%1/NETMASK").arg(NETWORK_SERVER), networks->subnet);
        settings->setValue(QString("%1/PRIDNS").arg(NETWORK_SERVER), networks->pridns);
        settings->setValue(QString("%1/SECDNS").arg(NETWORK_SERVER), networks->secdns);
        settings->setValue(QString("%1/PHYNAME").arg(NETWORK_SERVER), networks->phyName);

        //        settings->setValue(QString("%1/IP_MASTER").arg("USER"),masterIP);
        //        settings->setValue(QString("%1/IP_SLAVE").arg("USER"),slaveIP);

        //        settings->setValue(QString("%1/IP_ADDRESS").arg(TIME_SERVER),networks->ip_timeserver);
    } else {
        qDebug() << "Loading configuration from:" << cfgfile << " FILE NOT FOUND!";
    }
    qDebug() << "Loading configuration completed";
    delete settings;
}

void PLCServer::updateSNMP() {
    qDebug() << "updateSNMP";
    QSettings* settings;
    const QString cfgfile = FILESETTING;
    qDebug() << "Loading configuration from:" << cfgfile;
    if (QDir::isAbsolutePath(cfgfile)) {
        settings = new QSettings(cfgfile, QSettings::IniFormat);
        settings->setValue(QString("%1/IP_ADDRESS").arg(SNMP_SERVER), networks->ip_snmp);
        //        settings->setValue(QString("%1/IP_ADDRESS").arg(TIME_SERVER),networks->ip_timeserver);
    } else {
        qDebug() << "Loading configuration from:" << cfgfile << " FILE NOT FOUND!";
    }
    qDebug() << "Loading configuration completed";
    delete settings;
}

void PLCServer::updateSMTP() {
    qDebug() << "updateSMTP";
    QSettings* settings;
    const QString cfgfile = FILESETTING;
    qDebug() << "Loading configuration from:" << cfgfile;
    if (QDir::isAbsolutePath(cfgfile)) {
        settings = new QSettings(cfgfile, QSettings::IniFormat);
        settings->setValue(QString("%1/SENDER_EMAIL").arg(SMTP_SERVER), Email_Config->senderEmail);
        settings->setValue(QString("%1/SENDER_NAME").arg(SMTP_SERVER), Email_Config->senderName);
        settings->setValue(QString("%1/PASSWORD").arg(SMTP_SERVER), Email_Config->password);
        settings->setValue(QString("%1/RECIPIENT_EMAIL").arg(SMTP_SERVER), Email_Config->recipientEmail);
        settings->setValue(QString("%1/RECIPIENT_NAME").arg(SMTP_SERVER), Email_Config->recipientName);
        settings->setValue(QString("%1/SERVER").arg(SMTP_SERVER), Email_Config->smtpServer);
        settings->setValue(QString("%1/PORT").arg(SMTP_SERVER), Email_Config->smtpPort);
        //        settings->setValue(QString("%1/IP_ADDRESS").arg(TIME_SERVER),networks->ip_timeserver);
    } else {
        qDebug() << "Loading configuration from:" << cfgfile << " FILE NOT FOUND!";
    }
    qDebug() << "Loading configuration completed";
    delete settings;
}

void PLCServer::updateNTP() {
    qDebug() << "updateSMTP";
    QSettings* settings;
    const QString cfgfile = FILESETTING;
    qDebug() << "Loading configuration from:" << cfgfile;
    if (QDir::isAbsolutePath(cfgfile)) {
        settings = new QSettings(cfgfile, QSettings::IniFormat);
        settings->setValue(QString("%1/IP_ADDRESS").arg(TIME_SERVER), networks->ip_timeserver);
    } else {
        qDebug() << "Loading configuration from:" << cfgfile << " FILE NOT FOUND!";
    }
    qDebug() << "Loading configuration completed";
    delete settings;
}

void PLCServer::updateLocation() {
    qDebug() << "updateLocation";
    QSettings* settings;
    const QString cfgfile = FILESETTING;
    qDebug() << "Loading configuration from:" << cfgfile;
    if (QDir::isAbsolutePath(cfgfile)) {
        settings = new QSettings(cfgfile, QSettings::IniFormat);
        settings->setValue(QString("%1/LOCATION").arg(TIME_SERVER), networks->location_snmp);
    } else {
        qDebug() << "Loading configuration from:" << cfgfile << " FILE NOT FOUND!";
    }
    qDebug() << "Loading configuration completed";
    delete settings;
}

void PLCServer::updateNetwork(quint8 DHCP, QString LocalAddress, QString Netmask, QString Gateway, QString DNS1, QString DNS2, QString phyNetworkName) {
    qDebug() << "LocalAddress:" << LocalAddress << " Gateway:" << Gateway;
    QString strDhcpMethod = "off";
    if (DHCP) strDhcpMethod = "on";

    if (DHCP) {
        networking->setDHCPIpAddr3(phyNetworkName);
    } else {
        qDebug() << "setStaticIpAddr3";
        networking->setStaticIpAddr3(LocalAddress, Netmask, Gateway, DNS1, DNS2, phyNetworkName);
    }

    //    if(phyNetworkName == "eth0")
    //    {
    //        networks->dhcpmethod = QString(DHCP);
    //        networks->ip_address = LocalAddress;
    //        networks->subnet = Netmask;
    //        networks->ip_gateway = Gateway;
    //        networks->pridns = DNS1;
    //        networks->secdns = DNS2;
    //    }
    quint8 dhcpmethodInt;
    if (networks->dhcpmethod.contains("on"))
        dhcpmethodInt = 1;
    else
        dhcpmethodInt = 0;
    //    writeXMLConfigFile();
    updateNetwork();
}

QString PLCServer::getUPTime() {
    QProcess process;
    process.start(QStringLiteral("uptime"), QStringList{QStringLiteral("-p")});
    if (!process.waitForFinished(3000)) {
        process.kill();
        process.waitForFinished(1000);
        qWarning() << "[getUPTime] uptime command timeout";
        return QString();
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        qWarning() << "[getUPTime] uptime command failed"
                   << process.exitCode()
                   << QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
        return QString();
    }

    return QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed();
}

QString PLCServer::readLine(QString fileName) {
    QFile inputFile(fileName);
    inputFile.open(QIODevice::ReadOnly);
    if (!inputFile.isOpen()) return "";

    QTextStream stream(&inputFile);
    QString line = stream.readLine();
    inputFile.close();
    //    qDebug() << line;
    return line.trimmed();
}

void PLCServer::sendInitial(QWebSocket* client) {
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
    if (client->state() == QAbstractSocket::ConnectedState)
        emit sendMessage(raw_data, client);
    else
        qDebug() << "snmp_address:" << client->state();
    qDebug() << raw_data;
    QJsonObject().swap(Param);

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
    raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
    if (client->state() == QAbstractSocket::ConnectedState)
        emit sendMessage(raw_data, client);
    else
        qDebug() << "snmp_address:" << client->state();
    //    qDebug() << raw_data;
}

void PLCServer::updateFirmware() {
    foundfileupdate = true;
    const QStringList fileupdate = findFile();
    const QString updateDir = QStringLiteral("/tmp/update");

    if (!ensureDirExists(updateDir)) {
        qWarning() << "[updateFirmware] cannot create" << updateDir;
        foundfileupdate = false;
        return;
    }

    qDebug() << "fileupdate.size()" << fileupdate.size();
    if (fileupdate.size() > 0) {
        const QFileInfo srcInfo(QDir::cleanPath(fileupdate.at(0)));
        if (!srcInfo.exists() || !srcInfo.isFile() || srcInfo.isSymLink()) {
            qWarning() << "[updateFirmware] invalid update package:" << srcInfo.absoluteFilePath();
            foundfileupdate = false;
            return;
        }

        qDebug() << "Start update";
        updateStatus = 1;
        QString sendMessage = QString("{\"menuID\":\"update\", \"updateStatus\":%1}").arg(updateStatus);
        Q_UNUSED(sendMessage);

        const QString updateTar = QDir(updateDir).absoluteFilePath(QStringLiteral("update.tar"));
        if (!safeCopyFileReplace(srcInfo.absoluteFilePath(), updateTar)) {
            foundfileupdate = false;
            return;
        }

        if (!runProcessChecked(QStringLiteral("tar"),
                               QStringList{QStringLiteral("-xf"), updateTar,
                                           QStringLiteral("-C"), updateDir},
                               120000)) {
            foundfileupdate = false;
            return;
        }

        const QString updateScript = QDir(updateDir).absoluteFilePath(QStringLiteral("update.sh"));
        const QFileInfo scriptInfo(updateScript);
        if (!scriptInfo.exists() || !scriptInfo.isFile() || scriptInfo.isSymLink()) {
            qWarning() << "[updateFirmware] update.sh missing or unsafe:" << updateScript;
            foundfileupdate = false;
            return;
        }

        if (!runProcessChecked(QStringLiteral("sh"), QStringList{updateScript}, 180000)) {
            foundfileupdate = false;
            return;
        }

        updateStatus = 2;
        sendMessage = QString("{\"menuID\":\"update\", \"updateStatus\":%1}").arg(updateStatus);
        Q_UNUSED(sendMessage);
        qDebug() << "Update complete";
        getPATHReadme();
        exit(0);
    }
    foundfileupdate = false;
}

void PLCServer::getPATHReadme() {
    QString txt;
    QDir dir(QStringLiteral("/tmp/update/"));

    if (dir.exists()) {
        qDebug() << "getPATHReadme directory exists!";

        const QString monitorDir = QStringLiteral("/var/www/html/monitor_update");
        const QString fpgaDir = QStringLiteral("/var/www/html/fpga_update");
        ensureDirExists(monitorDir);
        ensureDirExists(fpgaDir);

        const QFileInfoList fileList = dir.entryInfoList(QDir::Files | QDir::NoSymLinks, QDir::Name);
        if (fileList.isEmpty()) {
            qDebug() << "No files found in the directory.";
        } else {
            qDebug() << "Files in the directory:";
            for (const QFileInfo& fileInfo : fileList) {
                const QString file = safeFileNameOnly(fileInfo.fileName());
                const QString srcPath = fileInfo.absoluteFilePath();

                if (file.endsWith(".txt", Qt::CaseInsensitive) ||
                    file.endsWith(".md", Qt::CaseInsensitive)) {
                    qDebug() << srcPath;
                    txt = srcPath;
                    safeCopyFileReplace(srcPath, QDir(fpgaDir).absoluteFilePath(file));
                    safeCopyFileReplace(srcPath, QDir(monitorDir).absoluteFilePath(file));
                }

                if (file.endsWith(".bin", Qt::CaseInsensitive) ||
                    file.endsWith(".tar", Qt::CaseInsensitive) ||
                    file.endsWith(".tar.xz", Qt::CaseInsensitive)) {
                    if (file.contains("monitor", Qt::CaseInsensitive)) {
                        qDebug() << "Monitor-related file found:" << file << "full path" << srcPath;
                        safeMoveFileReplace(srcPath, QDir(monitorDir).absoluteFilePath(file));
                    }

                    if (file.contains("sigsense", Qt::CaseInsensitive)) {
                        qDebug() << "SigSense-related file found:" << file << "full path" << srcPath;
                        safeMoveFileReplace(srcPath, QDir(fpgaDir).absoluteFilePath(file));
                    }
                }
            }
        }
    } else {
        qDebug() << "getPATHReadme directory does not exist....";
    }

    qDebug() << "getSetting";
    QSettings* settings = nullptr;
    const QString cfgfile = txt;
    qDebug() << "Loading configuration from:" << cfgfile;
    if (QDir::isAbsolutePath(cfgfile) && QFileInfo::exists(cfgfile)) {
        qDebug() << "isAbsolutePath";
        settings = new QSettings(cfgfile, QSettings::IniFormat);
        version->FPGA_version = settings->value(QString("SigSense"), "").toString();
        version->Monitor_version = settings->value(QString("iScreen"), "").toString();
        delete settings;
        settings = nullptr;
    } else {
        qDebug() << "Loading configuration from:" << cfgfile << "FILE NOT FOUND!";
    }
    qDebug() << "Loading configuration completed";

    const QString cfgfile2 = FILESETTING;
    qDebug() << "Loading configuration from:" << cfgfile2;
    if (QDir::isAbsolutePath(cfgfile2)) {
        qDebug() << "isAbsolutePath";
        settings = new QSettings(cfgfile2, QSettings::IniFormat);
        settings->setValue(QString("%1/FPGA").arg("DEVICE"), version->FPGA_version);
        settings->setValue(QString("%1/Monitor").arg("DEVICE"), version->Monitor_version);
        delete settings;
        settings = nullptr;
    } else {
        qDebug() << "Loading configuration from:" << cfgfile2 << "FILE NOT FOUND!";
    }
    qDebug() << "Loading configuration completed";
}

QStringList PLCServer::findFile() {
    QStringList listfilename;
    QString ss = "/var/www/html/uploads/";
    const char* sss;
    sss = ss.toStdString().c_str();
    QDir dir1("/var/www/html/uploads/");
    QString filepath;
    QString filename;
    QFileInfoList fi1List(dir1.entryInfoList(QDir::Files, QDir::Name));
    foreach (const QFileInfo& fi1, fi1List) {
        filepath = QString::fromUtf8(fi1.absoluteFilePath().toLocal8Bit());
        filename = QString::fromUtf8(fi1.fileName().toLocal8Bit());
        listfilename << filepath;
        qDebug() << filepath;  // << filepath.toUtf8().toHex();
    }
    return listfilename;
}

void PLCServer::scanFileUpdate() {
    QStringList fileupdate;
    fileupdate = findFile();
    if (fileupdate.size() > 0) {
        if (foundfileupdate == false) updateFirmware();
    }
}

void PLCServer::calculate(QString msg) {
    qDebug() << "calculate:" << msg;
    QJsonDocument d = QJsonDocument::fromJson(msg.toUtf8());
    QJsonObject command = d.object();
    QString getCommand = command.value("objectName").toString();

    // เก็บค่าก่อนหน้า
    double prevSagFactor = sagFactor;
    double prevSamplingRate = samplingRate;
    double prevDistanceToStart = distanceToStart;
    double prevDistanceToShow = distanceToShow;
    double prevFulldistance = fulldistance;
    double prevThresholdA = thresholdA;
    double prevThresholdB = thresholdB;
    double prevThresholdC = thresholdC;

    // อัปเดตค่าจาก JSON ถ้ามี
    if (command.contains("sagFactorInit")) sagFactor = command.value("sagFactorInit").toDouble();
    if (command.contains("samplingRateInit")) samplingRate = command.value("samplingRateInit").toDouble();
    if (command.contains("distanceToStartInit")) distanceToStart = command.value("distanceToStartInit").toDouble();
    if (command.contains("distanceToShowInit")) distanceToShow = command.value("distanceToShowInit").toDouble();
    if (command.contains("fulldistancesInit")) fulldistance = command.value("fulldistancesInit").toDouble();
    if (command.contains("thresholdInitA")) thresholdA = command.value("thresholdInitA").toDouble();
    if (command.contains("thresholdInitB")) thresholdB = command.value("thresholdInitB").toDouble();
    if (command.contains("thresholdInitC")) thresholdC = command.value("thresholdInitC").toDouble();

    // **ตรวจสอบค่าที่เปลี่ยนแปลง**
    bool isThresholdAChanged = (prevThresholdA != thresholdA);
    bool isThresholdBChanged = (prevThresholdB != thresholdB);
    bool isThresholdCChanged = (prevThresholdC != thresholdC);
    bool isSagFactorChanged = (prevSagFactor != sagFactor);
    bool isSamplingRateChanged = (prevSamplingRate != samplingRate);
    bool isDistanceToStartChanged = (prevDistanceToStart != distanceToStart);
    bool isDistanceToShowChanged = (prevDistanceToShow != distanceToShow);
    bool isFulldistanceChanged = (prevFulldistance != fulldistance);

    bool isAnyThresholdChanged = isThresholdAChanged || isThresholdBChanged || isThresholdCChanged;
    bool isAnyParameterChanged = isSagFactorChanged || isSamplingRateChanged || isDistanceToStartChanged || isDistanceToShowChanged || isFulldistanceChanged;

    // **ตรวจสอบค่าผิดปกติ**
    auto isValidValue = [](double val) { return !std::isnan(val) && !std::isinf(val) && val >= 0.0 && val <= 100000.0; };

    bool isValid = isValidValue(sagFactor) && isValidValue(samplingRate) && isValidValue(distanceToStart) && isValidValue(distanceToShow) && isValidValue(fulldistance) && isValidValue(thresholdA) && isValidValue(thresholdB) && isValidValue(thresholdC);

    // **แสดงผลลัพธ์ก่อนตัดสินใจ**
    qDebug() << "Current Params:" << sagFactor << samplingRate << distanceToStart << distanceToShow << fulldistance << thresholdA << thresholdB << thresholdC << "Valid:" << isValid << "Changed:" << (isAnyThresholdChanged || isAnyParameterChanged);

    // **เงื่อนไขการทำงาน**
    if (isValid) {
        qDebug() << " Parameters valid. Proceeding with calculations.";

        if (thresholdA > 0) {
            qDebug() << "Triggering plotGraphA()";
            plotGraphA(sagFactor, samplingRate, distanceToStart, distanceToShow, fulldistance, thresholdA);
            //            plotPatternA(sagFactor, samplingRate, distanceToStart, distanceToShow, fulldistance, thresholdA);
        }
        if (thresholdB > 0) {
            qDebug() << "Triggering plotGraphB()";
            plotGraphB(sagFactor, samplingRate, distanceToStart, distanceToShow, fulldistance, thresholdB);
            //            plotPatternB(sagFactor, samplingRate, distanceToStart, distanceToShow, fulldistance, thresholdB);
        }
        if (thresholdC > 0) {
            qDebug() << "Triggering plotGraphC()";
            plotGraphC(sagFactor, samplingRate, distanceToStart, distanceToShow, fulldistance, thresholdC);
            //            plotPatternC(sagFactor, samplingRate, distanceToStart, distanceToShow, fulldistance, thresholdC);
        }

    } else {
        qDebug() << "Parameters invalid or unchanged. No further action taken.";
    }
}

void PLCServer::plotGraphA(double sagFactorInit, double samplingRateInit, double distanceToStartInit, double distanceToShowInit, double fulldistance, double thresholdInitA) {
    qDebug() << "Debug plotGraphA:" << sagFactorInit << samplingRateInit << distanceToStartInit << distanceToShowInit << fulldistance << thresholdInitA;

    QString filePath = "/home/pi/Rawdata/data0.raw";
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Unable to open file:" << filePath;
        return;
    }

    QByteArray data = file.readAll();
    file.close();
    std::vector<float> normalizedValues;
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(data.constData());
    const uint8_t* endPtr = ptr + data.size();

    while (ptr + 2 <= endPtr) {
        uint16_t signedValue = static_cast<uint16_t>((static_cast<uint8_t>(ptr[1]) << 8) | static_cast<uint8_t>(ptr[0]));
        if (signedValue > 65000) {
            signedValue = 0.0f;
        }
               // qDebug() << "signedValueA:" << signedValue;

        float normalizedValue = (static_cast<float>(signedValue) / 65536.0f) * 4095;
        normalizedValues.push_back(normalizedValue);
        ptr += 2;
    }
    qDebug() << "ThresholdA Data.";

    qDebug() << "Total samples read: " << normalizedValues.size();

    const float threshold = thresholdInitA;  // Convert to mV
    qDebug() << "ThresholdA Data debug:" << threshold;
    auto startIt = std::find_if(normalizedValues.begin(), normalizedValues.end(), [threshold](float val) { return val >= threshold; });

    if (startIt == normalizedValues.end()) {
        qDebug() << "Threshold value not found in data. << " << normalizedValues;
        return;
    }
    int startIndex = std::distance(normalizedValues.begin(), startIt);
    //    qDebug() << "Starting index found at position:" << startIndex;

    int resampling = samplingRateInit / (60 * sagFactorInit);  // Resampling rate
    //    qDebug() << "Resampling rate:" << resampling;
    double fulldistance_del = fulldistance * 1000 * 60 * 0.983;
    double destination = distanceToShowInit * 1000 * 60 * 0.983;
    double numOfPoint = destination * 60 * 0.983;
    double err_Distance = (abs(numOfPoint - fulldistance_del));
    //    double distance_del = (err_Distance-fulldistance_del)/10;
    //    double reCalDisance = abs(fulldistance_del - distance_del);
    //    qDebug() << "reCalDisance:" << reCalDisance << err_Distance << fulldistance << distanceToShowInit << distance_del << fulldistance_del;

    float totalDistance = ((distanceToShowInit+10) - distanceToStartInit) * 2000;
    int fullpoint = static_cast<int>(totalDistance / (60 * sagFactorInit));
    int trueDistancepoint = (fullpoint / resampling);  // Points after resampling
    int paddedPoints = static_cast<int>(trueDistancepoint * 1.05);
    if (paddedPoints <= trueDistancepoint) ++paddedPoints;
    qDebug() << "ThresholdA Data1/2:" << totalDistance << fullpoint << trueDistancepoint;
    timeOfDistance = fullpoint * timepoint;  // s
    realDistanceA = (speedOfligth * (timeOfDistance / 2)) * sagFactorInit;
    qDebug() << "realDistance:" << realDistanceA << timeOfDistance << fullpoint << timepoint << speedOfligth;
    QJsonObject distanceObject;
    distanceObject.insert("objectName", "realDistanceA");
    distanceObject.insert("valueDistanceA", realDistanceA);
    QJsonDocument distanceDoc(distanceObject);
    QString distanceData = distanceDoc.toJson(QJsonDocument::Compact);
    emit sendToMonitor(distanceData);
    emit sendToVNC(distanceData);
    qDebug() << "Full points:" << fullpoint << " distanceToShow:" << distanceToShow << " normalizedValues size:" << normalizedValues.size();
    //    qDebug() << "True distance points after resampling:" << trueDistancepoint;

    float pointInterval = (totalDistance) / paddedPoints;
    qDebug() << "Point interval (m):" << pointInterval;
    std::vector<std::pair<float, float>> result;
    float currentDistance = 0;
    //    float currentDistance = distanceToStartInit * 1000;

    int i = 0;
    while (1) {
        //        qDebug() << "count:" << i<< " currentDistance:" << currentDistance;
        result.emplace_back(currentDistance / 2, normalizedValues[startIndex + i]);
        currentDistance = (samplingRateInit * sagFactorInit) * i;
        i += resampling;
        if (startIndex + i >= normalizedValues.size()) {
            qDebug() << "stop at index A:" << i;
            currentDistanceA = currentDistance;
            break;
        }
        if (currentDistance > distanceToShow * 1000 * 2) {
            qDebug() << "stop at index A:" << i;
            currentDistanceA = currentDistance;
            break;
        }
    }
    //    for (int i = 0; i < paddedPoints; ++i) {
    //        float currentValue = (i * resampling + startIndex < normalizedValues.size())
    //                             ? normalizedValues[i * resampling + startIndex]
    //                             : 0.0f;
    //        result.emplace_back(currentDistance, currentValue);
    //        currentDistance += pointInterval;
    //    }
    qDebug() << "ThresholdA Data3.";
    qDebug() << "Final Total PointsA:" << result.size();
    for (const auto& [distance, voltage] : result) {
        //        qDebug() << "X:" << distance / 1000.0 << " km, Y:" << voltage << " mV";
    }
    qDebug() << "ThresholdA Data4.result.size:" << result.size();
    QJsonObject mainObject;
    QJsonArray dist, volt;
    QList<double> volts;
    QList<float> dists;
    for (const auto& [distance, voltage] : result) {
        dist.push_back(distance / 1000);
        if (interlockPattern == true) {
            dists.append(distance);
        }
        volt.push_back(voltage);
        volts.append(voltage);
    }

    mainObject.insert("objectName", "dataPlotingA");
    mainObject.insert("distance", dist);
    mainObject.insert("voltage", volt);

    QJsonDocument jsonDoc(mainObject);
    QString raw_data = jsonDoc.toJson(QJsonDocument::Compact);
    testNoneSmoothA = QString();
    testNoneSmoothA = raw_data;
    // emit sendToSocketPLC(raw_data);
    //    qDebug() << "Generated JSONA:" << raw_data;

    rawdataArrayA = raw_data;
    if (interlockPattern == true) {
        voltageListA.append(volts);
        kmListA.append(dists);
        // qDebug() << "rawdataArrayA append success" << kmListA << modeName;
//        qDebug() << "rawdataArrayA append success" << voltageListA << modeName;
    } else {
        qDebug() << "else rawdataArrayA append success";
        //        QJsonDocument jsonDocs;
        //        QJsonObject Param;
        //        Param.insert("objectName","realDistanceA");
        //        Param.insert("valueDistanceA",realDistanceA/2000);
        //        jsonDocs.setObject(Param);
        //        QString raw_datas = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        //        emit sendToMonitor(raw_datas);

        //        findClosestDistance(Distance,realDistanceA);

        //        Param.insert("objectName","TOWER_NO");
        //        Param.insert("TransmissionLine",TowerNo[indexclosestValue]);
        //        Param.insert("FullDistance",totalDistance/2000);
        //        Param.insert("phase","A");
        //        jsonDoc.setObject(Param);
        //        raw_datas = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();

               // Q_FOREACH (QWebSocket *pClient, Monitor_address)
               // {
               //     if(pClient->state() == QAbstractSocket::ConnectedState)
               //         emit sendMessage(raw_datas, pClient);
               //     else
               //         qDebug() << "Monitor_address:" << pClient->state();
               // }
        qDebug() << "patternSelected:" << patternSelected;
        if (patternSelected) {
            calFLF(dist, volt, "A");
        }
        emit sendToMonitor(testNoneSmoothA);
        emit sendToVNC(testNoneSmoothA);
        // qWarning() << "emit sendToMonitor(testNoneSmoothA);";
        // qDebug() << " testNoneSmoothA" << testNoneSmoothA;
        auditEventDataPhase(QStringLiteral("phaseA"), FileTimeStamp, dist, volt);
        myDatabase->SumNormalizationandUpdateDb(modeName, "phaseA", FileTimeStamp, dist, volt);
        // reSamplingNormalizationA(result);
    }
}

void PLCServer::plotGraphB(double sagFactorInit, double samplingRateInit, double distanceToStartInit, double distanceToShowInit, double fulldistance, double thresholdInitB) {
    qDebug() << "Debug plotGraphB:" << sagFactorInit << samplingRateInit << distanceToStartInit << distanceToShowInit << fulldistance << thresholdInitB;
    QString filePath = "/home/pi/Rawdata/data1.raw";
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Unable to open file:" << filePath;
        return;
    }
    QByteArray data = file.readAll();
    file.close();

    std::vector<float> normalizedValues;
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(data.constData());
    const uint8_t* endPtr = ptr + data.size();

    while (ptr + 2 <= endPtr) {
        uint16_t signedValue = static_cast<uint16_t>((static_cast<uint8_t>(ptr[1]) << 8) | static_cast<uint8_t>(ptr[0]));
        // qDebug() << "normalizedValues signedValue" << ((signedValue & (1 << 23)) != 0);
        if (signedValue > 65000) {
            // qDebug() << "condition signedValue" << signedValue;
            signedValue = 0.0f;
        }
                  // qDebug() << "signedValueB:" << signedValue;

        float normalizedValue = (static_cast<float>(signedValue) / 65536.0f) * 4095;  // Convert to mV
        normalizedValues.push_back(normalizedValue);
        ptr += 2;
    }

    qDebug() << "Total samples read: " << normalizedValues.size();

    const float threshold = thresholdInitB;  // Convert to mV
    auto startIt = std::find_if(normalizedValues.begin(), normalizedValues.end(), [threshold](float val) { return val >= threshold; });

    if (startIt == normalizedValues.end()) {
        qDebug() << "Threshold value not found in data.";
        return;
    }
    int startIndex = std::distance(normalizedValues.begin(), startIt);
    //    qDebug() << "Starting index found at position:" << startIndex;

    int resampling = samplingRateInit / (60 * sagFactorInit);  // Resampling rate
    //    qDebug() << "Resampling rate:" << resampling;

    float totalDistance = ((distanceToShowInit+10) - distanceToStartInit) * 2000;
    int fullpoint = static_cast<int>(totalDistance / (60 * sagFactorInit));
    int trueDistancepoint = (fullpoint / resampling);  // Points after resampling
    int paddedPoints = static_cast<int>(trueDistancepoint * 1.00);
    timeOfDistance = fullpoint * timepoint;  // s
    realDistanceB = (speedOfligth * (timeOfDistance / 2)) * sagFactorInit;
    qDebug() << "realDistance:" << realDistanceB << timeOfDistance << fullpoint << timepoint << speedOfligth;
    QJsonObject distanceObject;
    distanceObject.insert("objectName", "realDistanceB");
    distanceObject.insert("valueDistanceB", realDistanceB);
    QJsonDocument distanceDoc(distanceObject);
    QString distanceData = distanceDoc.toJson(QJsonDocument::Compact);
    emit sendToMonitor(distanceData);
    emit sendToVNC(distanceData);
    qDebug() << "Full points:" << fullpoint;
    //    qDebug() << "True distance points after resampling:" << trueDistancepoint;

    float pointInterval = (totalDistance) / paddedPoints;  //    qDebug() << "Point interval (m):" << pointInterval;

    std::vector<std::pair<float, float>> result;
    float currentDistance = distanceToStartInit * 1000;  // Start in meters

    int i = 0;
    while (1) {
        //           qDebug() << "count:" << i<< " currentDistance:" << currentDistance;
        result.emplace_back(currentDistance / 2, normalizedValues[startIndex + i]);
        currentDistance = (samplingRateInit * sagFactorInit) * i;
        i += resampling;
        if (startIndex + i >= normalizedValues.size()) {
            qDebug() << "stop at index B:" << i;
            break;
        }
        if (currentDistance > distanceToShow * 1000 * 2) {
            qDebug() << "stop at index B:" << i;
            break;
        }
    }

    //       for (int i = 0; i < paddedPoints; ++i) {
    //           float currentValue = (i * resampling + startIndex < normalizedValues.size())
    //                                ? normalizedValues[i * resampling + startIndex]
    //                                : 0.0f;
    //           result.emplace_back(currentDistance, currentValue);
    //           currentDistance += pointInterval;
    //       }

    qDebug() << "Final Total Points:" << result.size();
    for (const auto& [distance, voltage] : result) {
        //        qDebug() << "X:" << distance / 1000.0 << " km, Y:" << voltage << " mV";
    }

    // JSON Output for Plotting
    QJsonObject mainObject;
    QJsonArray dist, volt;
    QList<double> volts;
    QList<float> dists;
    for (const auto& [distance, voltage] : result) {
        dist.push_back(distance / 1000);  // Convert m to km
        if (interlockPattern == true) {
            dists.append(distance);  // Convert double to string and store as bytes
        }
        volt.push_back(voltage);  // Already multiplied by 4096
        volts.append(voltage);    // Convert double to string and store as bytes
    }

    mainObject.insert("objectName", "dataPlotingB");
    mainObject.insert("distance", dist);
    mainObject.insert("voltage", volt);

    QJsonDocument jsonDoc(mainObject);
    QString raw_data = jsonDoc.toJson(QJsonDocument::Compact);

    testNoneSmoothB = QString();
    testNoneSmoothB = raw_data;
    emit sendToSocketPLC(raw_data);
    //       qDebug() << "Generated JSONB:" << raw_data;

    rawdataArrayB = raw_data;
    if (interlockPattern == true) {
        voltageListB.append(volts);
        kmListB.append(dists);
        // qDebug() << "rawdataArrayB append success" << voltageListB;
    } else {
        qDebug() << "else rawdataArrayB append success";
        //           QJsonDocument jsonDocs;
        //           QJsonObject Param;
        //           Param.insert("objectName","realDistanceB");
        //           Param.insert("valueDistanceB",realDistanceB);
        //           jsonDocs.setObject(Param);
        //           QString raw_datas = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        //           emit sendToMonitor(raw_datas);

        //           findClosestDistance(Distance,realDistanceB);

        //           Param.insert("objectName","TOWER_NO");
        //           Param.insert("TransmissionLine",TowerNo[indexclosestValue]);
        //           Param.insert("FullDistance",totalDistance/2000);
        //           Param.insert("phase","B");
        //           jsonDoc.setObject(Param);
        //           raw_datas = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();

        //           Q_FOREACH (QWebSocket *pClient, Monitor_address)
        //           {
        //               if(pClient->state() == QAbstractSocket::ConnectedState)
        //                   emit sendMessage(raw_datas, pClient);
        //               else
        //                   qDebug() << "Monitor_address:" << pClient->state();
        //           }
        qDebug() << "patternSelected:" << patternSelected;
        if (patternSelected) {
            // qWarning() << "patternSelected:" << patternSelected << " calFLF B";
            calFLF(dist, volt, "B");
        }
        emit sendToMonitor(testNoneSmoothB);
        emit sendToVNC(testNoneSmoothB);
        // qWarning() << "emit sendToMonitor(testNoneSmoothA);";
        // qDebug() << " testNoneSmoothB" << testNoneSmoothB;
        auditEventDataPhase(QStringLiteral("phaseB"), FileTimeStamp, dist, volt);
        myDatabase->SumNormalizationandUpdateDb(modeName, "phaseB", FileTimeStamp, dist, volt);
        // reSamplingNormalizationB(result);
    }
    //    emit plotingDataPhaseB(raw_data); reSamplingNormalizationB
}

void PLCServer::plotGraphC(double sagFactorInit, double samplingRateInit, double distanceToStartInit, double distanceToShowInit, double fulldistance, double thresholdInitC) {
    qDebug() << "Debug plotGraphC:" << sagFactorInit << samplingRateInit << distanceToStartInit << distanceToShowInit << fulldistance << thresholdInitC;
    QString filePath = "/home/pi/Rawdata/data2.raw";
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Unable to open file:" << filePath;
        return;
    }
    QByteArray data = file.readAll();
    file.close();

    std::vector<float> normalizedValues;
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(data.constData());
    const uint8_t* endPtr = ptr + data.size();

    while (ptr + 2 <= endPtr) {
        uint16_t signedValue = static_cast<uint16_t>((static_cast<uint8_t>(ptr[1]) << 8) | static_cast<uint8_t>(ptr[0]));
        if (signedValue > 65000) {
            signedValue = 0.0f;
        }
        float normalizedValue = (static_cast<float>(signedValue) / 65535.0f) * 4095;  // Convert to mV
        normalizedValues.push_back(normalizedValue);

        ptr += 2;
    }

    //    qDebug() << "Total samples read: " << normalizedValues.size();

    const float threshold = thresholdInitC;  // Convert to mV
    auto startIt = std::find_if(normalizedValues.begin(), normalizedValues.end(), [threshold](float val) { return val >= threshold; });

    if (startIt == normalizedValues.end()) {
        qDebug() << "Threshold value not found in data.";
        return;
    }
    int startIndex = std::distance(normalizedValues.begin(), startIt);
    //    qDebug() << "Starting index found at position:" << startIndex;

    int resampling = samplingRateInit / (60 * sagFactorInit);
    //    qDebug() << "Resampling rate:" << resampling;

    float totalDistance = ((distanceToShowInit+10) - distanceToStartInit) * 2000;
    int fullpoint = static_cast<int>(totalDistance / (60 * sagFactorInit));
    int trueDistancepoint = (fullpoint / resampling);  // Points after resampling
    timeOfDistance = fullpoint * timepoint;                // s
    realDistanceC = (speedOfligth * (timeOfDistance / 2)) * sagFactorInit;
    qDebug() << "realDistance:" << realDistanceC << timeOfDistance << fullpoint << timepoint << speedOfligth;
    QJsonObject distanceObject;
    distanceObject.insert("objectName", "realDistanceC");
    distanceObject.insert("valueDistanceC", realDistanceC);
    QJsonDocument distanceDoc(distanceObject);
    QString distanceData = distanceDoc.toJson(QJsonDocument::Compact);
    emit sendToMonitor(distanceData);
    emit sendToVNC(distanceData);
    //    qDebug() << "Full points:" << fullpoint;
    //    qDebug() << "True distance points after resampling:" << trueDistancepoint;

    float pointInterval = (totalDistance) / trueDistancepoint;
    //    qDebug() << "Point interval (m):" << pointInterval;

    std::vector<std::pair<float, float>> result;
    float currentDistance = distanceToStartInit * 1000;

    int i = 0;
    while (1) {
        //        qDebug() << "count:" << i<< " currentDistance:" << currentDistance;
        result.emplace_back(currentDistance / 2, normalizedValues[startIndex + i]);
        currentDistance = (samplingRateInit * sagFactorInit) * i;
        i += resampling;
        if (startIndex + i >= normalizedValues.size()) {
            qDebug() << "stop at index C:" << i;
            break;
        }
        if (currentDistance > distanceToShow * 1000 * 2) {
            qDebug() << "stop at index C:" << i;
            break;
        }
    }

    //    for (int i = 0; i < trueDistancepoint; ++i) {
    //        float currentValue = (i * resampling + startIndex < normalizedValues.size())
    //                             ? normalizedValues[i * resampling + startIndex]   // Multiply by 4096 here
    //                             : 0.0f;
    //        result.emplace_back(currentDistance, currentValue);
    //        currentDistance += pointInterval;
    //    }

    qDebug() << "Final Total Points:" << result.size();
    for (const auto& [distance, voltage] : result) {
        //        qDebug() << "X:" << distance / 1000.0 << " km, Y:" << voltage << " mV";
    }

    // JSON Output for Plotting
    QJsonObject mainObject;
    QJsonArray dist, volt;
    QList<double> volts;
    QList<float> dists;
    for (const auto& [distance, voltage] : result) {
        dist.push_back(distance / 1000);  // Convert m to km
        if (interlockPattern == true) {
            dists.append(distance);  // Convert double to string and store as bytes
        }
        volt.push_back(voltage);  // Already multiplied by 4096
        volts.append(voltage);    // Convert double to string and store as bytes
    }

    mainObject.insert("objectName", "dataPlotingC");
    mainObject.insert("distance", dist);
    mainObject.insert("voltage", volt);

    QJsonDocument jsonDoc(mainObject);
    QString raw_data = jsonDoc.toJson(QJsonDocument::Compact);
    testNoneSmoothC = QString();
    testNoneSmoothC = raw_data;
    // emit sendToSocketPLC(raw_data);
    //    qDebug() << "Generated JSON C:" << raw_data;

    rawdataArrayC = raw_data;
    //    emit plotingDataPhaseB(raw_data); reSamplingNormalizationC
    if (interlockPattern == true) {
        voltageListC.append(volts);
        kmListC.append(dists);
        // qDebug() << "rawdataArrayC append success" << voltageListC;
    } else {
        qDebug() << "else rawdataArrayC append success";
        //        QJsonDocument jsonDocs;
        //        QJsonObject Param;
        //        Param.insert("objectName","realDistanceC");
        //        Param.insert("valueDistanceC",realDistanceC);
        //        jsonDocs.setObject(Param);
        //        QString raw_datas = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        //        emit sendToMonitor(raw_datas);

        //        findClosestDistance(Distance,realDistanceC);

        //        Param.insert("objectName","TOWER_NO");
        //        Param.insert("TransmissionLine",TowerNo[indexclosestValue]);
        //        Param.insert("FullDistance",totalDistance/2000);
        //        Param.insert("phase","C");
        //        jsonDoc.setObject(Param);
        //        raw_datas = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();

        //        Q_FOREACH (QWebSocket *pClient, Monitor_address)
        //        {
        //            if(pClient->state() == QAbstractSocket::ConnectedState)
        //                emit sendMessage(raw_datas, pClient);
        //            else
        //                qDebug() << "Monitor_address:" << pClient->state();
        //        }
        qDebug() << "patternSelected:" << patternSelected;
        if (patternSelected) {
            calFLF(dist, volt, "C");
        }
        emit sendToMonitor(testNoneSmoothC);
        emit sendToVNC(testNoneSmoothC);
        // qWarning() << "emit sendToMonitor(testNoneSmoothA);";
        // qDebug() << " testNoneSmoothC" << testNoneSmoothC;
        auditEventDataPhase(QStringLiteral("phaseC"), FileTimeStamp, dist, volt);
        myDatabase->SumNormalizationandUpdateDb(modeName, "phaseC", FileTimeStamp, dist, volt);
        // reSamplingNormalizationC(result);
    }
}

void PLCServer::plotPatternA(double sagFactorInit, double samplingRateInit, double distanceToStartInit, double distanceToShowInit, double fulldistance, double thresholdInitA) {
    qDebug() << "Debug plotGraph:" << sagFactorInit << samplingRateInit << distanceToStartInit << distanceToShowInit << fulldistance << thresholdInitA;

    QString filePath = "/home/pi/Rawdata/data0.raw";
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Unable to open file:" << filePath;
        return;
    }
    QByteArray data = file.readAll();
    file.close();

    std::vector<float> normalizedValues;
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(data.constData());
    const uint8_t* endPtr = ptr + data.size();

    while (ptr + 2 <= endPtr) {
        uint16_t signedValue = static_cast<uint16_t>((static_cast<uint8_t>(ptr[1]) << 8) | static_cast<uint8_t>(ptr[0]));
        if (signedValue > 0xE000) {
            signedValue = 0.0f;
        }
        float normalizedValue = (static_cast<float>(signedValue) / 65535.0f) * 4095;  // Convert to mV
        normalizedValues.push_back(normalizedValue);

        ptr += 2;
    }

    //    qDebug() << "Total samples read: " << normalizedValues.size();

    const float threshold = thresholdInitA;  // Convert to mV
    auto startIt = std::find_if(normalizedValues.begin(), normalizedValues.end(), [threshold](float val) { return val >= threshold; });

    if (startIt == normalizedValues.end()) {
        qDebug() << "Threshold value not found in data.";
        return;
    }
    int startIndex = std::distance(normalizedValues.begin(), startIt);
    //    qDebug() << "Starting index found at position:" << startIndex;

    int resampling = samplingRateInit / (60 * sagFactorInit);  // Resampling rate
    //    qDebug() << "Resampling rate:" << resampling;

    float totalDistance = ((distanceToShowInit+10) - distanceToStartInit) * 1000 * 2;
    int fullpoint = static_cast<int>(totalDistance / (60 * sagFactorInit));
    int trueDistancepoint = 2 * (fullpoint / resampling);  // Points after resampling

    //    qDebug() << "Full points:" << fullpoint;
    //    qDebug() << "True distance points after resampling:" << trueDistancepoint;

    float pointInterval = (totalDistance / 2) / trueDistancepoint;  // Distance interval per point
    //    qDebug() << "Point interval (m):" << pointInterval;

    std::vector<std::pair<float, float>> result;
    float currentDistance = distanceToStartInit * 1000;  // Start in meters

    for (int i = 0; i < trueDistancepoint; ++i) {
        float currentValue = (i * resampling + startIndex < normalizedValues.size()) ? normalizedValues[i * resampling + startIndex]  // Multiply by 4096 here
                                                                                     : 0.0f;
        result.emplace_back(currentDistance, currentValue);
        currentDistance += pointInterval;
    }

    qDebug() << "Final Total Points:" << result.size();
    for (const auto& [distance, voltage] : result) {
        //        qDebug() << "X:" << distance / 1000.0 << " km, Y:" << voltage << " mV";
    }

    // JSON Output for Plotting
    QJsonObject mainObject;
    QJsonArray dist, volt;
    for (const auto& [distance, voltage] : result) {
        dist.push_back(distance / 1000);  // Convert m to km
        volt.push_back(voltage);          // Already multiplied by 4096
    }

    mainObject.insert("objectName", "patthernA");
    mainObject.insert("distance", dist);
    mainObject.insert("voltage", volt);

    QJsonDocument jsonDoc(mainObject);
    QString raw_data = jsonDoc.toJson(QJsonDocument::Compact);
    //    qDebug() << "Generated JSON:" << raw_data;

    //        rawdataArrayC = raw_data;
    reSamplingNormalizationPatternA(result);
}

void PLCServer::plotPatternB(double sagFactorInit, double samplingRateInit, double distanceToStartInit, double distanceToShowInit, double fulldistance, double thresholdInitB) {
    qDebug() << "Debug plotPatternB:" << sagFactorInit << samplingRateInit << distanceToStartInit << distanceToShowInit << fulldistance << thresholdInitB;

    QString filePath = "/home/pi/Rawdata/data0.raw";
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Unable to open file:" << filePath;
        return;
    }
    QByteArray data = file.readAll();
    file.close();

    std::vector<float> normalizedValues;
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(data.constData());
    const uint8_t* endPtr = ptr + data.size();

    while (ptr + 2 <= endPtr) {
        uint16_t signedValue = static_cast<uint16_t>((static_cast<uint8_t>(ptr[1]) << 8) | static_cast<uint8_t>(ptr[0]));
        if (signedValue > 0xE000) {
            signedValue = 0.0f;
        }
        float normalizedValue = (static_cast<float>(signedValue) / 65535.0f) * 4095;  // Convert to mV
        normalizedValues.push_back(normalizedValue);

        ptr += 2;
    }

    //    qDebug() << "Total samples read: " << normalizedValues.size();

    const float threshold = thresholdInitB;  // Convert to mV
    auto startIt = std::find_if(normalizedValues.begin(), normalizedValues.end(), [threshold](float val) { return val >= threshold; });

    if (startIt == normalizedValues.end()) {
        qDebug() << "Threshold value not found in data.";
        return;
    }
    int startIndex = std::distance(normalizedValues.begin(), startIt);
    //    qDebug() << "Starting index found at position:" << startIndex;

    int resampling = samplingRateInit / (60 * sagFactorInit);  // Resampling rate
    //    qDebug() << "Resampling rate:" << resampling;

    float totalDistance = ((distanceToShowInit+10) - distanceToStartInit) * 1000 * 2;
    int fullpoint = static_cast<int>(totalDistance / (60 * sagFactorInit));
    int trueDistancepoint = 2 * (fullpoint / resampling);  // Points after resampling

    //    qDebug() << "Full points:" << fullpoint;
    //    qDebug() << "True distance points after resampling:" << trueDistancepoint;

    float pointInterval = (totalDistance / 2) / trueDistancepoint;  // Distance interval per point
    //    qDebug() << "Point interval (m):" << pointInterval;

    std::vector<std::pair<float, float>> result;
    float currentDistance = distanceToStartInit * 1000;  // Start in meters

    for (int i = 0; i < trueDistancepoint; ++i) {
        float currentValue = (i * resampling + startIndex < normalizedValues.size()) ? normalizedValues[i * resampling + startIndex]  // Multiply by 4096 here
                                                                                     : 0.0f;
        result.emplace_back(currentDistance, currentValue);
        currentDistance += pointInterval;
    }

    qDebug() << "Final Total Points:" << result.size();
    for (const auto& [distance, voltage] : result) {
        //        qDebug() << "X:" << distance / 1000.0 << " km, Y:" << voltage << " mV";
    }

    // JSON Output for Plotting
    QJsonObject mainObject;
    QJsonArray dist, volt;
    for (const auto& [distance, voltage] : result) {
        dist.push_back(distance / 1000);  // Convert m to km
        volt.push_back(voltage);          // Already multiplied by 4096
    }

    mainObject.insert("objectName", "patthernB");
    mainObject.insert("distance", dist);
    mainObject.insert("voltage", volt);

    QJsonDocument jsonDoc(mainObject);
    QString raw_data = jsonDoc.toJson(QJsonDocument::Compact);
    //    qDebug() << "Generated JSON:" << raw_data;

    //        rawdataArrayB = raw_data;
    reSamplingNormalizationPatternB(result);
}

void PLCServer::plotPatternC(double sagFactorInit, double samplingRateInit, double distanceToStartInit, double distanceToShowInit, double fulldistance, double thresholdInitC) {
    qDebug() << "Debug plotGraph:" << sagFactorInit << samplingRateInit << distanceToStartInit << distanceToShowInit << fulldistance << thresholdInitC;

    QString filePath = "/home/pi/Rawdata/data0.raw";
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Unable to open file:" << filePath;
        return;
    }
    QByteArray data = file.readAll();
    file.close();

    std::vector<float> normalizedValues;
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(data.constData());
    const uint8_t* endPtr = ptr + data.size();

    while (ptr + 2 <= endPtr) {
        uint16_t signedValue = static_cast<uint16_t>((static_cast<uint8_t>(ptr[1]) << 8) | static_cast<uint8_t>(ptr[0]));
        if (signedValue > 0xE000) {
            signedValue = 0.0f;
        }
        float normalizedValue = (static_cast<float>(signedValue) / 65535.0f) * 4095;  // Convert to mV
        normalizedValues.push_back(normalizedValue);

        ptr += 2;
    }

    //    qDebug() << "Total samples read: " << normalizedValues.size();

    const float threshold = thresholdInitC;  // Convert to mV
    auto startIt = std::find_if(normalizedValues.begin(), normalizedValues.end(), [threshold](float val) { return val >= threshold; });

    if (startIt == normalizedValues.end()) {
        qDebug() << "Threshold value not found in data.";
        return;
    }
    int startIndex = std::distance(normalizedValues.begin(), startIt);
    //    qDebug() << "Starting index found at position:" << startIndex;

    int resampling = samplingRateInit / (60 * sagFactorInit);  // Resampling rate
    //    qDebug() << "Resampling rate:" << resampling;

    float totalDistance = ((distanceToShowInit+10) - distanceToStartInit) * 1000 * 2;
    int fullpoint = static_cast<int>(totalDistance / (60 * sagFactorInit));
    int trueDistancepoint = 2 * (fullpoint / resampling);  // Points after resampling

    //    qDebug() << "Full points:" << fullpoint;
    //    qDebug() << "True distance points after resampling:" << trueDistancepoint;

    float pointInterval = (totalDistance / 2) / trueDistancepoint;  // Distance interval per point
    //    qDebug() << "Point interval (m):" << pointInterval;

    std::vector<std::pair<float, float>> result;
    float currentDistance = distanceToStartInit * 1000;  // Start in meters

    for (int i = 0; i < trueDistancepoint; ++i) {
        float currentValue = (i * resampling + startIndex < normalizedValues.size()) ? normalizedValues[i * resampling + startIndex]  // Multiply by 4096 here
                                                                                     : 0.0f;
        result.emplace_back(currentDistance, currentValue);
        currentDistance += pointInterval;
    }

    qDebug() << "Final Total Points:" << result.size();
    for (const auto& [distance, voltage] : result) {
        //        qDebug() << "X:" << distance / 1000.0 << " km, Y:" << voltage << " mV";
    }

    // JSON Output for Plotting
    QJsonObject mainObject;
    QJsonArray dist, volt;
    for (const auto& [distance, voltage] : result) {
        dist.push_back(distance / 1000);  // Convert m to km
        volt.push_back(voltage);          // Already multiplied by 4096
    }

    mainObject.insert("objectName", "patthernC");
    mainObject.insert("distance", dist);
    mainObject.insert("voltage", volt);

    QJsonDocument jsonDoc(mainObject);
    QString raw_data = jsonDoc.toJson(QJsonDocument::Compact);
    //    qDebug() << "Generated JSON:" << raw_data;

    //      rawdataArrayC = raw_data;
    reSamplingNormalizationPatternC(result);
}

void PLCServer::reSamplingNormalizationA(const std::vector<std::pair<float, float>>& result) {
    if (result.size() < 3) {
        qDebug() << "Not enough data points to process." << result.size();
        return;
    }

    qDebug() << "Starting peak detection and smoothingA...result_size:" << result.size();

    std::vector<std::pair<float, double>> peakPoints;
    try {
        //        std::vector<std::pair<float, double>> smoothCurve;
        peakPoints.reserve(1000000);  // Reserve space if you know an estimate
    } catch (const std::bad_alloc& e) {
        std::cerr << "Memory allocation failed: " << e.what() << std::endl;
    }
    std::pair<float, double> startPoint = {0.0, 0.0};
    qDebug() << "for loop result1";
    for (const auto& point : result) {
        if (point.second > 0.0) {  // First non-zero voltage
            startPoint = {point.first, point.second};
            break;
        }
    }
    peakPoints.push_back(startPoint);
    qDebug() << "for loop result2";
    for (size_t i = 1; i < result.size() - 1; ++i) {
        float prevVoltage = result[i - 1].second;
        float currentVoltage = result[i].second;
        float nextVoltage = result[i + 1].second;

        if (prevVoltage < currentVoltage && currentVoltage > nextVoltage) {
            peakPoints.emplace_back(result[i]);
        }
    }

    peakPoints.push_back(result.back());
    qDebug() << "Starting DebugA1.";

    auto maxPeakIt = std::max_element(peakPoints.begin(), peakPoints.end(), [](const std::pair<float, double>& a, const std::pair<float, double>& b) { return a.second < b.second; });
    std::pair<float, double> maxPeak = *maxPeakIt;
    //    qDebug() << "Maximum peak detected at X:" << maxPeak.first / 1000.0 << "km, Y:" << maxPeak.second << "mV";

    if (std::find(peakPoints.begin(), peakPoints.end(), maxPeak) == peakPoints.end()) {
        peakPoints.push_back(maxPeak);
    }

    //    qDebug() << "Peaks detected. Total peaks:" << peakPoints.size();

    //    for (const auto& peak : peakPoints) {
    //        qDebug() << "Peak at X:" << peak.first / 1000.0 << "km, Y:" << peak.second << "mV";
    //    }
    qDebug() << "Starting DebugA2.";
    std::vector<std::pair<float, double>> downsampledPeaks;
    try {
        //        std::vector<std::pair<float, double>> smoothCurve;
        downsampledPeaks.reserve(1000000);  // Reserve space if you know an estimate
    } catch (const std::bad_alloc& e) {
        std::cerr << "Memory allocation failed: " << e.what() << std::endl;
    }
    size_t step = std::max<size_t>(1, peakPoints.size() / 75);  // Downsample to around 50 points
    qDebug() << "for loop peakPoints";
    for (size_t i = 0; i < peakPoints.size(); i += step) {
        downsampledPeaks.push_back(peakPoints[i]);
    }
    if (peakPoints.back() != downsampledPeaks.back()) {
        downsampledPeaks.push_back(peakPoints.back());  // Ensure the last point is included
    }

    if (std::find(downsampledPeaks.begin(), downsampledPeaks.end(), maxPeak) == downsampledPeaks.end()) {
        downsampledPeaks.push_back(maxPeak);
    }

    std::sort(downsampledPeaks.begin(), downsampledPeaks.end());  // Sort points by X
    //    qDebug() << "Downsampling completed. Total downsampled points:" << downsampledPeaks.size();
    qDebug() << "Starting DebugA3.";
    std::vector<std::pair<float, double>> smoothCurve;
    try {
        //        std::vector<std::pair<float, double>> smoothCurve;
        smoothCurve.reserve(1000000);  // Reserve space if you know an estimate
    } catch (const std::bad_alloc& e) {
        std::cerr << "Memory allocation failed: " << e.what() << std::endl;
    }
    qDebug() << "for loop downsampledPeaks" << downsampledPeaks.size();
    const int stepsPerSegment = 10;
    for (size_t i = 0; i + 1 < downsampledPeaks.size(); ++i) {
        float x1 = downsampledPeaks[i].first;
        float x2 = downsampledPeaks[i + 1].first;
        double y1 = downsampledPeaks[i].second;
        double y2 = downsampledPeaks[i + 1].second;

        if (x1 == x2) {
            // จุดซ้ำกัน → เพิ่มแค่ 1 จุด แล้วข้าม
            smoothCurve.emplace_back(x1, y1);
            continue;
        }

        for (int j = 0; j <= stepsPerSegment; ++j) {
            float t = static_cast<float>(j) / stepsPerSegment;
            float x = (1 - t) * x1 + t * x2;
            double y = (1 - t) * y1 + t * y2;
            smoothCurve.emplace_back(x, y);
        }
    }

    qDebug() << "Smoothing completed. Total points for the curveA:" << smoothCurve.size();

    if (smoothCurve.size() < 2) {
        qDebug() << "Not enough points for interpolation.";
        return;
    } else
        qDebug() << "smoothCurve:" << smoothCurve.size();
    int originalPoints = static_cast<int>(smoothCurve.size());
    int targetPointCount = static_cast<int>(originalPoints * 1.05);
    if (targetPointCount <= originalPoints) ++targetPointCount;

    float xStart = smoothCurve.front().first;
    float xEnd = smoothCurve.back().first;
    float interval = (xEnd - xStart) / (targetPointCount - 1);

    QJsonObject mainObject;
    QJsonArray dist, volt;
    distBkup = QJsonArray();
    voltBkup = QJsonArray();
    distanceArrayAPattern.clear();
    voltageArrayAPattern.clear();
    distanceArrayAPatternbkup.clear();
    voltageArrayAPatternbkup.clear();

    size_t j = 0;
    qDebug() << "for loop targetPointCount";
    for (int i = 0; i < targetPointCount; ++i) {
        if (i == targetPointCount) {
            break;
        }
        float xi = xStart + i * interval;

        while (j + 1 < smoothCurve.size() && smoothCurve[j + 1].first < xi) {
            ++j;
        }

        if (j + 1 >= smoothCurve.size()) break;

        float x1 = smoothCurve[j].first;
        float x2 = smoothCurve[j + 1].first;
        double y1 = smoothCurve[j].second;
        double y2 = smoothCurve[j + 1].second;

        double t = (xi - x1) / (x2 - x1);
        double yi = (1 - t) * y1 + t * y2;

        dist.push_back(xi / 1000.0);  // m → km
        volt.push_back(yi);
        distBkup.push_back(xi / 1000.0);
        voltBkup.push_back(yi);
        distanceArrayAPattern.push_back(xi / 1000.0);  // m → km
        voltageArrayAPattern.push_back(yi);
        distanceArrayAPatternbkup.push_back(xi / 1000.0);  // m → km
        voltageArrayAPatternbkup.push_back(yi);
    }

    qDebug() << "before end of the program";
    if (SurgeMode) {
        QJsonDocument jsonDoc2;
        QJsonObject Param;

        Param = QJsonObject();
        Param.insert("objectName", "SurgePlot");
        Param.insert("data", volt);
        Param.insert("phase", "A");
        jsonDoc2.setObject(Param);
        QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        emit sendToSocketPLC(testNoneSmoothA);
        SurgeMode = false;

        //        qDebug() << "SurgeModeRemote" << SurgeModeRemote;
        //        mainObject.insert("objectName", "dataPlotingA");
        //        mainObject.insert("distance", distBkup);
        //        mainObject.insert("voltage", voltBkup);

        //        QJsonDocument jsonDoc(mainObject);
        //        QString raw_data = jsonDoc.toJson(QJsonDocument::Compact);
        //        rawdataArrayA = "";
        //        rawdataArrayA = std::move(raw_data);

        ////        emit sendToMonitor(rawdataArrayA);
        //        emit sendToSocketPLC(raw_data);
        //        qDebug() << "SumNormalizationandUpdateDbA" << modeName;
        //        emit SumNormalizationandUpdateDb(modeName, "phaseA", FileTimeStamp, dist, volt);
        //        SurgeMode = false;
    } else {
        if (patternSelected) {
            calFLF(dist, volt, "A");
        }
        if (interlockPattern) {
            RecalculateWithMarginManual(dist, volt, "A");
            qDebug() << "RecalculateWithMarginManual:" << volt << dist << interlockPattern;
        } else {
            qDebug() << "check pattern interlockPattern:" << interlockPattern;
            mainObject.insert("objectName", "dataPlotingA");
            mainObject.insert("distance", dist);
            mainObject.insert("voltage", volt);

            QJsonDocument jsonDoc(mainObject);
            QString raw_data = jsonDoc.toJson(QJsonDocument::Compact);
            rawdataArrayA = "";
            rawdataArrayA = std::move(raw_data);
            qDebug() << "SumNormalization_dist:" << dist << "SumNormalization_volt:" << volt;
            //            emit sendToMonitor(rawdataArrayA);
            emit sendToMonitor(testNoneSmoothA);
            emit sendToVNC(testNoneSmoothA);
            // Q_FOREACH (QWebSocket* pClient, Monitor_address) {
            //     if (pClient->state() == QAbstractSocket::ConnectedState)
            //         emit sendMessage(rawdataArrayA, pClient);

            //     else
            //         qDebug() << "Monitor_address:" << pClient->state();
            // }
            qDebug() << "SumNormalizationandUpdateDbA" << modeName;
        }
        auditEventDataPhase(QStringLiteral("phaseA"), FileTimeStamp, dist, volt);
        myDatabase->SumNormalizationandUpdateDb(modeName, "phaseA", FileTimeStamp, dist, volt);
    }
    qDebug() << "reSamplingNormalizationA:" << patternSelected << interlockPattern;
}
//void PLCServer::reSamplingNormalizationA(const std::vector<std::pair<float, float>>& result) {
//    if (result.size() < 3) {
//        qDebug() << "Not enough data points to process." << result.size();
//        return;
//    }

//    qDebug() << "Starting peak detection and smoothingA...result_size:" << result.size();

//    std::vector<std::pair<float, double>> peakPoints;
//    try {
//        //        std::vector<std::pair<float, double>> smoothCurve;
//        peakPoints.reserve(1000000);  // Reserve space if you know an estimate
//    } catch (const std::bad_alloc& e) {
//        std::cerr << "Memory allocation failed: " << e.what() << std::endl;
//    }
//    std::pair<float, double> startPoint = {0.0, 0.0};
//    qDebug() << "for loop result1";
//    for (const auto& point : result) {
//        if (point.second > 0.0) {  // First non-zero voltage
//            startPoint = {point.first, point.second};
//            break;
//        }
//    }
//    peakPoints.push_back(startPoint);
//    qDebug() << "for loop result2";
//    for (size_t i = 1; i < result.size() - 1; ++i) {
//        float prevVoltage = result[i - 1].second;
//        float currentVoltage = result[i].second;
//        float nextVoltage = result[i + 1].second;

//        if (prevVoltage < currentVoltage && currentVoltage > nextVoltage) {
//            peakPoints.emplace_back(result[i]);
//        }
//    }

//    peakPoints.push_back(result.back());
//    qDebug() << "Starting DebugA1.";

//    auto maxPeakIt = std::max_element(peakPoints.begin(), peakPoints.end(), [](const std::pair<float, double>& a, const std::pair<float, double>& b) { return a.second < b.second; });
//    std::pair<float, double> maxPeak = *maxPeakIt;
//    //    qDebug() << "Maximum peak detected at X:" << maxPeak.first / 1000.0 << "km, Y:" << maxPeak.second << "mV";

//    if (std::find(peakPoints.begin(), peakPoints.end(), maxPeak) == peakPoints.end()) {
//        peakPoints.push_back(maxPeak);
//    }

//    //    qDebug() << "Peaks detected. Total peaks:" << peakPoints.size();

//    //    for (const auto& peak : peakPoints) {
//    //        qDebug() << "Peak at X:" << peak.first / 1000.0 << "km, Y:" << peak.second << "mV";
//    //    }
//    qDebug() << "Starting DebugA2.";
//    std::vector<std::pair<float, double>> downsampledPeaks;
//    try {
//        //        std::vector<std::pair<float, double>> smoothCurve;
//        downsampledPeaks.reserve(1000000);  // Reserve space if you know an estimate
//    } catch (const std::bad_alloc& e) {
//        std::cerr << "Memory allocation failed: " << e.what() << std::endl;
//    }
//    size_t step = std::max<size_t>(1, peakPoints.size() / 75);  // Downsample to around 50 points
//    qDebug() << "for loop peakPoints";
//    for (size_t i = 0; i < peakPoints.size(); i += step) {
//        downsampledPeaks.push_back(peakPoints[i]);
//    }
//    if (peakPoints.back() != downsampledPeaks.back()) {
//        downsampledPeaks.push_back(peakPoints.back());  // Ensure the last point is included
//    }

//    if (std::find(downsampledPeaks.begin(), downsampledPeaks.end(), maxPeak) == downsampledPeaks.end()) {
//        downsampledPeaks.push_back(maxPeak);
//    }

//    std::sort(downsampledPeaks.begin(), downsampledPeaks.end());  // Sort points by X
//    //    qDebug() << "Downsampling completed. Total downsampled points:" << downsampledPeaks.size();
//    qDebug() << "Starting DebugA3.";
//    std::vector<std::pair<float, double>> smoothCurve;
//    try {
//        //        std::vector<std::pair<float, double>> smoothCurve;
//        smoothCurve.reserve(1000000);  // Reserve space if you know an estimate
//    } catch (const std::bad_alloc& e) {
//        std::cerr << "Memory allocation failed: " << e.what() << std::endl;
//    }
//    qDebug() << "for loop downsampledPeaks" << downsampledPeaks.size();
//    const int stepsPerSegment = 10;
//    for (size_t i = 0; i + 1 < downsampledPeaks.size(); ++i) {
//        float x1 = downsampledPeaks[i].first;
//        float x2 = downsampledPeaks[i + 1].first;
//        double y1 = downsampledPeaks[i].second;
//        double y2 = downsampledPeaks[i + 1].second;

//        if (x1 == x2) {
//            // จุดซ้ำกัน → เพิ่มแค่ 1 จุด แล้วข้าม
//            smoothCurve.emplace_back(x1, y1);
//            continue;
//        }

//        for (int j = 0; j <= stepsPerSegment; ++j) {
//            float t = static_cast<float>(j) / stepsPerSegment;
//            float x = (1 - t) * x1 + t * x2;
//            double y = (1 - t) * y1 + t * y2;
//            smoothCurve.emplace_back(x, y);
//        }
//    }

//    qDebug() << "Smoothing completed. Total points for the curveA:" << smoothCurve.size();

//    if (smoothCurve.size() < 2) {
//        qDebug() << "Not enough points for interpolation.";
//        return;
//    } else
//        qDebug() << "smoothCurve:" << smoothCurve.size();
//    int originalPoints = static_cast<int>(smoothCurve.size());
//    int targetPointCount = static_cast<int>(originalPoints * 1.05);
//    if (targetPointCount <= originalPoints) ++targetPointCount;

//    float xStart = smoothCurve.front().first;
//    float xEnd = smoothCurve.back().first;
//    float interval = (xEnd - xStart) / (targetPointCount - 1);

//    QJsonObject mainObject;
//    QJsonArray dist, volt;
//    distBkup = QJsonArray();
//    voltBkup = QJsonArray();
//    distanceArrayAPattern.clear();
//    voltageArrayAPattern.clear();
//    distanceArrayAPatternbkup.clear();
//    voltageArrayAPatternbkup.clear();

//    size_t j = 0;
//    qDebug() << "for loop targetPointCount";
//    for (int i = 0; i < targetPointCount; ++i) {
//        if (i == targetPointCount) {
//            break;
//        }
//        float xi = xStart + i * interval;

//        while (j + 1 < smoothCurve.size() && smoothCurve[j + 1].first < xi) {
//            ++j;
//        }

//        if (j + 1 >= smoothCurve.size()) break;

//        float x1 = smoothCurve[j].first;
//        float x2 = smoothCurve[j + 1].first;
//        double y1 = smoothCurve[j].second;
//        double y2 = smoothCurve[j + 1].second;

//        double t = (xi - x1) / (x2 - x1);
//        double yi = (1 - t) * y1 + t * y2;

//        dist.push_back(xi / 1000.0);  // m → km
//        volt.push_back(yi);
//        distBkup.push_back(xi / 1000.0);
//        voltBkup.push_back(yi);
//        distanceArrayAPattern.push_back(xi / 1000.0);  // m → km
//        voltageArrayAPattern.push_back(yi);
//        distanceArrayAPatternbkup.push_back(xi / 1000.0);  // m → km
//        voltageArrayAPatternbkup.push_back(yi);
//    }

//    qDebug() << "before end of the program";
//    if (SurgeMode) {
//        QJsonDocument jsonDoc2;
//        QJsonObject Param;

//        Param = QJsonObject();
//        Param.insert("objectName", "SurgePlot");
//        Param.insert("data", volt);
//        Param.insert("phase", "A");
//        jsonDoc2.setObject(Param);
//        QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
//        emit sendToSocketPLC(raw_data);
//        SurgeMode = false;

//        //        qDebug() << "SurgeModeRemote" << SurgeModeRemote;
//        //        mainObject.insert("objectName", "dataPlotingA");
//        //        mainObject.insert("distance", distBkup);
//        //        mainObject.insert("voltage", voltBkup);

//        //        QJsonDocument jsonDoc(mainObject);
//        //        QString raw_data = jsonDoc.toJson(QJsonDocument::Compact);
//        //        rawdataArrayA = "";
//        //        rawdataArrayA = std::move(raw_data);

//        ////        emit sendToMonitor(rawdataArrayA);
//        //        emit sendToSocketPLC(raw_data);
//        //        qDebug() << "SumNormalizationandUpdateDbA" << modeName;
//        //        emit SumNormalizationandUpdateDb(modeName, "phaseA", FileTimeStamp, dist, volt);
//        //        SurgeMode = false;
//    } else {
//        if (patternSelected) {
//            calFLF(dist, volt, "A");
//        }
//        if (interlockPattern) {
//            RecalculateWithMarginManual(dist, volt, "A");
//            qDebug() << "RecalculateWithMarginManual:" << volt << dist << interlockPattern;
//        } else {
//            qDebug() << "check pattern interlockPattern:" << interlockPattern;
//            mainObject.insert("objectName", "dataPlotingA");
//            mainObject.insert("distance", dist);
//            mainObject.insert("voltage", volt);

//            QJsonDocument jsonDoc(mainObject);
//            QString raw_data = jsonDoc.toJson(QJsonDocument::Compact);
//            rawdataArrayA = "";
//            rawdataArrayA = std::move(raw_data);
//            qDebug() << "SumNormalization_dist:" << dist << "SumNormalization_volt:" << volt;
//            //            emit sendToMonitor(rawdataArrayA);
//            Q_FOREACH (QWebSocket* pClient, Monitor_address) {
//                if (pClient->state() == QAbstractSocket::ConnectedState)
//                    emit sendMessage(rawdataArrayA, pClient);

//                else
//                    qDebug() << "Monitor_address:" << pClient->state();
//            }
//            qDebug() << "SumNormalizationandUpdateDbA" << modeName;
//        }
//        myDatabase->SumNormalizationandUpdateDb(modeName, "phaseA", FileTimeStamp, dist, volt);
//    }
//    qDebug() << "reSamplingNormalizationA:" << patternSelected << interlockPattern;
//}

void PLCServer::reSamplingNormalizationB(const std::vector<std::pair<float, float>>& result) {
    if (result.size() < 3) {
        qDebug() << "Not enough data points to process.";
        return;
    }

    qDebug() << "Starting peak detection and smoothingB... result_size:" << result.size();

    std::vector<std::pair<float, double>> peakPoints;
    std::pair<float, double> startPoint = {0.0, 0.0};

    try {
        //        std::vector<std::pair<float, double>> peakPoints;
        peakPoints.reserve(1000000);  // Reserve space if you know an estimate
    } catch (const std::bad_alloc& e) {
        std::cerr << "Memory allocation failed: " << e.what() << std::endl;
    }

    for (const auto& point : result) {
        if (point.second > 0.0) {
            startPoint = {point.first, point.second};
            break;
        }
    }
    qDebug() << "After for loop const auto& point : result.";
    peakPoints.push_back(startPoint);

    for (size_t i = 1; i < result.size() - 1; ++i) {
        float prevVoltage = result[i - 1].second;
        float currentVoltage = result[i].second;
        float nextVoltage = result[i + 1].second;

        if (prevVoltage < currentVoltage && currentVoltage > nextVoltage) {
            peakPoints.emplace_back(result[i]);
        }
    }

    peakPoints.push_back(result.back());

    auto maxPeakIt = std::max_element(peakPoints.begin(), peakPoints.end(), [](const std::pair<float, double>& a, const std::pair<float, double>& b) { return a.second < b.second; });
    std::pair<float, double> maxPeak = *maxPeakIt;
    //    qDebug() << "Maximum peak detected at X:" << maxPeak.first / 1000.0 << "km, Y:" << maxPeak.second << "mV";

    if (std::find(peakPoints.begin(), peakPoints.end(), maxPeak) == peakPoints.end()) {
        peakPoints.push_back(maxPeak);
    }

    //    qDebug() << "Peaks detected. Total peaks:" << peakPoints.size();

    //    for (const auto& peak : peakPoints) {
    ////        qDebug() << "Peak at X:" << peak.first / 1000.0 << "km, Y:" << peak.second << "mV";
    //    }

    std::vector<std::pair<float, double>> downsampledPeaks;
    try {
        //        std::vector<std::pair<float, double>> smoothCurve;
        downsampledPeaks.reserve(1000000);  // Reserve space if you know an estimate
    } catch (const std::bad_alloc& e) {
        std::cerr << "Memory allocation failed: " << e.what() << std::endl;
    }
    size_t step = std::max<size_t>(1, peakPoints.size() / 75);  // Downsample to around 50 points
    for (size_t i = 0; i < peakPoints.size(); i += step) {
        downsampledPeaks.push_back(peakPoints[i]);
    }
    if (peakPoints.back() != downsampledPeaks.back()) {
        downsampledPeaks.push_back(peakPoints.back());  // Ensure the last point is included
    }

    if (std::find(downsampledPeaks.begin(), downsampledPeaks.end(), maxPeak) == downsampledPeaks.end()) {
        downsampledPeaks.push_back(maxPeak);
    }

    std::sort(downsampledPeaks.begin(), downsampledPeaks.end());  // Sort points by X
    //    qDebug() << "Downsampling completed. Total downsampled points:" << downsampledPeaks.size();

    std::vector<std::pair<float, double>> smoothCurve;
    try {
        //        std::vector<std::pair<float, double>> smoothCurve;
        smoothCurve.reserve(1000000);  // Reserve space if you know an estimate
    } catch (const std::bad_alloc& e) {
        std::cerr << "Memory allocation failed: " << e.what() << std::endl;
    }
    const int stepsPerSegment = 10;
    for (size_t i = 0; i + 1 < downsampledPeaks.size(); ++i) {
        float x1 = downsampledPeaks[i].first;
        float x2 = downsampledPeaks[i + 1].first;
        double y1 = downsampledPeaks[i].second;
        double y2 = downsampledPeaks[i + 1].second;

        if (x1 == x2) {
            // จุดซ้ำกัน → เพิ่มแค่ 1 จุด แล้วข้าม
            smoothCurve.emplace_back(x1, y1);
            continue;
        }

        for (int j = 0; j <= stepsPerSegment; ++j) {
            float t = static_cast<float>(j) / stepsPerSegment;
            float x = (1 - t) * x1 + t * x2;
            double y = (1 - t) * y1 + t * y2;
            smoothCurve.emplace_back(x, y);
        }
    }

    qDebug() << "Smoothing completed. Total points for the curve:" << smoothCurve.size();
    if (smoothCurve.size() < 2) {
        qDebug() << "Not enough points for interpolation.";
        return;
    }
    int originalPoints = static_cast<int>(smoothCurve.size());
    int targetPointCount = static_cast<int>(originalPoints * 1.05);
    if (targetPointCount <= originalPoints) ++targetPointCount;

    float xStart = smoothCurve.front().first;
    float xEnd = smoothCurve.back().first;
    float interval = (xEnd - xStart) / (targetPointCount - 1);

    QJsonObject mainObject;
    QJsonArray dist, volt;
    distanceArrayBPattern.clear();
    voltageArrayBPattern.clear();
    distanceArrayBPatternbkup.clear();
    voltageArrayBPatternbkup.clear();

    size_t j = 0;
    for (int i = 0; i < targetPointCount; ++i) {
        if (i == targetPointCount) {
            break;
        }
        float xi = xStart + i * interval;

        while (j + 1 < smoothCurve.size() && smoothCurve[j + 1].first < xi) {
            ++j;
        }

        if (j + 1 >= smoothCurve.size()) break;

        float x1 = smoothCurve[j].first;
        float x2 = smoothCurve[j + 1].first;
        double y1 = smoothCurve[j].second;
        double y2 = smoothCurve[j + 1].second;

        double t = (xi - x1) / (x2 - x1);
        double yi = (1 - t) * y1 + t * y2;

        dist.push_back(xi / 1000.0);  // m → km
        volt.push_back(yi);
        distanceArrayBPattern.push_back(xi / 1000.0);  // m → km
        voltageArrayBPattern.push_back(yi);
        distanceArrayBPatternbkup.push_back(xi / 1000.0);  // m → km
        voltageArrayBPatternbkup.push_back(yi);
    }

    if (SurgeMode) {
        QJsonDocument jsonDoc2;
        QJsonObject Param;

        Param = QJsonObject();
        Param.insert("objectName", "SurgePlot");
        Param.insert("data", volt);
        Param.insert("phase", "B");
        jsonDoc2.setObject(Param);
        QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        emit sendToSocketPLC(raw_data);
        SurgeMode = false;
    } else {
        if (patternSelected) {
            calFLF(dist, volt, "B");
        }
        if (interlockPattern) {
            RecalculateWithMarginManual(dist, volt, "B");
        } else {
            mainObject.insert("objectName", "dataPlotingB");
            mainObject.insert("distance", dist);
            mainObject.insert("voltage", volt);

            QJsonDocument jsonDoc(mainObject);
            QString raw_data = jsonDoc.toJson(QJsonDocument::Compact);
            rawdataArrayB = "";
            rawdataArrayB = std::move(raw_data);

            // emit sendToMonitor(rawdataArrayB);
            emit sendToMonitor(testNoneSmoothB);
            emit sendToVNC(testNoneSmoothB);
            qDebug() << "SumNormalizationandUpdateDbB" << modeName;
        }

        auditEventDataPhase(QStringLiteral("phaseB"), FileTimeStamp, dist, volt);
        myDatabase->SumNormalizationandUpdateDb(modeName, "phaseB", FileTimeStamp, dist, volt);
    }
}

void PLCServer::reSamplingNormalizationC(const std::vector<std::pair<float, float>>& result) {
    if (result.size() < 3) {
        qDebug() << "Not enough data points to process.";
        return;
    }
    qDebug() << "Starting peak detection and smoothingC...result_size:" << result.size();
    std::vector<std::pair<float, double>> peakPoints;
    std::pair<float, double> startPoint = {0.0, 0.0};

    try {
        //        std::vector<std::pair<float, double>> smoothCurve;
        peakPoints.reserve(1000000);  // Reserve space if you know an estimate
    } catch (const std::bad_alloc& e) {
        std::cerr << "Memory allocation failed: " << e.what() << std::endl;
    }

    qDebug() << "for loop result1";
    for (const auto& point : result) {
        if (point.second > 0.0) {  // First non-zero voltage
            startPoint = {point.first, point.second};
            break;
        }
    }
    peakPoints.push_back(startPoint);
    qDebug() << "for loop result2";
    for (size_t i = 1; i < result.size() - 1; ++i) {
        float prevVoltage = result[i - 1].second;
        float currentVoltage = result[i].second;
        float nextVoltage = result[i + 1].second;

        if (prevVoltage < currentVoltage && currentVoltage > nextVoltage) {
            peakPoints.emplace_back(result[i]);
        }
    }

    peakPoints.push_back(result.back());

    auto maxPeakIt = std::max_element(peakPoints.begin(), peakPoints.end(), [](const std::pair<float, double>& a, const std::pair<float, double>& b) { return a.second < b.second; });
    std::pair<float, double> maxPeak = *maxPeakIt;
    if (std::find(peakPoints.begin(), peakPoints.end(), maxPeak) == peakPoints.end()) {
        peakPoints.push_back(maxPeak);
    }

    //    for (const auto& peak : peakPoints) {
    //        qDebug() << "Peak at X:" << peak.first / 1000.0 << "km, Y:" << peak.second << "mV";
    //    }

    std::vector<std::pair<float, double>> downsampledPeaks;
    try {
        //        std::vector<std::pair<float, double>> smoothCurve;
        downsampledPeaks.reserve(1000000);  // Reserve space if you know an estimate
    } catch (const std::bad_alloc& e) {
        std::cerr << "Memory allocation failed: " << e.what() << std::endl;
    }
    size_t step = std::max<size_t>(1, peakPoints.size() / 75);  // Downsample to around 50 points
    qDebug() << "for loop peakPoints";
    for (size_t i = 0; i < peakPoints.size(); i += step) {
        downsampledPeaks.push_back(peakPoints[i]);
    }
    if (peakPoints.back() != downsampledPeaks.back()) {
        downsampledPeaks.push_back(peakPoints.back());  // Ensure the last point is included
    }

    if (std::find(downsampledPeaks.begin(), downsampledPeaks.end(), maxPeak) == downsampledPeaks.end()) {
        downsampledPeaks.push_back(maxPeak);
    }

    std::sort(downsampledPeaks.begin(), downsampledPeaks.end());  // Sort points by X
    //    qDebug() << "Downsampling completed. Total downsampled points:" << downsampledPeaks.size();

    std::vector<std::pair<float, double>> smoothCurve;
    try {
        //        std::vector<std::pair<float, double>> smoothCurve;
        smoothCurve.reserve(1000000);  // Reserve space if you know an estimate
    } catch (const std::bad_alloc& e) {
        std::cerr << "Memory allocation failed: " << e.what() << std::endl;
    }
    qDebug() << "for loop downsampledPeaks";
    const int stepsPerSegment = 10;
    for (size_t i = 0; i + 1 < downsampledPeaks.size(); ++i) {
        float x1 = downsampledPeaks[i].first;
        float x2 = downsampledPeaks[i + 1].first;
        double y1 = downsampledPeaks[i].second;
        double y2 = downsampledPeaks[i + 1].second;

        if (x1 == x2) {
            // จุดซ้ำกัน → เพิ่มแค่ 1 จุด แล้วข้าม
            smoothCurve.emplace_back(x1, y1);
            continue;
        }

        for (int j = 0; j <= stepsPerSegment; ++j) {
            float t = static_cast<float>(j) / stepsPerSegment;
            float x = (1 - t) * x1 + t * x2;
            double y = (1 - t) * y1 + t * y2;
            smoothCurve.emplace_back(x, y);
        }
    }

    qDebug() << "Smoothing completed. Total points for the curve:" << smoothCurve.size();
    if (smoothCurve.size() < 2) {
        qDebug() << "Not enough points for interpolation.";
        return;
    }

    int originalPoints = static_cast<int>(smoothCurve.size());
    int targetPointCount = static_cast<int>(originalPoints * 1.05);
    if (targetPointCount <= originalPoints) ++targetPointCount;  // อย่างน้อยให้เพิ่ม 1 จุด

    float xStart = smoothCurve.front().first;
    float xEnd = smoothCurve.back().first;
    float interval = (xEnd - xStart) / (targetPointCount - 1);

    QJsonObject mainObject;
    QJsonArray dist, volt;
    distanceArrayCPattern.clear();
    voltageArrayCPattern.clear();
    distanceArrayCPatternbkup.clear();
    voltageArrayCPatternbkup.clear();

    size_t j = 0;
    qDebug() << "for loop targetPointCount";
    for (int i = 0; i < targetPointCount; ++i) {
        if (i == targetPointCount) {
            break;
        }
        float xi = xStart + i * interval;

        while (j + 1 < smoothCurve.size() && smoothCurve[j + 1].first < xi) {
            ++j;
        }

        if (j + 1 >= smoothCurve.size()) break;

        float x1 = smoothCurve[j].first;
        float x2 = smoothCurve[j + 1].first;
        double y1 = smoothCurve[j].second;
        double y2 = smoothCurve[j + 1].second;

        double t = (xi - x1) / (x2 - x1);
        double yi = (1 - t) * y1 + t * y2;

        dist.push_back(xi / 1000.0);
        volt.push_back(yi);
        distanceArrayCPattern.push_back(xi / 1000.0);  // m → km
        voltageArrayCPattern.push_back(yi);
        distanceArrayCPatternbkup.push_back(xi / 1000.0);  // m → km
        voltageArrayCPatternbkup.push_back(yi);
    }
    qDebug() << "before end of program";
    if (SurgeMode) {
        QJsonDocument jsonDoc2;
        QJsonObject Param;

        Param = QJsonObject();
        Param.insert("objectName", "SurgePlot");
        Param.insert("data", volt);
        Param.insert("phase", "C");
        jsonDoc2.setObject(Param);
        QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        emit sendToSocketPLC(raw_data);
        SurgeMode = false;
    } else {
        if (patternSelected) {
            calFLF(dist, volt, "C");
        }
        if (interlockPattern) {
            RecalculateWithMarginManual(dist, volt, "C");
        } else {
            mainObject.insert("objectName", "dataPlotingC");
            mainObject.insert("distance", dist);
            mainObject.insert("voltage", volt);

            QJsonDocument jsonDoc(mainObject);
            QString raw_data = jsonDoc.toJson(QJsonDocument::Compact);
            rawdataArrayC = "";
            rawdataArrayC = std::move(raw_data);

            // emit sendToMonitor(rawdataArrayC);
            emit sendToMonitor(testNoneSmoothC);
            emit sendToVNC(testNoneSmoothC);

            qDebug() << "SumNormalizationandUpdateDbC" << modeName;
        }
        auditEventDataPhase(QStringLiteral("phaseC"), FileTimeStamp, dist, volt);
        myDatabase->SumNormalizationandUpdateDb(modeName, "phaseC", FileTimeStamp, dist, volt);
    }
}

void PLCServer::calcSegmentRange(double totalDistance,
                             int segmentCount,
                             int segmentIndex,
                             double &startpoint,
                             double &endpoint)
{
    startpoint = 0.0;
    endpoint = 0.0;

    if (segmentCount <= 0 || segmentIndex < 0)
        return;

    const double segmentLen = totalDistance / static_cast<double>(segmentCount);
    startpoint = segmentIndex * segmentLen;
    endpoint = (segmentIndex == segmentCount - 1)
             ? totalDistance
             : ((segmentIndex + 1) * segmentLen);
}

bool PLCServer::isDistanceInSegment(double distance,
                                double startpoint,
                                double endpoint,
                                bool isLastSegment)
{
    if (isLastSegment)
        return (distance >= startpoint && distance <= endpoint);

    return (distance >= startpoint && distance < endpoint);
}

void PLCServer::RecalculateWithMarginManual(QJsonArray dis, QJsonArray volt, QString phase) {
    qDebug() << "RecalculateWithMarginManual phase:" << phase;

    QVector<double> vectordis, vectorvolt;
    for (const auto& value : dis) {
        if (value.isDouble())
            vectordis.append(value.toDouble());
    }
    for (const auto& value : volt) {
        if (value.isDouble())
            vectorvolt.append(value.toDouble());
    }

    if (vectordis.isEmpty() || vectorvolt.isEmpty()) {
        qDebug() << "No data available for processing!";
        return;
    }

    auto applyMargins = [&](int marginCount, const int *marginValues) {
        if (marginCount <= 0 || marginValues == nullptr)
            return;

        for (int u = 0; u < marginCount; ++u) {
            double startpoint = 0.0;
            double endpoint = 0.0;
            calcSegmentRange(distanceToShow, marginCount, u, startpoint, endpoint);
            const bool isLastSegment = (u == marginCount - 1);

            double last_val = 0.0;
            for (int i = 0; i < vectorvolt.size() && i < vectordis.size(); ++i) {
                if (!isDistanceInSegment(vectordis[i], startpoint, endpoint, isLastSegment))
                    continue;

                if (isLastSegment) {
                    if (vectorvolt[i] == 0)
                        vectorvolt[i] = last_val + marginValues[u];
                    else {
                        vectorvolt[i] = vectorvolt[i] + marginValues[u];
                        last_val = vectorvolt[i];
                    }
                } else {
                    vectorvolt[i] = vectorvolt[i] + marginValues[u];
                }
            }
        }
    };

        if (phase == "A") {
            applyMargins(myDatabase->lenghtMarginA, myDatabase->valueOfMarginA);
        } else if (phase == "B") {
            applyMargins(myDatabase->lenghtMarginB, myDatabase->valueOfMarginB);
        } else if (phase == "C") {
            applyMargins(myDatabase->lenghtMarginC, myDatabase->valueOfMarginC);
        }

    QJsonArray distArray, voltArray;
    for (int i = 0; i < vectorvolt.size() && i < vectordis.size(); ++i) {
        distArray.append(vectordis[i]);
        voltArray.append(vectorvolt[i]);
    }

    QJsonObject mainObject;
    mainObject.insert("objectName", QString("pattern%1").arg(phase));
    mainObject.insert("distance", distArray);
    mainObject.insert("voltage", voltArray);
    mainObject.insert("text", "margin");

    QJsonDocument jsonDoc(mainObject);
    QString rawdataArray = jsonDoc.toJson(QJsonDocument::Compact);

    Q_FOREACH (QWebSocket* pClient, Monitor_address) {
        if (pClient->state() == QAbstractSocket::ConnectedState)
            emit sendMessage(rawdataArray, pClient);
    }
    emit sendToVNC(rawdataArray);
}

void PLCServer::RecalculateWithMarginManual(QWebSocket* w) {
    qDebug() << "RecalculateWithMarginManual(QWebSocket*) called:" << w;

    if (!w || w->state() != QAbstractSocket::ConnectedState)
        return;

    auto processPhase = [&](const QString &phase,
                            QVector<double> &distanceArray,
                            QVector<double> &voltageArray,
                            QVector<double> &voltageBkup,
                            int marginCount,
                            const int *marginValues) {
        if (distanceArray.isEmpty() || voltageArray.isEmpty() || marginCount <= 0 || marginValues == nullptr)
            return;

        QVector<double> workVoltage = voltageArray;

        for (int u = 0; u < marginCount; ++u) {
            double startpoint = 0.0;
            double endpoint = 0.0;
            calcSegmentRange(distanceToShow, marginCount, u, startpoint, endpoint);
            const bool isLastSegment = (u == marginCount - 1);

            double last_val = 0.0;
            for (int i = 0; i < workVoltage.size() && i < distanceArray.size() && i < voltageBkup.size(); ++i) {
                if (!isDistanceInSegment(distanceArray[i], startpoint, endpoint, isLastSegment))
                    continue;

                if (isLastSegment) {
                    if (voltageBkup[i] == 0) {
                        workVoltage[i] = last_val + marginValues[u];
                    } else {
                        workVoltage[i] = voltageBkup[i] + marginValues[u];
                        last_val = voltageBkup[i];
                    }
                } else {
                    workVoltage[i] = voltageBkup[i] + marginValues[u];
                }
            }
        }

        voltageArray = workVoltage;

        QJsonArray distArray, voltArray;
        for (int i = 0; i < distanceArray.size() && i < voltageArray.size(); ++i) {
            distArray.append(distanceArray[i]);
            voltArray.append(voltageArray[i]);
        }

        QJsonObject mainObject;
        mainObject.insert("objectName", QString("pattern%1").arg(phase));
        mainObject.insert("distance", distArray);
        mainObject.insert("voltage", voltArray);
        mainObject.insert("text", "margin");

        QString rawdataArray = QJsonDocument(mainObject).toJson(QJsonDocument::Compact);
        emit sendMessage(rawdataArray, w);
    };

    processPhase("A",
                 distanceArrayAPattern,
                 voltageArrayAPattern,
                 voltageArrayAPatternbkup,
                 myDatabase->lenghtMarginA,
                 myDatabase->valueOfMarginA);

    processPhase("B",
                 distanceArrayBPattern,
                 voltageArrayBPattern,
                 voltageArrayBPatternbkup,
                 myDatabase->lenghtMarginB,
                 myDatabase->valueOfMarginB);

    processPhase("C",
                 distanceArrayCPattern,
                 voltageArrayCPattern,
                 voltageArrayCPatternbkup,
                 myDatabase->lenghtMarginC,
                 myDatabase->valueOfMarginC);
}

void PLCServer::RecalculateWithMargin(QString msg) {
    qDebug() << "RecalculateWithMargin called with data:" << msg;

    QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8());
    QJsonObject obj = doc.object();
    QString getCommand = obj.value("objectName").toString();
    QString rawdataArray;

    if (getCommand.contains("combinedDataPhase") ||
        getCommand == "combinedDataPhaseA" ||
        getCommand == "combinedDataPhaseB" ||
        getCommand == "combinedDataPhaseC") {

        int margin = obj.value("margin").toInt();
        int valueVoltage = obj.value("valueVoltage").toInt();
        int focusIndex = obj.value("focusIndex").toInt();
        QString phase = obj.value("PHASE").toString();

        qDebug() << "Processing margin:" << margin
                 << "valueVoltage:" << valueVoltage
                 << "focusIndex:" << focusIndex
                 << "Phase:" << phase;

        if (margin <= 0 || focusIndex < 0) {
            qDebug() << "Invalid margin/focusIndex";
            return;
        }

        double startpoint = 0.0;
        double endpoint = 0.0;
        calcSegmentRange(distanceToShow, margin, focusIndex, startpoint, endpoint);
        const bool isLastSegment = (focusIndex == margin - 1);

        if (phase == "A") {
            if (distanceArrayAPattern.isEmpty()) {
                qDebug() << "No data available for processing!";
                return;
            }

            QVector<double> voltageArrayPattern = voltageArrayAPattern;
            double last_val = 0.0;

            for (int i = 0; i < voltageArrayPattern.size() && i < distanceArrayAPattern.size(); ++i) {
                if (!isDistanceInSegment(distanceArrayAPattern[i], startpoint, endpoint, isLastSegment))
                    continue;

                if (myDatabase->lenghtMarginA - 1 == focusIndex) {
                    if (voltageArrayAPatternbkup[i] == 0) {
                        voltageArrayPattern[i] = last_val + valueVoltage;
                    } else {
                        voltageArrayPattern[i] = voltageArrayAPatternbkup[i] + valueVoltage;
                        last_val = voltageArrayAPatternbkup[i];
                    }
                } else {
                    voltageArrayPattern[i] = voltageArrayAPatternbkup[i] + valueVoltage;
                }
            }

            voltageArrayAPattern = voltageArrayPattern;
            disAPattern = distanceArrayAPattern;
            volAPattern = voltageArrayAPattern;

            QJsonArray distArray, voltArray;
            for (int i = 0; i < distanceArrayAPattern.size() && i < voltageArrayAPattern.size(); ++i) {
                distArray.append(distanceArrayAPattern[i]);
                voltArray.append(voltageArrayAPattern[i]);
            }

            QJsonObject mainObject;
            mainObject.insert("objectName", "patternA");
            mainObject.insert("distance", distArray);
            mainObject.insert("voltage", voltArray);
            mainObject.insert("text", "margin");
            mainObject.insert("fileName", myDatabase->selectPatterName);

            QJsonDocument jsonDoc(mainObject);
            rawdataArray = jsonDoc.toJson(QJsonDocument::Compact);

            for (QWebSocket* pClient : qAsConst(Monitor_address)) {
                if (pClient->state() == QAbstractSocket::ConnectedState)
                    emit sendMessage(rawdataArray, pClient);
            }
            emit sendToVNC(rawdataArray);

        } else if (phase == "B") {
            if (distanceArrayBPattern.isEmpty()) {
                qDebug() << "No data available for processing!";
                return;
            }

            double last_val = 0.0;
            for (int i = 0; i < voltageArrayBPattern.size() && i < distanceArrayBPattern.size(); ++i) {
                if (!isDistanceInSegment(distanceArrayBPattern[i], startpoint, endpoint, isLastSegment))
                    continue;

                if (myDatabase->lenghtMarginB - 1 == focusIndex) {
                    if (voltageArrayBPatternbkup[i] == 0) {
                        voltageArrayBPattern[i] = last_val + valueVoltage;
                    } else {
                        voltageArrayBPattern[i] = voltageArrayBPatternbkup[i] + valueVoltage;
                        last_val = voltageArrayBPatternbkup[i];
                    }
                } else {
                    voltageArrayBPattern[i] = voltageArrayBPatternbkup[i] + valueVoltage;
                }
            }

            disBPattern = distanceArrayBPattern;
            volBPattern = voltageArrayBPattern;

            QJsonArray distArray, voltArray;
            for (int i = 0; i < distanceArrayBPattern.size() && i < voltageArrayBPattern.size(); ++i) {
                distArray.append(distanceArrayBPattern[i]);
                voltArray.append(voltageArrayBPattern[i]);
            }

            QJsonObject mainObject;
            mainObject.insert("objectName", "patternB");
            mainObject.insert("distance", distArray);
            mainObject.insert("voltage", voltArray);
            mainObject.insert("text", "margin");
            mainObject.insert("fileName", myDatabase->selectPatterName);

            QJsonDocument jsonDoc(mainObject);
            rawdataArray = jsonDoc.toJson(QJsonDocument::Compact);

            for (QWebSocket* pClient : qAsConst(Monitor_address)) {
                if (pClient->state() == QAbstractSocket::ConnectedState)
                    emit sendMessage(rawdataArray, pClient);
            }
            emit sendToVNC(rawdataArray);

        } else if (phase == "C") {
            if (distanceArrayCPattern.isEmpty()) {
                qDebug() << "No data available for processing!";
                return;
            }

            double last_val = 0.0;
            for (int i = 0; i < voltageArrayCPattern.size() && i < distanceArrayCPattern.size(); ++i) {
                if (!isDistanceInSegment(distanceArrayCPattern[i], startpoint, endpoint, isLastSegment))
                    continue;

                if (myDatabase->lenghtMarginC - 1 == focusIndex) {
                    if (voltageArrayCPatternbkup[i] == 0) {
                        voltageArrayCPattern[i] = last_val + valueVoltage;
                    } else {
                        voltageArrayCPattern[i] = voltageArrayCPatternbkup[i] + valueVoltage;
                        last_val = voltageArrayCPatternbkup[i];
                    }
                } else {
                    voltageArrayCPattern[i] = voltageArrayCPatternbkup[i] + valueVoltage;
                }
            }

            disCPattern = distanceArrayCPattern;
            volCPattern = voltageArrayCPattern;

            QJsonArray distArray, voltArray;
            for (int i = 0; i < distanceArrayCPattern.size() && i < voltageArrayCPattern.size(); ++i) {
                distArray.append(distanceArrayCPattern[i]);
                voltArray.append(voltageArrayCPattern[i]);
            }

            QJsonObject mainObject;
            mainObject.insert("objectName", "patternC");
            mainObject.insert("distance", distArray);
            mainObject.insert("voltage", voltArray);
            mainObject.insert("text", "margin");
            mainObject.insert("fileName", myDatabase->selectPatterName);

            QJsonDocument jsonDoc(mainObject);
            rawdataArray = jsonDoc.toJson(QJsonDocument::Compact);

            for (QWebSocket* pClient : qAsConst(Monitor_address)) {
                if (pClient->state() == QAbstractSocket::ConnectedState)
                    emit sendMessage(rawdataArray, pClient);
            }
            emit sendToVNC(rawdataArray);
        }

    } else if (getCommand.contains("autoSetValueMargin") ||
               getCommand.contains("autoSetValueMarginA") ||
               getCommand.contains("autoSetValueMarginB") ||
               getCommand.contains("autoSetValueMarginC")) {

        int valueVoltages = obj["autoValueVoltage"].toInt();
        QString phase = obj["PHASE"].toString();

        if (phase == "A") {
            if (distanceArrayAPattern.isEmpty()) return;

            double last_val = 0.0;
            for (int i = 0; i < voltageArrayAPattern.size(); ++i) {
                if (voltageArrayAPatternbkup[i] == 0)
                    voltageArrayAPattern[i] = last_val + valueVoltages;
                else {
                    voltageArrayAPattern[i] = voltageArrayAPatternbkup[i] + valueVoltages;
                    last_val = voltageArrayAPatternbkup[i];
                }
            }

            QJsonObject mainObject;
            QJsonArray dist, volt;
            disAPattern = distanceArrayAPattern;
            volAPattern = voltageArrayAPattern;

            for (int i = 0; i < voltageArrayAPattern.size(); ++i) {
                dist.push_back(distanceArrayAPattern[i]);
                volt.push_back(voltageArrayAPattern[i]);
            }

            mainObject.insert("objectName", "patternA");
            mainObject.insert("distance", dist);
            mainObject.insert("voltage", volt);
            mainObject.insert("text", "automargin");
            mainObject.insert("fileName", myDatabase->selectPatterName);

            rawdataArray = QJsonDocument(mainObject).toJson(QJsonDocument::Compact);
            Q_FOREACH (QWebSocket* pClient, Monitor_address) {
                if (pClient->state() == QAbstractSocket::ConnectedState)
                    emit sendMessage(rawdataArray, pClient);
            }
            emit sendToVNC(rawdataArray);

        } else if (phase == "B") {
            if (distanceArrayBPattern.isEmpty()) return;

            double last_val = 0.0;
            for (int i = 0; i < voltageArrayBPattern.size(); ++i) {
                if (voltageArrayBPatternbkup[i] == 0)
                    voltageArrayBPattern[i] = last_val + valueVoltages;
                else {
                    voltageArrayBPattern[i] = voltageArrayBPatternbkup[i] + valueVoltages;
                    last_val = voltageArrayBPatternbkup[i];
                }
            }

            QJsonObject mainObject;
            QJsonArray dist, volt;
            disBPattern = distanceArrayBPattern;
            volBPattern = voltageArrayBPattern;

            for (int i = 0; i < voltageArrayBPattern.size(); ++i) {
                dist.push_back(distanceArrayBPattern[i]);
                volt.push_back(voltageArrayBPattern[i]);
            }

            mainObject.insert("objectName", "patternB");
            mainObject.insert("distance", dist);
            mainObject.insert("voltage", volt);
            mainObject.insert("text", "automargin");
            mainObject.insert("fileName", myDatabase->selectPatterName);

            rawdataArray = QJsonDocument(mainObject).toJson(QJsonDocument::Compact);
            Q_FOREACH (QWebSocket* pClient, Monitor_address) {
                if (pClient->state() == QAbstractSocket::ConnectedState)
                    emit sendMessage(rawdataArray, pClient);
            }
            emit sendToVNC(rawdataArray);

        } else if (phase == "C") {
            if (distanceArrayCPattern.isEmpty()) return;

            double last_val = 0.0;
            for (int i = 0; i < voltageArrayCPattern.size(); ++i) {
                if (voltageArrayCPatternbkup[i] == 0)
                    voltageArrayCPattern[i] = last_val + valueVoltages;
                else {
                    voltageArrayCPattern[i] = voltageArrayCPatternbkup[i] + valueVoltages;
                    last_val = voltageArrayCPatternbkup[i];
                }
            }

            QJsonObject mainObject;
            QJsonArray dist, volt;
            disCPattern = distanceArrayCPattern;
            volCPattern = voltageArrayCPattern;

            for (int i = 0; i < voltageArrayCPattern.size(); ++i) {
                dist.push_back(distanceArrayCPattern[i]);
                volt.push_back(voltageArrayCPattern[i]);
            }

            mainObject.insert("objectName", "patternC");
            mainObject.insert("distance", dist);
            mainObject.insert("voltage", volt);
            mainObject.insert("text", "automargin");
            mainObject.insert("fileName", myDatabase->selectPatterName);

            rawdataArray = QJsonDocument(mainObject).toJson(QJsonDocument::Compact);
            Q_FOREACH (QWebSocket* pClient, Monitor_address) {
                if (pClient->state() == QAbstractSocket::ConnectedState)
                    emit sendMessage(rawdataArray, pClient);
            }
            emit sendToVNC(rawdataArray);
        }
    }
}
void PLCServer::reSamplingNormalizationPatternA(const std::vector<std::pair<float, float>>& result) {
    //    if (result.size() < 3) {
    //        qDebug() << "Not enough data points to process.";
    //        return;
    //    }
    qDebug() << "Starting peak detection and smoothing...";

    // Step 1: Detect Peaks
    std::vector<std::pair<float, double>> peakPoints;
    std::pair<float, double> startPoint = {0.0, 0.0};

    for (const auto& point : result) {
        if (point.second > 0.0) {  // First non-zero voltage
            startPoint = {point.first, point.second};
            break;
        }
    }
    peakPoints.push_back(startPoint);

    for (size_t i = 1; i < result.size() - 1; ++i) {
        float prevVoltage = result[i - 1].second;
        float currentVoltage = result[i].second;
        float nextVoltage = result[i + 1].second;

        if (prevVoltage < currentVoltage && currentVoltage > nextVoltage) {
            peakPoints.emplace_back(result[i]);
        }
    }

    peakPoints.push_back(result.back());

    auto maxPeakIt = std::max_element(peakPoints.begin(), peakPoints.end(), [](const std::pair<float, double>& a, const std::pair<float, double>& b) { return a.second < b.second; });
    std::pair<float, double> maxPeak = *maxPeakIt;
    //    qDebug() << "Maximum peak detected at X:" << maxPeak.first / 1000.0 << "km, Y:" << maxPeak.second << "mV";

    if (std::find(peakPoints.begin(), peakPoints.end(), maxPeak) == peakPoints.end()) {
        peakPoints.push_back(maxPeak);
    }

    qDebug() << "Peaks detected. Total peaks:" << peakPoints.size();

    for (const auto& peak : peakPoints) {
        //        qDebug() << "Peak at X:" << peak.first / 1000.0 << "km, Y:" << peak.second << "mV";
    }

    // Step 2: Downsample Peak Points
    std::vector<std::pair<float, double>> downsampledPeaks;
    size_t step = std::max<size_t>(1, peakPoints.size() / 50);  // Downsample to around 50 points
    for (size_t i = 0; i < peakPoints.size(); i += step) {
        downsampledPeaks.push_back(peakPoints[i]);
    }
    if (peakPoints.back() != downsampledPeaks.back()) {
        downsampledPeaks.push_back(peakPoints.back());  // Ensure the last point is included
    }

    if (std::find(downsampledPeaks.begin(), downsampledPeaks.end(), maxPeak) == downsampledPeaks.end()) {
        downsampledPeaks.push_back(maxPeak);
    }

    std::sort(downsampledPeaks.begin(), downsampledPeaks.end());  // Sort points by X
    //    qDebug() << "Downsampling completed. Total downsampled points:" << downsampledPeaks.size();

    // Step 3: Generate Smooth Curve
    std::vector<std::pair<float, double>> smoothCurve;
    for (size_t i = 0; i < downsampledPeaks.size() - 1; ++i) {
        float x1 = downsampledPeaks[i].first;
        float x2 = downsampledPeaks[i + 1].first;
        double y1 = downsampledPeaks[i].second;
        double y2 = downsampledPeaks[i + 1].second;

        for (float x = x1; x <= x2; x += (x2 - x1) / 10.0) {  // Add 10 points per segment
            double t = (x - x1) / (x2 - x1);                  // Normalize x between 0 and 1
            double y = (1 - t) * y1 + t * y2;                 // Linear interpolation
            smoothCurve.emplace_back(x, y);
        }
    }

    qDebug() << "Smoothing completed. Total points for the curve:" << smoothCurve.size();

    if (smoothCurve.size() < 2) {
        qDebug() << "Not enough points for interpolation.";
        return;
    }
    int originalPoints = static_cast<int>(smoothCurve.size());
    int targetPointCount = static_cast<int>(originalPoints * 1.05);
    if (targetPointCount <= originalPoints) ++targetPointCount;

    float xStart = smoothCurve.front().first;
    float xEnd = smoothCurve.back().first;
    float interval = (xEnd - xStart) / (targetPointCount - 1);

    QJsonObject mainObject;
    QJsonArray dist, volt;
    distanceArrayAPattern.clear();
    voltageArrayAPattern.clear();
    distanceArrayAPatternbkup.clear();
    voltageArrayAPatternbkup.clear();

    size_t j = 0;
    for (int i = 0; i < targetPointCount; ++i) {
        float xi = xStart + i * interval;

        while (j + 1 < smoothCurve.size() && smoothCurve[j + 1].first < xi) {
            ++j;
        }

        if (j + 1 >= smoothCurve.size()) break;

        float x1 = smoothCurve[j].first;
        float x2 = smoothCurve[j + 1].first;
        double y1 = smoothCurve[j].second;
        double y2 = smoothCurve[j + 1].second;

        double t = (xi - x1) / (x2 - x1);
        double yi = (1 - t) * y1 + t * y2;

        dist.push_back(xi / 1000.0);  // m → km
        volt.push_back(yi);
        distanceArrayAPattern.push_back(xi / 1000.0);  // m → km
        voltageArrayAPattern.push_back(yi);
        distanceArrayAPatternbkup.push_back(xi / 1000.0);  // m → km
        voltageArrayAPatternbkup.push_back(yi);
    }

    qDebug() << "interlockPattern:" << interlockPattern;
    if (!interlockPattern)
        mainObject.insert("objectName", "dataPlotingB");

    else
        mainObject.insert("objectName", "patternB");

    mainObject.insert("distance", dist);
    mainObject.insert("voltage", volt);

    QJsonDocument jsonDoc(mainObject);
    QString raw_data = jsonDoc.toJson(QJsonDocument::Compact);
    //    qDebug() << "Generated JSON for smoothed curve:" << raw_data;

    // Emit the signal for the curve
    rawdataPatternArrayA = std::move(raw_data);
    qDebug() << "sendToMonitor(rawdataPatternArrayA)" << rawdataPatternArrayA;
    emit sendToMonitor(rawdataPatternArrayA);
    emit sendToVNC(rawdataPatternArrayA);
}

void PLCServer::reSamplingNormalizationPatternB(const std::vector<std::pair<float, float>>& result) {
    //    if (result.size() < 3) {
    //        qDebug() << "Not enough data points to process.";
    //        return;
    //    }
    qDebug() << "Starting peak detection and smoothing...";

    // Step 1: Detect Peaks
    std::vector<std::pair<float, double>> peakPoints;
    std::pair<float, double> startPoint = {0.0, 0.0};

    for (const auto& point : result) {
        if (point.second > 0.0) {  // First non-zero voltage
            startPoint = {point.first, point.second};
            break;
        }
    }
    peakPoints.push_back(startPoint);

    for (size_t i = 1; i < result.size() - 1; ++i) {
        float prevVoltage = result[i - 1].second;
        float currentVoltage = result[i].second;
        float nextVoltage = result[i + 1].second;

        if (prevVoltage < currentVoltage && currentVoltage > nextVoltage) {
            peakPoints.emplace_back(result[i]);
        }
    }

    peakPoints.push_back(result.back());

    auto maxPeakIt = std::max_element(peakPoints.begin(), peakPoints.end(), [](const std::pair<float, double>& a, const std::pair<float, double>& b) { return a.second < b.second; });
    std::pair<float, double> maxPeak = *maxPeakIt;
    //    qDebug() << "Maximum peak detected at X:" << maxPeak.first / 1000.0 << "km, Y:" << maxPeak.second << "mV";

    if (std::find(peakPoints.begin(), peakPoints.end(), maxPeak) == peakPoints.end()) {
        peakPoints.push_back(maxPeak);
    }

    qDebug() << "Peaks detected. Total peaks:" << peakPoints.size();

    for (const auto& peak : peakPoints) {
        //        qDebug() << "Peak at X:" << peak.first / 1000.0 << "km, Y:" << peak.second << "mV";
    }

    // Step 2: Downsample Peak Points
    std::vector<std::pair<float, double>> downsampledPeaks;
    size_t step = std::max<size_t>(1, peakPoints.size() / 50);  // Downsample to around 50 points
    for (size_t i = 0; i < peakPoints.size(); i += step) {
        downsampledPeaks.push_back(peakPoints[i]);
    }
    if (peakPoints.back() != downsampledPeaks.back()) {
        downsampledPeaks.push_back(peakPoints.back());  // Ensure the last point is included
    }

    if (std::find(downsampledPeaks.begin(), downsampledPeaks.end(), maxPeak) == downsampledPeaks.end()) {
        downsampledPeaks.push_back(maxPeak);
    }

    std::sort(downsampledPeaks.begin(), downsampledPeaks.end());  // Sort points by X
    //    qDebug() << "Downsampling completed. Total downsampled points:" << downsampledPeaks.size();

    // Step 3: Generate Smooth Curve
    std::vector<std::pair<float, double>> smoothCurve;
    for (size_t i = 0; i < downsampledPeaks.size() - 1; ++i) {
        float x1 = downsampledPeaks[i].first;
        float x2 = downsampledPeaks[i + 1].first;
        double y1 = downsampledPeaks[i].second;
        double y2 = downsampledPeaks[i + 1].second;

        for (float x = x1; x <= x2; x += (x2 - x1) / 10.0) {  // Add 10 points per segment
            double t = (x - x1) / (x2 - x1);                  // Normalize x between 0 and 1
            double y = (1 - t) * y1 + t * y2;                 // Linear interpolation
            smoothCurve.emplace_back(x, y);
        }
    }

    qDebug() << "Smoothing completed. Total points for the curve:" << smoothCurve.size();
    if (smoothCurve.size() < 2) {
        qDebug() << "Not enough points for interpolation.";
        return;
    }
    int originalPoints = static_cast<int>(smoothCurve.size());
    int targetPointCount = static_cast<int>(originalPoints * 1.05);
    if (targetPointCount <= originalPoints) ++targetPointCount;

    float xStart = smoothCurve.front().first;
    float xEnd = smoothCurve.back().first;
    float interval = (xEnd - xStart) / (targetPointCount - 1);

    QJsonObject mainObject;
    QJsonArray dist, volt;
    distanceArrayBPattern.clear();
    voltageArrayBPattern.clear();
    distanceArrayBPatternbkup.clear();
    voltageArrayBPatternbkup.clear();

    size_t j = 0;
    for (int i = 0; i < targetPointCount; ++i) {
        float xi = xStart + i * interval;

        while (j + 1 < smoothCurve.size() && smoothCurve[j + 1].first < xi) {
            ++j;
        }

        if (j + 1 >= smoothCurve.size()) break;

        float x1 = smoothCurve[j].first;
        float x2 = smoothCurve[j + 1].first;
        double y1 = smoothCurve[j].second;
        double y2 = smoothCurve[j + 1].second;

        double t = (xi - x1) / (x2 - x1);
        double yi = (1 - t) * y1 + t * y2;

        dist.push_back(xi / 1000.0);  // m → km
        volt.push_back(yi);
        distanceArrayBPattern.push_back(xi / 1000.0);  // m → km
        voltageArrayBPattern.push_back(yi);
        distanceArrayBPatternbkup.push_back(xi / 1000.0);  // m → km
        voltageArrayBPatternbkup.push_back(yi);
    }

    if (!interlockPattern)
        mainObject.insert("objectName", "dataPlotingB");
    else
        mainObject.insert("objectName", "patternB");

    mainObject.insert("distance", dist);
    mainObject.insert("voltage", volt);

    QJsonDocument jsonDoc(mainObject);
    QString raw_data = jsonDoc.toJson(QJsonDocument::Compact);
    //    qDebug() << "Generated JSON for smoothed curve:" << raw_data;

    // Emit the signal for the curve
    rawdataPatternArrayB = std::move(raw_data);
    emit sendToMonitor(rawdataPatternArrayB);
    emit sendToVNC(rawdataPatternArrayB);
}

void PLCServer::reSamplingNormalizationPatternC(const std::vector<std::pair<float, float>>& result) {
    //    if (result.size() < 3) {
    //        qDebug() << "Not enough data points to process.";
    //        return;
    //    }
    qDebug() << "Starting peak detection and smoothing...";

    // Step 1: Detect Peaks
    std::vector<std::pair<float, double>> peakPoints;
    std::pair<float, double> startPoint = {0.0, 0.0};

    for (const auto& point : result) {
        if (point.second > 0.0) {  // First non-zero voltage
            startPoint = {point.first, point.second};
            break;
        }
    }
    peakPoints.push_back(startPoint);

    for (size_t i = 1; i < result.size() - 1; ++i) {
        float prevVoltage = result[i - 1].second;
        float currentVoltage = result[i].second;
        float nextVoltage = result[i + 1].second;

        if (prevVoltage < currentVoltage && currentVoltage > nextVoltage) {
            peakPoints.emplace_back(result[i]);
        }
    }

    peakPoints.push_back(result.back());

    auto maxPeakIt = std::max_element(peakPoints.begin(), peakPoints.end(), [](const std::pair<float, double>& a, const std::pair<float, double>& b) { return a.second < b.second; });
    std::pair<float, double> maxPeak = *maxPeakIt;
    //    qDebug() << "Maximum peak detected at X:" << maxPeak.first / 1000.0 << "km, Y:" << maxPeak.second << "mV";

    if (std::find(peakPoints.begin(), peakPoints.end(), maxPeak) == peakPoints.end()) {
        peakPoints.push_back(maxPeak);
    }

    qDebug() << "Peaks detected. Total peaks:" << peakPoints.size();

    for (const auto& peak : peakPoints) {
        //        qDebug() << "Peak at X:" << peak.first / 1000.0 << "km, Y:" << peak.second << "mV";
    }

    // Step 2: Downsample Peak Points
    std::vector<std::pair<float, double>> downsampledPeaks;
    size_t step = std::max<size_t>(1, peakPoints.size() / 50);  // Downsample to around 50 points
    for (size_t i = 0; i < peakPoints.size(); i += step) {
        downsampledPeaks.push_back(peakPoints[i]);
    }
    if (peakPoints.back() != downsampledPeaks.back()) {
        downsampledPeaks.push_back(peakPoints.back());  // Ensure the last point is included
    }

    if (std::find(downsampledPeaks.begin(), downsampledPeaks.end(), maxPeak) == downsampledPeaks.end()) {
        downsampledPeaks.push_back(maxPeak);
    }

    std::sort(downsampledPeaks.begin(), downsampledPeaks.end());  // Sort points by X
    //    qDebug() << "Downsampling completed. Total downsampled points:" << downsampledPeaks.size();

    // Step 3: Generate Smooth Curve
    std::vector<std::pair<float, double>> smoothCurve;
    for (size_t i = 0; i < downsampledPeaks.size() - 1; ++i) {
        float x1 = downsampledPeaks[i].first;
        float x2 = downsampledPeaks[i + 1].first;
        double y1 = downsampledPeaks[i].second;
        double y2 = downsampledPeaks[i + 1].second;

        for (float x = x1; x <= x2; x += (x2 - x1) / 10.0) {  // Add 10 points per segment
            double t = (x - x1) / (x2 - x1);                  // Normalize x between 0 and 1
            double y = (1 - t) * y1 + t * y2;                 // Linear interpolation
            smoothCurve.emplace_back(x, y);
        }
    }

    qDebug() << "Smoothing completed. Total points for the curve:" << smoothCurve.size();

    if (smoothCurve.size() < 2) {
        qDebug() << "Not enough points for interpolation.";
        return;
    }
    int originalPoints = static_cast<int>(smoothCurve.size());
    int targetPointCount = static_cast<int>(originalPoints * 1.05);
    if (targetPointCount <= originalPoints) ++targetPointCount;

    float xStart = smoothCurve.front().first;
    float xEnd = smoothCurve.back().first;
    float interval = (xEnd - xStart) / (targetPointCount - 1);

    QJsonObject mainObject;
    QJsonArray dist, volt;
    distanceArrayCPattern.clear();
    voltageArrayCPattern.clear();
    distanceArrayCPatternbkup.clear();
    voltageArrayCPatternbkup.clear();
    size_t j = 0;
    for (int i = 0; i < targetPointCount; ++i) {
        float xi = xStart + i * interval;

        while (j + 1 < smoothCurve.size() && smoothCurve[j + 1].first < xi) {
            ++j;
        }

        if (j + 1 >= smoothCurve.size()) break;

        float x1 = smoothCurve[j].first;
        float x2 = smoothCurve[j + 1].first;
        double y1 = smoothCurve[j].second;
        double y2 = smoothCurve[j + 1].second;

        double t = (xi - x1) / (x2 - x1);
        double yi = (1 - t) * y1 + t * y2;

        dist.push_back(xi / 1000.0);  // m → km
        volt.push_back(yi);
        distanceArrayCPattern.push_back(xi / 1000.0);  // m → km
        voltageArrayCPattern.push_back(yi);
        distanceArrayCPatternbkup.push_back(xi / 1000.0);  // m → km
        voltageArrayCPatternbkup.push_back(yi);
    }

    if (!interlockPattern)
        mainObject.insert("objectName", "dataPlotingC");
    else
        mainObject.insert("objectName", "patternC");

    mainObject.insert("distance", dist);
    mainObject.insert("voltage", volt);

    QJsonDocument jsonDoc(mainObject);
    QString raw_data = jsonDoc.toJson(QJsonDocument::Compact);
    //    qDebug() << "Generated JSON for smoothed curve:" << raw_data;

    rawdataPatternArrayC = std::move(raw_data);
    emit sendToMonitor(rawdataPatternArrayC);
    emit sendToVNC(rawdataPatternArrayC);
}

void PLCServer::getCsvMarginFile(QString fileName, QString category, QString date){
    qDebug() << "getCsvMarginFile";
    QString datefile = date.left(10);
    QString timefile = date.mid(11, 8);
    timefile.replace(":", "-");

    QString filePath = QString(EVENT_PATH + "%1/%2/%3/%4")
                           .arg("Pattern")
                           .arg(datefile)
                           .arg(timefile)
                           .arg(fileName + "_Margin");

    qDebug() << "filePath" << filePath;
    myDatabase->resetAllMarginTablesValueToZero();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning("Unable to open file for reading.");
        return;
    }

    QTextStream in(&file);

    if (!in.atEnd()) {
        in.readLine(); // skip header
    }

    int count = 0;
    int maxA = 0, maxB = 0, maxC = 0;
    QVector<int> savedValuesA;
    QVector<int> savedValuesB;
    QVector<int> savedValuesC;

    auto toSafeInt = [](const QString& text, int fallback = 0) -> int {
        const QString v = text.trimmed();
        return v.isEmpty() ? fallback : v.toInt();
    };

    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (line.trimmed().isEmpty()) {
            continue;
        }

        const QStringList values = line.split(",");
        if (values.size() < 6) {
            qDebug() << "Skip invalid margin line:" << line;
            continue;
        }

        count++;

        maxA = toSafeInt(values[0], maxA);
        maxB = toSafeInt(values[2], maxB);
        maxC = toSafeInt(values[4], maxC);

        const int valueA = toSafeInt(values[1], 0);
        const int valueB = toSafeInt(values[3], 0);
        const int valueC = toSafeInt(values[5], 0);

        savedValuesA.append(valueA);
        savedValuesB.append(valueB);
        savedValuesC.append(valueC);

        myDatabase->updataListOfMarginANotObject(count, valueA);
        myDatabase->updataListOfMarginBNotObject(count, valueB);
        myDatabase->updataListOfMarginCNotObject(count, valueC);

        const QString singleMarginDataA = QString(
            "{\"objectName\":\"marginlistCountA\", "
            "\"no\":%1, "
            "\"marginNo\":\"%2\", "
            "\"valueOfMargin\":%3, "
            "\"maxmargin\":%4, "
            "\"unit\":\"%5\"}")
            .arg(count)
            .arg("Margin" + QString::number(count))
            .arg(valueA)
            .arg(maxA)
            .arg("mV");

        const QString singleMarginDataB = QString(
            "{\"objectName\":\"marginlistCountB\", "
            "\"no\":%1, "
            "\"marginNo\":\"%2\", "
            "\"valueOfMargin\":%3, "
            "\"maxmargin\":%4, "
            "\"unit\":\"%5\"}")
            .arg(count)
            .arg("Margin" + QString::number(count))
            .arg(valueB)
            .arg(maxB)
            .arg("mV");

        const QString singleMarginDataC = QString(
            "{\"objectName\":\"marginlistCountC\", "
            "\"no\":%1, "
            "\"marginNo\":\"%2\", "
            "\"valueOfMargin\":%3, "
            "\"maxmargin\":%4, "
            "\"unit\":\"%5\"}")
            .arg(count)
            .arg("Margin" + QString::number(count))
            .arg(valueC)
            .arg(maxC)
            .arg("mV");

        const QString autoMarginDataA = QString(
            "{\"objectName\":\"valueMarginVoltageAauto\", "
            "\"valueVoltageA\":0}");

        const QString autoMarginDataB = QString(
            "{\"objectName\":\"valueMarginVoltageBauto\", "
            "\"valueVoltageB\":0}");

        const QString autoMarginDataC = QString(
            "{\"objectName\":\"valueMarginVoltageCauto\", "
            "\"valueVoltageC\":0}");

        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState) {
                if (count <= maxA) {
                    emit sendMessage(singleMarginDataA, pClient);
                    emit sendMessage(autoMarginDataA, pClient);
                }
                if (count <= maxB) {
                    emit sendMessage(singleMarginDataB, pClient);
                    emit sendMessage(autoMarginDataB, pClient);
                }
                if (count <= maxC) {
                    emit sendMessage(singleMarginDataC, pClient);
                    emit sendMessage(autoMarginDataC, pClient);
                }
            } else {
                qDebug() << "Monitor_address:" << pClient->state();
            }
        }

        if (count <= maxA) {
            emit sendToVNC(singleMarginDataA);
            emit sendToVNC(autoMarginDataA);
        }
        if (count <= maxB) {
            emit sendToVNC(singleMarginDataB);
            emit sendToVNC(autoMarginDataB);
        }
        if (count <= maxC) {
            emit sendToVNC(singleMarginDataC);
            emit sendToVNC(autoMarginDataC);
        }
    }

    file.close();

    myDatabase->UpdateMarginSettingParameter(maxA,0,0,"A");
    myDatabase->UpdateMarginSettingParameter(maxB,0,0,"B");
    myDatabase->UpdateMarginSettingParameter(maxC,0,0,"C");

    auto applySavedMargins = [this](const QVector<int>& savedValues,
                                    int maxMargin,
                                    const QString& phase) {
        QString objectName;
        if (phase == "A")
            objectName = "combinedDataPhaseA";
        else if (phase == "B")
            objectName = "combinedDataPhaseB";
        else
            objectName = "combinedDataPhaseC";

        // ✅ กรณี maxMargin = 0 ก็ยังต้องส่งข้อมูล 0 ออกไป
        if (maxMargin <= 0) {
            const QString combinedData = QString(
                "{"
                "\"objectName\":\"%1\","
                "\"margin\":1,"
                "\"valueVoltage\":0,"
                "\"focusIndex\":0,"
                "\"PHASE\":\"%2\""
                "}")
                .arg(objectName)
                .arg(phase);

            // qWarning() << "Restore saved margin (zero case):" << combinedData;

            myDatabase->updateMarginSettingParameter(1, 0, 0, phase);
            RecalculateWithMargin(combinedData);

            // ถ้าต้องการ emit ด้วยก็เปิดได้
            // if (phase == "A")
            //     emit updataListOfMarginA(combinedData);
            // else if (phase == "B")
            //     emit updataListOfMarginB(combinedData);
            // else
            //     emit updataListOfMarginC(combinedData);

            return;
        }

        bool appliedAny = false;

        for (int i = 0; i < savedValues.size() && i < maxMargin; ++i) {
            const int valueVoltage = savedValues[i];
            if (valueVoltage == 0) {
                continue;
            }

            const QString combinedData = QString(
                "{"
                "\"objectName\":\"%1\","
                "\"margin\":%2,"
                "\"valueVoltage\":%3,"
                "\"focusIndex\":%4,"
                "\"PHASE\":\"%5\""
                "}")
                .arg(objectName)
                .arg(maxMargin)
                .arg(valueVoltage)
                .arg(i)
                .arg(phase);

            // qWarning() << "Restore saved margin:" << combinedData;

            RecalculateWithMargin(combinedData);

            // if (phase == "A")
            //     emit updataListOfMarginA(combinedData);
            // else if (phase == "B")
            //     emit updataListOfMarginB(combinedData);
            // else
            //     emit updataListOfMarginC(combinedData);

            appliedAny = true;
        }

        // ✅ ถ้า maxMargin > 0 แต่ไม่มีค่าใน savedValues ที่ใช้ได้เลย ก็ยังส่ง 0 ออกไป
        if (!appliedAny) {
            const QString combinedData = QString(
                "{"
                "\"objectName\":\"%1\","
                "\"margin\":%2,"
                "\"valueVoltage\":0,"
                "\"focusIndex\":0,"
                "\"PHASE\":\"%3\""
                "}")
                .arg(objectName)
                .arg(maxMargin)
                .arg(phase);

            // qWarning() << "Restore saved margin (fallback zero):" << combinedData;

            myDatabase->updateMarginSettingParameter(maxMargin, 0, 0, phase);
            RecalculateWithMargin(combinedData);

            // if (phase == "A")
            //     emit updataListOfMarginA(combinedData);
            // else if (phase == "B")
            //     emit updataListOfMarginB(combinedData);
            // else
            //     emit updataListOfMarginC(combinedData);
        }
    };

    applySavedMargins(savedValuesA, maxA, "A");
    applySavedMargins(savedValuesB, maxB, "B");
    applySavedMargins(savedValuesC, maxC, "C");
}

void PLCServer::padPatternUntilDistance(QVector<double> &distanceArray,
                                        QVector<double> &distanceArrayBkup,
                                        QJsonArray &distPat,
                                        QVector<double> &disPattern,
                                        QVector<double> &voltageArray,
                                        QVector<double> &voltageArrayBkup,
                                        QJsonArray &voltPat,
                                        QVector<double> &volPattern,
                                        double distanceToShow,
                                        double step)
{
    if (step <= 0.0)
        return;

    if (distanceArray.isEmpty() || voltageArray.isEmpty())
        return;

    double lastDist = distanceArray.last();
    double lastVolt = voltageArray.last();
    // qWarning() << "lastDist:" << lastDist << " lastVolt:" << lastVolt
             // << " step:" << step << " lastDist < distanceToShow:" << (lastDist < distanceToShow);
    while (lastDist < distanceToShow) {
        lastDist += step;

        distanceArray.append(lastDist);
        distanceArrayBkup.append(lastDist);
        distPat.append(lastDist);
        disPattern.append(lastDist);

        voltageArray.append(lastVolt);
        voltageArrayBkup.append(lastVolt);
        voltPat.append(lastVolt);
        volPattern.append(lastVolt);
        // qWarning() << "lastDist plus << " << lastDist;
    }
}

void PLCServer::getCsvFile(QString fileName, QString category, QString date) {
    QString bkup_name,bkup_path,bkup_date;
    QString datefile = date.left(10);    // "2025-02-18"
    QString timefile = date.mid(11, 8);  // "03:30:17"
    timefile.replace(":", "-");          // Replace ":" with "-" to match the folder format
    int size_data;
    QString Phases;
    qDebug() << "getCsvFile: category:" << category << myDatabase->selectPatterName;
    bkup_name = myDatabase->selectPatterName;
    bkup_path = myDatabase->selectPatterPath;
    bkup_date = myDatabase->datetimefile;
    // Construct the full file path based on the directory structure
    QString filePath = QString(EVENT_PATH + "%1/%2/%3/%4").arg(category).arg(datefile).arg(timefile).arg(fileName);
    if (category == "Pattern") {
        distanceArrayAPattern.clear();
        distanceArrayBPattern.clear();
        distanceArrayCPattern.clear();
        distanceArrayAPatternbkup.clear();
        distanceArrayBPatternbkup.clear();
        distanceArrayCPatternbkup.clear();
        voltageArrayAPattern.clear();
        voltageArrayBPattern.clear();
        voltageArrayCPattern.clear();
        voltageArrayAPatternbkup.clear();
        voltageArrayBPatternbkup.clear();
        voltageArrayCPatternbkup.clear();
        disAPattern.clear();
        disBPattern.clear();
        disCPattern.clear();
        volAPattern.clear();
        volBPattern.clear();
        volCPattern.clear();
        myDatabase->selectPatterPath = filePath;
        myDatabase->selectPatterName = fileName;
        myDatabase->datetimefile = date;
        myDatabase->updateSelectPattern(filePath, fileName, date);
        qDebug() << "getCsvFile: category: Pattern==" << category << myDatabase->selectPatterName;
    }
    else{
        distA = QJsonArray();
        voltA = QJsonArray();
        distB = QJsonArray();
        voltB = QJsonArray();
        distC = QJsonArray();
        voltC = QJsonArray();
        distAPat = QJsonArray();
        voltAPat = QJsonArray();
        distBPat = QJsonArray();
        voltBPat = QJsonArray();
        distCPat = QJsonArray();
        voltCPat = QJsonArray();
        qDebug() << "getCsvFile: category: Pattern!=" << category;

        QString eventDate;
        QString eventTime;
        extractEventDateTimeFromText(filePath, eventDate, eventTime);

        const QString verifiedCsvPath = resolveExistingEventCsvPath(filePath,
                                                                    fileName,
                                                                    category,
                                                                    eventDate,
                                                                    eventTime);
        if (verifiedCsvPath.isEmpty()) {
            qWarning() << "[getCsvFile] event CSV does not exist/verify; block sendMail and read:"
                       << filePath;
            return;
        }
        filePath = verifiedCsvPath;

        // Historical event notification: recover the picture from the same
        // event date/time.  Missing picture must not be replaced by a global
        // path from another event. Reading the CSV itself still remains valid.
        const QString verifiedPicPath = resolveExistingPicturePath(QString(),
                                                                    QString(),
                                                                    eventDate,
                                                                    eventTime);
        if (!verifiedPicPath.isEmpty()) {
            // Historical notifications obey the same mandatory contract: both
            // Event CSV and Picture must pass two physical/stability checks.
            FileVerifySnapshot csvHist1;
            FileVerifySnapshot picHist1;
            const bool csvHist1Ok = captureVerifiedEventCsvSnapshot(verifiedCsvPath,
                                                                     EVENT_PATH,
                                                                     category,
                                                                     eventDate,
                                                                     eventTime,
                                                                     &csvHist1);
            const bool picHist1Ok = captureVerifiedPictureSnapshot(verifiedPicPath,
                                                                    PIC_PATH,
                                                                    eventDate,
                                                                    eventTime,
                                                                    &picHist1);
            bool historicalReady = csvHist1Ok && picHist1Ok;

            FileVerifySnapshot csvHist2;
            FileVerifySnapshot picHist2;
            if (historicalReady) {
                QThread::msleep(400);
                historicalReady = captureVerifiedEventCsvSnapshot(csvHist1.path,
                                                                   EVENT_PATH,
                                                                   category,
                                                                   eventDate,
                                                                   eventTime,
                                                                   &csvHist2) &&
                                  captureVerifiedPictureSnapshot(picHist1.path,
                                                                 PIC_PATH,
                                                                 eventDate,
                                                                 eventTime,
                                                                 &picHist2) &&
                                  snapshotsAreStable(csvHist1, csvHist2) &&
                                  snapshotsAreStable(picHist1, picHist2);
            }

            if (historicalReady) {
                QString patternPath;
                QString patternName;
                FileVerifySnapshot patternHist;
                if (captureOptionalPatternSnapshot(myDatabase ? myDatabase->selectPatterPath : QString(),
                                                   PATTERN_PATH,
                                                   &patternHist)) {
                    patternPath = patternHist.path;
                    patternName = QFileInfo(patternHist.path).fileName();
                }

                QJsonObject Param;
                Param.insert("objectName", "sendMail");
                Param.insert("CSVPATH", csvHist2.path);
                Param.insert("CSVname", QFileInfo(csvHist2.path).fileName());
                Param.insert("PicPATH", picHist2.path);
                Param.insert("Picname", QFileInfo(picHist2.path).fileName());
                Param.insert("CSVPatternPATH", patternPath);
                Param.insert("CSVPatternname", patternName);
                const QString raw_datas = QString::fromUtf8(
                    QJsonDocument(Param).toJson(QJsonDocument::Compact));
                qDebug() << "[getCsvFile][SENDMAIL] verified historical event payload:" << raw_datas;
                emit sendToMonitor(raw_datas);
                emit sendToVNC(raw_datas);
            } else {
                qWarning() << "[getCsvFile][EVENT-GATE][BLOCK] historical mandatory files failed second verification:"
                           << "csv=" << verifiedCsvPath << "pic=" << verifiedPicPath;
            }
        } else {
            qWarning() << "[getCsvFile] picture for event is not physically verified;"
                       << "skip sendMail but continue CSV read:"
                       << "csv=" << verifiedCsvPath
                       << "date=" << eventDate
                       << "time=" << eventTime;
        }
    }
    qDebug() << "select filePath:" << filePath << fileName;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning("Unable to open file for reading.");
        return;
    }

    QTextStream in(&file);

    // ข้ามบรรทัดแรก (Header)
    if (!in.atEnd()) {
        in.readLine();
        qDebug() << "!in.atEnd()";
    }
    else{
        qDebug() << "in.atEnd()";
        myDatabase->selectPatterName = bkup_name;
        myDatabase->selectPatterPath = bkup_path;
        myDatabase->datetimefile = bkup_date;
        myDatabase->updateSelectPattern(myDatabase->selectPatterPath, myDatabase->selectPatterName, myDatabase->datetimefile);
    }

    while (!in.atEnd()) {
        QString line = in.readLine();
        QStringList values = line.split(",");
        qDebug() << "values::" << values << " size::" << values.size();
        if (category == "Pattern") {
                    if (values.size() > 0) {  // ตรวจสอบว่ามีข้อมูล
                        if (values.size() > 0 && !values[0].isEmpty()) {
                            distanceArrayAPattern.append(values[0].toDouble());
                            distanceArrayAPatternbkup.append(values[0].toDouble());
                            distAPat.append(values[0].toDouble());
                            disAPattern.append(values[0].toDouble());
                        }
                        if (values.size() > 1 && !values[1].isEmpty()) {
                            voltageArrayAPattern.append(values[1].toDouble());
                            voltageArrayAPatternbkup.append(values[1].toDouble());
                            voltAPat.append(values[1].toDouble());
                            volAPattern.append(values[1].toDouble());
                        }

                        if (values.size() > 2 && !values[2].isEmpty()) {
                            distanceArrayBPattern.append(values[2].toDouble());
                            distanceArrayBPatternbkup.append(values[2].toDouble());
                            distBPat.append(values[2].toDouble());
                            disBPattern.append(values[2].toDouble());
                        }

                        if (values.size() > 3 && !values[3].isEmpty()) {
                            voltageArrayBPattern.append(values[3].toDouble());
                            voltageArrayBPatternbkup.append(values[3].toDouble());
                            voltBPat.append(values[3].toDouble());
                            volBPattern.append(values[3].toDouble());
                        }

                        if (values.size() > 4 && !values[4].isEmpty()) {
                            distanceArrayCPattern.append(values[4].toDouble());
                            distanceArrayCPatternbkup.append(values[4].toDouble());
                            distCPat.append(values[4].toDouble());
                            disCPattern.append(values[4].toDouble());
                        }
                        if (values.size() > 5 && !values[5].isEmpty()) {
                            voltageArrayCPattern.append(values[5].toDouble());
                            voltageArrayCPatternbkup.append(values[5].toDouble());
                            voltCPat.append(values[5].toDouble());
                            volCPattern.append(values[5].toDouble());
                        }
                    }
                }
        else {
            // qDebug() << "values.size() > 0" << (values.size() > 0) << " values.size():" << values.size();
            // qDebug() << "values::" << values << " size::" << values.size() << " values[0].size()" << values[0].size() << " values[0].toUtf8()" << values[0].toUtf8();
            if (values.size() > 0) {  // ตรวจสอบว่ามีข้อมูล
                if (values.size() > 0 && !values[0].isEmpty()) distA.append(values[0].toDouble());
                // qDebug() << "values0::" << values[0].toDouble();
                if (values.size() > 1 && !values[1].isEmpty()) voltA.append(values[1].toDouble());
                // qDebug() << "values1::" << values[1].toDouble();

                if (values.size() > 2 && !values[2].isEmpty()) distB.append(values[2].toDouble());
                // qDebug() << "values2::" << values[2].toDouble();
                if (values.size() > 3 && !values[3].isEmpty()) voltB.append(values[3].toDouble());
                // qDebug() << "values3::" << values[3].toDouble();

                if (values.size() > 4 && !values[4].isEmpty()) distC.append(values[4].toDouble());
                // qDebug() << "values4::" << values[4].toDouble();
                if (values.size() > 5 && !values[5].isEmpty()) voltC.append(values[5].toDouble());
                // qDebug() << "values5::" << values[5].toDouble();
            }
        }
    }
    file.close();

    // if(category == "Pattern"){
    //     double step = (samplingRate * sagFactor) / 1000;

    //     // qWarning() << "category:" << category << " step << " << step;
    //     padPatternUntilDistance(distanceArrayAPattern,
    //                             distanceArrayAPatternbkup,
    //                             distAPat,
    //                             disAPattern,
    //                             voltageArrayAPattern,
    //                             voltageArrayAPatternbkup,
    //                             voltAPat,
    //                             volAPattern,
    //                             distanceToShow,
    //                             step);

    //     padPatternUntilDistance(distanceArrayBPattern,
    //                             distanceArrayBPatternbkup,
    //                             distBPat,
    //                             disBPattern,
    //                             voltageArrayBPattern,
    //                             voltageArrayBPatternbkup,
    //                             voltBPat,
    //                             volBPattern,
    //                             distanceToShow,
    //                             step);

    //     padPatternUntilDistance(distanceArrayCPattern,
    //                             distanceArrayCPatternbkup,
    //                             distCPat,
    //                             disCPattern,
    //                             voltageArrayCPattern,
    //                             voltageArrayCPatternbkup,
    //                             voltCPat,
    //                             volCPattern,
    //                             distanceToShow,
    //                             step);

    // }
    // double val  = 60.0 * sagFactor;

    // if(distA < distanceToShow){
    //     distanceToShow
    // }

    if (category == "Pattern") {
        patternSelected = true;
        QJsonObject mainObject;
        mainObject.insert("objectName", "patternA");
        mainObject.insert("distance", distAPat);
        mainObject.insert("voltage", voltAPat);
        mainObject.insert("fileName", myDatabase->selectPatterName);

        // qWarning() << "patternA SIZE:" << distAPat.size() << voltAPat.size();
        qDebug() << "Starting patternA.";
        QJsonDocument jsonDoc(mainObject);
        QString raw_data = jsonDoc.toJson(QJsonDocument::Compact);

        // ส่งข้อมูลผ่าน Signal
        emit sendToMonitor(raw_data);
        emit sendToVNC(raw_data);

        distA = QJsonArray();
        voltA = QJsonArray();

        mainObject = QJsonObject();
        mainObject.insert("objectName", "patternB");
        mainObject.insert("distance", distBPat);
        mainObject.insert("voltage", voltBPat);
        mainObject.insert("fileName", myDatabase->selectPatterName);

        qDebug() << "Starting patternB.";
        //        jsonDoc = QJsonDocument();
        // qWarning() << "patternB SIZE:" << distBPat.size() << voltBPat.size();
        jsonDoc = QJsonDocument(mainObject);
        raw_data = jsonDoc.toJson(QJsonDocument::Compact);

        // ส่งข้อมูลผ่าน Signal
        emit sendToMonitor(raw_data);
        emit sendToVNC(raw_data);

        distB = QJsonArray();
        voltB = QJsonArray();

        mainObject = QJsonObject();
        mainObject.insert("objectName", "patternC");
        mainObject.insert("distance", distCPat);
        mainObject.insert("voltage", voltCPat);
        mainObject.insert("fileName", myDatabase->selectPatterName);

        qDebug() << "Starting patternC.";
        //        jsonDoc = QJsonDocument();
        // qWarning() << "patternC SIZE:" << distCPat.size() << voltCPat.size();
        jsonDoc = QJsonDocument(mainObject);
        raw_data = jsonDoc.toJson(QJsonDocument::Compact);

        // ส่งข้อมูลผ่าน Signal
        emit sendToMonitor(raw_data);
        emit sendToVNC(raw_data);
        qDebug() << "getCsvMarginFile(fileName,category,date); :::" << category;
        getCsvMarginFile(myDatabase->selectPatterName,category,myDatabase->datetimefile);
    } else {
        QJsonObject mainObject;
        mainObject.insert("objectName", "dataPlotingA");
        mainObject.insert("distance", distA);
        mainObject.insert("voltage", voltA);
        mainObject.insert("fileName", fileName);

        qDebug() << "Starting DebugA4.";
        QJsonDocument jsonDoc(mainObject);
        QString raw_data = jsonDoc.toJson(QJsonDocument::Compact);

        // ส่งข้อมูลผ่าน Signal
        emit sendToMonitor(raw_data);
        emit sendToVNC(raw_data);
        calFLF(distA, voltA, "A");

        distA = QJsonArray();
        voltA = QJsonArray();

        mainObject = QJsonObject();
        mainObject.insert("objectName", "dataPlotingB");
        mainObject.insert("distance", distB);
        mainObject.insert("voltage", voltB);
        mainObject.insert("fileName", fileName);

        qDebug() << "Starting DebugA4.";
        //        jsonDoc = QJsonDocument();

        jsonDoc = QJsonDocument(mainObject);
        raw_data = jsonDoc.toJson(QJsonDocument::Compact);

        // ส่งข้อมูลผ่าน Signal
        emit sendToMonitor(raw_data);
        emit sendToVNC(raw_data);
        calFLF(distB, voltB, "B");

        distB = QJsonArray();
        voltB = QJsonArray();

        mainObject = QJsonObject();
        mainObject.insert("objectName", "dataPlotingC");
        mainObject.insert("distance", distC);
        mainObject.insert("voltage", voltC);
        mainObject.insert("fileName", fileName);

        qDebug() << "Starting DebugA4.";
        //        jsonDoc = QJsonDocument();

        jsonDoc = QJsonDocument(mainObject);
        raw_data = jsonDoc.toJson(QJsonDocument::Compact);

        // ส่งข้อมูลผ่าน Signal
        emit sendToMonitor(raw_data);
        emit sendToVNC(raw_data);
        calFLF(distC, voltC, "C");

        // getCsvMarginFile(myDatabase->selectPatterName,"Pattern",myDatabase->datetimefile);
    }

    distA = QJsonArray();
    voltA = QJsonArray();
    distB = QJsonArray();
    voltB = QJsonArray();
    distC = QJsonArray();
    voltC = QJsonArray();
    distAPat = QJsonArray();
    voltAPat = QJsonArray();
    distBPat = QJsonArray();
    voltBPat = QJsonArray();
    distCPat = QJsonArray();
    voltCPat = QJsonArray();
}

void PLCServer::sendSocketThreeDevice() {
    if (Aux->fails == true) {
        if (OpenPLC_param->fn_fail()) {
            QString datetime = getChrrentDateTime();
            OpenPLC_param->LFL_FAIL = true;
            PLCserver_param->LFL_FAIL = true;
            QJsonDocument jsonDoc;
            QJsonObject Param;
            QJsonArray Reg;
            QString raw_datas;
            if (language == 0) {
                Param.insert("objectName", "writeCoilsFail");
                Param.insert("state", true);
                jsonDoc.setObject(Param);
                raw_datas = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                // OpenPLC_address
                if (INPUT_PLC_address->state() == QAbstractSocket::ConnectedState)
                    emit sendMessage(raw_datas, INPUT_PLC_address);
                else
                    qDebug() << "INPUT_PLC_address:" << INPUT_PLC_address->state();
            } else if (language == 1) {
                Reg.append(1);
                Param.insert("objectName", "writeCoils");
                Param.insert("index", 800);
                Param.insert("register", Reg);
                jsonDoc.setObject(Param);
                raw_datas = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                // OpenPLC_address
                if (OpenPLC_address->state() == QAbstractSocket::ConnectedState)
                    emit sendMessage(raw_datas, OpenPLC_address);
                else
                    qDebug() << "OpenPLC_address:" << OpenPLC_address->state();
            }

            QJsonObject().swap(Param);
            // qDebug() << "isEmpty:" << Param.isEmpty();
            Param.insert("TrapsAlert", "LFL_FAIL");
            Param.insert("state", OpenPLC_param->LFL_FAIL);
            Param.insert("time", datetime);
            jsonDoc.setObject(Param);
            raw_datas = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
            Q_FOREACH (QWebSocket* pClient, Monitor_address) {
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
            eventHistory->setEvent("LFL_FAIL", datetime, OpenPLC_param->LFL_FAIL);
        } else {
            QString datetime = getChrrentDateTime();

            OpenPLC_param->LFL_FAIL = false;
            PLCserver_param->LFL_FAIL = false;
            QJsonDocument jsonDoc;
            QJsonObject Param;
            QJsonArray Reg;
            QString raw_datas;
            if (language == 0) {
                Param.insert("objectName", "writeCoilsFail");
                Param.insert("state", false);
                jsonDoc.setObject(Param);
                raw_datas = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                // OpenPLC_address
                if (INPUT_PLC_address->state() == QAbstractSocket::ConnectedState)
                    emit sendMessage(raw_datas, INPUT_PLC_address);
                else
                    qDebug() << "INPUT_PLC_address:" << INPUT_PLC_address->state();
            } else if (language == 1) {
                Reg.append(0);
                Param.insert("objectName", "writeCoils");
                Param.insert("index", 800);
                Param.insert("register", Reg);
                jsonDoc.setObject(Param);
                raw_datas = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                // OpenPLC_address
                if (OpenPLC_address->state() == QAbstractSocket::ConnectedState)
                    emit sendMessage(raw_datas, OpenPLC_address);
                else
                    qDebug() << "OpenPLC_address:" << OpenPLC_address->state();
            }

            QJsonObject().swap(Param);
            Param.insert("TrapsAlert", "LFL_FAIL");
            Param.insert("state", OpenPLC_param->LFL_FAIL);
            Param.insert("time", datetime);
            jsonDoc.setObject(Param);
            raw_datas = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
            Q_FOREACH (QWebSocket* pClient, Monitor_address) {
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
            eventHistory->setEvent("LFL_FAIL", datetime, OpenPLC_param->LFL_FAIL);
        }
        OpenPLC_param->printinfo();
    }
}

void PLCServer::sendDIOActive() {
    QString currentTime = QTime::currentTime().toString("hh:mm:ss:zzz");
    QString currentDate = QDate::currentDate().toString("dd/MM/yyyy");

    OpenPLC_param->PLC_DO_ERROR = true;
    PLCserver_param->PLC_DO_ERROR = true;
    OpenPLC_param->PLC_DI_ERROR = true;
    PLCserver_param->PLC_DI_ERROR = true;
    QJsonDocument jsonDoc;
    QJsonObject Param;
    Param.insert("TrapsAlert", "PLC_DO_ERROR");
    Param.insert("state", OpenPLC_param->PLC_DO_ERROR);
    Param.insert("time", currentDate + " " + currentTime);
    jsonDoc.setObject(Param);
    QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
    Q_FOREACH (QWebSocket* pClient, Monitor_address) {
        if (pClient->state() == QAbstractSocket::ConnectedState)
            emit sendMessage(raw_data, pClient);
        else
            qDebug() << "Monitor_address:" << pClient->state();
    }
    emit sendToVNC(raw_data);
    if (snmp_address->state() == QAbstractSocket::ConnectedState)
        emit sendMessage(raw_data, snmp_address);
    else
        qDebug() << "snmp_address:" << snmp_address->state();

    QJsonObject().swap(Param);
    Param.insert("TrapsAlert", "PLC_DI_ERROR");
    Param.insert("state", OpenPLC_param->PLC_DI_ERROR);
    Param.insert("time", currentDate + " " + currentTime);
    jsonDoc.setObject(Param);
    raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
    Q_FOREACH (QWebSocket* pClient, Monitor_address) {
        if (pClient->state() == QAbstractSocket::ConnectedState)
            emit sendMessage(raw_data, pClient);
        else
            qDebug() << "Monitor_address:" << pClient->state();
    }
    emit sendToVNC(raw_data);
    if (snmp_address->state() == QAbstractSocket::ConnectedState)
        emit sendMessage(raw_data, snmp_address);
    else
        qDebug() << "snmp_address:" << snmp_address->state();
}

void PLCServer::sendDIOInctive() {
    QString currentTime = QTime::currentTime().toString("hh:mm:ss:zzz");
    QString currentDate = QDate::currentDate().toString("dd/MM/yyyy");

    OpenPLC_param->PLC_DO_ERROR = false;
    PLCserver_param->PLC_DO_ERROR = false;
    OpenPLC_param->PLC_DI_ERROR = false;
    PLCserver_param->PLC_DI_ERROR = false;
    // qDebug() << "raw_data OpenPLC_address";
    QJsonDocument jsonDoc;
    QJsonObject Param;
    Param.insert("TrapsAlert", "PLC_DO_ERROR");
    Param.insert("state", OpenPLC_param->PLC_DO_ERROR);
    Param.insert("time", currentDate + " " + currentTime);
    jsonDoc.setObject(Param);
    QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
    Q_FOREACH (QWebSocket* pClient, Monitor_address) {
        if (pClient->state() == QAbstractSocket::ConnectedState)
            emit sendMessage(raw_data, pClient);
        else
            qDebug() << "Monitor_address:" << pClient->state();
    }
    emit sendToVNC(raw_data);
    if (snmp_address->state() == QAbstractSocket::ConnectedState)
        emit sendMessage(raw_data, snmp_address);
    else
        qDebug() << "Monitor_address:" << snmp_address->state();

    QJsonObject().swap(Param);
    Param.insert("TrapsAlert", "PLC_DI_ERROR");
    Param.insert("state", OpenPLC_param->PLC_DI_ERROR);
    Param.insert("time", currentDate + " " + currentTime);
    jsonDoc.setObject(Param);
    raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
    Q_FOREACH (QWebSocket* pClient, Monitor_address) {
        if (pClient->state() == QAbstractSocket::ConnectedState)
            emit sendMessage(raw_data, pClient);
        else
            qDebug() << "Monitor_address:" << pClient->state();
    }
    if (snmp_address->state() == QAbstractSocket::ConnectedState)
        emit sendMessage(raw_data, snmp_address);
    else
        qDebug() << "Monitor_address:" << snmp_address->state();
}

void PLCServer::updateDelay() {
    qDebug() << "updateNetwork";
    QSettings* settings;
    const QString cfgfile = FILESETTING;
    qDebug() << "Loading configuration from:" << cfgfile;
    if (QDir::isAbsolutePath(cfgfile)) {
        settings = new QSettings(cfgfile, QSettings::IniFormat);
        settings->setValue(QString("%1/DELAY0").arg(DELAYS), delays->delay0);
        settings->setValue(QString("%1/DELAY1").arg(DELAYS), delays->delay1);
        settings->setValue(QString("%1/DELAY2").arg(DELAYS), delays->delay2);
        settings->setValue(QString("%1/DELAY3").arg(DELAYS), delays->delay3);
        settings->setValue(QString("%1/DELAY4").arg(DELAYS), delays->delay4);
        settings->setValue(QString("%1/DELAY5").arg(DELAYS), delays->delay5);
        settings->setValue(QString("%1/DELAY6").arg(DELAYS), delays->delay6);
        settings->setValue(QString("%1/DELAY7").arg(DELAYS), delays->delay7);
    } else {
        qDebug() << "Loading configuration from:" << cfgfile << " FILE NOT FOUND!";
    }
    qDebug() << "Loading configuration completed";
    delete settings;
}

void PLCServer::sendDelayToClient(QWebSocket* pClient) {
    QJsonDocument jsonDoc;
    QJsonObject Param;
    Param.insert("objectName", "PULSE_PERIOD");
    Param.insert("DELAY0", delays->delay0);
    Param.insert("DELAY1", delays->delay1);
    Param.insert("DELAY2", delays->delay2);
    Param.insert("DELAY3", delays->delay3);
    Param.insert("DELAY4", delays->delay4);
    Param.insert("DELAY5", delays->delay5);
    Param.insert("DELAY6", delays->delay6);
    Param.insert("DELAY7", delays->delay7);
    jsonDoc.setObject(Param);
    QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();

    if (pClient->state() == QAbstractSocket::ConnectedState)
        emit sendMessage(raw_data, pClient);
    else
        qDebug() << "Monitor_address:" << pClient->state();
}

void PLCServer::surgeEvent(double sagFactorInit, int PositionFromLocal, int PositionFromRemote, int resampling, int local_nanosec, int remote_nanosec, QString phase) {
    QJsonDocument jsonDoc;
    QJsonObject Param;
    QJsonArray jsonArray;

    qDebug() << "Debug surgeEvent:" << sagFactorInit << PositionFromLocal << PositionFromRemote << resampling << local_nanosec << remote_nanosec << "Phase:" << phase;
    QString filePath;
    if (phase == "A") {
        filePath = "/home/pi/Rawdata/data0.raw";
    } else if (phase == "B") {
        filePath = "/home/pi/Rawdata/data1.raw";
    } else if (phase == "C") {
        filePath = "/home/pi/Rawdata/data2.raw";
    } else {
        qDebug() << "Unknown phase:" << phase;
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Unable to open file:" << filePath;
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    std::vector<float> normalizedValues;
    std::vector<float> normalizedValuesReverse;
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(data.constData());
    const uint8_t* endPtr = ptr + data.size();

    while (ptr + 2 <= endPtr) {
        uint16_t signedValue = static_cast<uint16_t>((static_cast<uint8_t>(ptr[1]) << 8) | static_cast<uint8_t>(ptr[0]));
        if (signedValue > 65000) {
            signedValue = 0.0f;
        }
        float normalizedValue = (static_cast<float>(signedValue) / 65536.0f) * 4095;
        normalizedValues.push_back(normalizedValue);
        ptr += 2;
    }

    normalizedValuesReverse.clear();

    resampling = samplingRate / (60 * sagFactorInit);  // Resampling rate
    float timepoint = (remote_nanosec - local_nanosec) / 1e9f;
    int totalDistance = (PositionFromLocal + PositionFromRemote);
    int fullpoint = static_cast<int>(PositionFromLocal / (60 * sagFactorInit));
    int fullpointRemote = static_cast<int>(PositionFromRemote / (60 * sagFactorInit));
    int fullpointTotal = static_cast<int>(totalDistance / (60 * sagFactorInit));
    int trueDistancepoint = fullpointTotal / std::max(resampling, 1);

    fullPointLocal = fullpoint;
    fullPointRemote = fullpointRemote;

    timeOfDistance = fullpointTotal * timepoint;
    int timeOfDistanceTest = timepoint / 3.33f * 1e-6f;
    realDistanceA = (speedOfligth * (timeOfDistance) / 1000) * sagFactorInit;

    std::vector<std::pair<float, float>> result;
    currentDistanceSurge = 0;
    pointIntervalSurge = 0;
    pointIntervalSurge = static_cast<float>(totalDistance) / trueDistancepoint;

    qDebug() << "surgeEvent fullpoint:" << fullpoint << " pointIntervalSurge:" << pointIntervalSurge << " realDistanceA:" << realDistanceA << " trueDistancepoint:" << trueDistancepoint << " samplingRate:" << samplingRate << " sagFactorInit" << sagFactorInit << " PositionFromLocal" << PositionFromLocal << " PositionFromRemote:" << PositionFromRemote << " resampling:" << resampling << " totalDistance" << totalDistance << " timepoint" << timepoint << " fullpointTotal:" << fullpointTotal << " timeOfDistanceTest" << timeOfDistanceTest;

    if (pointIntervalSurge <= 0) {
        qDebug() << "pointIntervalSurge == 0";
        return;
    }

    //    qDebug() << "fullpoint data:"  << fullpoint << normalizedValues.size();

    //    if (phase == "A") {
    for (int i = 0; i < normalizedValues.size(); ++i) {
        if (i < fullpoint) {
            normalizedValuesReverse.push_back(normalizedValues[i]);
        } else {
            qDebug() << "stop count at index" << i;
            break;
        }
    }
    //    }

    std::reverse(normalizedValuesReverse.begin(), normalizedValuesReverse.end());

    //    if (phase == "A") {
    for (int i = 0; i < normalizedValues.size(); ++i) {
        if (i < fullpoint) {
            resultMaxListSurge.emplace_back(currentDistanceSurge, normalizedValuesReverse[i]);
        } else {
            qDebug() << "stop count at index" << i;
            break;
        }
        currentDistanceSurge += (60 * sagFactorInit);
    }
    //    }

    //    QJsonArray dist, volt;

    //    for (int i = 0; i < normalizedValuesReverse.size(); i++) {
    //        distBkupRemote.append(currentDistanceSurge / 1000.0);  // m → km
    //        voltBkupRemote.append(normalizedValuesReverse[i]);
    //        currentDistanceSurge += 60 * sagFactorInit;
    //    }

    qDebug() << "surgeEvent remote_link" << surge_event_check;
    if(surge_event_check){
        qDebug() << "surge_event_check remote_link" << remote_link
                 << " temp_url" << temp_url;
        // if(temp_url != ""){
            getRawDataADCRemote(temp_url);
        // }
        temp_url = "";
        surge_event_check = false;
        adc_event_check = false;
    }

    // remote_link = "";

    //    Param.insert("objectName", "dataPlotingA");
    //    Param.insert("distance", dist);
    //    Param.insert("voltage", volt);
    //    jsonDoc.setObject(Param);
    //    QString raw_data2 = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
    //    qDebug() << "sendMessage dataPlotingA";
    //    Q_FOREACH (QWebSocket *pClient, Monitor_address)
    //    {
    //        if(pClient->state() == QAbstractSocket::ConnectedState)
    //            emit sendMessage(raw_data2, pClient);
    //        else
    //            qDebug() << "Monitor_address:" << pClient->state();
    //    }

    //    if (phase == "A") {
    //        reSamplingNormalizationA(resultMaxListSurge);
    //    }
    //    if (phase == "B") {
    //        reSamplingNormalizationB(resultMaxListSurge);
    //    }
    //    if (phase == "C") {
    //        reSamplingNormalizationC(resultMaxListSurge);
    //    }

    //    resultMaxListSurge.clear();
    //    PositionFromLocal = PositionFromLocal /1000;
    //    PositionFromRemote = PositionFromRemote / 1000;
    //    QJsonObject distanceObject;
    //    distanceObject.insert("objectName", "realDistance" + phase);
    //    distanceObject.insert("valueDistance" + phase, totalDistance);
    //    QJsonDocument distanceDoc(distanceObject);
    //    emit sendToMonitor(distanceDoc.toJson(QJsonDocument::Compact));
    //    findClosestDistance(Distance,totalDistance);

    //    Param.insert("objectName","TOWER_NO");
    //    Param.insert("TransmissionLine",TowerNo[indexclosestValue]);
    //    Param.insert("FullDistance",totalDistance);
    //    Param.insert("phase",phase);
    //    jsonDoc.setObject(Param);
    //    QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();

    //    Q_FOREACH (QWebSocket *pClient, Monitor_address)
    //    {
    //        if(pClient->state() == QAbstractSocket::ConnectedState)
    //            emit sendMessage(raw_data, pClient);
    //        else
    //            qDebug() << "Monitor_address:" << pClient->state();
    //    }

    //    std::transform(normalizedValuesArr.begin(), normalizedValuesArr.end(), std::back_inserter(jsonArray),
    //                   [](float val) { return QJsonValue(val); });

    //    Param = QJsonObject();
    //    Param.insert("objectName","SurgePlot");
    //    Param.insert("data",jsonArray);
    //    Param.insert("phase",phase);
    //    jsonDoc.setObject(Param);
    //    raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
    //    emit sendToSocketPLC(raw_data);
}

void PLCServer::surgeEventRemote(double sagFactorInit, double PositionFromLocal, double PositionFromRemote, int resampling, int local_nanosec, int remote_nanosec, QString phase) {
    QJsonDocument jsonDoc;
    QJsonObject Param;
    QJsonArray jsonArray;

    qDebug() << "Debug surgeEventRemote:" << sagFactorInit << PositionFromLocal << PositionFromRemote << resampling << local_nanosec << remote_nanosec << "Phase:" << phase;
    QString filePath;
    if (phase == "A") {
        filePath = "/home/pi/Rawdata/data0.raw";
    } else if (phase == "B") {
        filePath = "/home/pi/Rawdata/data1.raw";
    } else if (phase == "C") {
        filePath = "/home/pi/Rawdata/data2.raw";
    } else {
        qDebug() << "Unknown phase:" << phase;
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Unable to open file:" << filePath;
        return;
    }

    QByteArray data = file.readAll();
    file.close();
    std::vector<float> normalizedValues;
    std::vector<float> normalizedValuesReverse;
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(data.constData());
    const uint8_t* endPtr = ptr + data.size();

    while (ptr + 2 <= endPtr) {
        uint16_t signedValue = static_cast<uint16_t>((static_cast<uint8_t>(ptr[1]) << 8) | static_cast<uint8_t>(ptr[0]));
        if (signedValue > 65000) {
            signedValue = 0.0f;
        }
        float normalizedValue = (static_cast<float>(signedValue) / 65536.0f) * 4095;
        normalizedValues.push_back(normalizedValue);
        ptr += 2;
    }

    normalizedValuesReverse.clear();

    resampling = samplingRate / (60 * sagFactorInit);  // Resampling rate
    float timepoint = (remote_nanosec - local_nanosec) / 1e9f;
    int totalDistance = (PositionFromLocal + PositionFromRemote);
    int fullpoint = static_cast<int>(PositionFromRemote / (60 * sagFactorInit));
    int fullpointRemote = static_cast<int>(PositionFromLocal / (60 * sagFactorInit));
    int fullpointTotal = static_cast<int>(totalDistance / (60 * sagFactorInit));
    int trueDistancepoint = fullpointTotal / std::max(resampling, 1);

    fullPointLocal = fullpoint;
    fullPointRemote = fullpointRemote;

    timeOfDistance = fullpointTotal * timepoint;
    int timeOfDistanceTest = timepoint / 3.33f * 1e-6f;
    realDistanceA = (speedOfligth * (timeOfDistance) / 1000) * sagFactorInit;

    std::vector<std::pair<float, float>> result;
    //    currentDistanceSurge = 0;
    pointIntervalSurge = 0;
    pointIntervalSurge = static_cast<float>(totalDistance) / trueDistancepoint;

    qDebug() << "surgeEventRemote fullpoint:" << fullpoint << " pointIntervalSurge:" << pointIntervalSurge << " realDistanceA:" << realDistanceA << " trueDistancepoint:" << trueDistancepoint << " samplingRate:" << samplingRate << " sagFactorInit" << sagFactorInit << " PositionFromLocal" << PositionFromLocal << " PositionFromRemote:" << PositionFromRemote << " resampling:" << resampling << " totalDistance" << totalDistance << " timepoint" << timepoint << " fullpointTotal:" << fullpointTotal << " timeOfDistanceTest" << timeOfDistanceTest;

    if (pointIntervalSurge <= 0) {
        qDebug() << "pointIntervalSurge == 0";
        // return;
    }

    //    qDebug() << "fullpoint data:"  << fullpoint << normalizedValues.size();

    //    if (phase == "A") {
    for (int i = 0; i < normalizedValues.size(); ++i) {
        if (i < fullpoint) {
            normalizedValuesReverse.push_back(normalizedValues[i]);
        } else {
            qDebug() << "stop count at index" << i;
            break;
        }
    }
    //    }

    //    std::reverse(normalizedValuesReverse.begin(), normalizedValuesReverse.end());

    //    if (phase == "A") {
    for (int i = 0; i < normalizedValues.size(); ++i) {
        if (i < fullpoint) {
            resultMaxListSurge.emplace_back(currentDistanceSurge, normalizedValuesReverse[i]);
        } else {
            qDebug() << "stop count at index" << i;
            break;
        }
        currentDistanceSurge += (60 * sagFactorInit);
    }

    qDebug() << "surgeEventRemote normalizedValuesReverse" << normalizedValuesReverse;
    //    }

    //    QJsonArray dist, volt;

    //    for (int i = 0; i < normalizedValuesReverse.size(); i++) {
    //        distBkupRemote.append(currentDistanceSurge / 1000.0);  // m → km
    //        voltBkupRemote.append(normalizedValuesReverse[i]);
    //        currentDistanceSurge += 60 * sagFactorInit;
    //    }

    //    Param.insert("objectName", "dataPlotingA");
    //    Param.insert("distance", distBkupRemote);
    //    Param.insert("voltage", voltBkupRemote);
    //    jsonDoc.setObject(Param);
    //    QString raw_data2 = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
    //    qDebug() << "sendMessage dataPlotingA";
    //    Q_FOREACH (QWebSocket *pClient, Monitor_address)
    //    {
    //        if(pClient->state() == QAbstractSocket::ConnectedState)
    //            emit sendMessage(raw_data2, pClient);
    //        else
    //            qDebug() << "Monitor_address:" << pClient->state();
    //    }

    QJsonObject mainObject;
    QJsonObject surgeObject;
    QJsonArray dist, volt;
    // QList<double> volts;
    // QList<float> dists;
    for (const auto& [distance, voltage] : resultMaxListSurge) {
        dist.push_back(distance / 1000);
        volt.push_back(voltage);
    }


    mainObject.insert("distance", dist);
    mainObject.insert("voltage", volt);

    surgeObject.insert("distance", dist);
    surgeObject.insert("voltage", volt);



    if (phase == "A") {
        mainObject.insert("objectName", "dataPlotingA");
        surgeObject.insert("objectName", "dataSurgeA");
        // reSamplingNormalizationA(resultMaxListSurge);
    }
    if (phase == "B") {
        mainObject.insert("objectName", "dataPlotingB");
        surgeObject.insert("objectName", "dataSurgeB");
        // reSamplingNormalizationB(resultMaxListSurge);
    }
    if (phase == "C") {
        mainObject.insert("objectName", "dataPlotingC");
        surgeObject.insert("objectName", "dataSurgeC");
        // reSamplingNormalizationC(resultMaxListSurge);
    }

    QJsonDocument jsonDocs(mainObject);
    QString raw_data = jsonDocs.toJson(QJsonDocument::Compact);

    QJsonDocument jsonDocsSurge(surgeObject);
    QString raw_dataSurge = jsonDocsSurge.toJson(QJsonDocument::Compact);

    qDebug() << "patternSelected:" << patternSelected << " MODE" << modeName;
    int firstIndex = 0;
    // if (patternSelected) {
    if (phase == "A") {
        // PositionFromLocal = static_cast<int>(obj["PositionFromLocal"].toDouble());
        // PositionFromRemote = static_cast<int>(obj["PositionFromRemote"].toDouble());

        QJsonDocument jsonDoc;
        QJsonObject Param;
        QString raw_datas;

        findClosestDistance(Distance,PositionFromLocal/1000);
        // qWarning() << "PositionFromLocal/1000 Distance[Distance.size() - 1]" << Distance[Distance.size() - 1]
        //               << " PositionFromLocal" << PositionFromLocal
        //               << " PositionFromLocal/1000" << PositionFromLocal/1000
        //               << " indexclosestValue" << indexclosestValue
        //               << " TowerNo[indexclosestValue]" << TowerNo[indexclosestValue]
        //               << " Distance[indexclosestValue]" << Distance[indexclosestValue];

        // findClosestDistance(Distance,PositionFromLocal);
        // qWarning() << "PositionFromLocal Distance[Distance.size() - 1]" << Distance[Distance.size() - 1]
        //               << " PositionFromLocal" << PositionFromLocal
        //               << " PositionFromLocal/1000" << PositionFromLocal/1000
        //               << " indexclosestValue" << indexclosestValue
        //               << " TowerNo[indexclosestValue]" << TowerNo[indexclosestValue];

        Param.insert("objectName", "TOWER_NO");
        Param.insert("TransmissionLine", TowerNo[indexclosestValue]);
        Param.insert("FullDistance", QString::number((PositionFromLocal/1000), 'f', 2).toFloat());
        Param.insert("phase", "A");
        jsonDoc.setObject(Param);
        raw_datas = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        Q_FOREACH (QWebSocket* pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(raw_datas, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(raw_datas);
        distanceA = QString::number((PositionFromLocal/1000), 'f', 2).toFloat()*1000;
        towerA = TowerNo[indexclosestValue];

        Param = QJsonObject();
        jsonDoc = QJsonDocument();
        Param.insert("objectName", "TOWER_NO");
        Param.insert("TransmissionLine", TowerNo[TowerNo.size() - 1]);
        Param.insert("FullDistance", Distance[Distance.size() - 1]);
        Param.insert("phase", "B");
        jsonDoc.setObject(Param);
        raw_datas = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();

        Q_FOREACH (QWebSocket* pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(raw_datas, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(raw_datas);
        distanceB = Distance[Distance.size() - 1]*1000;
        towerB = TowerNo[TowerNo.size() - 1];

        Param = QJsonObject();
        jsonDoc = QJsonDocument();
        Param.insert("objectName", "TOWER_NO");
        Param.insert("TransmissionLine", TowerNo[TowerNo.size() - 1]);
        Param.insert("FullDistance", Distance[Distance.size() - 1]);
        Param.insert("phase", "C");
        jsonDoc.setObject(Param);
        raw_datas = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();

        Q_FOREACH (QWebSocket* pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(raw_datas, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(raw_datas);
        distanceC = Distance[Distance.size() - 1]*1000;
        towerC = TowerNo[TowerNo.size() - 1];

        // calFLF(dist, volt, "A");
    }
    if (phase == "B") {

        QJsonDocument jsonDoc;
        QJsonObject Param;
        QString raw_datas;

        findClosestDistance(Distance,(PositionFromLocal/1000));

        Param.insert("objectName", "TOWER_NO");
        Param.insert("TransmissionLine", TowerNo[indexclosestValue]);
        Param.insert("FullDistance", QString::number((PositionFromLocal/1000), 'f', 2).toFloat());
        Param.insert("phase", "B");
        jsonDoc.setObject(Param);
        raw_datas = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        Q_FOREACH (QWebSocket* pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(raw_datas, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(raw_datas);
        distanceB = QString::number((PositionFromLocal/1000), 'f', 2).toFloat()*1000;
        towerB = TowerNo[indexclosestValue];

        Param = QJsonObject();
        jsonDoc = QJsonDocument();
        Param.insert("objectName", "TOWER_NO");
        Param.insert("TransmissionLine", TowerNo[TowerNo.size() - 1]);
        Param.insert("FullDistance", Distance[Distance.size() - 1]);
        Param.insert("phase", "A");
        jsonDoc.setObject(Param);
        raw_datas = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();

        Q_FOREACH (QWebSocket* pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(raw_datas, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(raw_datas);
        distanceA = Distance[Distance.size() - 1]*1000;
        towerA = TowerNo[TowerNo.size() - 1];

        Param = QJsonObject();
        jsonDoc = QJsonDocument();
        Param.insert("objectName", "TOWER_NO");
        Param.insert("TransmissionLine", TowerNo[TowerNo.size() - 1]);
        Param.insert("FullDistance", Distance[Distance.size() - 1]);
        Param.insert("phase", "C");
        jsonDoc.setObject(Param);
        raw_datas = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();

        Q_FOREACH (QWebSocket* pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(raw_datas, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(raw_datas);
        distanceC = Distance[Distance.size() - 1]*1000;
        towerC = TowerNo[TowerNo.size() - 1];

        // calFLF(dist, volt, "B");
    }
    if (phase == "C") {

        QJsonDocument jsonDoc;
        QJsonObject Param;
        QString raw_datas;

        findClosestDistance(Distance,(PositionFromLocal/1000));

        Param.insert("objectName", "TOWER_NO");
        Param.insert("TransmissionLine", TowerNo[indexclosestValue]);
        Param.insert("FullDistance", QString::number((PositionFromLocal/1000), 'f', 2).toFloat());
        Param.insert("phase", "C");
        jsonDoc.setObject(Param);
        raw_datas = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        Q_FOREACH (QWebSocket* pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(raw_datas, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(raw_datas);
        distanceC = QString::number((PositionFromLocal/1000), 'f', 2).toFloat()*1000;
        towerC = TowerNo[indexclosestValue];

        Param = QJsonObject();
        jsonDoc = QJsonDocument();
        Param.insert("objectName", "TOWER_NO");
        Param.insert("TransmissionLine", TowerNo[TowerNo.size() - 1]);
        Param.insert("FullDistance", Distance[Distance.size() - 1]);
        Param.insert("phase", "B");
        jsonDoc.setObject(Param);
        raw_datas = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();

        Q_FOREACH (QWebSocket* pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(raw_datas, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(raw_datas);
        distanceB = Distance[Distance.size() - 1]*1000;
        towerB = TowerNo[TowerNo.size() - 1];

        Param = QJsonObject();
        jsonDoc = QJsonDocument();
        Param.insert("objectName", "TOWER_NO");
        Param.insert("TransmissionLine", TowerNo[TowerNo.size() - 1]);
        Param.insert("FullDistance", Distance[Distance.size() - 1]);
        Param.insert("phase", "A");
        jsonDoc.setObject(Param);
        raw_datas = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();

        Q_FOREACH (QWebSocket* pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(raw_datas, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(raw_datas);
        distanceA = Distance[Distance.size() - 1]*1000;
        towerA = TowerNo[TowerNo.size() - 1];
        // calFLF(dist, volt, "C");
    }
    // }
    emit sendToMonitor(raw_dataSurge);
    emit sendToVNC(raw_dataSurge);
    // qDebug() << "surgeEventRemote raw_data" << raw_dataSurge;
    // qWarning() << "surgeEventRemote raw_data" << raw_dataSurge;
    QThread::msleep(30);
    if (phase == "A") {
        auditEventDataPhase(QStringLiteral("phaseA"), FileTimeStamp, dist, volt);
        myDatabase->SumNormalizationandUpdateDb(modeName, "phaseA", FileTimeStamp, dist, volt);
    }
    if (phase == "B") {
        auditEventDataPhase(QStringLiteral("phaseB"), FileTimeStamp, dist, volt);
        myDatabase->SumNormalizationandUpdateDb(modeName, "phaseB", FileTimeStamp, dist, volt);
    }
    if (phase == "C") {
        auditEventDataPhase(QStringLiteral("phaseC"), FileTimeStamp, dist, volt);
        myDatabase->SumNormalizationandUpdateDb(modeName, "phaseC", FileTimeStamp, dist, volt);
    }

    resultMaxListSurge.clear();
    distBkup = QJsonArray();
    voltBkup = QJsonArray();
    voltBkupRemote = QJsonArray();
    distBkupRemote = QJsonArray();
    // PositionFromLocal = PositionFromLocal / 1000;
    // PositionFromRemote = PositionFromRemote / 1000;
    // QJsonObject distanceObject;
    //    distanceObject.insert("objectName", "realDistance" + phase);
    //    distanceObject.insert("valueDistance" + phase, totalDistance/1000);
    //    QJsonDocument distanceDoc(distanceObject);
    //    emit sendToMonitor(distanceDoc.toJson(QJsonDocument::Compact));
    // findClosestDistance(Distance, totalDistance);

    // Param.insert("objectName", "TOWER_NO");
    // Param.insert("TransmissionLine", TowerNo[indexclosestValue]);
    // Param.insert("FullDistance", totalDistance / 1000);
    // Param.insert("phase", phase);
    // jsonDoc.setObject(Param);
    // raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();

    // Q_FOREACH (QWebSocket* pClient, Monitor_address) {
    //     if (pClient->state() == QAbstractSocket::ConnectedState)
    //         emit sendMessage(raw_data, pClient);
    //     else
    //         qDebug() << "Monitor_address:" << pClient->state();
    // }
}

void PLCServer::findClosestDistance(const QList<float>& distances, float realDistance) {
    auto closest = std::min_element(distances.begin(), distances.end(), [realDistance](float a, float b) { return std::abs(a - realDistance) < std::abs(b - realDistance); });

    int index = std::distance(distances.begin(), closest);
    float closestValues = *closest;
    float distanceDifference = std::abs(*closest - realDistance);
    closestValue = closestValues;
    indexclosestValue = index;
    qDebug() << "Real Distance:" << realDistance;
    qDebug() << "Closest Value:" << closestValues;
    qDebug() << "Index:" << index;
    qDebug() << "Distance Difference:" << distanceDifference;
}

void PLCServer::calFLF(QJsonArray dis, QJsonArray volt, QString text) {
    qDebug() << "textcalFLF:" << text;
    QVector<double> vectordis, vectorvolt;
    //    vectordis.reserve(dis.size());  // Optimize memory allocation
    //    vectorvolt.reserve(volt.size());  // Optimize memory allocation
    qDebug() << "Optimize memory allocation:";
    for (const auto& value : dis) {
        if (value.isDouble()) {  // Ensure the value is a number
            vectordis.append(value.toDouble());
        }
    }
    for (const auto& value : volt) {
        if (value.isDouble()) {  // Ensure the value is a number
            vectorvolt.append(value.toDouble());
        }
    }

    if (text == "A") {
        qDebug() << "convert QJsonArray to QVector<double>" << " vectordis:" << vectordis.size() << vectorvolt.size() << " distanceArrayAPattern:" << distanceArrayAPattern.size() << voltageArrayAPattern.size();

        int samplingRatesTemp = samplingRate;
        int errorLessPattern = samplingRatesTemp - samplingRatesTemp;
        int errorGreatPattern = samplingRatesTemp + samplingRatesTemp;

        int samplingRatesPlotTemp = samplingRate;
        int errorLessPlot = samplingRatesPlotTemp - samplingRatesPlotTemp;
        int errorGreatPlot = samplingRatesPlotTemp + samplingRatesPlotTemp;

        QVector<double> indexPlot, indexPattern;

        if (distanceArrayAPattern.size() < 0) {
            qDebug() << "distanceArrayAPattern is empty";
            return;
        }
        if (vectordis.size() < 0) {
            qDebug() << "vectordis is empty";
            return;
        }
        qDebug() << "distanceArrayAPattern.size():" << distanceArrayAPattern.size() << " vectordis.size():" << vectordis.size();

        int size = 0;
        if (distanceArrayAPattern.size() > vectordis.size()) {
            size = distanceArrayAPattern.size();
        } else {
            size = vectordis.size();
        }

        qDebug() << "before distanceArrayAPattern:" << size;
        for (int i = 0; i < size; i++) {
            if (i >= distanceArrayAPattern.size() || i >= vectordis.size()) {
                break;
            }

            if (i == 0) {
                int typicalIndex = 0;
                indexPattern.append(typicalIndex);
                indexPlot.append(typicalIndex);
                samplingRatesTemp += samplingRate;
                samplingRatesPlotTemp += samplingRate;
            }

            if (i < distanceArrayAPattern.size() && i > 0) {  // Fix 1
                if (distanceArrayAPattern[i] * 1000 > samplingRatesTemp) {
                    int typicalIndex = 0;
                    int n = abs(distanceArrayAPattern[i] * 1000 - samplingRatesTemp);
                    int n1 = (i > 0) ? abs(distanceArrayAPattern[i - 1] * 1000 - samplingRatesTemp) : 1000;
                    int n2 = (i > 1) ? abs(distanceArrayAPattern[i - 2] * 1000 - samplingRatesTemp) : 1000;
                    int n3 = (i > 2) ? abs(distanceArrayAPattern[i - 3] * 1000 - samplingRatesTemp) : 1000;
                    int n4 = (i > 3) ? abs(distanceArrayAPattern[i - 4] * 1000 - samplingRatesTemp) : 1000;
                    int n5 = (i > 4) ? abs(distanceArrayAPattern[i - 5] * 1000 - samplingRatesTemp) : 1000;

                    if (n < n1 && n < n2 && n < n3 && n < n4 && n < n5)
                        typicalIndex = i;
                    else if (n1 < n2 && n1 < n3 && n1 < n4 && n1 < n5)
                        typicalIndex = i - 1;
                    else if (n2 < n3 && n2 < n4 && n2 < n5)
                        typicalIndex = i - 2;
                    else if (n3 < n4 && n3 < n5)
                        typicalIndex = i - 3;
                    else if (n4 < n5)
                        typicalIndex = i - 4;
                    else
                        typicalIndex = i - 5;

                    indexPattern.append(typicalIndex);
                    samplingRatesTemp += samplingRate;
                }
            }

            if (i < vectordis.size() && i > 0) {  // Fix 2
                if (vectordis[i] * 1000 > samplingRatesPlotTemp) {
                    int typicalIndex = 0;
                    int n = abs(vectordis[i] * 1000 - samplingRatesPlotTemp);
                    int n1 = (i > 0) ? abs(vectordis[i - 1] * 1000 - samplingRatesPlotTemp) : 1000;
                    int n2 = (i > 1) ? abs(vectordis[i - 2] * 1000 - samplingRatesPlotTemp) : 1000;
                    int n3 = (i > 2) ? abs(vectordis[i - 3] * 1000 - samplingRatesPlotTemp) : 1000;
                    int n4 = (i > 3) ? abs(vectordis[i - 4] * 1000 - samplingRatesPlotTemp) : 1000;
                    int n5 = (i > 4) ? abs(vectordis[i - 5] * 1000 - samplingRatesPlotTemp) : 1000;

                    if (n < n1 && n < n2 && n < n3 && n < n4 && n < n5)
                        typicalIndex = i;
                    else if (n1 < n2 && n1 < n3 && n1 < n4 && n1 < n5)
                        typicalIndex = i - 1;
                    else if (n2 < n3 && n2 < n4 && n2 < n5)
                        typicalIndex = i - 2;
                    else if (n3 < n4 && n3 < n5)
                        typicalIndex = i - 3;
                    else if (n4 < n5)
                        typicalIndex = i - 4;
                    else
                        typicalIndex = i - 5;

                    indexPlot.append(typicalIndex);
                    samplingRatesPlotTemp += samplingRate;
                }
            }
        }

        qDebug() << "after distanceArrayAPattern:";

        int indexCompareSize = 0;
        if (indexPattern.size() > indexPlot.size()) {
            indexCompareSize = indexPattern.size();
        } else {
            indexCompareSize = indexPlot.size();
        }
        // qDebug() << "indexCompareSize:" << indexCompareSize << " indexPlot.size()" << indexPlot.size() << " indexPattern.size()" << indexPattern.size();

        int firstIndex = 0, lastIndex = 0, countIndex;
        double patternTemp,plotTemp;
        bool LFL = false;

        // qDebug() << "indexPattern:" << indexPattern << "indexPlot:" << indexPlot;

        for (int i = 0; i < indexCompareSize; i++) {
            qDebug() << "i::" << i
                     << " indexCompareSize::" << indexCompareSize;
            if (i >= indexPattern.size() || i >= indexPlot.size()) {
                qDebug() << "break;";
                break;
            }
            if (indexPattern[i] < volAPattern.size()) {
                patternTemp = volAPattern[indexPattern[i]];
            } else {
                // qDebug() << "indexPattern[i] out of range:" << indexPattern[i];
                continue;
            }

            if (indexPlot[i] < vectorvolt.size()) {
                plotTemp = vectorvolt[indexPlot[i]];
            } else {
                // qDebug() << "indexPlot[i] out of range:" << indexPlot[i];
                continue;
            }
            qDebug() << "patternTemp:" << patternTemp
                     << " plotTemp:" << plotTemp;
            if (plotTemp > patternTemp) {
                if (lastIndex != 0 && lastIndex == i - 1) {
                    if (firstIndex == 0) {
                        firstIndex = indexPlot[i];
                                           qDebug() << "indexPlot[i]:" << indexPlot[i] << " index:" << i;
                    }
                    countIndex++;
                                   qDebug() << "if:" << countIndex;
                } else {
                    firstIndex = 0;
                    countIndex = 0;
                                   qDebug() << "else:" << countIndex;
                }
                lastIndex = i;
                           // qDebug() << "lastIndex:" << lastIndex << " firstIndex:" << firstIndex << " countIndex:" << countIndex;
            }
            if (countIndex == lenghtFLF) {
                           // qDebug() << "countIndex == lenghtFLF:";
                LFL = true;
                break;
            }
        }

        // qDebug() << "find index indexPattern and indexPlot" << " indexPattern.size():" << indexPattern.size() << " indexPlot.size():" << indexPlot.size();
        qDebug() << "LFL:" << LFL;

        if (LFL) {
            findClosestDistance(Distance, distanceArrayAPattern[firstIndex]);

            QJsonDocument jsonDoc;
            QJsonObject Param;
            Param.insert("objectName", "TOWER_NO");
//            if (masterLFL == "MASTER") {
                Param.insert("TransmissionLine", TowerNo[indexclosestValue]);
                Param.insert("FullDistance", QString::number(distanceArrayAPattern[firstIndex], 'f', 2).toFloat());
//            }
//            else if (masterLFL == "SLAVE") {
//                Param.insert("TransmissionLine", TowerNo[indexclosestValue]);
//                float dis = Distance[(Distance.size() - 1)] - Distance[indexclosestValue];
//                Param.insert("FullDistance", QString::number(dis, 'f', 2).toFloat());
//            }
            Param.insert("phase", "A");
            jsonDoc.setObject(Param);
            QString raw_datas = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();

            Q_FOREACH (QWebSocket* pClient, Monitor_address) {
                if (pClient->state() == QAbstractSocket::ConnectedState)
                    emit sendMessage(raw_datas, pClient);
                else
                    qDebug() << "Monitor_address:" << pClient->state();
            }
            emit sendToVNC(raw_datas);
            distanceA = QString::number(distanceArrayAPattern[firstIndex], 'f', 2).toFloat()*1000;
            qDebug() << "distanceA:" << distanceA << " QString::number(distanceArrayAPattern[firstIndex], 'f', 2).toFloat()" << QString::number(distanceArrayAPattern[firstIndex], 'f', 2).toFloat();
            towerA = TowerNo[indexclosestValue];
        } else {
            QJsonDocument jsonDoc;
            QJsonObject Param;
            Param.insert("objectName", "TOWER_NO");
            Param.insert("TransmissionLine", TowerNo[TowerNo.size() - 1]);
            Param.insert("FullDistance", fulldistance);
            // Param.insert("FullDistance", Distance[Distance.size() - 1]);
            Param.insert("phase", "A");
            jsonDoc.setObject(Param);
            QString raw_datas = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();

            Q_FOREACH (QWebSocket* pClient, Monitor_address) {
                if (pClient->state() == QAbstractSocket::ConnectedState)
                    emit sendMessage(raw_datas, pClient);
                else
                    qDebug() << "Monitor_address:" << pClient->state();
            }
            emit sendToVNC(raw_datas);
            distanceA = fulldistance*1000;
            towerA = TowerNo[TowerNo.size() - 1];
            // distanceA = Distance[Distance.size() - 1]*1000;
            qDebug() << "distanceA:" << distanceA << " QString::number(distanceArrayAPattern[firstIndex], 'f', 2).toFloat()" << Distance;
        }
    } else if (text == "B") {
        qDebug() << "convert QJsonArray to QVector<double>" << " vectordis:" << vectordis.size() << vectorvolt.size() << " distanceArrayBPattern:" << distanceArrayBPattern.size() << voltageArrayBPattern.size();

        int samplingRatesTemp = samplingRate;
        int errorLessPattern = samplingRatesTemp - samplingRatesTemp;
        int errorGreatPattern = samplingRatesTemp + samplingRatesTemp;

        int samplingRatesPlotTemp = samplingRate;
        int errorLessPlot = samplingRatesPlotTemp - samplingRatesPlotTemp;
        int errorGreatPlot = samplingRatesPlotTemp + samplingRatesPlotTemp;

        QVector<double> indexPlot, indexPattern;

        if (distanceArrayBPattern.size() < 0) {
            qDebug() << "distanceArrayBPattern is empty";
            return;
        }
        if (vectordis.size() < 0) {
            qDebug() << "vectordis is empty";
            return;
        }
        qDebug() << "distanceArrayBPattern.size():" << distanceArrayBPattern.size() << " vectordis.size():" << vectordis.size();

        int size = 0;
        if (distanceArrayBPattern.size() > vectordis.size()) {
            size = distanceArrayBPattern.size();
        } else {
            size = vectordis.size();
        }

        qDebug() << "before distanceArrayBPattern:" << size;
        for (int i = 0; i < size; i++) {
            if (i >= distanceArrayBPattern.size() || i >= vectordis.size()) {
                break;
            }

            if (i == 0) {
                int typicalIndex = 0;
                indexPattern.append(typicalIndex);
                indexPlot.append(typicalIndex);
                samplingRatesTemp += samplingRate;
                samplingRatesPlotTemp += samplingRate;
            }

            if (i < distanceArrayBPattern.size() && i > 0) {  // Fix 1
                if (distanceArrayBPattern[i] * 1000 > samplingRatesTemp) {
                    int typicalIndex = 0;
                    int n = abs(distanceArrayBPattern[i] * 1000 - samplingRatesTemp);
                    int n1 = (i > 0) ? abs(distanceArrayBPattern[i - 1] * 1000 - samplingRatesTemp) : 1000;
                    int n2 = (i > 1) ? abs(distanceArrayBPattern[i - 2] * 1000 - samplingRatesTemp) : 1000;
                    int n3 = (i > 2) ? abs(distanceArrayBPattern[i - 3] * 1000 - samplingRatesTemp) : 1000;
                    int n4 = (i > 3) ? abs(distanceArrayBPattern[i - 4] * 1000 - samplingRatesTemp) : 1000;
                    int n5 = (i > 4) ? abs(distanceArrayBPattern[i - 5] * 1000 - samplingRatesTemp) : 1000;

                    if (n < n1 && n < n2 && n < n3 && n < n4 && n < n5)
                        typicalIndex = i;
                    else if (n1 < n2 && n1 < n3 && n1 < n4 && n1 < n5)
                        typicalIndex = i - 1;
                    else if (n2 < n3 && n2 < n4 && n2 < n5)
                        typicalIndex = i - 2;
                    else if (n3 < n4 && n3 < n5)
                        typicalIndex = i - 3;
                    else if (n4 < n5)
                        typicalIndex = i - 4;
                    else
                        typicalIndex = i - 5;

                    indexPattern.append(typicalIndex);
                    samplingRatesTemp += samplingRate;
                }
            }

            if (i < vectordis.size() && i > 0) {  // Fix 2
                if (vectordis[i] * 1000 > samplingRatesPlotTemp) {
                    int typicalIndex = 0;
                    int n = abs(vectordis[i] * 1000 - samplingRatesPlotTemp);
                    int n1 = (i > 0) ? abs(vectordis[i - 1] * 1000 - samplingRatesPlotTemp) : 1000;
                    int n2 = (i > 1) ? abs(vectordis[i - 2] * 1000 - samplingRatesPlotTemp) : 1000;
                    int n3 = (i > 2) ? abs(vectordis[i - 3] * 1000 - samplingRatesPlotTemp) : 1000;
                    int n4 = (i > 3) ? abs(vectordis[i - 4] * 1000 - samplingRatesPlotTemp) : 1000;
                    int n5 = (i > 4) ? abs(vectordis[i - 5] * 1000 - samplingRatesPlotTemp) : 1000;

                    if (n < n1 && n < n2 && n < n3 && n < n4 && n < n5)
                        typicalIndex = i;
                    else if (n1 < n2 && n1 < n3 && n1 < n4 && n1 < n5)
                        typicalIndex = i - 1;
                    else if (n2 < n3 && n2 < n4 && n2 < n5)
                        typicalIndex = i - 2;
                    else if (n3 < n4 && n3 < n5)
                        typicalIndex = i - 3;
                    else if (n4 < n5)
                        typicalIndex = i - 4;
                    else
                        typicalIndex = i - 5;

                    indexPlot.append(typicalIndex);
                    samplingRatesPlotTemp += samplingRate;
                }
            }
        }

        qDebug() << "after distanceArrayBPattern:";

        int indexCompareSize = 0;
        if (indexPattern.size() > indexPlot.size()) {
            indexCompareSize = indexPattern.size();
        } else {
            indexCompareSize = indexPlot.size();
        }
        // qDebug() << "indexCompareSize:" << indexCompareSize << " indexPlot.size()" << indexPlot.size() << " indexPattern.size()" << indexPattern.size();

        int patternTemp, plotTemp, firstIndex = 0, lastIndex = 0, countIndex;
        bool LFL = false;

        // qDebug() << "indexPattern:" << indexPattern << "indexPlot:" << indexPlot;

        for (int i = 0; i < indexCompareSize; i++) {
            if (i >= indexPattern.size() || i >= indexPlot.size()) {
                break;
            }

            if (indexPattern[i] < volBPattern.size()) {
                patternTemp = volBPattern[indexPattern[i]];
            } else {
                // qDebug() << "indexPattern[i] out of range:" << indexPattern[i];
                continue;
            }

            if (indexPlot[i] < vectorvolt.size()) {
                plotTemp = vectorvolt[indexPlot[i]];
            } else {
                // qDebug() << "indexPlot[i] out of range:" << indexPlot[i];
                continue;
            }

            if (plotTemp > patternTemp) {
                if (lastIndex != 0 && lastIndex == i - 1) {
                    if (firstIndex == 0) {
                        firstIndex = indexPlot[i];
                        //                    qDebug() << "indexPlot[i]:" << indexPlot[i] << " index:" << i;
                    }
                    countIndex++;
                    //                qDebug() << "if:" << countIndex;
                } else {
                    firstIndex = 0;
                    countIndex = 0;
                    //                qDebug() << "else:" << countIndex;
                }
                lastIndex = i;
                //            qDebug() << "lastIndex:" << lastIndex << " firstIndex:" << firstIndex << " countIndex:" << countIndex;
            }
            if (countIndex == lenghtFLF) {
                //            qDebug() << "countIndex == lenghtFLF:";
                LFL = true;
                break;
            }
        }

        // qDebug() << "find index indexPattern and indexPlot" << " indexPattern.size():" << indexPattern.size() << " indexPlot.size():" << indexPlot.size();
        // qDebug() << "LFL:" << LFL;

        if (LFL) {
            findClosestDistance(Distance, distanceArrayBPattern[firstIndex]);

            QJsonDocument jsonDoc;
            QJsonObject Param;
            Param.insert("objectName", "TOWER_NO");
//            if (masterLFL == "MASTER") {
                Param.insert("TransmissionLine", TowerNo[indexclosestValue]);
                Param.insert("FullDistance", QString::number(distanceArrayAPattern[firstIndex], 'f', 2).toFloat());
//            }
//            else if (masterLFL == "SLAVE") {
//                Param.insert("TransmissionLine", TowerNo[indexclosestValue]);
//                float dis = Distance[(Distance.size() - 1)] - Distance[indexclosestValue];
//                Param.insert("FullDistance", QString::number(dis, 'f', 2).toFloat());
//            }
            Param.insert("phase", "B");
            jsonDoc.setObject(Param);
            QString raw_datas = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();

            Q_FOREACH (QWebSocket* pClient, Monitor_address) {
                if (pClient->state() == QAbstractSocket::ConnectedState)
                    emit sendMessage(raw_datas, pClient);
                else
                    qDebug() << "Monitor_address:" << pClient->state();
            }
            emit sendToVNC(raw_datas);
            distanceB = QString::number(distanceArrayAPattern[firstIndex], 'f', 2).toFloat()*1000;
            towerB = TowerNo[indexclosestValue];
        } else {
            QJsonDocument jsonDoc;
            QJsonObject Param;
            Param.insert("objectName", "TOWER_NO");
            Param.insert("TransmissionLine", TowerNo[TowerNo.size() - 1]);
            Param.insert("FullDistance", fulldistance);
            // Param.insert("FullDistance", Distance[Distance.size() - 1]);
            Param.insert("phase", "B");
            jsonDoc.setObject(Param);
            QString raw_datas = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();

            Q_FOREACH (QWebSocket* pClient, Monitor_address) {
                if (pClient->state() == QAbstractSocket::ConnectedState)
                    emit sendMessage(raw_datas, pClient);
                else
                    qDebug() << "Monitor_address:" << pClient->state();
            }
            emit sendToVNC(raw_datas);
            distanceB = fulldistance*1000;
            // distanceB = Distance[Distance.size() - 1]*1000;
            towerB = TowerNo[TowerNo.size() - 1];
        }
    } else if (text == "C") {
        qDebug() << "convert QJsonArray to QVector<double>" << " vectordis:" << vectordis.size() << vectorvolt.size() << " distanceArrayCPattern:" << distanceArrayCPattern.size() << distanceArrayCPattern.size();

        int samplingRatesTemp = samplingRate;
        int errorLessPattern = samplingRatesTemp - samplingRatesTemp;
        int errorGreatPattern = samplingRatesTemp + samplingRatesTemp;

        int samplingRatesPlotTemp = samplingRate;
        int errorLessPlot = samplingRatesPlotTemp - samplingRatesPlotTemp;
        int errorGreatPlot = samplingRatesPlotTemp + samplingRatesPlotTemp;

        QVector<double> indexPlot, indexPattern;

        if (distanceArrayCPattern.size() < 0) {
            qDebug() << "distanceArrayCPattern is empty";
            return;
        }
        if (vectordis.size() < 0) {
            qDebug() << "vectordis is empty";
            return;
        }
        qDebug() << "distanceArrayCPattern.size():" << distanceArrayCPattern.size() << " vectordis.size():" << vectordis.size();

        int size = 0;
        if (distanceArrayCPattern.size() > vectordis.size()) {
            size = distanceArrayCPattern.size();
        } else {
            size = vectordis.size();
        }

        qDebug() << "before distanceArrayCPattern:" << size;
        for (int i = 0; i < size; i++) {
            if (i >= distanceArrayCPattern.size() || i >= vectordis.size()) {
                break;
            }

            if (i == 0) {
                int typicalIndex = 0;
                indexPattern.append(typicalIndex);
                indexPlot.append(typicalIndex);
                samplingRatesTemp += samplingRate;
                samplingRatesPlotTemp += samplingRate;
            }

            if (i < distanceArrayCPattern.size() && i > 0) {  // Fix 1
                if (distanceArrayCPattern[i] * 1000 > samplingRatesTemp) {
                    int typicalIndex = 0;
                    int n = abs(distanceArrayCPattern[i] * 1000 - samplingRatesTemp);
                    int n1 = (i > 0) ? abs(distanceArrayCPattern[i - 1] * 1000 - samplingRatesTemp) : 1000;
                    int n2 = (i > 1) ? abs(distanceArrayCPattern[i - 2] * 1000 - samplingRatesTemp) : 1000;
                    int n3 = (i > 2) ? abs(distanceArrayCPattern[i - 3] * 1000 - samplingRatesTemp) : 1000;
                    int n4 = (i > 3) ? abs(distanceArrayCPattern[i - 4] * 1000 - samplingRatesTemp) : 1000;
                    int n5 = (i > 4) ? abs(distanceArrayCPattern[i - 5] * 1000 - samplingRatesTemp) : 1000;

                    if (n < n1 && n < n2 && n < n3 && n < n4 && n < n5)
                        typicalIndex = i;
                    else if (n1 < n2 && n1 < n3 && n1 < n4 && n1 < n5)
                        typicalIndex = i - 1;
                    else if (n2 < n3 && n2 < n4 && n2 < n5)
                        typicalIndex = i - 2;
                    else if (n3 < n4 && n3 < n5)
                        typicalIndex = i - 3;
                    else if (n4 < n5)
                        typicalIndex = i - 4;
                    else
                        typicalIndex = i - 5;

                    indexPattern.append(typicalIndex);
                    samplingRatesTemp += samplingRate;
                }
            }

            if (i < vectordis.size() && i > 0) {  // Fix 2
                if (vectordis[i] * 1000 > samplingRatesPlotTemp) {
                    int typicalIndex = 0;
                    int n = abs(vectordis[i] * 1000 - samplingRatesPlotTemp);
                    int n1 = (i > 0) ? abs(vectordis[i - 1] * 1000 - samplingRatesPlotTemp) : 1000;
                    int n2 = (i > 1) ? abs(vectordis[i - 2] * 1000 - samplingRatesPlotTemp) : 1000;
                    int n3 = (i > 2) ? abs(vectordis[i - 3] * 1000 - samplingRatesPlotTemp) : 1000;
                    int n4 = (i > 3) ? abs(vectordis[i - 4] * 1000 - samplingRatesPlotTemp) : 1000;
                    int n5 = (i > 4) ? abs(vectordis[i - 5] * 1000 - samplingRatesPlotTemp) : 1000;

                    if (n < n1 && n < n2 && n < n3 && n < n4 && n < n5)
                        typicalIndex = i;
                    else if (n1 < n2 && n1 < n3 && n1 < n4 && n1 < n5)
                        typicalIndex = i - 1;
                    else if (n2 < n3 && n2 < n4 && n2 < n5)
                        typicalIndex = i - 2;
                    else if (n3 < n4 && n3 < n5)
                        typicalIndex = i - 3;
                    else if (n4 < n5)
                        typicalIndex = i - 4;
                    else
                        typicalIndex = i - 5;

                    indexPlot.append(typicalIndex);
                    samplingRatesPlotTemp += samplingRate;
                }
            }
        }
        qDebug() << "after distanceArrayCPattern:";

        int indexCompareSize = 0;
        if (indexPattern.size() > indexPlot.size()) {
            indexCompareSize = indexPattern.size();
        } else {
            indexCompareSize = indexPlot.size();
        }
        // qDebug() << "indexCompareSize:" << indexCompareSize << " indexPlot.size()" << indexPlot.size() << " indexPattern.size()" << indexPattern.size();

        int patternTemp, plotTemp, firstIndex = 0, lastIndex = 0, countIndex;
        bool LFL = false;

        qDebug() << "indexPattern:" << indexPattern << "indexPlot:" << indexPlot;

        qDebug() << "lenghtFLF:" << lenghtFLF << " countIndex" << countIndex;
        for (int i = 0; i < indexCompareSize; i++) {
            if (i >= indexPattern.size() || i >= indexPlot.size()) {
                break;
            }
            if (indexPattern[i] < volCPattern.size()) {
                patternTemp = volCPattern[indexPattern[i]];
            } else {
                // qDebug() << "indexPattern[i] out of range:" << indexPattern[i];
                continue;
            }

            if (indexPlot[i] < vectorvolt.size()) {
                plotTemp = vectorvolt[indexPlot[i]];
            } else {
                // qDebug() << "indexPlot[i] out of range:" << indexPlot[i];
                continue;
            }
            if (plotTemp > patternTemp) {
                if (lastIndex != 0 && lastIndex == i - 1) {
                    if (firstIndex == 0) {
                        firstIndex = indexPlot[i];
                                            qDebug() << "indexPlot[i]:" << indexPlot[i] << " index:" << i;
                    }
                    countIndex++;
                                    qDebug() << "if:" << countIndex;
                } else {
                    firstIndex = 0;
                    countIndex = 0;
                                    qDebug() << "else:" << countIndex;
                }
                lastIndex = i;
                            // qDebug() << "lastIndex:" << lastIndex << " firstIndex:" << firstIndex << " countIndex:" << countIndex;
            }
            if (countIndex == lenghtFLF) {
                            qDebug() << "countIndex == lenghtFLF:";
                LFL = true;
                break;
            }
        }

        // qDebug() << "find index indexPattern and indexPlot" << " indexPattern.size():" << indexPattern.size() << " indexPlot.size():" << indexPlot.size();
        // qDebug() << "LFL:" << LFL;

        if (LFL) {
            findClosestDistance(Distance, distanceArrayCPattern[firstIndex]);

            QJsonDocument jsonDoc;
            QJsonObject Param;
            Param.insert("objectName", "TOWER_NO");
//            if (masterLFL == "MASTER") {
                Param.insert("TransmissionLine", TowerNo[indexclosestValue]);
                Param.insert("FullDistance", QString::number(distanceArrayAPattern[firstIndex], 'f', 2).toFloat());
//            }
//            else if (masterLFL == "SLAVE") {
//                Param.insert("TransmissionLine", TowerNo[indexclosestValue]);
//                float dis = Distance[(Distance.size() - 1)] - Distance[indexclosestValue];
//                Param.insert("FullDistance", QString::number(dis, 'f', 2).toFloat());
//            }
            Param.insert("phase", "C");
            jsonDoc.setObject(Param);
            QString raw_datas = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();

            Q_FOREACH (QWebSocket* pClient, Monitor_address) {
                if (pClient->state() == QAbstractSocket::ConnectedState)
                    emit sendMessage(raw_datas, pClient);
                else
                    qDebug() << "Monitor_address:" << pClient->state();
            }
            emit sendToVNC(raw_datas);
            distanceC = QString::number(distanceArrayAPattern[firstIndex], 'f', 2).toFloat()*1000;
            towerC = TowerNo[indexclosestValue];
        } else {
            QJsonDocument jsonDoc;
            QJsonObject Param;
            Param.insert("objectName", "TOWER_NO");
            Param.insert("TransmissionLine", TowerNo[TowerNo.size() - 1]);
            Param.insert("FullDistance", fulldistance);
            // Param.insert("FullDistance", Distance[Distance.size() - 1]);
            Param.insert("phase", "C");
            jsonDoc.setObject(Param);
            QString raw_datas = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();

            Q_FOREACH (QWebSocket* pClient, Monitor_address) {
                if (pClient->state() == QAbstractSocket::ConnectedState)
                    emit sendMessage(raw_datas, pClient);
                else
                    qDebug() << "Monitor_address:" << pClient->state();
            }
            emit sendToVNC(raw_datas);
            distanceC = fulldistance*1000;
            // distanceC = Distance[Distance.size() - 1]*1000;
            towerC = TowerNo[TowerNo.size() - 1];
        }
    }

    //    qDebug() << "indexPattern:" << indexPattern
    //             << " indexPlot:" << indexPlot;
}

void PLCServer::patternSelectDelete(QString path, QString name, QString date){
    QJsonDocument jsonDoc;
    QJsonObject Param;
    Param.insert("objectName", "CLEAR_NAME");
    jsonDoc.setObject(Param);
    QString raw_datas = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();

    Q_FOREACH (QWebSocket* pClient, Monitor_address) {
        if (pClient->state() == QAbstractSocket::ConnectedState)
            emit sendMessage(raw_datas, pClient);
        else
            qDebug() << "Monitor_address:" << pClient->state();
    }
    emit sendToVNC(raw_datas);
}
