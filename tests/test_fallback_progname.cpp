#define CONFIG_TEST_FORCE_FALLBACK_PROGNAME
#include <config/config.hpp>
#include <gtest/gtest.h>

TEST(FallbackTest, ProgramNameFallback)
{
    // Test get_program_name() failure -> returns "config_app"
    // AND get_appdata_path() success -> returns appdata / "config_app"
    config::ConfigStore store("test.json", config::Path::AppData);

    std::filesystem::path p(store.get_store_path());
    // The path should contain "config_app" directory
    bool found = false;
    for (const auto &part : p)
    {
        if (part == "config_app")
        {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "Path should contain 'config_app' fallback name, got: " << p;
}
