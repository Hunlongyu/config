#include <config/config.hpp>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

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

        // We can't easily check file content here because the file might not exist or be empty if it didn't exist
        // before. But we can check if we reload, it's not there.
    }
    // File might not even be created if save() wasn't called?
    // Actually, constructor calls load(), if file doesn't exist, it does nothing.
    // set() doesn't save.

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

        // Verify it looks like an AppData path (platform dependent checks)
        // We can just trust the resolved path is what we need to clean.
    }

    // Clean up
    // We need to resolve it again or find it.
    // Re-instantiate to get path
    auto store = std::make_unique<config::ConfigStore>(filename, config::Path::AppData);
    std::filesystem::remove(store->get_store_path());
}
