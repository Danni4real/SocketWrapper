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

bool Socket::run_as_udp_server(uint port) {
    std::lock_guard lk(m_current_role_mtx);

    if (m_current_role != NOT_SET) {
        std::cout << "Err: Role already set!" << std::endl;
        return false;
    }

    m_socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_socket_fd == -1) {
        std::cout << "Err: socket() failed!" << std::endl;
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (bind(m_socket_fd, (sockaddr *) &addr, sizeof(addr)) < 0) {
        std::cout << "Err: bind() failed!" << std::endl;

        close();
        return false;
    }

    m_current_role = UDP_SERVER;

    return true;
}

bool Socket::run_as_tcp_server(const uint port,
                               const uint client_max_num,
                               const std::function<void(int)> &new_client_connect_callback) {
    std::lock_guard lk(m_current_role_mtx);

    if (m_current_role != NOT_SET) {
        std::cout << "Err: Role already set!" << std::endl;
        return false;
    }

    m_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket_fd == -1) {
        std::cout << "Err: socket() failed!" << std::endl;
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (bind(m_socket_fd, (sockaddr *) &addr, sizeof(addr)) < 0) {
        std::cout << "Err: bind() failed!" << std::endl;

        close();
        return false;
    }

    if (listen(m_socket_fd, client_max_num) < 0) {
        std::cout << "Err: listen() failed!" << std::endl;

        close();
        return false;
    }

    std::thread([=] {
        for (int i = 0; i < client_max_num; i++) {
            int client_fd = accept(m_socket_fd, nullptr, nullptr);
            if (client_fd < 0) {
                std::cout << "Err: accept() failed!" << std::endl;
                continue;
            }

            m_fd_to_send_mutex_map[client_fd];
            m_fd_to_recv_mutex_map[client_fd];
            m_fd_to_send_mem_map[client_fd] = std::make_unique<char[]>(MSG_MAX_LEN + 4);
            m_fd_to_recv_mem_map[client_fd] = std::make_unique<char[]>(MSG_MAX_LEN);

            new_client_connect_callback(client_fd);
        }
    }).detach();

    m_current_role = TCP_SERVER;

    return true;
}

bool Socket::run_as_udp_client() {
    std::lock_guard lk(m_current_role_mtx);

    if (m_current_role != NOT_SET) {
        std::cout << "Err: Role already set!" << std::endl;
        return false;
    }

    m_socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_socket_fd == -1) {
        std::cout << "Err: socket() failed!" << std::endl;
        return false;
    }

    m_current_role = UDP_CLIENT;

    return true;
}

bool Socket::run_as_tcp_client(const std::string &ip, const uint port) {
    std::lock_guard lk(m_current_role_mtx);

    if (m_current_role != NOT_SET) {
        std::cout << "Err: Role already set!" << std::endl;
        return false;
    }

    m_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket_fd == -1) {
        std::cout << "Err: socket() failed!" << std::endl;
        return false;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr);
    if (connect(m_socket_fd, (sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        std::cout << "Err: connect() failed!" << std::endl;

        close();
        return false;
    }

    m_current_role = TCP_CLIENT;

    m_fd_to_send_mutex_map[m_socket_fd];
    m_fd_to_recv_mutex_map[m_socket_fd];
    m_fd_to_send_mem_map[m_socket_fd] = std::make_unique<char[]>(MSG_MAX_LEN + 4);
    m_fd_to_recv_mem_map[m_socket_fd] = std::make_unique<char[]>(MSG_MAX_LEN);

    return true;
}

bool Socket::send(const std::string &str) {
    if (m_current_role != TCP_CLIENT) {
        std::cout << "Err: wrong send api called!" << std::endl;
        return false;
    }

    return tcp_send(m_socket_fd, str);
}

bool Socket::send(int client_fd, const std::string &str) {
    if (m_current_role != TCP_SERVER) {
        std::cout << "Err: wrong send api called!" << std::endl;
        return false;
    }

    return tcp_send(client_fd, str);
}

bool Socket::send(const std::string &ip, uint port, const std::string &str) {
    if (m_current_role != UDP_CLIENT) {
        std::cout << "Err: wrong send api called!" << std::endl;
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    auto str_len = str.length();
    int ret = sendto(m_socket_fd, str.data(), str_len, 0, (sockaddr *) &addr, sizeof(addr));
    if (ret < str_len) {
        std::cout << "Err: sendto() failed!" << std::endl;
        return false;
    }

    return true;
}

std::string Socket::recv() {
    if (m_current_role == TCP_CLIENT)
        return tcp_recv(m_socket_fd);
    if (m_current_role == UDP_SERVER)
        return udp_recv();

    std::cout << "Err: wrong recv api called!" << std::endl;
    return "";
}

std::string Socket::recv(int socket_fd) {
    if (m_current_role != TCP_SERVER) {
        std::cout << "Err: wrong recv api called!" << std::endl;
        return "";
    }

    return tcp_recv(socket_fd);
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

bool Socket::tcp_send(int fd, const std::string &str) {
    std::lock_guard lk(m_fd_to_send_mutex_map[fd]);

    if (fd == INVALID_SOCKET_FD) {
        std::cout << "Err: invalid socket fd!" << std::endl;
        return false;
    }

    if (str.length() > MSG_MAX_LEN) {
        std::cout << "Err: string oversized!" << std::endl;
        return false;
    }

    char *buf = m_fd_to_send_mem_map[fd].get();

    const uint32_t str_len = str.length();
    const uint32_t msg_len = str_len + 4;

    memcpy(buf, &str_len, 4);
    memcpy(buf + 4, str.data(), str_len);

    uint32_t bytes_sent = 0;
    while (bytes_sent < msg_len) {
        int ret = ::send(fd, buf + bytes_sent, msg_len - bytes_sent, 0);
        if (ret <= 0) {
            std::cout << "Err: send failed, socket may be disconnected!" << std::endl;
            return false;
        }
        bytes_sent += ret;
    }

    return true;
}

std::string Socket::tcp_recv(int fd) {
    std::lock_guard lk(m_fd_to_recv_mutex_map[fd]);

    if (fd == INVALID_SOCKET_FD) {
        std::cout << "Err: invalid socket fd!" << std::endl;
        return "";
    }

    char *buf = m_fd_to_recv_mem_map[fd].get();

    int ret = ::recv(fd, buf, 4, 0);
    if (ret <= 0) {
        std::cout << "Err: recv failed, socket may be disconnect!" << std::endl;
        return "";
    }

    uint32_t msg_len = 0;
    memcpy(&msg_len, buf, 4);

    if (msg_len > MSG_MAX_LEN) {
        // normally, this may never happen
        std::cout << "Fatal: message oversized!" << std::endl;
        exit(1);
    }

    uint32_t bytes_recvd = 0;
    while (bytes_recvd < msg_len) {
        int ret = ::recv(fd, buf + bytes_recvd, msg_len - bytes_recvd, 0);
        if (ret <= 0) {
            std::cout << "Err: recv failed, socket may be disconnect!" << std::endl;
            return "";
        }
        bytes_recvd += ret;
    }

    return std::string(buf, msg_len);
}

std::string Socket::udp_recv() {
    static std::mutex mtx;
    std::lock_guard lk(mtx);

    static char buf[MSG_MAX_LEN] = {};
    static sockaddr addr{};
    static socklen_t addr_len = sizeof(addr);

    int msg_len = recvfrom(m_socket_fd, buf, MSG_MAX_LEN, 0, &addr, &addr_len);
    if (msg_len <= 0) {
        std::cout << "Err: recvfrom() failed!" << std::endl;
        return "";
    }

    return std::string(buf, msg_len);
}
