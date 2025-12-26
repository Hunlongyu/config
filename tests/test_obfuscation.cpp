#include <config/config.hpp>
#include <gtest/gtest.h>

class ObfuscationTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        store       = std::make_unique<config::ConfigStore>("test_obf.json");
        test_string = "TestSecret123!@#";
    }

    void TearDown() override
    {
        std::filesystem::remove("test_obf.json");
    }

    std::unique_ptr<config::ConfigStore> store;
    std::string test_string;
};

// Base64 混淆
TEST_F(ObfuscationTest, Base64Obfuscation)
{
    store->set("pwd", test_string, config::Obfuscate::Base64);
    EXPECT_EQ(store->get<std::string>("pwd"), test_string);
}

// Hex 混淆
TEST_F(ObfuscationTest, HexObfuscation)
{
    store->set("pwd", test_string, config::Obfuscate::Hex);
    EXPECT_EQ(store->get<std::string>("pwd"), test_string);
}

// ROT13 混淆
TEST_F(ObfuscationTest, ROT13Obfuscation)
{
    store->set("pwd", test_string, config::Obfuscate::ROT13);
    EXPECT_EQ(store->get<std::string>("pwd"), test_string);
}

// Reverse 混淆
TEST_F(ObfuscationTest, ReverseObfuscation)
{
    store->set("pwd", test_string, config::Obfuscate::Reverse);
    EXPECT_EQ(store->get<std::string>("pwd"), test_string);
}

// Combined 混淆
TEST_F(ObfuscationTest, CombinedObfuscation)
{
    store->set("pwd", test_string, config::Obfuscate::Combined);
    EXPECT_EQ(store->get<std::string>("pwd"), test_string);
}

// 持久化测试
TEST_F(ObfuscationTest, ObfuscationPersistence)
{
    {
        auto &s = config::get_store("persist_obf.json");
        s.set("secret", test_string, config::Obfuscate::Combined);
    }

    // 重新加载
    {
        auto &s = config::get_store("persist_obf.json");
        EXPECT_EQ(s.get<std::string>("secret"), test_string);
    }

    std::filesystem::remove("persist_obf.json");
}

// 多种混淆同时存在
TEST_F(ObfuscationTest, MultipleObfuscationTypes)
{
    store->set("b64", "value1", config::Obfuscate::Base64);
    store->set("hex", "value2", config::Obfuscate::Hex);
    store->set("rot", "value3", config::Obfuscate::ROT13);

    EXPECT_EQ(store->get<std::string>("b64"), "value1");
    EXPECT_EQ(store->get<std::string>("hex"), "value2");
    EXPECT_EQ(store->get<std::string>("rot"), "value3");
}
