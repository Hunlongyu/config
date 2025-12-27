#include <config/config.hpp>
#include <filesystem>
#include <gtest/gtest.h>

class GlobalApiTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Global API uses "config.json" by default.
        if (std::filesystem::exists("config.json"))
            std::filesystem::remove("config.json");
    }

    void TearDown() override
    {
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
