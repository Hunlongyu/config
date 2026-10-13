#include "test_support.hpp"
#include <config/paths.hpp>
#include <map>
#include <memory>
#include <type_traits>
#include <vector>
using namespace config_test;

template <class T, class Fallback>
concept AcceptsFallback = requires(const config::Store &store, Fallback &&fallback) {
    { store.template get<T>("/value", std::forward<Fallback>(fallback)) } -> std::same_as<T>;
};
template <class Fallback>
concept DeducesGetType = requires(const config::Store &store, Fallback &&fallback) {
    store.get("/value", std::forward<Fallback>(fallback));
};
static_assert(AcceptsFallback<int, int>);
static_assert(AcceptsFallback<std::string, const char *>);
static_assert(AcceptsFallback<bool, decltype([] { return false; })>);
static_assert(!AcceptsFallback<int, decltype([] {})>);
static_assert(!AcceptsFallback<bool, decltype([] {})>);
static_assert(!AcceptsFallback<int, decltype([] { return std::string("wrong"); })>);
static_assert(!AcceptsFallback<int, decltype([](int value) { return value; })>);
static_assert(!DeducesGetType<int>);

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
TEST_F(StoreTest, GetValueOnlyFallsBackForMissing)
{
    EXPECT_EQ(store.get<int>("/missing", 7), 7);
    EXPECT_EQ(store.get<std::string>("/missing", "localhost"), "localhost");
    const std::string fallback = "fallback";
    EXPECT_EQ(store.get<std::string>("/missing", fallback), fallback);
    store.set("/present", 3);
    EXPECT_EQ(store.get<int>("/present", 7), 3);
    expect_error(config::ErrorCode::missing, [&] { (void)store.get<int>("/missing"); });
    store.set("/value", "text");
    expect_error(config::ErrorCode::type, [&] { (void)store.get<int>("/value", 7); });
    store.set("/value", nullptr);
    expect_error(config::ErrorCode::type, [&] { (void)store.get<int>("/value"); });
    expect_error(config::ErrorCode::type, [&] { (void)store.get<int>("/value", 7); });
}
TEST_F(StoreTest, GetFactoryRunsOnlyForMissingValues)
{
    int calls    = 0;
    auto factory = [&] {
        ++calls;
        return 7;
    };
    store.set("/present", 3);
    EXPECT_EQ(store.get<int>("/present", factory), 3);
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(store.get<int>("/missing", factory), 7);
    EXPECT_EQ(calls, 1);
    store.set("/wrong", "text");
    expect_error(config::ErrorCode::type, [&] { (void)store.get<int>("/wrong", factory); });
    store.set("/null", nullptr);
    expect_error(config::ErrorCode::type, [&] { (void)store.get<int>("/null", factory); });
    EXPECT_EQ(calls, 1);
    EXPECT_TRUE(store
                    .get<json>("/null",
                               [&]() -> json {
                                   if (store.contains("/null"))
                                       throw std::logic_error("Null is present");
                                   return 7;
                               })
                    .is_null());
    EXPECT_TRUE(store.get<json>("/null", json(7)).is_null());
}
TEST_F(StoreTest, GetFactoryIsSelectedBeforeValueConversions)
{
    EXPECT_FALSE(store.get<bool>("/missing", [] { return false; }));
    EXPECT_EQ(store.get<std::string>("/missing", [] { return "localhost"; }), "localhost");
    struct Callable
    {
        int operator()() const
        {
            return 7;
        }
        operator int() const
        {
            return 99;
        }
    };
    EXPECT_EQ(store.get<int>("/missing", Callable{}), 7);
}
TEST_F(StoreTest, GetFactoryForwardsMoveOnlyAndMutableCallables)
{
    EXPECT_EQ(store.get<int>("/missing", [value = std::make_unique<int>(42)] { return *value; }), 42);
    auto factory       = [calls = 0]() mutable { return ++calls; };
    const auto &reader = store;
    EXPECT_EQ(reader.get<int>("/missing", factory), 1);
    EXPECT_EQ(reader.get<int>("/missing", factory), 2);
    struct RvalueOnly
    {
        int operator()() &&
        {
            return 5;
        }
    };
    static_assert(AcceptsFallback<int, RvalueOnly>);
    static_assert(!AcceptsFallback<int, RvalueOnly &>);
    EXPECT_EQ(reader.get<int>("/missing", RvalueOnly{}), 5);
}
TEST_F(StoreTest, GetFactoryExceptionsPropagateUnchanged)
{
    const auto revision = store.revision();
    EXPECT_THROW((void)store.get<int>("/missing",
                                      [&]() -> int {
                                          if (!store.contains("/missing"))
                                              throw std::logic_error("Factory failed");
                                          return 7;
                                      }),
                 std::logic_error);
    EXPECT_THROW((void)store.get<int>("/missing", [&] { return store.snapshot().at("absent").get<int>(); }),
                 json::out_of_range);
    EXPECT_EQ(store.revision(), revision);
    EXPECT_FALSE(store.dirty());
    EXPECT_TRUE(store.snapshot().empty());
}
TEST(StoreReads, FallbacksDoNotWriteOrPublishAndCanReadAgain)
{
    TempDirectory dir;
    auto backend = std::make_shared<ControlledBackend>();
    config::Store store(dir.file(), {.defaults = {{"port", 8080}}, .backend = backend});
    int events          = 0;
    auto subscription   = store.subscribe([&](const auto &) { ++events; });
    const auto revision = store.revision();
    const auto caller   = std::this_thread::get_id();
    EXPECT_EQ(store.get<int>("/missing", 80), 80);
    EXPECT_EQ(store.get<int>("/missing",
                             [&] {
                                 EXPECT_EQ(std::this_thread::get_id(), caller);
                                 return store.get<int>("/port");
                             }),
              8080);
    EXPECT_EQ(store.revision(), revision);
    EXPECT_FALSE(store.contains("/missing"));
    EXPECT_FALSE(store.dirty());
    EXPECT_EQ(backend->writes, 0);
    EXPECT_EQ(events, 0);
    EXPECT_FALSE(std::filesystem::exists(dir.file()));
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
    int calls = 0;
    for (const auto *key : {"", "bare", "/bad~2", "/trailing~"})
    {
        expect_error(config::ErrorCode::path, [&] { (void)store.get<int>(key); });
        expect_error(config::ErrorCode::path, [&] { (void)store.get<int>(key, 2); });
        expect_error(config::ErrorCode::path, [&] { (void)store.get<int>(key, [&] { return ++calls; }); });
        expect_error(config::ErrorCode::path, [&] { store.set(key, 2); });
        expect_error(config::ErrorCode::path, [&] { (void)store.contains(key); });
        expect_error(config::ErrorCode::path, [&] { store.erase(key); });
    }
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(store.snapshot(), json({{"good", 1}}));
}
TEST_F(StoreTest, EraseNestedKeysAndArrayElements)
{
    store.set("/a", json::array({1, 2, 3}));
    EXPECT_TRUE(store.erase("/a/1"));
    EXPECT_EQ(store.get<json>("/a"), json::array({1, 3}));
    EXPECT_FALSE(store.erase("/missing"));
    store.set("/object/leaf", 5);
    EXPECT_TRUE(store.erase("/object"));
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
        if (data.at("value").get<int>() == 2)
            throw std::logic_error("reject");
    }),
                 std::logic_error);
    EXPECT_EQ(store.get<int>("/value"), 1);
}
TEST_F(StoreTest, UpdateSupportsMoveOnlyCallbacks)
{
    int calls = 0;
    store.update([value = std::make_unique<int>(42), &calls](json &data) {
        ++calls;
        data["value"] = *value;
    });
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(store.get<int>("/value"), 42);
    EXPECT_TRUE(store.dirty());
}
TEST_F(StoreTest, InvalidSnapshotViewThrowsWithoutChangingState)
{
    store.set("/value", 1);
    const auto revision = store.revision();
    expect_error(config::ErrorCode::type, [&] { (void)store.snapshot(static_cast<config::View>(99)); });
    EXPECT_EQ(store.revision(), revision);
    EXPECT_EQ(store.snapshot(), json({{"value", 1}}));
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
    expect_error(config::ErrorCode::path, [] { (void)config::user_config_directory("../escape"); });
    expect_error(config::ErrorCode::path, [] { (void)config::user_config_directory(""); });
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
