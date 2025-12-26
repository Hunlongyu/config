#include <config/config.hpp>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

class AdvancedTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        store = std::make_unique<config::ConfigStore>("test_advanced.json");
    }

    void TearDown() override
    {
        // 确保文件被删除，即使测试失败
        try
        {
            if (std::filesystem::exists("test_advanced.json"))
                std::filesystem::remove("test_advanced.json");
        }
        catch (...)
        {
        }
    }

    std::unique_ptr<config::ConfigStore> store;
};

// 1. 测试复杂嵌套结构和 JSON Pointer 深度访问
TEST_F(AdvancedTest, DeepNestedStructure)
{
    // 构建深度嵌套对象
    store->set("level1/level2/level3/value", 123);
    EXPECT_EQ(store->get<int>("level1/level2/level3/value"), 123);

    // 验证中间层结构自动创建
    EXPECT_TRUE(store->contains("level1"));
    EXPECT_TRUE(store->contains("level1/level2"));

    // 修改中间层
    store->set("level1/level2/other", "test");
    EXPECT_EQ(store->get<std::string>("level1/level2/other"), "test");
    EXPECT_EQ(store->get<int>("level1/level2/level3/value"), 123); // 原值应保留
}

// 2. 测试数组操作
TEST_F(AdvancedTest, ArrayOperations)
{
    std::vector<int> numbers = {1, 2, 3, 4, 5};
    store->set("numbers", numbers);

    auto retrieved = store->get<std::vector<int>>("numbers");
    EXPECT_EQ(retrieved.size(), 5);
    EXPECT_EQ(retrieved[0], 1);
    EXPECT_EQ(retrieved[4], 5);

    // 通过索引访问
    EXPECT_EQ(store->get<int>("numbers/0"), 1);
    EXPECT_EQ(store->get<int>("numbers/4"), 5);

    // 修改数组元素
    store->set("numbers/2", 99);
    EXPECT_EQ(store->get<int>("numbers/2"), 99);

    // 验证整体变化
    auto updated = store->get<std::vector<int>>("numbers");
    EXPECT_EQ(updated[2], 99);
}

// 3. 测试特殊字符 Key
TEST_F(AdvancedTest, SpecialKeys)
{
    store->set("key with spaces", "value1");
    store->set("key.with.dots", "value2");
    store->set("key-with-dashes", "value3");
    // store->set("key/with/slashes", "value4"); // 这是一个路径，不是单个key

    EXPECT_EQ(store->get<std::string>("key with spaces"), "value1");
    EXPECT_EQ(store->get<std::string>("key.with.dots"), "value2");
    EXPECT_EQ(store->get<std::string>("key-with-dashes"), "value3");
}

// 4. 测试类型转换安全性
TEST_F(AdvancedTest, TypeConversionSafety)
{
    store->set("int_val", 123);

    // 尝试读取为 string (JSON 库通常支持自动转换)
    // nlohmann::json get<string> 对于数字会抛出异常，除非使用 dump?
    // 让我们验证一下默认行为

    // 默认 get<string> 对 int 会抛出异常，ConfigStore 应该捕获并返回默认值
    auto str_val = store->get<std::string>("int_val", "default");
    EXPECT_EQ(str_val, "default");

    store->set("str_num", "456");
    // 尝试读取为 int
    auto int_val = store->get<int>("str_num", -1);
    EXPECT_EQ(int_val, -1); // 转换失败应该返回默认值
}

// 5. 测试异常保存场景（只读目录）
TEST_F(AdvancedTest, SaveFailureHandling)
{
    // 构造一个绝对无法创建的路径
    // Windows 上的 NUL 是保留设备名，但行为不一定是一致的
    // 更好的方法是使用一个已经存在的目录名作为文件名
    // 或者尝试在根目录写入（需要管理员权限，普通用户会失败，但如果是管理员会成功）

    // 我们尝试使用一个必定失败的路径：
    // 创建一个目录，然后尝试创建一个同名文件（或者在不存在的盘符下）

    std::filesystem::create_directory("test_conflict_dir");

    // 尝试创建同名文件，应该失败吗？
    // Windows 下目录也是一种文件，通常无法创建同名文件
    // 但我们的 ConfigStore 会尝试打开 std::ofstream("test_conflict_dir")
    // 打开目录作为文件写入通常会失败

    auto &bad_store = config::get_store("test_conflict_dir", config::Path::Relative, config::SaveStrategy::Manual);
    bad_store.set("key", "value");

    bool result = bad_store.save();

    // 清理
    try
    {
        std::filesystem::remove("test_conflict_dir");
    }
    catch (...)
    {
    }

    EXPECT_FALSE(result);
}

// 6. 测试大量数据读写性能与稳定性
TEST_F(AdvancedTest, LargeDataset)
{
    const int count = 1000;
    for (int i = 0; i < count; ++i)
    {
        store->set("data/" + std::to_string(i), i);
    }

    EXPECT_EQ(store->get<int>("data/0"), 0);
    EXPECT_EQ(store->get<int>("data/999"), 999);

    // 批量验证
    for (int i = 0; i < count; ++i)
    {
        EXPECT_TRUE(store->contains("data/" + std::to_string(i)));
    }
}

// 7. 测试 UTF-8 支持
TEST_F(AdvancedTest, UTF8Support)
{
    std::string chinese  = "测试中文";
    std::string emoji    = "😊";
    std::string combined = "Hello 世界 🌍";

    store->set("utf8/cn", chinese);
    store->set("utf8/emoji", emoji);
    store->set("utf8/combined", combined);

    EXPECT_EQ(store->get<std::string>("utf8/cn"), chinese);
    EXPECT_EQ(store->get<std::string>("utf8/emoji"), emoji);
    EXPECT_EQ(store->get<std::string>("utf8/combined"), combined);

    // 验证保存后再加载
    store->save();
    store->reload();

    EXPECT_EQ(store->get<std::string>("utf8/cn"), chinese);
}

// 8. 测试 set 异常捕获（构造非法 JSON Pointer）
TEST_F(AdvancedTest, InvalidSetOperations)
{
    // 设置一个普通值
    store->set("simple", 1);

    // 尝试将普通值当做对象使用路径访问 (类型冲突)
    // simple 是 int，不能在 simple 下创建子键
    EXPECT_THROW(store->set("simple/child", 2), std::runtime_error);
}
