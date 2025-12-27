#include <config/config.hpp>
#include <filesystem>
#include <gtest/gtest.h>

class CoreTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Use a unique file for core tests
        store = std::make_unique<config::ConfigStore>("test_core.json");
    }

    void TearDown() override
    {
        if (std::filesystem::exists("test_core.json"))
            std::filesystem::remove("test_core.json");
    }

    std::unique_ptr<config::ConfigStore> store;
};

// 1. Basic Types
TEST_F(CoreTest, BasicTypes)
{
    // String
    EXPECT_TRUE(store->set("str", "hello"));
    EXPECT_EQ(store->get<std::string>("str"), "hello");

    // Int
    EXPECT_TRUE(store->set("int", 42));
    EXPECT_EQ(store->get<int>("int"), 42);

    // Double
    EXPECT_TRUE(store->set("dbl", 3.14));
    EXPECT_DOUBLE_EQ(store->get<double>("dbl"), 3.14);

    // Bool
    EXPECT_TRUE(store->set("bool", true));
    EXPECT_TRUE(store->get<bool>("bool"));
}

// 2. Nested Keys (JSON Pointer)
TEST_F(CoreTest, NestedKeys)
{
    EXPECT_TRUE(store->set("section/subsection/key", "value"));
    EXPECT_EQ(store->get<std::string>("section/subsection/key"), "value");

    // Access intermediate object (should fail or return default if asking for string)
    // Actually, asking for a generic json object is not directly supported by get<T> unless T is json
    // But let's check basic retrieval
}

// 3. Default Values
TEST_F(CoreTest, DefaultValues)
{
    EXPECT_EQ(store->get<std::string>("missing", "default"), "default");
    EXPECT_EQ(store->get<int>("missing_int", 100), 100);
}

// 4. Container Operations
TEST_F(CoreTest, ContainerOperations)
{
    store->set("key1", "val1");
    store->set("key2", "val2");

    EXPECT_TRUE(store->contains("key1"));
    EXPECT_FALSE(store->contains("key3"));

    store->remove("key1");
    EXPECT_FALSE(store->contains("key1"));
    EXPECT_TRUE(store->contains("key2"));

    store->clear();
    EXPECT_FALSE(store->contains("key2"));
}

// 5. Type Safety (Mismatch Handling)
TEST_F(CoreTest, TypeMismatch)
{
    store->set("string_key", "not_an_number");

    // Try to get as int - should trigger the catch block in get()
    // Default behavior is to return default value (T{})
    EXPECT_EQ(store->get<int>("string_key"), 0);

    // With explicit default
    EXPECT_EQ(store->get<int>("string_key", 999), 999);
}

// 6. Empty Keys
TEST_F(CoreTest, EmptyKeys)
{
    // Set empty key
    // Implementation check: if (key.empty()) return false;
    EXPECT_FALSE(store->set("", "value"));

    // Get empty key
    // Implementation check: if (key.empty()) return default;
    EXPECT_EQ(store->get<std::string>("", "fallback"), "fallback");
}

// 7. Invalid JSON Pointers (Edge Case)
TEST_F(CoreTest, InvalidJsonPointers)
{
    // Accessing a key that conflicts with structure
    store->set("a", "string_value");

    // "a/b" implies "a" is an object, but it is a string.
    // This should trigger exception in nlohmann::json, caught by ConfigStore and rethrown as runtime_error
    try
    {
        store->set("a/b", "value");
        FAIL() << "Expected std::runtime_error";
    }
    catch (const std::runtime_error &e)
    {
        // Success
        SUCCEED();
    }
    catch (...)
    {
        FAIL() << "Caught unknown exception type";
    }

    // Get should be safe and return default
    EXPECT_EQ(store->get<std::string>("a/b", "def"), "def");
}
