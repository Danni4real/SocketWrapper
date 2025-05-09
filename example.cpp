#include <thread>
#include <iostream>

#include "Socket.h"

Socket tcp_server;
Socket udp_server;

void handle_tcp_client_connected(int client_fd) {
    std::thread([=]() {
        while (true) {
            std::string str = tcp_server.recv(client_fd);
            if (str.empty()) {
                std::cout << "recv() failed!" << std::endl;
                exit(1);
            }

            std::cout << "tcp server recvd: " << str << std::endl;
            tcp_server.send(client_fd, "ack--" + str);
        }
    }).detach();
}

void wait_udp_messages() {
    std::thread([]() {
        while (true) {
            std::string str = udp_server.recv();
            if (str.empty()) {
                std::cout << "recv() failed!" << std::endl;
                exit(1);
            }

            std::cout << "udp server recvd: " << str << std::endl;
        }
    }).detach();
}

void run_tcp_client(Socket &client, const std::string &client_name) {
    std::thread([=,&client]() {
        client.run_as_tcp_client("127.0.0.1", 8088);

        int i = 0;
        std::thread([&] {
            while (true) {
                std::string str = client.recv();
                if (str.empty()) {
                    std::cout << "recv() failed!" << std::endl;
                    exit(1);
                }
                std::cout << client_name << " recvd: " << str << std::endl;

                std::this_thread::sleep_for(std::chrono::microseconds(1));
                client.send(client_name + "--" + std::to_string(++i));
            }
        }).detach();

        client.send(client_name + "--" + std::to_string(i));

        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(60));
        }
    }).detach();
}

void run_udp_client(Socket &client, const std::string &client_name) {
    std::thread([=,&client]() {
        client.run_as_udp_client();

        int i = 0;
        while (true) {
            bool ret = client.send("127.0.0.1",
                                   8087,
                                   client_name + "--" + std::to_string(++i));
            if (!ret) {
                std::cout << "send() failed!" << std::endl;
                exit(1);
            }

            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    }).detach();
}

int main() {
    udp_server.run_as_udp_server(8087);
    wait_udp_messages();

    tcp_server.run_as_tcp_server(8088,
                                 2,
                                 handle_tcp_client_connected);

    std::this_thread::sleep_for(std::chrono::seconds(3));

    Socket tcp_client1;
    Socket tcp_client2;

    Socket udp_client1;
    Socket udp_client2;

    run_tcp_client(tcp_client1, "tcp_client1");
    run_tcp_client(tcp_client2, "tcp_client2");

    run_udp_client(udp_client1, "udp_client1");
    run_udp_client(udp_client2, "udp_client2");

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(60));
    }
}
