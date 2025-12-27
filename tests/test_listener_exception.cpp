#define CONFIG_TEST_FORCE_GET_VALUE_EXCEPTION
#include <config/config.hpp>
#include <gtest/gtest.h>

TEST(ListenerTest, GetValueException)
{
    // Test that when get_value_at throws, it returns empty json and listener receives it.
    config::ConfigStore store("test_listener_ex.json");
    store.set("key", "val");

    bool called = false;
    store.connect("key", [&](const nlohmann::json &j) {
        called = true;
        EXPECT_TRUE(j.is_null()); // json() constructor creates null
    });

    // Trigger notify -> calls get_value_at -> throws (forced) -> returns json() -> callback
    store.set("key", "new_val");

    EXPECT_TRUE(called);
}
