//
// Created by dan on 25-4-30.
//

#ifndef SOCKET_H
#define SOCKET_H

#include <arpa/inet.h>

#include <mutex>
#include <string>
#include <functional>
#include <unordered_map>

// only support little endian platform
class Socket {
public:
    enum Role {
        NOT_SET,
        TCP_CLIENT,
        TCP_SERVER,
        UDP_CLIENT,
        UDP_SERVER,
    };

    static constexpr uint32_t MSG_MAX_LEN = 1024; //bytes
    static constexpr int INVALID_SOCKET_FD = -1;

    // udp server can only receive messages
    bool run_as_udp_server(uint port);

    bool run_as_tcp_server(uint port,
                           uint client_max_num,
                           const std::function<void(int/*client_fd*/)> &new_client_connect_callback);

    // udp client can only send messages
    bool run_as_udp_client();

    bool run_as_tcp_client(const std::string &ip, uint port);

    bool send(const std::string &str); // used by tcp client

    bool send(int client_fd, const std::string &str); // used by tcp server

    bool send(const std::string &ip, uint port, const std::string &str); // used by udp client

    std::string recv(); // used by tcp client or udp server

    std::string recv(int socket_fd); // used by tcp server

    void close();

    static void close(int client_fd);

private:
    int m_current_role{NOT_SET};
    std::mutex m_current_role_mtx;

    int m_socket_fd{INVALID_SOCKET_FD};

    std::unordered_map<int, std::unique_ptr<char[]> > m_fd_to_send_mem_map;
    std::unordered_map<int, std::unique_ptr<char[]> > m_fd_to_recv_mem_map;

    std::unordered_map<int, std::mutex> m_fd_to_send_mutex_map;
    std::unordered_map<int, std::mutex> m_fd_to_recv_mutex_map;

    std::string udp_recv();
    std::string tcp_recv(int fd);

    bool tcp_send(int fd, const std::string &str);
};


#endif //SOCKET_H
