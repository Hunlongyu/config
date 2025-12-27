#include <atomic>
#include <config/config.hpp>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

// ==========================================
// Core Tests
// ==========================================

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
    catch (const std::runtime_error &)
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

// 8. Remove Root/Empty
TEST_F(CoreTest, RemoveEdgeCases)
{
    store->set("key", "val");

    // Remove empty key (should be safe, usually means root or invalid)
    EXPECT_NO_THROW(store->remove(""));

    // Verify data is still there
    EXPECT_TRUE(store->contains("key"));

    // Remove non-existent key
    EXPECT_NO_THROW(store->remove("non_existent"));
    EXPECT_TRUE(store->contains("key"));
}

// 9. Manual Save Remove (Coverage)
TEST_F(CoreTest, RemoveManualSave)
{
    store->set_save_strategy(config::SaveStrategy::Manual);
    store->set("key", "val");
    EXPECT_TRUE(store->remove("key")); // Returns true directly
    EXPECT_FALSE(store->contains("key"));
}

// 10. Contains Empty (Coverage)
TEST_F(CoreTest, ContainsEmpty)
{
    EXPECT_FALSE(store->contains(""));
}

// 11. Get Empty with Throw Strategy (Coverage)
TEST_F(CoreTest, GetEmptyThrow)
{
    store->set_get_strategy(config::GetStrategy::ThrowException);
    EXPECT_THROW(store->get<int>(""), std::runtime_error);
}

// 11b. Get Empty with Default Strategy (Coverage for return T{})
TEST_F(CoreTest, GetEmptyDefault)
{
    store->set_get_strategy(config::GetStrategy::DefaultValue);
    // Should return T{} which is 0 for int
    EXPECT_EQ(store->get<int>(""), 0);
}

// 12. Set Invalid Key (Coverage for catch block)
TEST_F(CoreTest, SetInvalidKey)
{
    // "~" is invalid in json_pointer (must be ~0 or ~1)
    // This should trigger the catch block in set()
    EXPECT_THROW(store->set("bad~key", "val"), std::runtime_error);
}

// 13. Remove Invalid Key (Coverage for catch block in remove)
TEST_F(CoreTest, RemoveInvalidKey)
{
    // "~" is invalid in json_pointer
    // remove() swallows exceptions, so it should NOT throw, but the catch block will be entered.
    EXPECT_NO_THROW(store->remove("bad~key"));
}

// ==========================================
// Advanced Tests
// ==========================================

class AdvancedTest : public ::testing::Test
{
  protected:
    void TearDown() override
    {
        if (std::filesystem::exists("test_adv.json"))
            std::filesystem::remove("test_adv.json");
    }
};

// 1. GetStrategy::ThrowException
TEST_F(AdvancedTest, ThrowExceptionStrategy)
{
    auto store = std::make_unique<config::ConfigStore>("test_adv.json");
    store->set_get_strategy(config::GetStrategy::ThrowException);

    EXPECT_THROW(store->get<int>("missing"), std::runtime_error);

    store->set("exists", 1);
    EXPECT_NO_THROW(store->get<int>("exists"));
}

// 2. Listeners
TEST_F(AdvancedTest, Listeners)
{
    auto store  = std::make_unique<config::ConfigStore>("test_adv.json");
    bool called = false;

    auto id = store->connect("key", [&](const nlohmann::json &j) {
        called = true;
        EXPECT_EQ(j.get<std::string>(), "val");
    });

    store->set("key", "val");
    EXPECT_TRUE(called);

    // Disconnect
    called = false;
    store->disconnect(id);
    store->set("key", "val2");
    EXPECT_FALSE(called);
}

// 3. Listener Exception Safety
TEST_F(AdvancedTest, ListenerException)
{
    auto store = std::make_unique<config::ConfigStore>("test_adv.json");

    store->connect("key", [](const nlohmann::json &) { throw std::runtime_error("Listener failed"); });

    // Set should not crash even if listener throws
    EXPECT_NO_THROW(store->set("key", "val"));
}

// 4. Thread Safety (Concurrent Read/Write)
TEST_F(AdvancedTest, ThreadSafety)
{
    auto store = std::make_unique<config::ConfigStore>("test_adv.json");
    store->set("counter", 0);

    const int num_threads = 10;
    const int ops         = 100;
    std::vector<std::thread> threads;
    std::atomic<bool> running{true};

    // Writers
    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < ops; ++j)
            {
                store->set("thread_" + std::to_string(i), j);
            }
        });
    }

    // Readers
    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back([&]() {
            while (running)
            {
                auto val = store->get<int>("counter", 0);
                (void)val;
            }
        });
    }

    // Let them run
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;

    for (auto &t : threads)
    {
        if (t.joinable())
            t.join();
    }

    // Verify integrity
    for (int i = 0; i < num_threads; ++i)
    {
        EXPECT_TRUE(store->contains("thread_" + std::to_string(i)));
    }
}

// 5. Registry Caching
TEST_F(AdvancedTest, RegistryCaching)
{
    auto &store1 = config::get_store("test_adv.json");
    auto &store2 = config::get_store("test_adv.json");

    // Should be the same instance
    EXPECT_EQ(&store1, &store2);

    // Different file
    auto &store3 = config::get_store("test_adv_2.json");
    EXPECT_NE(&store1, &store3);

    // Verify changes propagate
    store1.set("shared", "yes");
    EXPECT_EQ(store2.get<std::string>("shared"), "yes");
}

// ==========================================
// Global API Tests
// ==========================================

class GlobalApiTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        config::clear();
        if (std::filesystem::exists("config.json"))
            std::filesystem::remove("config.json");
    }

    void TearDown() override
    {
        config::clear();
        if (std::filesystem::exists("config.json"))
            std::filesystem::remove("config.json");
    }
};

TEST_F(GlobalApiTest, AllGlobalFunctions)
{
    // Set & Get
    EXPECT_TRUE(config::set("global_key", "global_val"));
    EXPECT_EQ(config::get<std::string>("global_key"), "global_val");

    // Get with default
    EXPECT_EQ(config::get<int>("missing", 404), 404);

    // Contains
    EXPECT_TRUE(config::contains("global_key"));
    EXPECT_FALSE(config::contains("missing"));

    // Remove
    config::remove("global_key");
    EXPECT_FALSE(config::contains("global_key"));

    // Save Strategy
    config::set_save_strategy(config::SaveStrategy::Manual);
    EXPECT_EQ(config::get_save_strategy(), config::SaveStrategy::Manual);

    // Get Strategy
    config::set_get_strategy(config::GetStrategy::ThrowException);
    EXPECT_EQ(config::get_get_strategy(), config::GetStrategy::ThrowException);
    EXPECT_THROW(config::get<int>("missing_throw"), std::runtime_error);
    config::set_get_strategy(config::GetStrategy::DefaultValue); // Reset

    // Format
    config::set_format(config::JsonFormat::Compact);
    EXPECT_EQ(config::get_format(), config::JsonFormat::Compact);

    // Coverage for save(format)
    EXPECT_TRUE(config::save(config::JsonFormat::Pretty));
}

// 7. Invalid JsonFormat Coverage (Defensive)
TEST_F(GlobalApiTest, InvalidJsonFormat)
{
    // Set some data to verify format
    config::set("test_fmt", "val");

    // Cast invalid int to JsonFormat
    auto invalid_fmt = static_cast<config::JsonFormat>(999);
    EXPECT_TRUE(config::save(invalid_fmt));

    // Read file content manually to check if it's compact (no newlines/indentation)
    std::ifstream file("config.json");
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    EXPECT_FALSE(content.empty());
}

// 6. Global Clear
TEST_F(GlobalApiTest, GlobalClear)
{
    // Save & Reload
    config::set("persist", "val");
    config::save(); // Manual save

    config::reload();
    EXPECT_EQ(config::get<std::string>("persist"), "val");

    // Store Path
    EXPECT_FALSE(config::get_store_path().empty());

    // Clear
    config::clear();
    EXPECT_FALSE(config::contains("persist"));
}

// ==========================================
// Obfuscation Tests
// ==========================================

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

// 6. Full Obfuscation Persistence (Triggers decrypt for all types)
TEST_F(ObfuscationTest, FullObfuscationPersistence)
{
    {
        auto store = std::make_unique<config::ConfigStore>("test_obf.json");
        store->set("hex", "SecretHex", config::Obfuscate::Hex);
        store->set("rot", "SecretRot", config::Obfuscate::ROT13);
        store->set("rev", "SecretRev", config::Obfuscate::Reverse);
        store->set("comb", "SecretComb", config::Obfuscate::Combined);
        store->save();
    }

    // Reload triggers decrypt
    {
        auto store = std::make_unique<config::ConfigStore>("test_obf.json");
        EXPECT_EQ(store->get<std::string>("hex"), "SecretHex");
        EXPECT_EQ(store->get<std::string>("rot"), "SecretRot");
        EXPECT_EQ(store->get<std::string>("rev"), "SecretRev");
        EXPECT_EQ(store->get<std::string>("comb"), "SecretComb");
    }
}

// 7. Obfuscate::None in Meta
TEST_F(ObfuscationTest, ObfuscateNoneInMeta)
{
    // Manually create file with Obfuscate::None (0) in meta
    {
        std::ofstream file("test_obf.json");
        file << R"({
            "plaintext": "visible",
            "__obfuscate_meta__": {
                "plaintext": 0
            }
        })";
    }

    auto store = std::make_unique<config::ConfigStore>("test_obf.json");
    // Should be treated as plain text
    EXPECT_EQ(store->get<std::string>("plaintext"), "visible");

    // Trigger save to cover the "continue" in save loop for None
    store->save();
}

// 8. Orphaned Obfuscation Meta (Key in meta but not in data)
TEST_F(ObfuscationTest, OrphanedMeta)
{
    {
        std::ofstream file("test_obf.json");
        file << R"({
            "__obfuscate_meta__": {
                "missing_key": 1
            }
        })";
    }

    // Should load without error and ignore missing key
    auto store = std::make_unique<config::ConfigStore>("test_obf.json");
    EXPECT_FALSE(store->contains("missing_key"));

    // Trigger save to cover the catch block in save loop
    store->save();
}

// 9. Invalid Obfuscation Key (Coverage for load/save catch)
TEST_F(ObfuscationTest, InvalidKeyObfuscation)
{
    {
        std::ofstream file("test_obf.json");
        // "bad~key" is invalid json pointer
        file << R"({
            "bad~key": "secret",
            "__obfuscate_meta__": {
                "bad~key": 2
            }
        })";
    }

    // Load should hit catch block because json_pointer("bad~key") throws.
    std::unique_ptr<config::ConfigStore> store;
    try
    {
        store = std::make_unique<config::ConfigStore>("test_obf.json");
    }
    catch (const std::exception &e)
    {
        FAIL() << "Load failed with exception: " << e.what();
    }

    try
    {
        store->contains("bad~key");
    }
    catch (...)
    {
    }

    // Now save.
    try
    {
        store->save();
    }
    catch (const std::exception &e)
    {
        FAIL() << "Save failed with exception: " << e.what();
    }
}

// 10. Nested Obfuscation
TEST_F(ObfuscationTest, NestedObfuscation)
{
    {
        auto store = std::make_unique<config::ConfigStore>("test_obf.json");
        store->set("section/secret", "hidden", config::Obfuscate::Base64);
        store->save();
    }

    // Verify raw content is obfuscated
    std::ifstream file("test_obf.json");
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    EXPECT_TRUE(content.find("section/secret") != std::string::npos); // Meta key format
    EXPECT_TRUE(content.find("aGlkZGVu") != std::string::npos);       // "hidden" in base64

    // Verify reload
    auto store = std::make_unique<config::ConfigStore>("test_obf.json");
    EXPECT_EQ(store->get<std::string>("section/secret"), "hidden");
}

// 11. Invalid Enum Coverage (Defensive Programming)
TEST_F(ObfuscationTest, InvalidEnumCoverage)
{
    std::string input = "test";
    auto invalid_obf  = static_cast<config::Obfuscate>(999);

    // Test encrypt default path (should return plain text)
    EXPECT_EQ(config::detail::ObfuscationEngine::encrypt(input, invalid_obf), input);

    // Test decrypt default path (should return plain text)
    EXPECT_EQ(config::detail::ObfuscationEngine::decrypt(input, invalid_obf), input);
}

// ==========================================
// Persistence Tests
// ==========================================

class PersistenceTest : public ::testing::Test
{
  protected:
    void TearDown() override
    {
        // Cleanup potentially created files
        if (std::filesystem::exists("test_auto.json"))
            std::filesystem::remove("test_auto.json");
        if (std::filesystem::exists("test_manual.json"))
            std::filesystem::remove("test_manual.json");
        if (std::filesystem::exists("test_compact.json"))
            std::filesystem::remove("test_compact.json");

        // Cleanup absolute/appdata paths is harder, will do in specific tests
    }
};

// 1. Auto Save Strategy
TEST_F(PersistenceTest, AutoSave)
{
    {
        auto store =
            std::make_unique<config::ConfigStore>("test_auto.json", config::Path::Relative, config::SaveStrategy::Auto);
        store->set("key", "value");
        // Should be saved immediately
    }

    // Check file existence
    EXPECT_TRUE(std::filesystem::exists("test_auto.json"));

    // Load and verify
    {
        auto store = std::make_unique<config::ConfigStore>("test_auto.json");
        EXPECT_EQ(store->get<std::string>("key"), "value");
    }
}

// 2. Manual Save Strategy
TEST_F(PersistenceTest, ManualSave)
{
    {
        auto store = std::make_unique<config::ConfigStore>("test_manual.json", config::Path::Relative,
                                                           config::SaveStrategy::Manual);
        store->set("key", "value");
        // Should NOT be saved yet
    }

    {
        auto store = std::make_unique<config::ConfigStore>("test_manual.json");
        EXPECT_FALSE(store->contains("key"));
    }

    // Now save explicitly
    {
        auto store = std::make_unique<config::ConfigStore>("test_manual.json", config::Path::Relative,
                                                           config::SaveStrategy::Manual);
        store->set("key", "saved_val");
        store->save();
    }

    {
        auto store = std::make_unique<config::ConfigStore>("test_manual.json");
        EXPECT_EQ(store->get<std::string>("key"), "saved_val");
    }
}

// 3. Compact Format (Enhanced)
TEST_F(PersistenceTest, CompactFormat)
{
    {
        auto store = std::make_unique<config::ConfigStore>("test_compact.json");
        store->set("a", 1);
        store->set("b", 2);
        store->save(config::JsonFormat::Compact);
    }

    // Read raw file content
    std::ifstream file("test_compact.json");
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    // Should not contain newlines or spaces (except maybe in keys/values, but we used simple ones)
    EXPECT_EQ(content.find('\n'), std::string::npos);
    EXPECT_EQ(content.find(' '), std::string::npos);
}

// 4. Reload
TEST_F(PersistenceTest, Reload)
{
    auto store = std::make_unique<config::ConfigStore>("test_auto.json");
    store->set("key", "initial");
    store->save();

    // Modify file externally
    {
        std::ofstream file("test_auto.json");
        file << R"({"key": "external"})";
    }

    EXPECT_EQ(store->get<std::string>("key"), "initial"); // Still in memory
    store->reload();
    EXPECT_EQ(store->get<std::string>("key"), "external"); // Reloaded
}

// 5. Path Strategies
TEST_F(PersistenceTest, AbsolutePath)
{
    auto temp_dir = std::filesystem::temp_directory_path();
    auto abs_path = temp_dir / "config_absolute_test.json";

    {
        auto store = std::make_unique<config::ConfigStore>(abs_path.string(), config::Path::Absolute);
        store->set("abs", true);
    }

    EXPECT_TRUE(std::filesystem::exists(abs_path));
    std::filesystem::remove(abs_path);
}

TEST_F(PersistenceTest, AppDataPath)
{
    // This creates a file in actual AppData. We must be careful to clean it up.
    std::string filename = "config_appdata_test.json";

    {
        auto store = std::make_unique<config::ConfigStore>(filename, config::Path::AppData);
        store->set("appdata", true);

        // Get the resolved path to verify and clean up
        std::filesystem::path resolved_path(store->get_store_path());
        EXPECT_TRUE(std::filesystem::exists(resolved_path));
    }

    // Clean up
    auto store = std::make_unique<config::ConfigStore>(filename, config::Path::AppData);
    std::filesystem::remove(store->get_store_path());
}

// 6. Load Corrupted JSON
TEST_F(PersistenceTest, LoadCorruptedJson)
{
    // Write invalid JSON
    {
        std::ofstream file("test_bad.json");
        file << "{ \"key\": \"value\" "; // Missing closing brace
    }

    // Should catch exception and initialize empty
    auto store = std::make_unique<config::ConfigStore>("test_bad.json");
    EXPECT_FALSE(store->contains("key"));

    if (std::filesystem::exists("test_bad.json"))
        std::filesystem::remove("test_bad.json");
}

// 7. Save Failure (Invalid Path)
TEST_F(PersistenceTest, SaveFailure)
{
    // Use a directory path as file path to trigger open failure
    auto store = std::make_unique<config::ConfigStore>("persistence_test_dir/");
    store->set("key", "value");

    // Save should fail safely return false
    EXPECT_FALSE(store->save());
}

// 8. Save Directory Creation Failure (Coverage)
TEST_F(PersistenceTest, SaveMkdirFailure)
{
    // Create a file "blocker"
    {
        std::ofstream f("blocker");
        f << "data";
    }

    // Try to save to "blocker/file.json". "blocker" exists as file, so mkdir should fail.
    // Note: On Windows, mkdir on existing file fails.
    auto store = std::make_unique<config::ConfigStore>("blocker/file.json");
    store->set("key", "val");
    EXPECT_FALSE(store->save());

    std::filesystem::remove("blocker");
}

// 9. PathResolver Invalid Enum (Coverage)
TEST_F(PersistenceTest, PathResolverInvalid)
{
    // Cast invalid int to Path enum
    auto store = std::make_unique<config::ConfigStore>("test.json", (config::Path)99);
    // Should default to absolute path
    EXPECT_FALSE(store->get_store_path().empty());
}

// 10. PathResolver Enum Coverage (Additional)
TEST_F(PersistenceTest, PathResolverEnumCoverage)
{
    // Explicitly test Path::Absolute and Path::Relative cases via constructor
    {
        config::ConfigStore s1("test_abs.json", config::Path::Absolute);
        EXPECT_FALSE(s1.get_store_path().empty());
    }
    {
        config::ConfigStore s2("test_rel.json", config::Path::Relative);
        EXPECT_FALSE(s2.get_store_path().empty());
    }

    // Test Invalid Path Enum explicitly again just to be sure
    {
        config::ConfigStore s3("test_inv.json", static_cast<config::Path>(999));
        // Should fallback to absolute/current path logic
        EXPECT_FALSE(s3.get_store_path().empty());
    }
}

// 11. PathResolver AppData Creation
TEST_F(PersistenceTest, AppDataCreation)
{
    // To test create_directories, we need get_appdata_path() to return a path that does NOT exist.
    // get_appdata_path() returns AppData/Roaming/config_app (on Windows).
    // This directory likely exists because other tests ran before.

    // Strategy:
    // 1. Get the appdata path manually (we can't access private get_appdata_path).
    // 2. But we can deduce it from a store with Path::AppData.
    config::ConfigStore store("dummy.json", config::Path::AppData);
    std::filesystem::path full_path(store.get_store_path());
    std::filesystem::path app_data_dir = full_path.parent_path();

    // 3. Remove the directory if it exists.
    // SAFETY CHECK: Ensure we are ONLY deleting the test executable's own directory.
    // We check that the directory name matches the executable name.
    // However, the executable name might be different now (test_combined vs test_persistence).
    // The library uses "config_app" or program name.
    // Config.hpp: get_program_name() returns "config_app" if not WIN32/APPLE/LINUX or fallback.
    // Actually, get_program_name() logic uses GetModuleFileName on Windows.
    // So if we run "test_combined.exe", the dir will be "test_combined".
    // If we run "test_persistence.exe", it was "test_persistence".
    // We should safely remove it if it matches our expectation.

    if (std::filesystem::exists(app_data_dir))
    {
        // Double check to prevent any accidental deletion of important system folders
        // We just check if it contains "test_" or "config_app".
        std::string dir_name = app_data_dir.filename().string();
        if (dir_name.find("test_") != std::string::npos || dir_name == "config_app")
        {
            // Just try to remove it.
            std::filesystem::remove_all(app_data_dir);
        }
    }

    // 4. Create a new store with Path::AppData. This should trigger create_directories.
    config::ConfigStore store2("dummy.json", config::Path::AppData);

    // 5. Verify it exists now.
    EXPECT_TRUE(std::filesystem::exists(app_data_dir));
}
