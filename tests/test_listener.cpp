#include <config/config.hpp>
#include <gtest/gtest.h>

// 基础监听器
TEST(ListenerTest, BasicListener)
{
    auto &store = config::get_store("listener_test.json");

    int call_count = 0;
    std::string last_value;

    auto id = store.connect("key", [&](const nlohmann::json &value) {
        call_count++;
        last_value = value.get<std::string>();
    });

    store.set("key", "value1");
    store.set("key", "value2");

    EXPECT_EQ(call_count, 2);
    EXPECT_EQ(last_value, "value2");

    store.disconnect(id);

    store.set("key", "value3");
    EXPECT_EQ(call_count, 2); // 不应该再增加

    std::filesystem::remove("listener_test.json");
}

// 路径监听
TEST(ListenerTest, PathListener)
{
    auto &store = config::get_store("path_listener_test.json");

    int call_count = 0;

    auto id = store.connect("user/profile", [&](const nlohmann::json &) { call_count++; });

    store.set("user/profile/name", "张三");   // 触发
    store.set("user/profile/age", 25);        // 触发
    store.set("user/settings/theme", "dark"); // 不触发

    EXPECT_EQ(call_count, 2);

    store.disconnect(id);
    std::filesystem::remove("path_listener_test.json");
}

// 多个监听器
TEST(ListenerTest, MultipleListeners)
{
    auto &store = config::get_store("multi_listener_test.json");

    int count1 = 0, count2 = 0;

    auto id1 = store.connect("key", [&](const nlohmann::json &) { count1++; });
    auto id2 = store.connect("key", [&](const nlohmann::json &) { count2++; });

    store.set("key", "value");

    EXPECT_EQ(count1, 1);
    EXPECT_EQ(count2, 1);

    store.disconnect(id1);
    store.set("key", "value2");

    EXPECT_EQ(count1, 1); // 不变
    EXPECT_EQ(count2, 2); // 增加

    store.disconnect(id2);
    std::filesystem::remove("multi_listener_test.json");
}

// 断开不存在的监听器
TEST(ListenerTest, DisconnectNonexistent)
{
    auto &store = config::get_store("disconnect_test.json");

    EXPECT_NO_THROW(store.disconnect(999999));

    std::filesystem::remove("disconnect_test.json");
}
