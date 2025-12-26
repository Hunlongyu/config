#include <config/config.hpp>
#include <gtest/gtest.h>

// 相对路径测试
TEST(PathTest, RelativePath)
{
    auto &store = config::get_store("relative.json", config::Path::Relative);
    store.set("key", "value");

    auto path = store.get_store_path();
    EXPECT_TRUE(std::filesystem::exists(path));

    std::filesystem::remove("relative.json");
}

// 绝对路径测试
TEST(PathTest, AbsolutePath)
{
#ifdef _WIN32
    auto abs_path = "C:/temp/config_test_absolute.json";
#else
    auto abs_path = "/tmp/config_test_absolute.json";
#endif

    auto &store = config::get_store(abs_path, config::Path::Absolute);
    store.set("key", "value");

    EXPECT_TRUE(std::filesystem::exists(abs_path));

    std::filesystem::remove(abs_path);
}

// AppData 路径测试
TEST(PathTest, AppDataPath)
{
    auto &store = config::get_store("TestApp/test.json", config::Path::AppData);
    store.set("key", "value");

    auto path = store.get_store_path();
    EXPECT_TRUE(std::filesystem::exists(path));

#ifdef _WIN32
    EXPECT_TRUE(path.find("AppData") != std::string::npos || path.find("APPDATA") != std::string::npos);
#elif __APPLE__
    EXPECT_TRUE(path.find("Library/Application Support") != std::string::npos);
#else
    EXPECT_TRUE(path.find(".config") != std::string::npos);
#endif

    std::filesystem::remove(path);
    std::filesystem::remove(std::filesystem::path(path).parent_path());
}

// 多个 store 不同路径
TEST(PathTest, MultipleStores)
{
    auto &s1 = config::get_store("store1.json");
    auto &s2 = config::get_store("store2.json");

    s1.set("id", "store1");
    s2.set("id", "store2");

    EXPECT_EQ(s1.get<std::string>("id"), "store1");
    EXPECT_EQ(s2.get<std::string>("id"), "store2");

    std::filesystem::remove("store1.json");
    std::filesystem::remove("store2.json");
}
