#pragma once

#include <QString>

namespace CrashDiagnostics {

enum Checkpoint : int {
    CpUnknown = 0,
    CpBoot = 10,
    CpEventLoop = 11,
    CpQmlLoad = 12,
    CpAppQuit = 13,

    CpMainCtor = 100,
    CpMainDtor = 101,
    CpMainFileUpdate = 102,
    CpMainReset5G = 103,
    CpMainSendMessage = 104,
    CpMainCommand = 105,
    CpMainSql = 106,
    CpMainFftEmitBegin = 107,
    CpMainFftEmitEnd = 108,
    CpMainSmeterEmitBegin = 109,
    CpMainSmeterEmitEnd = 110,
    CpMainWaterfallColorEmitBegin = 111,
    CpMainWaterfallColorEmitEnd = 112,
    CpMainFrequencyEmitBegin = 113,
    CpMainFrequencyEmitEnd = 114,
    CpNativeSpectrumFrame = 115,
    CpNativeWaterfallFrame = 116,
    CpNativeSpectrumPaint = 117,
    CpNativeWaterfallPaint = 118,

    CpWsCtor = 200,
    CpWsDtor = 201,
    CpWsConnect = 202,
    CpWsText = 203,
    CpWsBinary = 204,
    CpWsAudioSd = 205,
    CpWsAudioHd = 206,
    CpWsAudioPostPush = 207,
    CpWsAudioBranchDone = 208,
    CpWsBinaryReturn = 209,

    CpAudioCtor = 300,
    CpAudioDtor = 301,
    CpAudioStart = 302,
    CpAudioStop = 303,
    CpAudioInit = 304,
    CpAudioLoop = 305,
    CpAudioWait = 306,
    CpAudioWrite = 307,
    CpAudioRecover = 308,
    CpAudioClose = 309,
    CpAudioPush = 310,
    CpAudioPushBeforeLock = 311,
    CpAudioPushLocked = 312,
    CpAudioPushTrim = 313,
    CpAudioPushEnqueue = 314,
    CpAudioPushWake = 315,
    CpAudioPushDone = 316,

    CpChatCtor = 400,
    CpChatDtor = 401,
    CpChatNewConnection = 402,
    CpChatCommand = 403,
    CpChatDisconnect = 404,
    CpChatSendWeb = 405,
    CpChatSendRec = 406,

    CpRecorderCtor = 500,
    CpRecorderDtor = 501,
    CpRecorderLoadConfig = 502,
    CpRecorderApplyConfig = 503,
    CpRecorderCheckAlive = 504,
    CpRecorderUpdate = 505,

    CpFileWatcher = 600,
    CpI2cRead = 700,
    CpI2cWrite = 701,
    CpSigmaIo = 800,
    CpNetworkAsync = 900,

    CpVolumeRotary = 1000,
    CpVolumeState = 1001,
    CpVolumeHardware = 1002,
    CpVolumeSoftware = 1003,
    CpVolumeDrawer = 1004
};

void init(const QString &path = QString());
void shutdown();
void installFatalSignalHandlers();

void checkpoint(Checkpoint cp) noexcept;
void event(Checkpoint cp,
           const char *domain,
           const char *action,
           const void *object = nullptr,
           qint64 value1 = 0,
           qint64 value2 = 0,
           const QString &detail = QString());

void heartbeat(qint64 eventLoopDeltaMs = 0);

// R16.2: tracks the innermost Qt event currently being dispatched on each
// thread. These functions are allocation-free/noexcept so DiagnosticApplication
// can bracket QGuiApplication::notify() without changing event semantics.
void qtEventEnter(const void *receiver, int eventType, const char *className) noexcept;
void qtEventLeave() noexcept;

// Safe to call from SIGTERM/SIGINT bridge: uses write() only.
// senderPid/senderUid/siCode come from siginfo_t when SA_SIGINFO is active.
void signalSafeTermination(int signalNumber,
                           int senderPid,
                           unsigned senderUid,
                           int siCode) noexcept;
// Safe to call from std::terminate() before abort().
void fatalLiteral(const char *text) noexcept;

} // namespace CrashDiagnostics
