#include "client.hpp"

int main() {
    try {
        // 创建RPC客户端
        RPCClient client("127.0.0.1", 8080);

        // 调用远程add方法
        std::vector<int> args = {5, 3};
        int result = client.call<int>("compute", "add", args);

        std::cout << "5 + 3 = " << result << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}