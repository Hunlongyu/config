#include "test_support.hpp"
#include <future>
using namespace config_test;

template <class Function> void retry_conflict(Function function)
{
    for (;;)
        try
        {
            function();
            return;
        }
        catch (const config::Error &error)
        {
            if (error.code() != config::ErrorCode::conflict)
                throw;
        }
}
TEST(Concurrency, SaveSerializesWithMutations)
{
    TempDirectory dir;
    Gate write_gate;
    auto backend = std::make_shared<ControlledBackend>();
    config::Store store(dir.file(), {.save = config::SaveMode::manual, .backend = backend});
    store.set("/value", "old");
    backend->before_write = [&](int number) {
        if (number == 1)
            write_gate.block();
    };
    auto old_save = std::async(std::launch::async, [&] { store.save(); });
    ASSERT_TRUE(write_gate.entered());
    std::promise<void> started;
    auto ready = started.get_future();
    auto newer = std::async(std::launch::async, [&] {
        started.set_value();
        retry_conflict([&] { store.set("/value", "new"); });
        store.save();
    });
    ready.wait();
    const auto blocked = newer.wait_for(100ms);
    write_gate.release();
    old_save.get();
    newer.get();
    EXPECT_EQ(blocked, std::future_status::timeout);
    EXPECT_EQ(json::parse(read(dir.file())).at("values").at("value"), "new");
    EXPECT_EQ(store.get<std::string>("/value"), "new");
}
TEST(Concurrency, ReloadConflictNeverPublishesStaleCandidate)
{
    TempDirectory dir;
    Gate validation;
    std::atomic<bool> armed = false;
    config::Store store(dir.file(), {.save = config::SaveMode::manual, .validate = [&](const json &data) {
                                         if (armed.exchange(false) && data.contains("value"))
                                             validation.block();
                                     }});
    store.set("/value", 1);
    store.save();
    write(dir.file(), R"({"value":2})");
    armed = true;
    auto reload =
        std::async(std::launch::async, [&] { expect_error(config::ErrorCode::conflict, [&] { store.reload(); }); });
    ASSERT_TRUE(validation.entered());
    EXPECT_EQ(store.get<int>("/value"), 1);
    store.set("/concurrent", 99);
    validation.release();
    reload.get();
    EXPECT_EQ(store.get<int>("/value"), 1);
    EXPECT_EQ(store.get<int>("/concurrent"), 99);
}
TEST(Concurrency, UpdateCallbackRunsOnceAndConflictIsExplicit)
{
    TempDirectory dir;
    Gate edit;
    config::Store store(dir.file(), {.save = config::SaveMode::manual});
    std::atomic<int> calls = 0;
    auto old               = std::async(std::launch::async, [&] {
        expect_error(config::ErrorCode::conflict, [&] {
            store.update([&](json &data) {
                ++calls;
                data["old"] = 1;
                edit.block();
            });
        });
    });
    ASSERT_TRUE(edit.entered());
    store.set("/new", 2);
    edit.release();
    old.get();
    EXPECT_EQ(calls, 1);
    EXPECT_FALSE(store.contains("/old"));
    EXPECT_EQ(store.get<int>("/new"), 2);
}
TEST(Concurrency, ParallelUpdatesRetainEveryIncrementAndOrderedEvent)
{
    TempDirectory dir;
    config::Store store(dir.file(), {.save = config::SaveMode::manual});
    store.set("/count", 0);
    std::mutex mutex;
    std::vector<std::uint64_t> versions;
    auto sub = store.subscribe([&](const auto &event) {
        std::lock_guard lock(mutex);
        versions.push_back(event.revision);
    });
    std::vector<std::future<void>> jobs;
    for (int thread = 0; thread < 4; ++thread)
        jobs.push_back(std::async(std::launch::async, [&] {
            for (int i = 0; i < 100; ++i)
                retry_conflict([&] { store.update([](json &data) { data["count"] = data["count"].get<int>() + 1; }); });
        }));
    for (auto &job : jobs)
        job.get();
    EXPECT_EQ(store.get<int>("/count"), 400);
    ASSERT_EQ(versions.size(), 400u);
    EXPECT_TRUE(std::is_sorted(versions.begin(), versions.end()));
}
TEST(Concurrency, RegistrationCanRunAlongsideDispatch)
{
    TempDirectory dir;
    config::Store store(dir.file(), {.save = config::SaveMode::manual});
    auto registration = std::async(std::launch::async, [&] {
        for (int i = 0; i < 300; ++i)
        {
            auto sub = store.subscribe([&](auto &) { store.snapshot(); });
        }
    });
    for (int i = 0; i < 100; ++i)
        store.set("/v", i);
    registration.get();
    EXPECT_EQ(store.get<int>("/v"), 99);
}
TEST(Concurrency, ValidatorCanSaveWithoutDeadlockAndCommitDetectsConflict)
{
    TempDirectory dir;
    config::Store *current = nullptr;
    config::Store store(dir.file(), {.validate = [&](const json &) {
                            if (current)
                                current->save();
                        }});
    store.set("/value", 1);
    current = &store;
    expect_error(config::ErrorCode::conflict, [&] { store.set("/value", 2); });
    EXPECT_EQ(store.get<int>("/value"), 1);
}
