#include "evdev_fd_client.hpp"

#include <QByteArray>
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QtGlobal>

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {

constexpr int kHelperSocketFd = 3;

bool writeAll(int fd, const void *data, size_t len) {
    const char *p = static_cast<const char *>(data);
    while (len > 0) {
        const ssize_t n = write(fd, p, len);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        p += static_cast<size_t>(n);
        len -= static_cast<size_t>(n);
    }
    return true;
}

bool readBytes(int fd, void *data, size_t len) {
    char *p = static_cast<char *>(data);
    while (len > 0) {
        const ssize_t n = read(fd, p, len);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        if (n == 0) {
            return false;
        }
        p += static_cast<size_t>(n);
        len -= static_cast<size_t>(n);
    }
    return true;
}

bool sendPath(int fd, const QByteArray &pathUtf8) {
    const uint32_t len = static_cast<uint32_t>(pathUtf8.size());
    if (!writeAll(fd, &len, sizeof(len))) {
        return false;
    }
    return writeAll(fd, pathUtf8.constData(), pathUtf8.size());
}

bool recvFd(int sock, int &outFd) {
    outFd = -1;
    struct msghdr msg {};
    struct iovec io {};
    char cmsgbuf[CMSG_SPACE(sizeof(int))];
    char buf;
    io.iov_base = &buf;
    io.iov_len = 1;
    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);

    const ssize_t n = recvmsg(sock, &msg, 0);
    if (n < 0) {
        return false;
    }
    if (msg.msg_flags & MSG_CTRUNC) {
        return false;
    }
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    if (!cmsg || cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS) {
        return false;
    }
    std::memcpy(&outFd, CMSG_DATA(cmsg), sizeof(int));
    return outFd >= 0;
}

QString defaultHelperPath() {
    if (QCoreApplication::instance() != nullptr) {
        const QString sibling =
            QCoreApplication::applicationDirPath() + QStringLiteral("/../libexec/p2td/p2td-evdev-acquire");
        if (QFile::exists(sibling)) {
            return QFileInfo(sibling).canonicalFilePath();
        }
    }
    return QStringLiteral("/usr/libexec/p2td/p2td-evdev-acquire");
}

} // namespace

bool acquireEvdevFdsViaHelper(const QStringList &devicePaths, std::vector<int> &outFds) {
    outFds.clear();
    if (devicePaths.isEmpty()) {
        return false;
    }

    const QString helperPath = qEnvironmentVariableIsEmpty("P2TD_EVDEV_ACQUIRE")
                                   ? defaultHelperPath()
                                   : qEnvironmentVariable("P2TD_EVDEV_ACQUIRE");
    if (!QFile::exists(helperPath)) {
        return false;
    }

    int sv[2] = {-1, -1};
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        qWarning() << "evdev helper: socketpair failed:" << strerror(errno);
        return false;
    }

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addclose(&actions, sv[0]);
    posix_spawn_file_actions_adddup2(&actions, sv[1], kHelperSocketFd);
    posix_spawn_file_actions_addclose(&actions, sv[1]);

    const QByteArray helperUtf8 = helperPath.toUtf8();
    std::vector<char> helperArgv(helperUtf8.size() + 1);
    std::memcpy(helperArgv.data(), helperUtf8.constData(), static_cast<size_t>(helperUtf8.size()));
    helperArgv[helperUtf8.size()] = '\0';
    char *argv[] = {helperArgv.data(), nullptr};

    pid_t child = -1;
    const int spawnErr = posix_spawn(&child, helperUtf8.constData(), &actions, nullptr, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    close(sv[1]);

    if (spawnErr != 0) {
        qWarning() << "evdev helper: posix_spawn failed:" << strerror(spawnErr);
        close(sv[0]);
        return false;
    }

    for (const QString &path : devicePaths) {
        if (!sendPath(sv[0], path.toUtf8())) {
            qWarning() << "evdev helper: failed sending device path";
            close(sv[0]);
            waitpid(child, nullptr, 0);
            return false;
        }
    }
    const uint32_t end = 0;
    if (!writeAll(sv[0], &end, sizeof(end))) {
        close(sv[0]);
        waitpid(child, nullptr, 0);
        return false;
    }

    uint32_t fdCount = 0;
    if (!readBytes(sv[0], &fdCount, sizeof(fdCount))) {
        close(sv[0]);
        waitpid(child, nullptr, 0);
        return false;
    }

    outFds.reserve(fdCount);
    for (uint32_t i = 0; i < fdCount; ++i) {
        int fd = -1;
        if (!recvFd(sv[0], fd)) {
            for (int openFd : outFds) {
                close(openFd);
            }
            outFds.clear();
            close(sv[0]);
            waitpid(child, nullptr, 0);
            return false;
        }
        const int flags = fcntl(fd, F_GETFL, 0);
        if (flags >= 0) {
            fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }
        outFds.push_back(fd);
    }

    close(sv[0]);
    int status = 0;
    waitpid(child, &status, 0);
    if (status != 0 || outFds.empty()) {
        for (int fd : outFds) {
            close(fd);
        }
        outFds.clear();
        return false;
    }
    return true;
}
