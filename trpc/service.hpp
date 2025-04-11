#pragma once

#include <string>
#include <unordered_map>
#include <memory>

/*
    服务和实现
    +本地服务注册器
*/
namespace trpc {

class BaseService {
    public:
        BaseService(const std::string& name) : name_(name) {}
        virtual ~BaseService() = default;

    private:
        std::string name_;
};

class LocalServiceRegistry {
    public:
        void registerService(const std::string& name, std::unique_ptr<BaseService> service) {
            services_[name] = std::move(service);
        }

        BaseService* getService(const std::string& name) {
            auto it = services_.find(name);
            if (it != services_.end()) {
                return it->second.get();
            }
            return nullptr;
        }

    private:
        std::unordered_map<std::string, std::unique_ptr<BaseService>> services_;
}; 

template <typename T>
class ComputeService : public BaseService {
    public:
        ComputeService() : BaseService("compute") {}
        T execute(const std::string& method, const std::vector<T>& args) {
            if (method == "add") {
                return add(args[0], args[1]);
            } else if (method == "sub") {
                return sub(args[0], args[1]);
            } else if (method == "mul") {
                return mul(args[0], args[1]);
            } else if (method == "div") {
                return div(args[0], args[1]);
            }
            throw std::runtime_error("Unknown method: " + method);
        }
    private:
        T add(T a, T b) { return a + b; }
        T sub(T a, T b) { return a - b; }
        T mul(T a, T b) { return a * b; }
        T div(T a, T b) { return a / b; }        
};

} // namespace trpc 