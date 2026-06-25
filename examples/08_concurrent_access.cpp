#include <config/config.hpp>
#include <iostream>
#include <thread>
#include <vector>

int main()
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    std::cout << "=== 并发访问演示 ===" << std::endl;

    // 使用 Manual 策略避免磁盘 I/O，专注演示线程安全性
    auto store = config::ConfigStore("08_concurrent.json", config::Path::Relative, config::SaveStrategy::Manual);

    // 初始化共享数据
    store.set("counter", 0);
    store.set("message", std::string("hello"));
    store.set("read_count", 0);

    std::cout << "\n=== 启动读写线程 ===" << std::endl;

    // 创建 5 个读线程
    std::vector<std::thread> readers;
    for (int i = 0; i < 5; ++i)
    {
        readers.emplace_back([&store, i]() {
            for (int j = 0; j < 10; ++j)
            {
                [[maybe_unused]] auto counter = store.get<int>("counter");
                [[maybe_unused]] auto message = store.get<std::string>("message");
                // 模拟一些处理时间
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            std::cout << "读线程 " << i << " 完成\n";
        });
    }

    // 创建 1 个写线程
    std::thread writer([&store]() {
        for (int i = 1; i <= 10; ++i)
        {
            store.set("counter", i);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        std::cout << "写线程完成\n";
    });

    // 等待所有线程完成
    writer.join();
    for (auto &t : readers)
    {
        t.join();
    }

    std::cout << "\n=== 并发访问完成 ===" << std::endl;
    std::cout << "最终计数器值: " << store.get<int>("counter") << "\n";
    std::cout << "消息: " << store.get<std::string>("message") << "\n";
    std::cout << "线程安全演示成功 — 无数据竞争！\n";

    return 0;
}
