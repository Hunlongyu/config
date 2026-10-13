#include "test_support.hpp"
#include <config/paths.hpp>
#include <map>
#include <vector>
using namespace config_test;

struct Settings
{
    std::string host = "localhost";
    int port         = 80;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Settings, host, port)
};

class StoreTest : public testing::Test
{
  protected:
    TempDirectory dir;
    config::Store store{dir.file(), {.save = config::SaveMode::manual}};
};

TEST_F(StoreTest, ScalarsContainersAndUserTypes)
{
    store.set("/int", 42);
    store.set("/double", 2.5);
    store.set("/bool", true);
    store.set("/text", "hello");
    store.set("/null", nullptr);
    store.set("/array", std::vector<int>{1, 2, 3});
    store.set("/map", std::map<std::string, int>{{"one", 1}});
    store.set("/struct", Settings{"example", 9000});
    EXPECT_EQ(store.get<int>("/int"), 42);
    EXPECT_DOUBLE_EQ(store.get<double>("/double"), 2.5);
    EXPECT_TRUE(store.get<bool>("/bool"));
    EXPECT_EQ(store.get<std::string>("/text"), "hello");
    EXPECT_TRUE(store.get<json>("/null").is_null());
    EXPECT_EQ(store.get<std::vector<int>>("/array"), (std::vector<int>{1, 2, 3}));
    EXPECT_EQ((store.get<std::map<std::string, int>>("/map").at("one")), 1);
    EXPECT_EQ(store.get<Settings>("/struct").port, 9000);
    store.set("/partial", json{{"host", "custom"}});
    EXPECT_EQ(store.get<Settings>("/partial").port, 80);
}
TEST_F(StoreTest, GetOrOnlyFallsBackForMissing)
{
    EXPECT_EQ(store.get_or<int>("/missing", 7), 7);
    expect_error(config::ErrorCode::missing, [&] { store.get<int>("/missing"); });
    store.set("/value", "text");
    expect_error(config::ErrorCode::type, [&] { store.get_or<int>("/value", 7); });
    store.set("/value", nullptr);
    expect_error(config::ErrorCode::type, [&] { store.get<int>("/value"); });
}
TEST_F(StoreTest, PathsPreserveEscapesAndEmptySegments)
{
    store.set("/a~1b/x~0y", 4);
    store.set("//leaf", 5);
    EXPECT_EQ(store.get<int>("//leaf"), 5);
    store.set("/", "empty key");
    EXPECT_EQ(store.get<int>("/a~1b/x~0y"), 4);
    EXPECT_EQ(store.get<std::string>("/"), "empty key");
    EXPECT_TRUE(store.contains("/a~1b"));
}
TEST_F(StoreTest, InvalidPathsLeaveStateUnchanged)
{
    store.set("/good", 1);
    for (const auto *key : {"", "bare", "/bad~2", "/trailing~"})
    {
        expect_error(config::ErrorCode::path, [&] { store.set(key, 2); });
        expect_error(config::ErrorCode::path, [&] { store.contains(key); });
        expect_error(config::ErrorCode::path, [&] { store.erase(key); });
    }
    EXPECT_EQ(store.snapshot(), json({{"good", 1}}));
}
TEST_F(StoreTest, EraseNestedKeysAndArrayElements)
{
    store.set("/a", json::array({1, 2, 3}));
    store.erase("/a/1");
    EXPECT_EQ(store.get<json>("/a"), json::array({1, 3}));
    store.erase("/missing");
    store.set("/object/leaf", 5);
    store.erase("/object");
    EXPECT_FALSE(store.contains("/object"));
}
TEST_F(StoreTest, SnapshotIsDetachedAndRootUpdateIsAtomic)
{
    store.set("/old", 1);
    auto old = store.snapshot();
    store.update([](json &data) { data = json{{"new", 2}, {"also", 3}}; });
    EXPECT_EQ(old, json({{"old", 1}}));
    EXPECT_EQ(store.snapshot().size(), 2u);
    expect_error(config::ErrorCode::type, [&] { store.update([](json &data) { data = json::array(); }); });
    EXPECT_TRUE(store.contains("/new"));
}
TEST_F(StoreTest, FailedUpdateDoesNotCommit)
{
    store.set("/value", 1);
    EXPECT_THROW(store.update([](json &data) {
        data["value"] = 2;
        throw std::logic_error("reject");
    }),
                 std::logic_error);
    EXPECT_EQ(store.get<int>("/value"), 1);
}
TEST_F(StoreTest, DeepMergeReplacesArraysAndPreservesOtherKeys)
{
    store.merge(json{{"nested", {{"a", 1}, {"b", 2}}}, {"array", json::array({1, 2})}});
    store.merge(json{{"nested", {{"a", 3}}}, {"array", json::array({4})}});
    EXPECT_EQ(store.get<int>("/nested/b"), 2);
    EXPECT_EQ(store.get<int>("/nested/a"), 3);
    EXPECT_EQ(store.get<json>("/array"), json::array({4}));
    expect_error(config::ErrorCode::type, [&] { store.merge(json::array()); });
}
TEST_F(StoreTest, InvalidEncodingDoesNotMutate)
{
    expect_error(config::ErrorCode::type, [&] { store.set("/a", 1, config::Encoding::base64); });
    expect_error(config::ErrorCode::type, [&] { store.set("/a", "x", static_cast<config::Encoding>(999)); });
    EXPECT_TRUE(store.snapshot().empty());
}
TEST_F(StoreTest, SameFileAliasesHaveOneOwner)
{
    expect_error(config::ErrorCode::in_use, [&] { config::Store duplicate(dir.path() / "." / "config.json"); });
    store.save();
    auto alias = dir.file("hardlink.json");
    std::error_code error;
    std::filesystem::create_hard_link(dir.file(), alias, error);
    if (!error)
        expect_error(config::ErrorCode::in_use, [&] { config::Store duplicate(alias); });
#if defined(_WIN32)
    expect_error(config::ErrorCode::in_use, [&] { config::Store duplicate(dir.file("CONFIG.JSON")); });
#endif
}
TEST(StoreOwnership, DestructionAllowsReopening)
{
    TempDirectory dir;
    {
        config::Store store(dir.file());
        store.set("/value", 7);
    }
    config::Store reopened(dir.file());
    EXPECT_EQ(reopened.get<int>("/value"), 7);
}
TEST(Paths, UserDirectoryHasNoCreationSideEffects)
{
    const auto path = config::user_config_directory("config-library-path-probe");
    EXPECT_TRUE(path.is_absolute());
    EXPECT_EQ(path.filename(), "config-library-path-probe");
    expect_error(config::ErrorCode::path, [] { config::user_config_directory("../escape"); });
    expect_error(config::ErrorCode::path, [] { config::user_config_directory(""); });
}
TEST(StoreOptions, InvalidOptionsAndDefaultsAreRejected)
{
    TempDirectory dir;
    expect_error(config::ErrorCode::type,
                 [&] { config::Store invalid(dir.file(), {.save = static_cast<config::SaveMode>(99)}); });
    expect_error(config::ErrorCode::type,
                 [&] { config::Store invalid(dir.file(), {.format = static_cast<config::Format>(99)}); });
    expect_error(config::ErrorCode::type, [&] { config::Store invalid(dir.file(), {.defaults = json::array()}); });
}
