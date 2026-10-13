#include "test_support.hpp"
using namespace config_test;
TEST(Subscriptions, AllMutationEntrypointsEmitBatchEvents)
{
    TempDirectory dir;
    config::Store store(dir.file(), {.save = config::SaveMode::manual});
    std::vector<config::Change> events;
    auto subscription = store.subscribe([&](const auto &event) { events.push_back(event); });
    store.set("/value", 1);
    store.merge(json{{"merged", 2}});
    write(dir.file("overlay"), R"({"imported":3})");
    store.merge_file(dir.file("overlay"));
    store.erase("/merged");
    store.update([](json &data) { data = {{"replacement", 4}}; });
    write(dir.file(), R"({"loaded":5})");
    store.reload();
    EXPECT_EQ(events.size(), 6u);
    for (std::size_t i = 1; i < events.size(); ++i)
    {
        EXPECT_LT(events[i - 1].revision, events[i].revision);
        EXPECT_EQ(events[i - 1].after, events[i].before);
    }
    store.set("/loaded", 5);
    store.save();
    EXPECT_EQ(events.size(), 6u);
}
TEST(Subscriptions, MutableCallbackStatePersists)
{
    TempDirectory dir;
    config::Store store(dir.file(), {.save = config::SaveMode::manual});
    std::vector<int> seen;
    auto sub = store.subscribe([count = 0, &seen](const auto &) mutable { seen.push_back(++count); });
    store.set("/v", 1);
    store.set("/v", 2);
    EXPECT_EQ(seen, (std::vector<int>{1, 2}));
}
TEST(Subscriptions, CallbackMaySubscribeDisconnectAndMutate)
{
    TempDirectory dir;
    config::Store store(dir.file(), {.save = config::SaveMode::manual});
    std::vector<int> seen;
    config::Subscription second;
    auto first = store.subscribe([&](const auto &event) {
        seen.push_back(event.after.at("v").template get<int>());
        if (!second.connected())
        {
            second =
                store.subscribe([&](const auto &next) { seen.push_back(next.after.at("v").template get<int>() * 10); });
            store.set("/v", 2);
        }
    });
    store.set("/v", 1);
    EXPECT_EQ(seen, (std::vector<int>{1, 2, 20}));
    first.disconnect();
    second.disconnect();
    EXPECT_FALSE(first.connected());
    EXPECT_FALSE(second.connected());
}
TEST(Subscriptions, DisconnectDuringDispatchSuppressesLaterCallback)
{
    TempDirectory dir;
    config::Store store(dir.file(), {.save = config::SaveMode::manual});
    config::Subscription second;
    int calls  = 0;
    auto first = store.subscribe([&](auto &) { second.disconnect(); });
    second     = store.subscribe([&](auto &) { ++calls; });
    store.set("/v", 1);
    EXPECT_EQ(calls, 0);
}
TEST(Subscriptions, OutlivingStoreIsSafeAndReportsDisconnected)
{
    TempDirectory dir;
    config::Subscription sub;
    {
        config::Store store(dir.file());
        sub = store.subscribe([](auto &) {});
        EXPECT_TRUE(sub.connected());
    }
    EXPECT_FALSE(sub.connected());
    sub.disconnect();
}
TEST(Subscriptions, ExceptionsAreReportedWithoutStoppingOtherListeners)
{
    TempDirectory dir;
    int errors = 0;
    int calls  = 0;
    config::Store store(dir.file(),
                        {.save = config::SaveMode::manual, .on_callback_error = [&](std::exception_ptr error) {
                             ++errors;
                             EXPECT_THROW(std::rethrow_exception(error), std::logic_error);
                         }});
    auto bad  = store.subscribe([](auto &) { throw std::logic_error("listener"); });
    auto good = store.subscribe([&](auto &) { ++calls; });
    store.set("/v", 1);
    EXPECT_EQ(errors, 1);
    EXPECT_EQ(calls, 1);
}
TEST(Subscriptions, MoveAssignmentDisconnectsPreviousSubscription)
{
    TempDirectory dir;
    config::Store store(dir.file());
    int a = 0, b = 0;
    auto first  = store.subscribe([&](auto &) { ++a; });
    auto second = store.subscribe([&](auto &) { ++b; });
    first       = std::move(second);
    store.set("/v", 1);
    EXPECT_EQ(a, 0);
    EXPECT_EQ(b, 1);
    EXPECT_FALSE(second.connected());
}
TEST(Subscriptions, StoreMayBeDestroyedByItsCallback)
{
    TempDirectory dir;
    auto store = std::make_unique<config::Store>(dir.file());
    auto sub   = store->subscribe([&](auto &) { store.reset(); });
    store->set("/v", 1);
    EXPECT_FALSE(store);
    EXPECT_FALSE(sub.connected());
}
