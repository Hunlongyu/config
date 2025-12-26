#include <config/config.hpp>
#include <gtest/gtest.h>

// DefaultValue 策略测试
TEST(GetStrategyTest, DefaultValueStrategy)
{
    auto &store = config::get_store("get_default_test.json");
    store.set_get_strategy(config::GetStrategy::DefaultValue);

    // 键不存在，返回类型默认值
    EXPECT_EQ(store.get<std::string>("missing"), "");
    EXPECT_EQ(store.get<int>("missing"), 0);
    EXPECT_EQ(store.get<double>("missing"), 0.0);
    EXPECT_EQ(store.get<bool>("missing"), false);

    std::filesystem::remove("get_default_test.json");
}

// ThrowException 策略测试
TEST(GetStrategyTest, ThrowExceptionStrategy)
{
    auto &store = config::get_store("get_throw_test.json");
    store.set_get_strategy(config::GetStrategy::ThrowException);

    // 键不存在，应该抛出异常
    EXPECT_THROW(store.get<std::string>("missing"), std::runtime_error);

    // 键存在，不应抛出异常
    store.set("existing", "value");
    EXPECT_NO_THROW({ auto val = store.get<std::string>("existing"); });

    std::filesystem::remove("get_throw_test.json");
}

// 带默认值的 get 不受策略影响
TEST(GetStrategyTest, GetWithDefaultIgnoresStrategy)
{
    auto &store = config::get_store("get_ignore_test.json");
    store.set_get_strategy(config::GetStrategy::ThrowException);

    // 即使是异常策略，带默认值的 get 也不会抛异常
    EXPECT_NO_THROW({
        auto val = store.get<std::string>("missing", "default");
        EXPECT_EQ(val, "default");
    });

    std::filesystem::remove("get_ignore_test.json");
}

// 策略切换测试
TEST(GetStrategyTest, StrategySwitching)
{
    auto &store = config::get_store("strategy_switch_test.json");

    EXPECT_EQ(store.get_get_strategy(), config::GetStrategy::DefaultValue);

    store.set_get_strategy(config::GetStrategy::ThrowException);
    EXPECT_EQ(store.get_get_strategy(), config::GetStrategy::ThrowException);

    std::filesystem::remove("strategy_switch_test.json");
}
