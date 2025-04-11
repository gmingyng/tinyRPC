#include "server.hpp"
#include "service.hpp"
#include <iostream>

int main() {
    try {
        // 创建服务器实例
        trpc::Server server(8080);

        // 创建并注册计算服务
        auto compute_service = std::make_unique<trpc::ComputeService<int>>();
        server.registerService("compute", std::move(compute_service));

        // 启动服务器
        std::cout << "Server started on port 8080" << std::endl;
        server.start();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
