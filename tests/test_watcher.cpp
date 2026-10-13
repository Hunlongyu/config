#include "test_support.hpp"
#include <config/watcher.hpp>
#include <future>
using namespace config_test;

TEST(Watcher, DetectsContentChangeEvenWhenTimestampIsUnchanged)
{
    TempDirectory dir;
    config::Store store(dir.file());
    store.set("/value", 1);
    const auto stamp = std::filesystem::last_write_time(dir.file());
    config::Watcher watcher(store, 10ms);
    write(dir.file(), R"({"value":2})");
    std::filesystem::last_write_time(dir.file(), stamp);
    EXPECT_TRUE(eventually([&] { return store.get<int>("/value") == 2; }));
    EXPECT_FALSE(watcher.last_error().has_value());
}
TEST(Watcher, RejectedVersionIsRetriedWithoutAnotherFileChange)
{
    TempDirectory dir;
    std::atomic<bool> reject  = false;
    std::atomic<int> attempts = 0;
    config::Store store(dir.file(), {.validate = [&](const json &data) {
                            if (data.value("value", 0) == 2)
                            {
                                ++attempts;
                                if (reject.exchange(false))
                                    throw std::runtime_error("temporary rejection");
                            }
                        }});
    store.set("/value", 1);
    std::atomic<int> errors = 0;
    config::Watcher watcher(store, 10ms, [&](const config::Error &) { ++errors; });
    reject = true;
    write(dir.file(), R"({"value":2})");
    EXPECT_TRUE(eventually([&] { return store.get<int>("/value") == 2; }));
    EXPECT_GE(attempts, 2);
    EXPECT_GE(errors, 1);
}
TEST(Watcher, ConcurrentCommitConflictRetriesTheSameCandidate)
{
    TempDirectory dir;
    Gate validation;
    std::atomic<bool> armed = false;
    config::Store store(dir.file(), {.validate = [&](const json &data) {
                            if (data.value("value", 0) == 2 && armed.exchange(false))
                                validation.block();
                        }});
    store.set("/value", 1);
    std::atomic<int> errors = 0;
    config::Watcher watcher(store, 10ms, [&](const config::Error &error) {
        if (error.code() == config::ErrorCode::conflict)
            ++errors;
    });
    armed = true;
    write(dir.file(), R"({"value":2})");
    ASSERT_TRUE(validation.entered());
    store.save(); // 推进提交版本，迫使已经准备好的 reload 检查冲突。
    write(dir.file(), R"({"value":2})");
    validation.release();
    EXPECT_TRUE(eventually([&] { return store.get<int>("/value") == 2; }));
    EXPECT_GE(errors, 1);
}
TEST(Watcher, CorruptFilePreservesLastGoodStateAndExposesError)
{
    TempDirectory dir;
    config::Store store(dir.file());
    store.set("/value", 1);
    config::Watcher watcher(store, 10ms);
    write(dir.file(), "{invalid");
    EXPECT_TRUE(eventually([&] { return watcher.last_error().has_value(); }));
    EXPECT_EQ(store.get<int>("/value"), 1);
    write(dir.file(), R"({"value":3})");
    EXPECT_TRUE(eventually([&] { return store.get<int>("/value") == 3; }));
    EXPECT_TRUE(eventually([&] { return !watcher.last_error().has_value(); }));
}
TEST(Watcher, UnsavedManualChangesAreNotDiscarded)
{
    TempDirectory dir;
    config::Store store(dir.file(), {.save = config::SaveMode::manual});
    store.set("/value", 1);
    store.save();
    store.set("/value", 2);
    config::Watcher watcher(store, 10ms);
    write(dir.file(), R"({"value":3})");
    EXPECT_TRUE(eventually([&] { return watcher.last_error().has_value(); }));
    EXPECT_EQ(store.get<int>("/value"), 2);
    store.reload(); // 用户显式决定丢弃未保存修改。
    EXPECT_EQ(store.get<int>("/value"), 3);
}
TEST(Watcher, StopInterruptsLongPollingIntervalAndIsIdempotent)
{
    TempDirectory dir;
    config::Store store(dir.file());
    config::Watcher watcher(store, std::chrono::hours(1));
    const auto begin = std::chrono::steady_clock::now();
    watcher.stop();
    watcher.stop();
    EXPECT_LT(std::chrono::steady_clock::now() - begin, 1s);
}
TEST(Watcher, MayOutliveStore)
{
    TempDirectory dir;
    auto store   = std::make_unique<config::Store>(dir.file());
    auto watcher = std::make_unique<config::Watcher>(*store, 10ms);
    store.reset();
    watcher->stop();
    EXPECT_NO_THROW(config::Store reopened(dir.file()));
}
TEST(Watcher, MayBeDestroyedFromOwnErrorCallback)
{
    TempDirectory dir;
    config::Store store(dir.file());
    store.set("/value", 1);
    std::unique_ptr<config::Watcher> watcher;
    std::atomic<bool> destroyed = false;
    watcher                     = std::make_unique<config::Watcher>(store, 20ms, [&](const config::Error &) {
        watcher.reset();
        destroyed = true;
    });
    write(dir.file(), "{invalid");
    EXPECT_TRUE(eventually([&] { return destroyed.load(); }));
}
