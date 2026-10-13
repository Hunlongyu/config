#include "test_support.hpp"
#include <config/version.h>
#include <future>
#include <limits>
#include <span>
#include <string_view>

using namespace config_test;

template <class T>
concept CanRead = requires(const config::Store &store) {
    { store.template get<T>("/value") } -> std::same_as<T>;
};
template <class T>
concept CanFind = requires(const config::Store &store) { store.template find<T>("/value"); };
template <class T, class F>
concept CanFallback =
    requires(const config::Store &store, F &&fallback) { store.template get<T>("/value", std::forward<F>(fallback)); };

static_assert(CanRead<int> && CanFind<int> && CanRead<std::string> && CanFind<json>);
static_assert(!CanRead<std::string_view> && !CanFind<std::string_view>);
static_assert(!CanRead<const std::string *> && !CanFind<const std::string *>);
static_assert(!CanRead<int &> && !CanFind<int &> && !CanRead<const int> && !CanRead<void>);
static_assert(!CanRead<std::span<const int>> && !CanFind<std::span<const int>>);
static_assert(!CanFallback<std::string_view, const char *>);
static_assert(!CanFallback<std::string_view, decltype([] { return "borrowed"; })>);
static_assert(!CanFallback<const std::string *, decltype([]() -> const std::string * { return nullptr; })>);
static_assert(CanFallback<bool, bool> && CanFallback<bool, decltype([] { return false; })>);
static_assert(!CanFallback<bool, decltype([](int) { return false; })>);
static_assert(!CanFallback<bool, bool (*)(int)> && !CanFallback<bool, int>);
static_assert(!CanFallback<bool, const char *> && !CanFallback<bool, decltype([] { return 1; })>);
static_assert(!CanFallback<int, bool> && !CanFallback<int, decltype([] { return true; })>);
static_assert(CanFallback<int, double> && CanFallback<int, decltype([] { return 2.0; })>);
static_assert(std::same_as<decltype(config::version()), std::string_view>);
static_assert(noexcept(config::version()) && config::version() == config::version_string);

struct WrongFactory
{
    int operator()(int) const;
    operator int() const;
};
static_assert(!CanFallback<int, WrongFactory>);

struct OwnedValue
{
    std::unique_ptr<std::string> text;
    friend void from_json(const json &value, OwnedValue &result)
    {
        result.text = std::make_unique<std::string>(value.get<std::string>());
    }
};

class ReadsTest : public testing::Test
{
  protected:
    TempDirectory dir;
    config::Store store{dir.file(), {.save = config::SaveMode::manual}};

    template <class T> void integer_boundaries()
    {
        store.set("/value", (std::numeric_limits<T>::max)());
        EXPECT_EQ(store.get<T>("/value"), (std::numeric_limits<T>::max)());
        store.set("/value", (std::numeric_limits<T>::lowest)());
        EXPECT_EQ(store.get<T>("/value"), (std::numeric_limits<T>::lowest)());
        if constexpr (std::numeric_limits<T>::digits < 64)
        {
            store.set("/value", static_cast<std::uint64_t>((std::numeric_limits<T>::max)()) + 1);
            expect_error(config::ErrorCode::type, [&] { (void)store.get<T>("/value"); });
        }
        if constexpr (!std::is_signed_v<T> || std::numeric_limits<T>::digits < 63)
        {
            store.set("/value", static_cast<std::int64_t>((std::numeric_limits<T>::lowest)()) - 1);
            expect_error(config::ErrorCode::type, [&] { (void)store.get<T>("/value"); });
        }
    }
};

TEST_F(ReadsTest, IntegerConversionsCheckBothEndsAndSignedness)
{
    integer_boundaries<std::int8_t>();
    integer_boundaries<std::uint8_t>();
    integer_boundaries<std::int32_t>();
    integer_boundaries<std::uint32_t>();
    integer_boundaries<std::int64_t>();
    integer_boundaries<std::uint64_t>();
    integer_boundaries<char>();
    integer_boundaries<wchar_t>();
    store.set("/value", std::uint64_t{42});
    EXPECT_EQ(store.get<int>("/value"), 42);
    store.set("/value", 42);
    EXPECT_EQ(store.get<unsigned>("/value"), 42u);
}

TEST_F(ReadsTest, FloatingToIntegerChecksFractionsAndExclusive64BitBounds)
{
    for (const double value : {2.9, -2.9, std::numeric_limits<double>::infinity(),
                               -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
    {
        store.set("/value", value);
        expect_error(config::ErrorCode::type, [&] { (void)store.get<int>("/value"); });
    }
    store.set("/value", 42.0);
    EXPECT_EQ(store.get<int>("/value"), 42);
    store.set("/value", -0.0);
    EXPECT_EQ(store.get<unsigned>("/value"), 0u);

    const double signed_limit = std::ldexp(1.0, 63);
    store.set("/value", -signed_limit);
    EXPECT_EQ(store.get<std::int64_t>("/value"), (std::numeric_limits<std::int64_t>::lowest)());
    store.set("/value", std::nextafter(-signed_limit, -std::numeric_limits<double>::infinity()));
    expect_error(config::ErrorCode::type, [&] { (void)store.get<std::int64_t>("/value"); });
    store.set("/value", signed_limit);
    expect_error(config::ErrorCode::type, [&] { (void)store.get<std::int64_t>("/value"); });
    store.set("/value", std::nextafter(signed_limit, 0.0));
    EXPECT_EQ(store.get<std::int64_t>("/value"), static_cast<std::int64_t>(std::nextafter(signed_limit, 0.0)));

    const double unsigned_limit = std::ldexp(1.0, 64);
    store.set("/value", unsigned_limit);
    expect_error(config::ErrorCode::type, [&] { (void)store.get<std::uint64_t>("/value"); });
    store.set("/value", std::nextafter(unsigned_limit, 0.0));
    EXPECT_EQ(store.get<std::uint64_t>("/value"), static_cast<std::uint64_t>(std::nextafter(unsigned_limit, 0.0)));
    store.set("/value", -1.0);
    expect_error(config::ErrorCode::type, [&] { (void)store.get<std::uint64_t>("/value"); });
}

TEST_F(ReadsTest, FloatingConversionsAllowRoundingButRejectInvalidRanges)
{
    store.set("/value", 0.1);
    EXPECT_EQ(store.get<float>("/value"), static_cast<float>(0.1));
    store.set("/value", (std::numeric_limits<float>::max)());
    EXPECT_EQ(store.get<float>("/value"), (std::numeric_limits<float>::max)());
    store.set("/value", std::numeric_limits<float>::denorm_min());
    EXPECT_EQ(store.get<float>("/value"), std::numeric_limits<float>::denorm_min());
    store.set("/value", (std::numeric_limits<std::uint64_t>::max)());
    EXPECT_EQ(store.get<double>("/value"), static_cast<double>((std::numeric_limits<std::uint64_t>::max)()));
    for (const double value : {(std::numeric_limits<double>::max)(), (std::numeric_limits<double>::lowest)(),
                               std::numeric_limits<double>::denorm_min(), std::numeric_limits<double>::infinity(),
                               std::numeric_limits<double>::quiet_NaN()})
    {
        store.set("/value", value);
        expect_error(config::ErrorCode::type, [&] { (void)store.get<float>("/value"); });
    }
    store.set("/value", std::numeric_limits<double>::infinity());
    expect_error(config::ErrorCode::type, [&] { (void)store.get<double>("/value"); });
}

TEST_F(ReadsTest, BooleansAndNumbersAreDistinct)
{
    store.set("/value", true);
    EXPECT_TRUE(store.get<bool>("/value"));
    expect_error(config::ErrorCode::type, [&] { (void)store.get<int>("/value"); });
    store.set("/value", 1);
    expect_error(config::ErrorCode::type, [&] { (void)store.get<bool>("/value"); });
    EXPECT_FALSE(store.get<bool>("/absent", false));
}

TEST_F(ReadsTest, AllReadOverloadsUseCheckedConversions)
{
    int calls    = 0;
    auto factory = [&] {
        ++calls;
        return 7;
    };
    store.set("/value", 2.9);
    expect_error(config::ErrorCode::type, [&] { (void)store.get<int>("/value", 7); });
    expect_error(config::ErrorCode::type, [&] { (void)store.get<int>("/value", factory); });
    expect_error(config::ErrorCode::type, [&] { (void)store.find<int>("/value"); });
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(store.get<int>("/absent", 7.0), 7);
    EXPECT_EQ(store.get<int>("/absent", [] { return 7.0; }), 7);
    expect_error(config::ErrorCode::type, [&] { (void)store.get<int>("/absent", 2.9); });
    expect_error(config::ErrorCode::type, [&] { (void)store.get<unsigned>("/absent", -1); });
    expect_error(config::ErrorCode::type, [&] { (void)store.get<std::int64_t>("/absent", std::ldexp(1.0, 63)); });
    expect_error(config::ErrorCode::type, [&] {
        (void)store.get<int>("/absent", [&] {
            ++calls;
            return 2.9;
        });
    });
    EXPECT_EQ(calls, 1);
    store.set("/value", 42);
    EXPECT_EQ(store.get<int>("/value", 2.9), 42);
    EXPECT_THROW((void)store.get<int>("/absent",
                                      [&]() -> int {
                                          if (!store.contains("/absent"))
                                              throw std::range_error("User factory");
                                          return 7;
                                      }),
                 std::range_error);
}

TEST_F(ReadsTest, FindDistinguishesMissingNullAndWrongType)
{
    EXPECT_EQ(store.find<int>("/absent"), std::nullopt);
    store.set("/zero", 0);
    EXPECT_EQ(store.find<int>("/zero"), std::optional<int>{0});
    store.set("/null", nullptr);
    const auto null = store.find<json>("/null");
    ASSERT_TRUE(null.has_value());
    EXPECT_TRUE(null->is_null());
    expect_error(config::ErrorCode::type, [&] { (void)store.find<int>("/null"); });
    store.set("/wrong", "text");
    expect_error(config::ErrorCode::type, [&] { (void)store.find<int>("/wrong"); });
    expect_error(config::ErrorCode::path, [&] { (void)store.find<int>("bare"); });
    store.set("/a~1b/x~0y", 5);
    EXPECT_EQ(store.find<int>("/a~1b/x~0y"), std::optional<int>{5});
}

TEST_F(ReadsTest, OwningAndMoveOnlyResultsSurviveChanges)
{
    store.set("/value", std::string(128, 'x'));
    const auto text  = store.get<std::string>("/value");
    const auto value = store.get<OwnedValue>("/value");
    const auto found = store.find<OwnedValue>("/value");
    ASSERT_TRUE(found);
    EXPECT_TRUE(store.erase("/value"));
    EXPECT_EQ(text, std::string(128, 'x'));
    EXPECT_EQ(*value.text, text);
    EXPECT_EQ(*found->text, text);
}

TEST_F(ReadsTest, KeyErrorsIncludeFileAndOriginalPointer)
{
    auto check = [&](config::ErrorCode code, std::string_view key, const auto &operation) {
        try
        {
            operation();
            FAIL() << "Expected key error";
        }
        catch (const config::Error &error)
        {
            EXPECT_EQ(error.code(), code);
            EXPECT_EQ(error.file(), store.path());
            EXPECT_EQ(error.key(), key);
        }
    };
    check(config::ErrorCode::missing, "/a~1b", [&] { (void)store.get<int>("/a~1b"); });
    store.set("/a~1b", 2.9);
    check(config::ErrorCode::type, "/a~1b", [&] { (void)store.find<int>("/a~1b"); });
    check(config::ErrorCode::type, "/absent", [&] { (void)store.get<int>("/absent", 2.9); });
    for (const auto key : {"", "bare", "/bad~2"})
    {
        check(config::ErrorCode::path, key, [&] { (void)store.get<int>(key); });
        check(config::ErrorCode::path, key, [&] { (void)store.find<int>(key); });
        check(config::ErrorCode::path, key, [&] { (void)store.contains(key); });
        check(config::ErrorCode::path, key, [&] { store.set(key, 1); });
        check(config::ErrorCode::path, key, [&] { store.erase(key); });
    }
    check(config::ErrorCode::path, "/a~1b/child", [&] { store.set("/a~1b/child", 1); });
    check(config::ErrorCode::type, "/encoding", [&] { store.set("/encoding", 1, config::Encoding::base64); });
    EXPECT_TRUE(config::Error(config::ErrorCode::write, "File error").key().empty());
}

struct BlockingValue
{
    inline static Gate *gate = nullptr;
    std::string first;
    std::string second;
    friend void from_json(const json &value, BlockingValue &result)
    {
        result.first = value.at("first").get<std::string>();
        gate->block();
        result.second = value.at("second").get<std::string>();
    }
};

TEST_F(ReadsTest, FindKeepsOneSnapshotAliveDuringConcurrentMutation)
{
    Gate conversion;
    BlockingValue::gate = &conversion;
    store.set("/value", json{{"first", "old"}, {"second", "old"}});
    auto reading = std::async(std::launch::async, [&] { return store.find<BlockingValue>("/value"); });
    ASSERT_TRUE(conversion.entered());
    store.set("/value", json{{"first", "new"}, {"second", "new"}});
    conversion.release();
    const auto result   = reading.get();
    BlockingValue::gate = nullptr;
    ASSERT_TRUE(result);
    EXPECT_EQ(result->first, "old");
    EXPECT_EQ(result->second, "old");
    EXPECT_EQ(store.get<std::string>("/value/second"), "new");
}
