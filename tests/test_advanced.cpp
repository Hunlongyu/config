#include <atomic>
#include <config/config.hpp>
#include <filesystem>
#include <gtest/gtest.h>
#include <thread>

class AdvancedTest : public ::testing::Test
{
  protected:
    void TearDown() override
    {
        if (std::filesystem::exists("test_adv.json"))
            std::filesystem::remove("test_adv.json");
    }
};

// 1. GetStrategy::ThrowException
TEST_F(AdvancedTest, ThrowExceptionStrategy)
{
    auto store = std::make_unique<config::ConfigStore>("test_adv.json");
    store->set_get_strategy(config::GetStrategy::ThrowException);

    EXPECT_THROW(store->get<int>("missing"), std::runtime_error);

    store->set("exists", 1);
    EXPECT_NO_THROW(store->get<int>("exists"));
}

// 2. Listeners
TEST_F(AdvancedTest, Listeners)
{
    auto store  = std::make_unique<config::ConfigStore>("test_adv.json");
    bool called = false;

    auto id = store->connect("key", [&](const nlohmann::json &j) {
        called = true;
        EXPECT_EQ(j.get<std::string>(), "val");
    });

    store->set("key", "val");
    EXPECT_TRUE(called);

    // Disconnect
    called = false;
    store->disconnect(id);
    store->set("key", "val2");
    EXPECT_FALSE(called);
}

// 3. Listener Exception Safety
TEST_F(AdvancedTest, ListenerException)
{
    auto store = std::make_unique<config::ConfigStore>("test_adv.json");

    store->connect("key", [](const nlohmann::json &) { throw std::runtime_error("Listener failed"); });

    // Set should not crash even if listener throws
    EXPECT_NO_THROW(store->set("key", "val"));
}

// 4. Thread Safety (Concurrent Read/Write)
TEST_F(AdvancedTest, ThreadSafety)
{
    auto store = std::make_unique<config::ConfigStore>("test_adv.json");
    store->set("counter", 0);

    const int num_threads = 10;
    const int ops         = 100;
    std::vector<std::thread> threads;
    std::atomic<bool> running{true};

    // Writers
    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < ops; ++j)
            {
                store->set("thread_" + std::to_string(i), j);
            }
        });
    }

    // Readers
    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back([&]() {
            while (running)
            {
                auto val = store->get<int>("counter", 0);
                (void)val;
            }
        });
    }

    // Let them run
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;

    for (auto &t : threads)
    {
        if (t.joinable())
            t.join();
    }

    // Verify integrity
    for (int i = 0; i < num_threads; ++i)
    {
        EXPECT_TRUE(store->contains("thread_" + std::to_string(i)));
    }
}
