/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "Socket.hpp"

#include <cstring>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "ws2_32.lib")
   typedef SOCKET socket_t;
   typedef int socklen_t;
#  define CLOSESOCKET closesocket
#  define LAST_SOCK_ERROR WSAGetLastError()
#else
#  include <arpa/inet.h>
#  include <errno.h>
#  include <fcntl.h>
#  include <netdb.h>
#  include <netinet/in.h>
#  include <netinet/tcp.h>
#  include <sys/select.h>
#  include <sys/socket.h>
#  include <unistd.h>
   typedef int socket_t;
#  define INVALID_SOCKET (-1)
#  define CLOSESOCKET ::close
#  define LAST_SOCK_ERROR errno
#endif

namespace loadtest
{
    namespace
    {
        std::string SockError(const char* what)
        {
            return std::string(what) + " failed (errno " +
                   std::to_string(LAST_SOCK_ERROR) + ")";
        }

        bool SetBlocking(socket_t fd, bool blocking)
        {
#ifdef _WIN32
            u_long mode = blocking ? 0 : 1;
            return ioctlsocket(fd, FIONBIO, &mode) == 0;
#else
            const int flags = fcntl(fd, F_GETFL, 0);
            if (flags < 0)
            {
                return false;
            }
            const int wanted = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
            return fcntl(fd, F_SETFL, wanted) == 0;
#endif
        }
    }

    bool InitSockets(std::string& error)
    {
#ifdef _WIN32
        WSADATA data;
        const int rc = WSAStartup(MAKEWORD(2, 2), &data);
        if (rc != 0)
        {
            error = "WSAStartup failed (" + std::to_string(rc) + ")";
            return false;
        }
#else
        (void)error;
#endif
        return true;
    }

    void ShutdownSockets()
    {
#ifdef _WIN32
        WSACleanup();
#endif
    }

    Socket::Socket()
        : m_fd(INVALID_SOCKET),
          m_peerClosed(false)
    {
    }

    Socket::~Socket()
    {
        Close();
    }

    bool Socket::IsOpen() const
    {
        return socket_t(m_fd) != INVALID_SOCKET;
    }

    void Socket::Close()
    {
        if (IsOpen())
        {
            CLOSESOCKET(socket_t(m_fd));
            m_fd = INVALID_SOCKET;
        }
    }

    bool Socket::Connect(const std::string& host, uint16 port, int timeoutMs,
                         std::string& error)
    {
        Close();
        m_peerClosed = false;

        addrinfo hints;
        std::memset(&hints, 0, sizeof(hints));
        hints.ai_family   = AF_INET;          // the redirect payload is IPv4 here
        hints.ai_socktype = SOCK_STREAM;

        addrinfo* resolved = NULL;
        const std::string service = std::to_string(port);
        if (getaddrinfo(host.c_str(), service.c_str(), &hints, &resolved) != 0 ||
            resolved == NULL)
        {
            error = "cannot resolve " + host + ":" + service;
            return false;
        }

        const socket_t fd = socket(resolved->ai_family, resolved->ai_socktype,
                                   resolved->ai_protocol);
        if (fd == INVALID_SOCKET)
        {
            freeaddrinfo(resolved);
            error = SockError("socket()");
            return false;
        }

        // Non-blocking for the connect alone, so a server that is not listening
        // fails on our deadline instead of on the OS's (which is minutes).
        SetBlocking(fd, false);

        const int rc = connect(fd, resolved->ai_addr, socklen_t(resolved->ai_addrlen));
        freeaddrinfo(resolved);

        if (rc != 0)
        {
            fd_set writable;
            FD_ZERO(&writable);
            FD_SET(fd, &writable);

            timeval deadline;
            deadline.tv_sec  = timeoutMs / 1000;
            deadline.tv_usec = (timeoutMs % 1000) * 1000;

            if (select(int(fd) + 1, NULL, &writable, NULL, &deadline) <= 0)
            {
                CLOSESOCKET(fd);
                error = "connect to " + host + ":" + service + " timed out";
                return false;
            }

            int soError = 0;
            socklen_t len = sizeof(soError);
            if (getsockopt(fd, SOL_SOCKET, SO_ERROR,
                           reinterpret_cast<char*>(&soError), &len) != 0 || soError != 0)
            {
                CLOSESOCKET(fd);
                error = "connect to " + host + ":" + service + " refused (" +
                        std::to_string(soError) + ")";
                return false;
            }
        }

        SetBlocking(fd, true);

        // The handshake is a strict ping-pong of small packets, and Nagle would
        // sit on each one waiting for a partner that only arrives after the
        // server has replied to it. That is a deadlock made of two 200 ms
        // timers, and it looks exactly like a server that stopped answering.
        int one = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
                   reinterpret_cast<const char*>(&one), sizeof(one));

        m_fd = fd;
        return true;
    }

    bool Socket::SendAll(const uint8* data, size_t len, std::string& error)
    {
        if (!IsOpen())
        {
            error = "send on a closed socket";
            return false;
        }

        size_t sent = 0;
        while (sent < len)
        {
            const int wrote = int(send(socket_t(m_fd),
                                       reinterpret_cast<const char*>(data + sent),
                                       int(len - sent), 0));
            if (wrote <= 0)
            {
                error = SockError("send()");
                return false;
            }
            sent += size_t(wrote);
        }
        return true;
    }

    bool Socket::Recv(std::vector<uint8>& out, int timeoutMs, std::string& error)
    {
        if (!IsOpen())
        {
            error = "recv on a closed socket";
            return false;
        }

        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(socket_t(m_fd), &readable);

        timeval deadline;
        deadline.tv_sec  = timeoutMs / 1000;
        deadline.tv_usec = (timeoutMs % 1000) * 1000;

        const int ready = select(int(m_fd) + 1, &readable, NULL, NULL, &deadline);
        if (ready < 0)
        {
            error = SockError("select()");
            return false;
        }
        if (ready == 0)
        {
            return true;                                  // nothing yet, not a fault
        }

        uint8 buffer[8192];
        const int got = int(recv(socket_t(m_fd), reinterpret_cast<char*>(buffer),
                                 int(sizeof(buffer)), 0));
        if (got < 0)
        {
            error = SockError("recv()");
            return false;
        }
        if (got == 0)
        {
            m_peerClosed = true;
            error = "the server closed the connection";
            return false;
        }

        out.insert(out.end(), buffer, buffer + got);
        return true;
    }
}
