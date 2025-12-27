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
    // This tests the default case in PathResolver::resolve
    // Note: We need to access private/internal detail or use public API that calls it.
    // ConfigStore calls resolve in constructor.
    auto store = std::make_unique<config::ConfigStore>("test.json", (config::Path)99);
    // Should default to absolute path
    EXPECT_FALSE(store->get_store_path().empty());

    // Test resolve with invalid enum (Defensive coverage)
    // Access resolve directly? No, it's private.
    // But we already triggered it via constructor above.
    // The default case in switch(type) inside resolve() handles it.
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

    // Path::AppData is tested in main tests, but let's double check coverage
    // if we can mock or just run it (it might create dirs).
    // The previous tests likely covered it.

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
    // We check that the directory name matches the executable name (test_persistence).
    if (std::filesystem::exists(app_data_dir))
    {
        // Double check to prevent any accidental deletion of important system folders
        if (app_data_dir.filename().string().find("test_persistence") != std::string::npos)
        {
            std::filesystem::remove_all(app_data_dir);
        }
    }

    // 4. Create a new store with Path::AppData. This should trigger create_directories.
    config::ConfigStore store2("dummy.json", config::Path::AppData);

    // 5. Verify it exists now.
    EXPECT_TRUE(std::filesystem::exists(app_data_dir));
}
