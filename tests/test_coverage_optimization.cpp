#include <config/config.hpp>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

class CoverageTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Clean up any existing files
        if (std::filesystem::exists("test_coverage.json"))
            std::filesystem::remove("test_coverage.json");
    }

    void TearDown() override
    {
        if (std::filesystem::exists("test_coverage.json"))
            std::filesystem::remove("test_coverage.json");
    }
};

// 1. Test Decryption and Obfuscation details
TEST_F(CoverageTest, FullObfuscationCycle)
{
    // Create a store and set values with different obfuscation types
    {
        auto store = std::make_unique<config::ConfigStore>("test_coverage.json");
        store->set("b64", "base64_val", config::Obfuscate::Base64);
        store->set("hex", "hex_val", config::Obfuscate::Hex);
        store->set("rot", "rot13_val", config::Obfuscate::ROT13);
        store->set("rev", "reverse_val", config::Obfuscate::Reverse);
        store->set("comb", "combined_val", config::Obfuscate::Combined);
        store->set("none", "none_val", config::Obfuscate::None);
        store->save();
    }

    // Load in a NEW store instance to force decryption
    {
        auto store = std::make_unique<config::ConfigStore>("test_coverage.json");
        EXPECT_EQ(store->get<std::string>("b64"), "base64_val");
        EXPECT_EQ(store->get<std::string>("hex"), "hex_val");
        EXPECT_EQ(store->get<std::string>("rot"), "rot13_val");
        EXPECT_EQ(store->get<std::string>("rev"), "reverse_val");
        EXPECT_EQ(store->get<std::string>("comb"), "combined_val");
        EXPECT_EQ(store->get<std::string>("none"), "none_val");
    }
}

// 2. Test GetStrategy::ThrowException
TEST_F(CoverageTest, GetStrategyThrow)
{
    auto store = std::make_unique<config::ConfigStore>("test_coverage.json");
    store->set_get_strategy(config::GetStrategy::ThrowException);

    // Test missing simple key
    EXPECT_THROW(store->get<int>("missing_key"), std::runtime_error);

    // Test missing path key
    EXPECT_THROW(store->get<int>("missing/nested/key"), std::runtime_error);

    // Test existing key works
    store->set("exists", 1);
    EXPECT_NO_THROW(store->get<int>("exists"));
}

// 3. Test clear()
TEST_F(CoverageTest, ClearStore)
{
    auto store = std::make_unique<config::ConfigStore>("test_coverage.json");
    store->set("a", 1);
    store->set("b", 2);
    store->save();

    store->clear();
    EXPECT_FALSE(store->contains("a"));
    EXPECT_FALSE(store->contains("b"));

    // Verify persistence of clear if Auto save (default)
    {
        auto store2 = std::make_unique<config::ConfigStore>("test_coverage.json");
        EXPECT_FALSE(store2->contains("a"));
    }
}

// 4. Test Obfuscation Edge Cases (Malformed Hex)
TEST_F(CoverageTest, MalformedHex)
{
    {
        std::ofstream file("test_coverage.json");
        file << R"({
            "bad_hex": "ZZ",
            "__obfuscate_meta__": {
                "bad_hex": 2
            }
        })";
    }
    {
        auto store = std::make_unique<config::ConfigStore>("test_coverage.json");
        // "ZZ" is not valid hex. Implementation might produce garbage or empty string.
        // Current hex_decode implementation:
        // "ZZ" -> strtol("ZZ", 16) -> 0 -> '\0'.
        // So it returns "\0".
        auto val = store->get<std::string>("bad_hex");
        EXPECT_EQ(val.length(), 1);
        EXPECT_EQ(val[0], '\0');
    }

    // Test Odd Hex (should fail and return empty or original?)
    // Implementation says: if (input.length() % 2 != 0) return "";
    {
        std::ofstream file("test_coverage.json");
        file << R"({
            "odd_hex": "ABC",
            "__obfuscate_meta__": {
                "odd_hex": 2
            }
        })";
    }
    {
        auto store = std::make_unique<config::ConfigStore>("test_coverage.json");
        // Decryption fails -> returns ""?
        EXPECT_EQ(store->get<std::string>("odd_hex"), "");
    }
}

// 5. Test Listener Exception Safety
TEST_F(CoverageTest, ListenerException)
{
    auto store = std::make_unique<config::ConfigStore>("test_coverage.json");
    store->set("key", 1);

    store->connect("key", [](const nlohmann::json &j) { throw std::runtime_error("Listener error"); });

    // Should not crash
    EXPECT_NO_THROW(store->set("key", 2));
}

// 6. Test Remove Empty Key (Should NOT clear store)
TEST_F(CoverageTest, RemoveEmptyKey)
{
    auto store = std::make_unique<config::ConfigStore>("test_coverage.json");
    store->set("a", 1);
    store->set("", 2); // Set a key with empty string name
    store->save();

    // Remove empty key
    store->remove("");

    EXPECT_TRUE(store->contains("a")); // "a" should remain!
    EXPECT_FALSE(store->contains("")); // "" should be gone

    // Remove non-existent empty key (should be safe)
    store->remove("");
    EXPECT_TRUE(store->contains("a")); // "a" should still remain
}
