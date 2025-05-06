//
// Created by dan on 25-4-30.
//

#include <unistd.h>
#include <arpa/inet.h>

#include <thread>
#include <memory>
#include <cstring>
#include <ostream>
#include <iostream>

#include "Socket.h"

bool Socket::run_as_server(const uint port,
                           const uint client_max_num,
                           const std::function<void(int)> &new_client_connect_callback) {
    std::lock_guard lk(m_current_role_mtx);

    if (m_current_role != NOT_SET) {
        std::cout << "Role already set!" << std::endl;
        return false;
    }

    m_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket_fd == -1) {
        std::cout << "socket() failed!" << std::endl;
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (bind(m_socket_fd, (sockaddr *) &addr, sizeof(addr)) < 0) {
        std::cout << "bind() failed!" << std::endl;
        return false;
    }

    if (listen(m_socket_fd, client_max_num) < 0) {
        std::cout << "listen() failed!" << std::endl;
        return false;
    }

    std::thread([=] {
        for (int i = 0; i < client_max_num; i++) {
            int client_fd = accept(m_socket_fd, nullptr, nullptr);
            if (client_fd < 0) {
                std::cout << "accept() failed!" << std::endl;
                continue;
            }

            m_fd_to_send_mutex_map[client_fd];
            m_fd_to_recv_mutex_map[client_fd];
            m_fd_to_send_mem_map[client_fd] = std::make_unique<char[]>(MSG_MAX_LEN + 4);
            m_fd_to_recv_mem_map[client_fd] = std::make_unique<char[]>(MSG_MAX_LEN);

            new_client_connect_callback(client_fd);
        }
    }).detach();

    m_current_role = SERVER;

    return true;
}

bool Socket::run_as_client(const std::string &ip, const uint port) {
    std::lock_guard lk(m_current_role_mtx);

    if (m_current_role != NOT_SET) {
        std::cout << "Role already set!" << std::endl;
        return false;
    }

    m_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket_fd == -1) {
        std::cout << "socket() failed!" << std::endl;
        return false;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr);
    if (connect(m_socket_fd, (sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        std::cout << "connect() failed!" << std::endl;
        return false;
    }

    m_current_role = CLIENT;

    m_fd_to_send_mutex_map[m_socket_fd];
    m_fd_to_recv_mutex_map[m_socket_fd];
    m_fd_to_send_mem_map[m_socket_fd] = std::make_unique<char[]>(MSG_MAX_LEN + 4);
    m_fd_to_recv_mem_map[m_socket_fd] = std::make_unique<char[]>(MSG_MAX_LEN);

    return true;
}

bool Socket::send(const std::string &str) {
    return send(m_socket_fd, str);
}

bool Socket::send(int socket_fd, const std::string &str) {
    if (m_current_role == NOT_SET) {
        std::cout << "Role not set yet!" << std::endl;
        return false;
    }

    if (socket_fd == INVALID_SOCKET_FD) {
        std::cout << "invalid socket fd!" << std::endl;
        return false;
    }

    if (str.length() > MSG_MAX_LEN) {
        std::cout << "string oversized!" << std::endl;
        return false;
    }

    return inner_send(socket_fd, str);
}

std::string Socket::recv() {
    return recv(m_socket_fd);
}

std::string Socket::recv(int socket_fd) {
    if (m_current_role == NOT_SET) {
        std::cout << "Role not set yet!" << std::endl;
        return "";
    }

    if (socket_fd == INVALID_SOCKET_FD) {
        std::cout << "invalid socket fd!" << std::endl;
        return "";
    }

    return inner_recv(socket_fd);
}

void Socket::close() {
    if (m_socket_fd != INVALID_SOCKET_FD) {
        ::close(m_socket_fd);
        m_socket_fd = INVALID_SOCKET_FD;
    }
}

void Socket::close(int client_fd) {
    if (client_fd != INVALID_SOCKET_FD) {
        ::close(client_fd);
    }
}

bool Socket::inner_send(int fd, const std::string &str) {
    std::lock_guard lk(m_fd_to_send_mutex_map[fd]);

    char *buf = m_fd_to_send_mem_map[fd].get();

    const uint32_t str_len = str.length();
    const uint32_t msg_len = str_len + 4;

    memcpy(buf, &str_len, 4);
    memcpy(buf + 4, str.data(), str_len);

    uint32_t bytes_sent = 0;
    while (bytes_sent < msg_len) {
        int ret = ::send(fd, buf + bytes_sent, msg_len - bytes_sent, 0);
        if (ret <= 0) {
            std::cout << "send failed, socket may be disconnected!" << std::endl;
            return false;
        }
        bytes_sent += ret;
    }

    return true;
}

std::string Socket::inner_recv(int fd) {
    std::lock_guard lk(m_fd_to_recv_mutex_map[fd]);

    char *buf = m_fd_to_recv_mem_map[fd].get();

    int ret = ::recv(fd, buf, 4, 0);
    if (ret <= 0) {
        std::cout << "recv failed, socket may be disconnect!" << std::endl;
        return "";
    }

    uint32_t msg_len = 0;
    memcpy(&msg_len, buf, 4);

    if (msg_len > MSG_MAX_LEN) {
        // normally, this may never happen
        std::cout << "message oversized!" << std::endl;
        exit(1);
    }

    uint32_t bytes_recvd = 0;
    while (bytes_recvd < msg_len) {
        int ret = ::recv(fd, buf + bytes_recvd, msg_len - bytes_recvd, 0);
        if (ret <= 0) {
            std::cout << "recv failed, socket may be disconnect!" << std::endl;
            return "";
        }
        bytes_recvd += ret;
    }

    return std::string(buf, msg_len);
}
