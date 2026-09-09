#include "CrashDiagnostics.h"

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QThread>

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
#include <ucontext.h>

namespace CrashDiagnostics {
namespace {

int g_fd = -1;
std::atomic<int> g_checkpoint {CpUnknown};
thread_local volatile sig_atomic_t g_threadCheckpoint = CpUnknown;
static_assert(std::atomic<int>::is_always_lock_free, "R13 fatal checkpoint requires lock-free int atomics");

// R16.2 event-dispatch breadcrumb. QML/JS crashes observed so far happen after
// WebSocket callbacks return, so the normal checkpoint can legitimately remain
// at WS_BINARY_RETURN. Keep a separate, nested per-thread event snapshot that
// survives until QGuiApplication::notify() returns. The fatal signal handler can
// then report the *actual Qt event being executed* at the faulting instruction.
struct QtEventSnapshot {
    uintptr_t receiver = 0;
    int type = -1;
    unsigned long long sequence = 0;
    char className[48] = {};
};
constexpr int kQtEventStackDepth = 8;
thread_local QtEventSnapshot g_qtEventStack[kQtEventStackDepth];
thread_local volatile sig_atomic_t g_qtEventDepth = 0;
std::atomic<unsigned long long> g_qtEventSequence {0};
static_assert(std::atomic<unsigned long long>::is_always_lock_free,
              "R16.2 Qt event sequence requires lock-free atomics");

alignas(16) unsigned char g_altSignalStack[64 * 1024];

qint64 monotonicMs()
{
    struct timespec ts {};
    if (::clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0;
    return static_cast<qint64>(ts.tv_sec) * 1000LL + ts.tv_nsec / 1000000LL;
}

long currentTid()
{
#ifdef SYS_gettid
    return static_cast<long>(::syscall(SYS_gettid));
#else
    return static_cast<long>(reinterpret_cast<quintptr>(QThread::currentThreadId()));
#endif
}

void writeRaw(const char *data, size_t length) noexcept
{
    if (!data || length == 0)
        return;

    const int fd = g_fd;
    if (fd >= 0) {
        const char *p = data;
        size_t left = length;
        while (left > 0) {
            const ssize_t n = ::write(fd, p, left);
            if (n > 0) {
                p += n;
                left -= static_cast<size_t>(n);
                continue;
            }
            if (n < 0 && errno == EINTR)
                continue;
            break;
        }
    }
}

void writeRawBoth(const char *data, size_t length) noexcept
{
    writeRaw(data, length);
    const char *p = data;
    size_t left = length;
    while (left > 0) {
        const ssize_t n = ::write(STDERR_FILENO, p, left);
        if (n > 0) {
            p += n;
            left -= static_cast<size_t>(n);
            continue;
        }
        if (n < 0 && errno == EINTR)
            continue;
        break;
    }
}

size_t appendLiteral(char *buf, size_t cap, size_t pos, const char *text) noexcept
{
    if (!text)
        return pos;
    while (*text && pos < cap)
        buf[pos++] = *text++;
    return pos;
}

size_t appendUnsigned(char *buf, size_t cap, size_t pos, unsigned long long value) noexcept
{
    char tmp[32];
    size_t n = 0;
    do {
        tmp[n++] = static_cast<char>('0' + (value % 10ULL));
        value /= 10ULL;
    } while (value && n < sizeof(tmp));
    while (n > 0 && pos < cap)
        buf[pos++] = tmp[--n];
    return pos;
}

size_t appendHex(char *buf, size_t cap, size_t pos, uintptr_t value) noexcept
{
    static const char digits[] = "0123456789abcdef";
    pos = appendLiteral(buf, cap, pos, "0x");
    bool emitted = false;
    for (int shift = static_cast<int>(sizeof(uintptr_t) * 8) - 4; shift >= 0; shift -= 4) {
        const unsigned nibble = static_cast<unsigned>((value >> shift) & 0xFU);
        if (nibble != 0 || emitted || shift == 0) {
            if (pos < cap)
                buf[pos++] = digits[nibble];
            emitted = true;
        }
    }
    return pos;
}

const char *qtEventTypeName(int type) noexcept
{
    // QEvent::Type numeric values are ABI-stable for the Qt 5 series used by
    // this target. Keep this switch independent from Qt object access so it is
    // safe inside the fatal signal handler.
    switch (type) {
    case 0: return "None";
    case 1: return "Timer";
    case 12: return "Paint";
    case 14: return "Resize";
    case 17: return "Show";
    case 18: return "Hide";
    case 24: return "WindowActivate";
    case 25: return "WindowDeactivate";
    case 43: return "MetaCall";
    case 52: return "DeferredDelete";
    case 68: return "ChildAdded";
    case 69: return "ChildPolished";
    case 71: return "ChildRemoved";
    case 74: return "PolishRequest";
    case 75: return "Polish";
    case 76: return "LayoutRequest";
    case 77: return "UpdateRequest";
    case 1000: return "User";
    default: return "Other";
    }
}

const char *checkpointName(int cp) noexcept
{
    switch (cp) {
    case CpBoot: return "BOOT";
    case CpEventLoop: return "EVENT_LOOP";
    case CpQmlLoad: return "QML_LOAD";
    case CpAppQuit: return "APP_QUIT";
    case CpMainCtor: return "MAIN_CTOR";
    case CpMainDtor: return "MAIN_DTOR";
    case CpMainFileUpdate: return "MAIN_FILE_UPDATE";
    case CpMainReset5G: return "MAIN_RESET_5G";
    case CpMainSendMessage: return "MAIN_SEND_MESSAGE";
    case CpMainCommand: return "MAIN_COMMAND";
    case CpMainSql: return "MAIN_SQL";
    case CpMainFftEmitBegin: return "MAIN_FFT_EMIT_BEGIN";
    case CpMainFftEmitEnd: return "MAIN_FFT_EMIT_END";
    case CpMainSmeterEmitBegin: return "MAIN_SMETER_EMIT_BEGIN";
    case CpMainSmeterEmitEnd: return "MAIN_SMETER_EMIT_END";
    case CpMainWaterfallColorEmitBegin: return "MAIN_WFCOLOR_EMIT_BEGIN";
    case CpMainWaterfallColorEmitEnd: return "MAIN_WFCOLOR_EMIT_END";
    case CpMainFrequencyEmitBegin: return "MAIN_FREQ_EMIT_BEGIN";
    case CpMainFrequencyEmitEnd: return "MAIN_FREQ_EMIT_END";
    case CpNativeSpectrumFrame: return "NATIVE_SPECTRUM_FRAME";
    case CpNativeWaterfallFrame: return "NATIVE_WATERFALL_FRAME";
    case CpNativeSpectrumPaint: return "NATIVE_SPECTRUM_PAINT";
    case CpNativeWaterfallPaint: return "NATIVE_WATERFALL_PAINT";
    case CpWsCtor: return "WS_CTOR";
    case CpWsDtor: return "WS_DTOR";
    case CpWsConnect: return "WS_CONNECT";
    case CpWsText: return "WS_TEXT";
    case CpWsBinary: return "WS_BINARY";
    case CpWsAudioSd: return "WS_AUDIO_SD";
    case CpWsAudioHd: return "WS_AUDIO_HD";
    case CpWsAudioPostPush: return "WS_AUDIO_POST_PUSH";
    case CpWsAudioBranchDone: return "WS_AUDIO_BRANCH_DONE";
    case CpWsBinaryReturn: return "WS_BINARY_RETURN";
    case CpAudioCtor: return "AUDIO_CTOR";
    case CpAudioDtor: return "AUDIO_DTOR";
    case CpAudioStart: return "AUDIO_START";
    case CpAudioStop: return "AUDIO_STOP";
    case CpAudioInit: return "AUDIO_INIT";
    case CpAudioLoop: return "AUDIO_LOOP";
    case CpAudioWait: return "AUDIO_WAIT";
    case CpAudioWrite: return "AUDIO_WRITE";
    case CpAudioRecover: return "AUDIO_RECOVER";
    case CpAudioClose: return "AUDIO_CLOSE";
    case CpAudioPush: return "AUDIO_PUSH";
    case CpAudioPushBeforeLock: return "AUDIO_PUSH_BEFORE_LOCK";
    case CpAudioPushLocked: return "AUDIO_PUSH_LOCKED";
    case CpAudioPushTrim: return "AUDIO_PUSH_TRIM";
    case CpAudioPushEnqueue: return "AUDIO_PUSH_ENQUEUE";
    case CpAudioPushWake: return "AUDIO_PUSH_WAKE";
    case CpAudioPushDone: return "AUDIO_PUSH_DONE";
    case CpChatCtor: return "CHAT_CTOR";
    case CpChatDtor: return "CHAT_DTOR";
    case CpChatNewConnection: return "CHAT_NEW_CONNECTION";
    case CpChatCommand: return "CHAT_COMMAND";
    case CpChatDisconnect: return "CHAT_DISCONNECT";
    case CpChatSendWeb: return "CHAT_SEND_WEB";
    case CpChatSendRec: return "CHAT_SEND_REC";
    case CpRecorderCtor: return "REC_CTOR";
    case CpRecorderDtor: return "REC_DTOR";
    case CpRecorderLoadConfig: return "REC_LOAD_CONFIG";
    case CpRecorderApplyConfig: return "REC_APPLY_CONFIG";
    case CpRecorderCheckAlive: return "REC_CHECK_ALIVE";
    case CpRecorderUpdate: return "REC_UPDATE";
    case CpFileWatcher: return "FILE_WATCHER";
    case CpI2cRead: return "I2C_READ";
    case CpI2cWrite: return "I2C_WRITE";
    case CpSigmaIo: return "SIGMA_IO";
    case CpNetworkAsync: return "NETWORK_ASYNC";
    case CpVolumeRotary: return "VOLUME_ROTARY";
    case CpVolumeState: return "VOLUME_STATE";
    case CpVolumeHardware: return "VOLUME_HARDWARE";
    case CpVolumeSoftware: return "VOLUME_SOFTWARE";
    case CpVolumeDrawer: return "VOLUME_DRAWER";
    default: return "UNKNOWN";
    }
}

void fatalSignalHandler(int sig, siginfo_t *info, void *context) noexcept
{
    char buf[768];
    size_t pos = 0;
    pos = appendLiteral(buf, sizeof(buf), pos, "\n[R13-FATAL] signal=");
    pos = appendUnsigned(buf, sizeof(buf), pos, static_cast<unsigned>(sig));
    pos = appendLiteral(buf, sizeof(buf), pos, " pid=");
    pos = appendUnsigned(buf, sizeof(buf), pos, static_cast<unsigned long long>(::getpid()));
#ifdef SYS_gettid
    pos = appendLiteral(buf, sizeof(buf), pos, " tid=");
    pos = appendUnsigned(buf, sizeof(buf), pos,
                         static_cast<unsigned long long>(::syscall(SYS_gettid)));
#endif
    pos = appendLiteral(buf, sizeof(buf), pos, " checkpoint=");
    const int cp = g_checkpoint.load(std::memory_order_relaxed);
    pos = appendUnsigned(buf, sizeof(buf), pos, static_cast<unsigned>(cp));
    pos = appendLiteral(buf, sizeof(buf), pos, "(");
    pos = appendLiteral(buf, sizeof(buf), pos, checkpointName(cp));
    pos = appendLiteral(buf, sizeof(buf), pos, ") thread_checkpoint=");
    const int tcp = static_cast<int>(g_threadCheckpoint);
    pos = appendUnsigned(buf, sizeof(buf), pos, static_cast<unsigned>(tcp));
    pos = appendLiteral(buf, sizeof(buf), pos, "(");
    pos = appendLiteral(buf, sizeof(buf), pos, checkpointName(tcp));
    pos = appendLiteral(buf, sizeof(buf), pos, ")");
    if (info) {
        pos = appendLiteral(buf, sizeof(buf), pos, " code=");
        pos = appendUnsigned(buf, sizeof(buf), pos,
                             static_cast<unsigned long long>(static_cast<unsigned>(info->si_code)));
        pos = appendLiteral(buf, sizeof(buf), pos, " addr=");
        pos = appendHex(buf, sizeof(buf), pos, reinterpret_cast<uintptr_t>(info->si_addr));
    }

    uintptr_t pc = 0;
    uintptr_t sp = 0;
    uintptr_t fp = 0;
    uintptr_t lr = 0;
    uintptr_t x0 = 0;
    uintptr_t x1 = 0;
    uintptr_t x2 = 0;
    uintptr_t x3 = 0;
    if (context) {
        const ucontext_t *uc = static_cast<const ucontext_t *>(context);
#if defined(__aarch64__)
        pc = static_cast<uintptr_t>(uc->uc_mcontext.pc);
        sp = static_cast<uintptr_t>(uc->uc_mcontext.sp);
        fp = static_cast<uintptr_t>(uc->uc_mcontext.regs[29]);
        lr = static_cast<uintptr_t>(uc->uc_mcontext.regs[30]);
        x0 = static_cast<uintptr_t>(uc->uc_mcontext.regs[0]);
        x1 = static_cast<uintptr_t>(uc->uc_mcontext.regs[1]);
        x2 = static_cast<uintptr_t>(uc->uc_mcontext.regs[2]);
        x3 = static_cast<uintptr_t>(uc->uc_mcontext.regs[3]);
#elif defined(__x86_64__) && defined(REG_RIP) && defined(REG_RSP)
        pc = static_cast<uintptr_t>(uc->uc_mcontext.gregs[REG_RIP]);
        sp = static_cast<uintptr_t>(uc->uc_mcontext.gregs[REG_RSP]);
#endif
    }
    if (pc != 0) {
        pos = appendLiteral(buf, sizeof(buf), pos, " pc=");
        pos = appendHex(buf, sizeof(buf), pos, pc);
    }
    if (sp != 0) {
        pos = appendLiteral(buf, sizeof(buf), pos, " sp=");
        pos = appendHex(buf, sizeof(buf), pos, sp);
    }
#if defined(__aarch64__)
    if (fp != 0) {
        pos = appendLiteral(buf, sizeof(buf), pos, " fp=");
        pos = appendHex(buf, sizeof(buf), pos, fp);
    }
    if (lr != 0) {
        pos = appendLiteral(buf, sizeof(buf), pos, " lr=");
        pos = appendHex(buf, sizeof(buf), pos, lr);
    }
    pos = appendLiteral(buf, sizeof(buf), pos, " x0=");
    pos = appendHex(buf, sizeof(buf), pos, x0);
    pos = appendLiteral(buf, sizeof(buf), pos, " x1=");
    pos = appendHex(buf, sizeof(buf), pos, x1);
    pos = appendLiteral(buf, sizeof(buf), pos, " x2=");
    pos = appendHex(buf, sizeof(buf), pos, x2);
    pos = appendLiteral(buf, sizeof(buf), pos, " x3=");
    pos = appendHex(buf, sizeof(buf), pos, x3);
#endif

    const int eventDepth = static_cast<int>(g_qtEventDepth);
    if (eventDepth > 0) {
        const int index = eventDepth <= kQtEventStackDepth
                        ? eventDepth - 1
                        : kQtEventStackDepth - 1;
        const QtEventSnapshot &snapshot = g_qtEventStack[index];
        pos = appendLiteral(buf, sizeof(buf), pos, " qt_event=");
        if (snapshot.type < 0) {
            pos = appendLiteral(buf, sizeof(buf), pos, "-");
            pos = appendUnsigned(buf, sizeof(buf), pos,
                                 static_cast<unsigned long long>(-snapshot.type));
        } else {
            pos = appendUnsigned(buf, sizeof(buf), pos,
                                 static_cast<unsigned long long>(snapshot.type));
        }
        pos = appendLiteral(buf, sizeof(buf), pos, "(");
        pos = appendLiteral(buf, sizeof(buf), pos, qtEventTypeName(snapshot.type));
        pos = appendLiteral(buf, sizeof(buf), pos, ") event_receiver=");
        pos = appendHex(buf, sizeof(buf), pos, snapshot.receiver);
        pos = appendLiteral(buf, sizeof(buf), pos, " event_class=");
        pos = appendLiteral(buf, sizeof(buf), pos, snapshot.className);
        pos = appendLiteral(buf, sizeof(buf), pos, " event_depth=");
        pos = appendUnsigned(buf, sizeof(buf), pos,
                             static_cast<unsigned long long>(eventDepth));
        pos = appendLiteral(buf, sizeof(buf), pos, " event_seq=");
        pos = appendUnsigned(buf, sizeof(buf), pos, snapshot.sequence);
    } else {
        pos = appendLiteral(buf, sizeof(buf), pos, " qt_event=idle");
    }

    pos = appendLiteral(buf, sizeof(buf), pos, "\n");
    writeRawBoth(buf, pos);

    struct sigaction def {};
    def.sa_handler = SIG_DFL;
    ::sigemptyset(&def.sa_mask);
    def.sa_flags = 0;
    (void)::sigaction(sig, &def, nullptr);
    if (::kill(::getpid(), sig) != 0)
        _exit(128 + sig);
    // The signal is blocked while its handler runs. Returning lets the queued
    // re-raised signal be delivered with SIG_DFL, preserving the normal core-
    // dump/exit semantics instead of converting the crash into a clean _exit.
    return;
}

QString readStatusValue(const QByteArray &status, const QByteArray &key)
{
    const QByteArray needle = key + ':';
    const int start = status.indexOf(needle);
    if (start < 0)
        return QStringLiteral("?");
    int end = status.indexOf('\n', start);
    if (end < 0)
        end = status.size();
    return QString::fromLatin1(status.mid(start + needle.size(), end - start - needle.size()).trimmed());
}

} // namespace

void init(const QString &path)
{
    if (g_fd >= 0)
        return;

    const QByteArray configured = qgetenv("ISCAN_DIAG_LOG");
    const QByteArray filePath = !configured.isEmpty()
        ? configured
        : (path.isEmpty() ? QByteArray("/tmp/iScanMR10-debug.log") : path.toLocal8Bit());

    g_fd = ::open(filePath.constData(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
    checkpoint(CpBoot);

    QByteArray line("\n========== iScanMR10 R13 diagnostic boot ==========" "\n");
    writeRaw(line.constData(), static_cast<size_t>(line.size()));
    event(CpBoot, "BOOT", "diagnostics initialized", nullptr, ::getpid(), 0,
          QString::fromLocal8Bit(filePath));

    // Preserve the ASLR mapping for this exact run. If a fatal handler records
    // pc=0x..., this map lets us resolve the address even for PIE/shared libs.
    QFile mapsFile(QStringLiteral("/proc/self/maps"));
    if (mapsFile.open(QIODevice::ReadOnly)) {
        const QByteArray maps = mapsFile.readAll();
        static const char mapBegin[] = "----- /proc/self/maps begin -----\n";
        static const char mapEnd[] = "----- /proc/self/maps end -----\n";
        writeRaw(mapBegin, sizeof(mapBegin) - 1);
        writeRaw(maps.constData(), static_cast<size_t>(maps.size()));
        writeRaw(mapEnd, sizeof(mapEnd) - 1);
    }
}

void shutdown()
{
    event(CpAppQuit, "BOOT", "diagnostics shutdown");
    const int fd = g_fd;
    g_fd = -1;
    if (fd >= 0)
        ::close(fd);
}

void installFatalSignalHandlers()
{
    stack_t ss {};
    ss.ss_sp = g_altSignalStack;
    ss.ss_size = sizeof(g_altSignalStack);
    ss.ss_flags = 0;
    (void)::sigaltstack(&ss, nullptr);

    struct sigaction action {};
    action.sa_sigaction = fatalSignalHandler;
    ::sigemptyset(&action.sa_mask);
    action.sa_flags = SA_SIGINFO | SA_ONSTACK;

    const int fatalSignals[] = {SIGSEGV, SIGBUS, SIGABRT, SIGFPE, SIGILL};
    for (int sig : fatalSignals)
        (void)::sigaction(sig, &action, nullptr);

    event(CpBoot, "BOOT", "fatal signal handlers installed");
}

void checkpoint(Checkpoint cp) noexcept
{
    g_threadCheckpoint = static_cast<sig_atomic_t>(cp);
    g_checkpoint.store(static_cast<int>(cp), std::memory_order_relaxed);
}

void event(Checkpoint cp,
           const char *domain,
           const char *action,
           const void *object,
           qint64 value1,
           qint64 value2,
           const QString &detail)
{
    checkpoint(cp);
    if (g_fd < 0)
        return;

    QByteArray safeDetail = detail.toUtf8();
    safeDetail.replace('\n', ' ');
    safeDetail.replace('\r', ' ');
    if (safeDetail.size() > 512)
        safeDetail.truncate(512);

    QByteArray line;
    line.reserve(900);
    line += "[R13] mono_ms=" + QByteArray::number(monotonicMs());
    line += " wall=" + QDateTime::currentDateTime().toString(Qt::ISODateWithMs).toUtf8();
    line += " pid=" + QByteArray::number(static_cast<qlonglong>(::getpid()));
    line += " tid=" + QByteArray::number(static_cast<qlonglong>(currentTid()));
    line += " cp=" + QByteArray::number(static_cast<int>(cp));
    line += "(" + QByteArray(checkpointName(static_cast<int>(cp))) + ")";
    line += " domain=" + QByteArray(domain ? domain : "?");
    line += " event=" + QByteArray(action ? action : "?");
    if (object) {
        line += " obj=0x";
        line += QByteArray::number(static_cast<qulonglong>(reinterpret_cast<quintptr>(object)), 16);
    }
    line += " v1=" + QByteArray::number(value1);
    line += " v2=" + QByteArray::number(value2);
    if (!safeDetail.isEmpty())
        line += " detail=" + safeDetail;
    line += '\n';
    writeRaw(line.constData(), static_cast<size_t>(line.size()));
}

void heartbeat(qint64 eventLoopDeltaMs)
{
    QFile statusFile(QStringLiteral("/proc/self/status"));
    QByteArray status;
    if (statusFile.open(QIODevice::ReadOnly))
        status = statusFile.readAll();

    const QString detail = QStringLiteral("VmRSS=%1 VmSize=%2 VmSwap=%3 Threads=%4 FDs=%5 loop_delta_ms=%6")
        .arg(readStatusValue(status, "VmRSS"))
        .arg(readStatusValue(status, "VmSize"))
        .arg(readStatusValue(status, "VmSwap"))
        .arg(readStatusValue(status, "Threads"))
        .arg(QDir(QStringLiteral("/proc/self/fd")).entryList(QDir::NoDotAndDotDot | QDir::AllEntries).size())
        .arg(eventLoopDeltaMs);

    event(CpEventLoop, "HEALTH", "heartbeat", nullptr, eventLoopDeltaMs, 0, detail);
}

void qtEventEnter(const void *receiver, int eventType, const char *className) noexcept
{
    int depth = static_cast<int>(g_qtEventDepth);
    int index = depth;
    if (index >= kQtEventStackDepth)
        index = kQtEventStackDepth - 1;

    QtEventSnapshot &snapshot = g_qtEventStack[index];
    snapshot.receiver = reinterpret_cast<uintptr_t>(receiver);
    snapshot.type = eventType;
    snapshot.sequence = g_qtEventSequence.fetch_add(1, std::memory_order_relaxed) + 1ULL;

    size_t i = 0;
    if (className) {
        for (; i + 1 < sizeof(snapshot.className) && className[i] != '\0'; ++i)
            snapshot.className[i] = className[i];
    }
    snapshot.className[i] = '\0';
    for (++i; i < sizeof(snapshot.className); ++i)
        snapshot.className[i] = '\0';

    if (depth < kQtEventStackDepth)
        g_qtEventDepth = static_cast<sig_atomic_t>(depth + 1);
    else
        g_qtEventDepth = static_cast<sig_atomic_t>(kQtEventStackDepth);
}

void qtEventLeave() noexcept
{
    const int depth = static_cast<int>(g_qtEventDepth);
    if (depth > 0)
        g_qtEventDepth = static_cast<sig_atomic_t>(depth - 1);
}

void signalSafeTermination(int signalNumber,
                           int senderPid,
                           unsigned senderUid,
                           int siCode) noexcept
{
    char buf[256];
    size_t pos = 0;
    pos = appendLiteral(buf, sizeof(buf), pos, "[R13-TERM] signal=");
    pos = appendUnsigned(buf, sizeof(buf), pos, static_cast<unsigned>(signalNumber));
    pos = appendLiteral(buf, sizeof(buf), pos, " sender_pid=");
    pos = appendUnsigned(buf, sizeof(buf), pos,
                         static_cast<unsigned long long>(senderPid >= 0 ? senderPid : 0));
    pos = appendLiteral(buf, sizeof(buf), pos, " sender_uid=");
    pos = appendUnsigned(buf, sizeof(buf), pos, static_cast<unsigned long long>(senderUid));
    pos = appendLiteral(buf, sizeof(buf), pos, " si_code=");
    if (siCode < 0) {
        pos = appendLiteral(buf, sizeof(buf), pos, "-");
        pos = appendUnsigned(buf, sizeof(buf), pos,
                             static_cast<unsigned long long>(-(static_cast<long long>(siCode))));
    } else {
        pos = appendUnsigned(buf, sizeof(buf), pos, static_cast<unsigned long long>(siCode));
    }
    pos = appendLiteral(buf, sizeof(buf), pos, " checkpoint=");
    pos = appendUnsigned(buf, sizeof(buf), pos,
                         static_cast<unsigned>(g_checkpoint.load(std::memory_order_relaxed)));
    pos = appendLiteral(buf, sizeof(buf), pos, " thread_checkpoint=");
    pos = appendUnsigned(buf, sizeof(buf), pos, static_cast<unsigned>(g_threadCheckpoint));
    pos = appendLiteral(buf, sizeof(buf), pos, "\n");
    writeRawBoth(buf, pos);
}

void fatalLiteral(const char *text) noexcept
{
    static const char prefix[] = "[R13-FATAL] ";
    writeRawBoth(prefix, sizeof(prefix) - 1);
    if (text)
        writeRawBoth(text, std::strlen(text));
    static const char nl[] = "\n";
    writeRawBoth(nl, sizeof(nl) - 1);
}

} // namespace CrashDiagnostics
