#pragma once

#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include "json.hpp"

class RPCClient {
public:
    RPCClient(const std::string& server_ip, int port) 
        : server_ip_(server_ip), port_(port) {
        // 创建socket
        client_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (client_fd_ == -1) {
            throw std::runtime_error("Failed to create socket");
        }

        // 设置服务器地址
        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port_);
        server_addr.sin_addr.s_addr = inet_addr(server_ip_.c_str());

        // 连接到服务器
        if (connect(client_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
            throw std::runtime_error("Failed to connect to server");
        }
    }

    ~RPCClient() {
        close(client_fd_);
    }

    template<typename T>
    T call(const std::string& service_name, const std::string& method_name, 
           const std::vector<T>& args) {
        // 构造请求JSON
        nlohmann::json request;
        request["service_name"] = service_name;
        request["method_name"] = method_name;
        request["args"] = args;

        std::string request_str = request.dump();
        
        // 发送请求
        if (send(client_fd_, request_str.c_str(), request_str.size(), 0) == -1) {
            throw std::runtime_error("Failed to send request");
        }

        // 接收响应
        char buffer[1024];
        int bytes_received = recv(client_fd_, buffer, sizeof(buffer), 0);
        if (bytes_received == -1) {
            throw std::runtime_error("Failed to receive response");
        }

        // 解析响应
        std::string response_str(buffer, bytes_received);
        auto response = nlohmann::json::parse(response_str);
        
        return response["result"].get<T>();
    }

private:
    std::string server_ip_;
    int port_;
    int client_fd_;
};


