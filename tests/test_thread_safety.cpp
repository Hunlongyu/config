#include <config/config.hpp>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

// 并发读写测试
TEST(ThreadSafetyTest, ConcurrentReadWrite)
{
    auto &store = config::get_store("thread_test.json");
    store.set("counter", 0);

    const int num_threads    = 10;
    const int ops_per_thread = 100;

    std::vector<std::thread> threads;

    // 创建多个写线程
    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back([&store, i, ops_per_thread]() {
            for (int j = 0; j < ops_per_thread; ++j)
            {
                store.set("thread_" + std::to_string(i), j);
            }
        });
    }

    // 创建多个读线程
    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back([&store, i, ops_per_thread]() {
            for (int j = 0; j < ops_per_thread; ++j)
            {
                auto val = store.get<int>("counter", 0);
                (void)val; // 避免未使用警告
            }
        });
    }

    for (auto &t : threads)
    {
        t.join();
    }

    // 验证所有数据都已写入
    for (int i = 0; i < num_threads; ++i)
    {
        EXPECT_TRUE(store.contains("thread_" + std::to_string(i)));
    }

    std::filesystem::remove("thread_test.json");
}

// 并发监听器测试
TEST(ThreadSafetyTest, ConcurrentListeners)
{
    auto &store = config::get_store("listener_thread_test.json");

    std::atomic<int> call_count{0};

    auto id = store.connect("shared_key", [&](const nlohmann::json &) { call_count++; });

    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i)
    {
        threads.emplace_back([&store, i]() {
            for (int j = 0; j < 50; ++j)
            {
                store.set("shared_key", i * 50 + j);
            }
        });
    }

    for (auto &t : threads)
    {
        t.join();
    }

    EXPECT_EQ(call_count, 500); // 10 * 50

    store.disconnect(id);
    std::filesystem::remove("listener_thread_test.json");
}
