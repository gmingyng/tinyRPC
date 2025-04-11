#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <functional>
#include <memory>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include "json.hpp"
#include "service.hpp"

namespace trpc {

/*
    事件驱动模型
    +Reactor ： 处理IO事件
    +ThreadPool ： 处理任务
    +Server ： 综合功能，提供面向外界的服务代理
*/
class Reactor {
    public:
        Reactor() : epoll_fd_(-1) {
            epoll_fd_ = epoll_create1(0);
            if (epoll_fd_ == -1) {
                throw std::runtime_error("Failed to create epoll instance");
            }
        }
        
        ~Reactor() {
            if (epoll_fd_ != -1) {
                close(epoll_fd_);
            }
        }

        void addFd(int fd, uint32_t events) {
            struct epoll_event ev;
            ev.events = events;
            ev.data.fd = fd;
            if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
                throw std::runtime_error("Failed to add fd to epoll");
            }
        }

        void modifyFd(int fd, uint32_t events) {
            struct epoll_event ev;
            ev.events = events;
            ev.data.fd = fd;
            if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) == -1) {
                throw std::runtime_error("Failed to modify fd in epoll");
            }
        }

        void removeFd(int fd) {
            if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) == -1) {
                throw std::runtime_error("Failed to remove fd from epoll");
            }
        }

        void run(std::function<void(int)> callback) {
            const int MAX_EVENTS = 64;
            struct epoll_event events[MAX_EVENTS];
            
            while (true) {
                int nfds = epoll_wait(epoll_fd_, events, MAX_EVENTS, -1);
                if (nfds == -1) {
                    throw std::runtime_error("epoll_wait failed");
                }

                for (int i = 0; i < nfds; ++i) {
                    callback(events[i].data.fd);
                }
            }
        }
        
    private:
        int epoll_fd_;
};

class ThreadPool {
    public:
        ThreadPool(int numThreads) : stop_(false) {
            for (int i = 0; i < numThreads; ++i) {
                threads_.emplace_back([this] {
                    while (true) {
                        std::function<void()> task;
                        {
                            std::unique_lock<std::mutex> lock(mutex_);
                            condition_.wait(lock, [this] {
                                return stop_ || !tasks_.empty();
                            });
                            if (stop_ && tasks_.empty()) return;
                            task = std::move(tasks_.front());
                            tasks_.pop();
                        }
                        task();
                    }
                });
            }
        }

        ~ThreadPool() {
            {
                std::unique_lock<std::mutex> lock(mutex_);
                stop_ = true;
            }
            condition_.notify_all();
            for (auto& thread : threads_) {
                thread.join();
            }
        }

        void addTask(std::function<void()> task) {
            {
                std::unique_lock<std::mutex> lock(mutex_);
                if (stop_) {
                    throw std::runtime_error("ThreadPool is stopped");
                }
                tasks_.emplace(std::move(task));
            }
            condition_.notify_one();
        }
        
    private:
        std::vector<std::thread> threads_;
        std::queue<std::function<void()>> tasks_;
        std::mutex mutex_;
        std::condition_variable condition_;
        bool stop_;
};

class ServerCore {
    public:
        ServerCore(int port) : port_(port) {
            // 创建监听socket
            listen_fd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
            if (listen_fd_ == -1) {
                throw std::runtime_error("Failed to create socket");
            }

            // 设置socket选项
            int opt = 1;
            setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

            // 绑定地址
            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            addr.sin_port = htons(port_);
            if (bind(listen_fd_, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
                throw std::runtime_error("Failed to bind socket");
            }

            // 开始监听
            if (listen(listen_fd_, SOMAXCONN) == -1) {
                throw std::runtime_error("Failed to listen on socket");
            }
        }

        ~ServerCore() {
            close(listen_fd_);
        }

        int getListenFd() const { return listen_fd_; }
        int getPort() const { return port_; }

        int acceptConnection() {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(listen_fd_, (struct sockaddr*)&client_addr, &client_len);
            
            if (client_fd == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return -1;
                }
                throw std::runtime_error("Failed to accept connection");
            }

            // 设置非阻塞模式
            int flags = fcntl(client_fd, F_GETFL, 0);
            fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

            return client_fd;
        }

        std::string readData(int fd) {
            char buffer[1024];
            std::string message;
            
            while (true) {
                ssize_t bytes_read = read(fd, buffer, sizeof(buffer));
                if (bytes_read == -1) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        break;
                    }
                    throw std::runtime_error("Failed to read from socket");
                } else if (bytes_read == 0) {
                    // 客户端关闭连接
                    return "";
                }
                
                message.append(buffer, bytes_read);
            }

            return message;
        }

    private:
        int port_;
        int listen_fd_;
};

class Server {
    public:
        Server(int port) : port_(port), 
                          server_core_(std::make_unique<ServerCore>(port)),
                          reactor_(std::make_unique<Reactor>()),
                          threadPool_(std::make_unique<ThreadPool>(4)) {
            // 将监听socket添加到epoll
            reactor_->addFd(server_core_->getListenFd(), EPOLLIN | EPOLLET);
        }

        ~Server() = default;

        void registerService(const std::string& name, std::unique_ptr<BaseService> service) {
            registry_.registerService(name, std::move(service));
        }

        void start() {
            reactor_->run([this](int fd) {
                if (fd == server_core_->getListenFd()) {
                    handleNewConnection();
                } else {
                    handleClientData(fd);
                }
            });
        }

    private:
        void handleNewConnection() {
            while (true) {
                int client_fd = server_core_->acceptConnection();
                if (client_fd == -1) break;
                
                // 将新连接添加到epoll
                reactor_->addFd(client_fd, EPOLLIN | EPOLLET);
            }
        }

        void handleClientData(int fd) {
            std::string message = server_core_->readData(fd);
            if (message.empty()) {
                reactor_->removeFd(fd);
                close(fd);
                return;
            }

            // 将消息放入线程池处理
            threadPool_->addTask([this, fd, message]() {
                try {
                    auto json_msg = nlohmann::json::parse(message);
                    std::string service_name = json_msg["service_name"];
                    std::string method_name = json_msg["method_name"];
                    auto args = json_msg["args"].get<std::vector<int>>();

                    // 获取服务实例
                    auto service = registry_.getService(service_name);
                    if (!service) {
                        throw std::runtime_error("Service not found: " + service_name);
                    }

                    // 调用服务方法
                    auto compute_service = dynamic_cast<ComputeService<int>*>(service);
                    if (!compute_service) {
                        throw std::runtime_error("Invalid service type");
                    }

                    int result = compute_service->execute(method_name, args);

                    // 构造响应
                    nlohmann::json response;
                    response["result"] = result;
                    std::string response_str = response.dump();

                    // 发送响应
                    if (send(fd, response_str.c_str(), response_str.size(), 0) == -1) {
                        throw std::runtime_error("Failed to send response");
                    }

                } catch (const std::exception& e) {
                    std::cerr << "Error processing message: " << e.what() << std::endl;
                    
                    // 发送错误响应
                    nlohmann::json error_response;
                    error_response["error"] = e.what();
                    std::string error_str = error_response.dump();
                    send(fd, error_str.c_str(), error_str.size(), 0);
                }
            });
        }

        int port_;
        std::unique_ptr<ServerCore> server_core_;
        std::unique_ptr<Reactor> reactor_;
        std::unique_ptr<ThreadPool> threadPool_;
        LocalServiceRegistry registry_;
};

} // namespace trpc 