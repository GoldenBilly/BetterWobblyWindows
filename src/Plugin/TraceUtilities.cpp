/*
    SPDX-FileCopyrightText: 2026 va111222666 / goldenbilly / Va126

    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "BetterWobblyWindows/TraceUtilities.h"
#include "BetterWobblyWindows/WobblyWindowsConstants.h"

#include <cstddef>

#include <QByteArray>
#include <QDateTime>
#include <QFile>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace KWin::BetterWobblyWindows
{
namespace
{
constexpr qsizetype MaximumKernelMessageBytes = 900;

class OwnedFileDescriptor
{
  public:
    explicit OwnedFileDescriptor(int fileDescriptor) : _fileDescriptor(fileDescriptor) {}

    ~OwnedFileDescriptor()
    {
        if (_fileDescriptor >= 0) {
            ::close(_fileDescriptor);
        }
    }

    OwnedFileDescriptor(const OwnedFileDescriptor &) = delete;
    OwnedFileDescriptor &operator=(const OwnedFileDescriptor &) = delete;

    int Get() const
    {
        return _fileDescriptor;
    }

  private:
    int _fileDescriptor = -1;
};

bool EnvironmentFlagEnabled(const char *name)
{
    const char *value = std::getenv(name);
    return value && value[0] != '\0' && std::strcmp(value, "0") != 0;
}

int KernelMessageFileDescriptor()
{
    // Cache the result. Environment and /dev/kmsg permissions do not normally
    // change during a KWin process lifetime, and repeated failed open() calls in
    // the paint path are surprisingly expensive.
    static const OwnedFileDescriptor fileDescriptor(::open("/dev/kmsg", O_WRONLY | O_CLOEXEC | O_NONBLOCK));
    return fileDescriptor.Get();
}

bool WriteAll(int fileDescriptor, const char *data, std::size_t size)
{
    while (size > 0) {
        const ssize_t written = ::write(fileDescriptor, data, size);
        if (written > 0) {
            data += written;
            size -= static_cast<std::size_t>(written);
            continue;
        }
        if (written < 0 && errno == EINTR) {
            continue;
        }
        return false;
    }
    return true;
}

QByteArray MakeKernelMessage(const QByteArray &line, KernelLogPriority priority)
{
    QByteArray sanitized = line.left(MaximumKernelMessageBytes);
    sanitized.replace('\n', ' ');
    sanitized.replace('\r', ' ');

    QByteArray message;
    message.reserve(sanitized.size() + 32);
    message += '<';
    message += QByteArray::number(static_cast<int>(priority));
    message += ">better-wobbly: ";
    message += sanitized;
    message += '\n';
    return message;
}

void AppendTraceFileSecurely(const QString &path, const QByteArray &payload)
{
    const QByteArray nativePath = QFile::encodeName(path);
    const int fileDescriptor =
        ::open(nativePath.constData(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC | O_NOFOLLOW, S_IRUSR | S_IWUSR);
    if (fileDescriptor < 0) {
        return;
    }
    WriteAll(fileDescriptor, payload.constData(), static_cast<std::size_t>(payload.size()));
    ::close(fileDescriptor);
}
} // namespace

bool WriteKernelLogLine(const QString &line, KernelLogPriority priority)
{
    const QByteArray utf8 = line.toUtf8();
    return WriteKernelLogLine(utf8.constData(), priority);
}

bool WriteKernelLogLine(const char *line, KernelLogPriority priority)
{
    if (!line) {
        return false;
    }

    const int fileDescriptor = KernelMessageFileDescriptor();
    if (fileDescriptor < 0) {
        return false;
    }

    const QByteArray message = MakeKernelMessage(QByteArray(line), priority);
    return WriteAll(fileDescriptor, message.constData(), static_cast<std::size_t>(message.size()));
}

void WriteHardTraceLine(const char *line)
{
    static const bool enabled = EnvironmentFlagEnabled("BETTER_WOBBLY_HARDTRACE");
    if (!enabled || !line) {
        return;
    }

    QByteArray message(TraceMagic);
    message += ' ';
    message += line;
    WriteKernelLogLine(message.constData(), KernelLogPriority::Debug);

    if (!IsTraceFileOutputEnabled()) {
        return;
    }

    char temporaryPath[256];
    char runtimePath[256];
    const uid_t userId = ::getuid();
    ::snprintf(temporaryPath,
        sizeof(temporaryPath),
        "/tmp/better-wobbly-hardtrace-%u.log",
        static_cast<unsigned>(userId));
    ::snprintf(runtimePath,
        sizeof(runtimePath),
        "/run/user/%u/better-wobbly-hardtrace.log",
        static_cast<unsigned>(userId));

    message += '\n';
    AppendTraceFileSecurely(QString::fromLocal8Bit(runtimePath), message);
    AppendTraceFileSecurely(QString::fromLocal8Bit(temporaryPath), message);
}

QString GetTraceWindowId(EffectWindow *window)
{
    return QStringLiteral("0x%1").arg(reinterpret_cast<quintptr>(window), 0, 16);
}

QStringList GetBlurTracePaths()
{
    const uint userId = static_cast<uint>(::getuid());
    QStringList paths;
    paths << QStringLiteral("/tmp/better-wobbly-blur-trace-%1.log").arg(userId);
    paths << QStringLiteral("/run/user/%1/better-wobbly-blur-trace.log").arg(userId);
    return paths;
}

void WriteBlurTraceLine(const QString &line)
{
    if (!IsBlurTraceEnabled()) {
        return;
    }

    WriteKernelLogLine(line, KernelLogPriority::Debug);

    if (!IsTraceFileOutputEnabled()) {
        return;
    }

    const QByteArray stamped = (QDateTime::currentDateTime().toString(Qt::ISODateWithMs) +
        QStringLiteral(" better-wobbly: ") + line + QLatin1Char('\n'))
                                   .toUtf8();

    // Prefer the runtime directory. The /tmp fallback uses O_NOFOLLOW and mode
    // 0600 so a symlink or world-writable file cannot redirect trace output.
    const QStringList paths = GetBlurTracePaths();
    for (auto it = paths.crbegin(); it != paths.crend(); ++it) {
        AppendTraceFileSecurely(*it, stamped);
    }
}

bool IsBlurTraceEnabled()
{
    static const bool enabled = EnvironmentFlagEnabled("BETTER_WOBBLY_BLUR_TRACE");
    return enabled;
}

bool IsTraceFileOutputEnabled()
{
    static const bool enabled = EnvironmentFlagEnabled("BETTER_WOBBLY_TRACE_FILES");
    return enabled;
}

void ResetBlurTraceFiles()
{
    if (!IsTraceFileOutputEnabled()) {
        return;
    }
    for (const QString &path : GetBlurTracePaths()) {
        QFile::remove(path);
    }
}
} // namespace KWin::BetterWobblyWindows
