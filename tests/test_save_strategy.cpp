#include <config/config.hpp>
#include <gtest/gtest.h>

// 自动保存测试
TEST(SaveStrategyTest, AutoSave)
{
    {
        auto &store = config::get_store("auto_test.json", config::Path::Relative, config::SaveStrategy::Auto);
        store.set("key", "value");
    }

    // 重新加载验证已保存
    {
        auto &store = config::get_store("auto_test.json");
        EXPECT_EQ(store.get<std::string>("key"), "value");
    }

    std::filesystem::remove("auto_test.json");
}

// 手动保存测试
TEST(SaveStrategyTest, ManualSave)
{
    auto &store = config::get_store("manual_test.json", config::Path::Relative, config::SaveStrategy::Manual);

    store.set("key", "value");

    // 未调用 save()，重新加载应该没有数据
    store.reload();
    EXPECT_EQ(store.get<std::string>("key", ""), "");

    // 保存后重新加载
    store.set("key", "value");
    store.save();
    store.reload();
    EXPECT_EQ(store.get<std::string>("key"), "value");

    std::filesystem::remove("manual_test.json");
}

// 策略切换测试
TEST(SaveStrategyTest, StrategySwitching)
{
    auto &store = config::get_store("switch_test.json");

    EXPECT_EQ(store.get_save_strategy(), config::SaveStrategy::Auto);

    store.set_save_strategy(config::SaveStrategy::Manual);
    EXPECT_EQ(store.get_save_strategy(), config::SaveStrategy::Manual);

    std::filesystem::remove("switch_test.json");
}

// 格式测试
TEST(SaveStrategyTest, SaveFormat)
{
    auto &store = config::get_store("format_test.json");

    store.set("key", "value");
    store.save(config::JsonFormat::Compact);

    // 读取文件验证格式
    {
        std::ifstream file("format_test.json");
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

        // Compact 格式不应该有换行符
        EXPECT_EQ(content.find('\n'), std::string::npos);
    }
    // file 对象析构，文件句柄关闭

    std::filesystem::remove("format_test.json");
}
