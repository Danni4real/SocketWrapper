#include <thread>
#include <iostream>

#include "Socket.h"

Socket socket_server;

void handle_new_client_connection(int client_fd) {
    std::thread([=]() {
        while (true) {
            std::string str = socket_server.recv(client_fd);
            if (str.empty()) {
                std::cout << "recv() failed!" << std::endl;
                return;
            }

            std::cout << "server: recvd a message: " << str << std::endl;
            socket_server.send(client_fd, "ack: " + str);
        }
    }).detach();
}

void run_client(Socket &client, const std::string &client_name) {
    std::thread([=,&client]() {
        client.run_as_client("127.0.0.1", 8088);

        int i = 0;
        std::thread([&] {
            while (true) {
                std::string str = client.recv();
                if (str.empty()) {
                    std::cout << "recv() failed!" << std::endl;
                    return;
                }
                std::cout << client_name << ": recvd a message: " << str << std::endl;

                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                client.send(client_name + ": " + std::to_string(++i));
            }
        }).detach();

        client.send(client_name + ": " + std::to_string(i));

        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(60));
        }
    }).detach();
}

int main() {
    socket_server.run_as_server(8088,
                                2,
                                handle_new_client_connection);

    std::this_thread::sleep_for(std::chrono::seconds(3));

    Socket socket_client1;
    Socket socket_client2;

    run_client(socket_client1, "client1");
    run_client(socket_client2, "client2");

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(60));
    }
}
