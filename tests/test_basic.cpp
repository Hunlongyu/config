#include <config/config.hpp>
#include <gtest/gtest.h>

class BasicTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        store = std::make_unique<config::ConfigStore>("test_basic.json");
    }

    void TearDown() override
    {
        std::filesystem::remove("test_basic.json");
    }

    std::unique_ptr<config::ConfigStore> store;
};

// 基本数据类型
TEST_F(BasicTest, SetAndGetString)
{
    store->set("key", "value");
    EXPECT_EQ(store->get<std::string>("key"), "value");
}

TEST_F(BasicTest, SetAndGetInt)
{
    store->set("number", 42);
    EXPECT_EQ(store->get<int>("number"), 42);
}

TEST_F(BasicTest, SetAndGetDouble)
{
    store->set("pi", 3.14159);
    EXPECT_DOUBLE_EQ(store->get<double>("pi"), 3.14159);
}

TEST_F(BasicTest, SetAndGetBool)
{
    store->set("flag", true);
    EXPECT_TRUE(store->get<bool>("flag"));
}

// JSON Pointer 路径
TEST_F(BasicTest, JSONPointerPath)
{
    store->set("user/profile/name", "张三");
    store->set("user/profile/age", 25);

    EXPECT_EQ(store->get<std::string>("user/profile/name"), "张三");
    EXPECT_EQ(store->get<int>("user/profile/age"), 25);
}

// 默认值
TEST_F(BasicTest, GetWithDefaultValue)
{
    auto value = store->get<std::string>("nonexistent", "default");
    EXPECT_EQ(value, "default");

    auto number = store->get<int>("nonexistent", 100);
    EXPECT_EQ(number, 100);
}

// Contains
TEST_F(BasicTest, Contains)
{
    store->set("exists", "value");

    EXPECT_TRUE(store->contains("exists"));
    EXPECT_FALSE(store->contains("not_exists"));
}

// Remove
TEST_F(BasicTest, Remove)
{
    store->set("temp", "value");
    EXPECT_TRUE(store->contains("temp"));

    store->remove("temp");
    EXPECT_FALSE(store->contains("temp"));
}

// Clear
TEST_F(BasicTest, Clear)
{
    store->set("key1", "value1");
    store->set("key2", "value2");

    store->clear();

    EXPECT_FALSE(store->contains("key1"));
    EXPECT_FALSE(store->contains("key2"));
}

// 获取路径
TEST_F(BasicTest, GetStorePath)
{
    auto path = store->get_store_path();
    EXPECT_FALSE(path.empty());
    EXPECT_TRUE(path.find("test_basic.json") != std::string::npos);
}
