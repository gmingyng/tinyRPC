# TinyRPC

一个轻量级的RPC框架，使用epoll和线程池处理并发请求，支持异步调用和Redis缓存。

## 功能特性

- 基于Reactor模式的高性能服务器
- 异步RPC调用支持
- Redis缓存集成
- 线程池处理请求
- 非阻塞IO
- 类型安全的服务调用

## 项目结构

```
tinyRPC/
├── trpc/                    # 核心代码
│   ├── client.hpp          # RPC客户端实现
│   ├── server.hpp          # RPC服务器实现
│   ├── service.hpp         # 服务接口定义
│   └── json.hpp            # JSON序列化支持
├── example/                # 示例代码
│   ├── server.cpp         # 服务器示例
│   └── test_add.cpp       # 客户端示例
├── build/                  # 构建目录
│   ├── obj/               # 目标文件
│   └── bin/               # 可执行文件
└── Makefile               # 构建脚本
```

## 核心组件

### 1. Reactor模式
- 事件驱动架构
- 非阻塞IO
- 高效的事件分发

### 2. 线程池
- 固定大小线程池
- 任务队列管理
- 支持优雅关闭

### 3. 服务注册
- 基于智能指针的服务管理
- 支持动态服务注册
- 类型安全的服务调用

### 4. Redis缓存
- 结果缓存
- 自动过期
- 缓存键管理

### 5. 异步调用
- 消息队列
- Future/Promise模式
- 异常处理

## 构建说明

### 依赖项
- C++14
- hiredis
- Redis服务器

### 运行
1. 启动Redis服务器
```bash
redis-server
```

2. 启动RPC服务器
```bash
./build/bin/server
```

3. 运行客户端测试
```bash
./build/bin/client
```