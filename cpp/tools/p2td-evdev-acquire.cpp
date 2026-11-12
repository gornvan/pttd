// Setgid helper: open /dev/input/* and pass fds to p2td via SCM_RIGHTS (socket on fd 3).
// The main GUI binary must not be setgid — KDE StatusNotifierItem rejects that.

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace {

constexpr int kSocketFd = 3;

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

bool readAll(int fd, void *data, size_t len) {
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

bool sendFd(int sock, int fd) {
    struct msghdr msg {};
    struct iovec io {};
    char buf = '!';
    io.iov_base = &buf;
    io.iov_len = 1;
    msg.msg_iov = &io;
    msg.msg_iovlen = 1;

    char cmsgbuf[CMSG_SPACE(sizeof(int))];
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    std::memcpy(CMSG_DATA(cmsg), &fd, sizeof(int));

    return sendmsg(sock, &msg, 0) >= 0;
}

} // namespace

int main() {
    std::vector<std::string> paths;
    while (true) {
        uint32_t len = 0;
        if (!readAll(kSocketFd, &len, sizeof(len))) {
            break;
        }
        if (len == 0) {
            break;
        }
        std::string path(len, '\0');
        if (!readAll(kSocketFd, path.data(), len)) {
            return 1;
        }
        paths.push_back(std::move(path));
    }

    std::vector<int> opened;
    opened.reserve(paths.size());
    for (const std::string &path : paths) {
        const int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd < 0) {
            continue;
        }
        opened.push_back(fd);
    }

    const uint32_t count = static_cast<uint32_t>(opened.size());
    if (!writeAll(kSocketFd, &count, sizeof(count))) {
        for (int fd : opened) {
            close(fd);
        }
        return 1;
    }

    for (int fd : opened) {
        if (!sendFd(kSocketFd, fd)) {
            close(fd);
            return 1;
        }
        close(fd);
    }

    return 0;
}
