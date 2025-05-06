//
// Created by dan on 25-4-30.
//

#ifndef SOCKET_H
#define SOCKET_H

#include <mutex>
#include <string>
#include <functional>
#include <unordered_map>

// only support little endian platform
class Socket {
public:
    enum Role {
        NOT_SET,
        CLIENT,
        SERVER,
    };

    static constexpr uint32_t MSG_MAX_LEN = 1024; //bytes
    static constexpr int INVALID_SOCKET_FD = -1;

    bool run_as_server(uint port,
                       uint client_max_num,
                       const std::function<void(int/*client_fd*/)> &new_client_connect_callback);

    bool run_as_client(const std::string &ip, uint port);

    bool send(const std::string &str);

    bool send(int socket_fd, const std::string &str);

    std::string recv();

    std::string recv(int socket_fd);

    void close();

    static void close(int client_fd);

private:
    int m_current_role{NOT_SET};
    std::mutex m_current_role_mtx;

    int m_socket_fd{INVALID_SOCKET_FD};

    std::unordered_map<int, std::unique_ptr<char[]>> m_fd_to_send_mem_map;
    std::unordered_map<int, std::unique_ptr<char[]>> m_fd_to_recv_mem_map;

    std::unordered_map<int, std::mutex> m_fd_to_send_mutex_map;
    std::unordered_map<int, std::mutex> m_fd_to_recv_mutex_map;

    std::string inner_recv(int fd);

    bool inner_send(int fd, const std::string &str);
};


#endif //SOCKET_H
