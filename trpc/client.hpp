#pragma once

#include <iostream>
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <future>
#include <thread>
#include <atomic>
#include <memory>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include "json.hpp"

class MessageQueue {
public:
    struct Message {
        std::string service_name;
        std::string method_name;
        std::string request_data;
        std::promise<std::string> response_promise;
    };

    void push(Message&& msg) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(msg));
        cv_.notify_one();
    }

    Message pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return !queue_.empty(); });
        auto msg = std::move(queue_.front());
        queue_.pop();
        return msg;
    }

private:
    std::queue<Message> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

class RPCClient {
public:
    RPCClient(const std::string& server_ip, int port) 
        : server_ip_(server_ip), port_(port), running_(true) {
        // 启动消息处理线程
        worker_thread_ = std::thread(&RPCClient::processMessages, this);
    }

    ~RPCClient() {
        running_ = false;
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }

    template<typename T>
    std::future<T> callAsync(const std::string& service_name, 
                           const std::string& method_name,
                           const std::vector<T>& args) {
        // 创建promise和future
        std::promise<std::string> response_promise;
        auto response_future = response_promise.get_future();

        // 构造请求
        nlohmann::json request;
        request["service_name"] = service_name;
        request["method_name"] = method_name;
        request["args"] = args;

        // 将消息放入队列
        MessageQueue::Message msg{
            service_name,
            method_name,
            request.dump(),
            std::move(response_promise)
        };
        message_queue_.push(std::move(msg));

        // 返回future，允许异步获取结果
        return std::async(std::launch::deferred, [response_future = std::move(response_future)]() mutable {
            auto response_str = response_future.get();
            auto response = nlohmann::json::parse(response_str);
            return response["result"].get<T>();
        });
    }

private:
    void processMessages() {
        while (running_) {
            try {
                auto msg = message_queue_.pop();
                
                // 创建socket连接
                int client_fd = socket(AF_INET, SOCK_STREAM, 0);
                if (client_fd == -1) {
                    msg.response_promise.set_exception(
                        std::make_exception_ptr(std::runtime_error("Failed to create socket")));
                    continue;
                }

                // 设置服务器地址
                struct sockaddr_in server_addr;
                server_addr.sin_family = AF_INET;
                server_addr.sin_port = htons(port_);
                server_addr.sin_addr.s_addr = inet_addr(server_ip_.c_str());

                // 连接到服务器
                if (connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
                    msg.response_promise.set_exception(
                        std::make_exception_ptr(std::runtime_error("Failed to connect to server")));
                    close(client_fd);
                    continue;
                }

                // 发送请求
                if (send(client_fd, msg.request_data.c_str(), msg.request_data.size(), 0) == -1) {
                    msg.response_promise.set_exception(
                        std::make_exception_ptr(std::runtime_error("Failed to send request")));
                    close(client_fd);
                    continue;
                }

                // 接收响应
                char buffer[1024];
                int bytes_received = recv(client_fd, buffer, sizeof(buffer), 0);
                if (bytes_received == -1) {
                    msg.response_promise.set_exception(
                        std::make_exception_ptr(std::runtime_error("Failed to receive response")));
                    close(client_fd);
                    continue;
                }

                // 设置响应结果
                std::string response(buffer, bytes_received);
                msg.response_promise.set_value(response);
                
                close(client_fd);
            } catch (const std::exception& e) {
                // 处理异常
                std::cerr << "Error processing message: " << e.what() << std::endl;
            }
        }
    }

    std::string server_ip_;
    int port_;
    MessageQueue message_queue_;
    std::thread worker_thread_;
    std::atomic<bool> running_;
};


