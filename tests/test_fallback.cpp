#define CONFIG_TEST_FORCE_FALLBACK_APPDATA
#include <config/config.hpp>
#include <gtest/gtest.h>

TEST(FallbackTest, AppDataFallback)
{
    // Test get_appdata_path() failure -> returns current_path()
    config::ConfigStore store("test.json", config::Path::AppData);

    std::filesystem::path resolved_path(store.get_store_path());
    std::filesystem::path expected = std::filesystem::current_path() / "test.json";

    EXPECT_EQ(resolved_path, expected);
}
