#include <config/config.hpp>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

class ObfuscationTest : public ::testing::Test
{
  protected:
    void TearDown() override
    {
        if (std::filesystem::exists("test_obf.json"))
            std::filesystem::remove("test_obf.json");
    }
};

// 1. Standard Algorithms
TEST_F(ObfuscationTest, StandardAlgorithms)
{
    auto store         = std::make_unique<config::ConfigStore>("test_obf.json");
    std::string secret = "Secret123";

    store->set("b64", secret, config::Obfuscate::Base64);
    store->set("hex", secret, config::Obfuscate::Hex);
    store->set("rot", secret, config::Obfuscate::ROT13);
    store->set("rev", secret, config::Obfuscate::Reverse);
    store->set("comb", secret, config::Obfuscate::Combined);

    // Verify retrieval (decryption)
    EXPECT_EQ(store->get<std::string>("b64"), secret);
    EXPECT_EQ(store->get<std::string>("hex"), secret);
    EXPECT_EQ(store->get<std::string>("rot"), secret);
    EXPECT_EQ(store->get<std::string>("rev"), secret);
    EXPECT_EQ(store->get<std::string>("comb"), secret);
}

// 2. Base64 Padding Boundaries (Enhanced)
TEST_F(ObfuscationTest, Base64Padding)
{
    auto store = std::make_unique<config::ConfigStore>("test_obf.json");

    // Length % 3 == 0 (0 padding)
    std::string p0 = "abc";
    store->set("p0", p0, config::Obfuscate::Base64);

    // Length % 3 == 2 (1 padding '=')
    std::string p1 = "abcde";
    store->set("p1", p1, config::Obfuscate::Base64);

    // Length % 3 == 1 (2 padding '==')
    std::string p2 = "abcd";
    store->set("p2", p2, config::Obfuscate::Base64);

    store->save();

    // Reload and check
    {
        auto store2 = std::make_unique<config::ConfigStore>("test_obf.json");
        EXPECT_EQ(store2->get<std::string>("p0"), p0);
        EXPECT_EQ(store2->get<std::string>("p1"), p1);
        EXPECT_EQ(store2->get<std::string>("p2"), p2);
    }
}

// 3. Malformed Hex (Enhanced)
TEST_F(ObfuscationTest, MalformedHex)
{
    // Write a file with invalid hex manually
    {
        std::ofstream file("test_obf.json");
        file << R"({
            "bad_hex": "ZZ",
            "odd_hex": "ABC",
            "__obfuscate_meta__": {
                "bad_hex": 2,
                "odd_hex": 2
            }
        })";
    }
    // Hex enum value is 2

    auto store = std::make_unique<config::ConfigStore>("test_obf.json");

    // "ZZ" is not hex. strtol returns 0. So likely "\0"
    std::string bad = store->get<std::string>("bad_hex");
    EXPECT_FALSE(bad.empty()); // Contains \0
    EXPECT_EQ(bad[0], '\0');

    // "ABC" is odd length. Implementation returns empty string
    std::string odd = store->get<std::string>("odd_hex");
    EXPECT_TRUE(odd.empty());
}

// 4. Empty Strings
TEST_F(ObfuscationTest, EmptyStrings)
{
    auto store = std::make_unique<config::ConfigStore>("test_obf.json");
    store->set("empty", "", config::Obfuscate::Base64);
    EXPECT_EQ(store->get<std::string>("empty"), "");

    store->save();

    auto store2 = std::make_unique<config::ConfigStore>("test_obf.json");
    EXPECT_EQ(store2->get<std::string>("empty"), "");
}

// 5. Persistence of Obfuscation Meta
TEST_F(ObfuscationTest, MetaPersistence)
{
    {
        auto store = std::make_unique<config::ConfigStore>("test_obf.json");
        store->set("key", "val", config::Obfuscate::ROT13);
        store->save();
    }

    // Verify file content has __obfuscate_meta__
    std::ifstream file("test_obf.json");
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    EXPECT_TRUE(content.find("__obfuscate_meta__") != std::string::npos);
    EXPECT_TRUE(content.find("\"key\":") != std::string::npos); // Meta should contain the key mapping
}
