#include "test_support.hpp"
using namespace config_test;

TEST(Sources, AllReadMethodsUseTheEffectiveView)
{
    TempDirectory dir;
    config::Store store(dir.file(), {.save = config::SaveMode::manual, .defaults = {{"port", 8080}}});
    EXPECT_EQ(store.get<int>("/port"), 8080);
    EXPECT_EQ(store.get<int>("/port", 3000), 8080);
    EXPECT_EQ(store.get<int>("/port",
                             [&]() -> int {
                                 if (store.contains("/port"))
                                     throw std::logic_error("Default already supplies the value");
                                 return 3000;
                             }),
              8080);
    EXPECT_TRUE(store.contains("/port"));
    EXPECT_EQ(store.find<int>("/port"), std::optional<int>{8080});
    EXPECT_EQ(store.snapshot().at("port"), 8080);
    EXPECT_TRUE(store.snapshot(config::View::writable).empty());
    store.set("/port", 9000);
    EXPECT_TRUE(store.erase("/port"));
    EXPECT_EQ(store.get<int>("/port"), 8080);
    const auto revision = store.revision();
    EXPECT_FALSE(store.erase("/port"));
    EXPECT_EQ(store.revision(), revision);
    store.update([](json &data) { data.clear(); });
    EXPECT_EQ(store.get<int>("/port"), 8080);
}
TEST(Sources, LayerPrecedenceAndEnvironmentAreNotWrittenBack)
{
    TempDirectory dir;
    ScopedEnv variable("CONFIG_V2_SERVER_PORT", "9090");
    write(dir.file("base.json"), R"({"server":{"port":80,"host":"layer"}})");
    write(dir.file("later.json"), R"({"theme":"light"})");
    write(dir.file(), R"({"server":{"port":81},"theme":"file"})");
    config::Store store(dir.file(), {.defaults   = {{"server", {{"port", 70}}}, {"defaultOnly", true}},
                                     .layers     = {{dir.file("base.json")}, {dir.file("later.json")}},
                                     .env_prefix = "CONFIG_V2_"});
    EXPECT_EQ(store.get<int>("/server/port"), 9090);
    EXPECT_EQ(store.get<std::string>("/server/host"), "layer");
    EXPECT_EQ(store.get<std::string>("/theme"), "file");
    EXPECT_EQ(store.find<int>("/server/port"), std::optional<int>{9090});
    EXPECT_EQ(store.find<std::string>("/server/host"), std::optional<std::string>{"layer"});
    EXPECT_EQ(store.find<std::string>("/theme"), std::optional<std::string>{"file"});
    EXPECT_EQ(store.find<bool>("/defaultOnly"), std::optional<bool>{true});
    store.set("/theme", "dark");
    const auto persisted = json::parse(read(dir.file())).at("values");
    EXPECT_EQ(persisted["server"]["port"], 81);
    EXPECT_FALSE(persisted["server"].contains("host"));
    EXPECT_FALSE(persisted.contains("defaultOnly"));
    EXPECT_EQ(persisted["theme"], "dark");
}
TEST(Sources, EnvironmentBindingsOverridePrefixAndRefreshOnReload)
{
    TempDirectory dir;
    ScopedEnv prefix("CONFIG_V2_PORT", "12");
    ScopedEnv bound("CONFIG_V2_BOUND", "13");
    config::Store store(dir.file(), {.env_prefix = "CONFIG_V2_", .env_bindings = {{"/port", "CONFIG_V2_BOUND"}}});
    EXPECT_EQ(store.get<int>("/port"), 13);
    {
        ScopedEnv changed("CONFIG_V2_BOUND", "14");
        store.reload();
        EXPECT_EQ(store.get<int>("/port"), 14);
    }
    store.reload();
    EXPECT_EQ(store.get<int>("/port"), 13);
}
TEST(Sources, EnvironmentSupportsJsonAndUtf8)
{
    TempDirectory dir;
    ScopedEnv number("CONFIG_V2_NUMBER", "4");
    ScopedEnv text("CONFIG_V2_TEXT", "hello");
    ScopedEnv array("CONFIG_V2_ARRAY", "[1,2]");
    config::Store store(dir.file(), {.env_prefix = "CONFIG_V2_"});
    EXPECT_EQ(store.get<int>("/number"), 4);
    EXPECT_EQ(store.get<std::string>("/text"), "hello");
    EXPECT_EQ(store.get<json>("/array"), json::array({1, 2}));
#if defined(_WIN32)
    ASSERT_TRUE(SetEnvironmentVariableW(L"CONFIG_V2_UTF8", L"配置"));
    store.reload();
    EXPECT_EQ(store.get<std::string>("/utf8"), "配置");
    ASSERT_TRUE(SetEnvironmentVariableW(L"CONFIG_V2_UTF8", nullptr));
#endif
}
TEST(Sources, EncodedFilesUseSameCodecForMergeAndLayers)
{
    TempDirectory dir;
    {
        config::Store source(dir.file("layer.json"));
        source.set("/value", "alpha", config::Encoding::base64);
    }
    config::Store layered(dir.file(), {.layers = {{dir.file("layer.json")}}});
    EXPECT_EQ(layered.get<std::string>("/value"), "alpha");
    EXPECT_FALSE(layered.contains("/$config"));
    config::Store merged(dir.file("merged.json"));
    merged.merge_file(dir.file("layer.json"));
    merged.reload();
    EXPECT_EQ(merged.get<std::string>("/value"), "alpha");
}
TEST(Sources, RequiredMissingAndCorruptOptionalLayersAreErrors)
{
    TempDirectory dir;
    expect_error(config::ErrorCode::read,
                 [&] { config::Store store(dir.file(), {.layers = {{dir.file("missing")}}}); });
    {
        config::Store store(dir.file(), {.layers = {{dir.file("missing"), true}}});
        EXPECT_TRUE(store.snapshot().empty());
    }
    write(dir.file("bad"), "{invalid");
    expect_error(config::ErrorCode::parse,
                 [&] { config::Store store(dir.file(), {.layers = {{dir.file("bad"), true}}}); });
}
TEST(Sources, ReloadRebuildsLayersWithoutAccumulatingStaleKeys)
{
    TempDirectory dir;
    write(dir.file("layer"), R"({"old":1})");
    config::Store store(dir.file(), {.layers = {{dir.file("layer")}}});
    int calls = 0;
    auto sub  = store.subscribe([&](auto &) { ++calls; });
    write(dir.file("layer"), R"({"new":2})");
    store.reload();
    EXPECT_FALSE(store.contains("/old"));
    EXPECT_EQ(store.get<int>("/new"), 2);
    EXPECT_EQ(calls, 1);
}
TEST(Sources, ValidationCoversConstructionAndEveryMutation)
{
    TempDirectory dir;
    auto validator = [](const json &data) {
        if (data.contains("value") && data.at("value").get<int>() < 0)
            throw std::runtime_error("negative");
    };
    write(dir.file(), R"({"value":-1})");
    expect_error(config::ErrorCode::validation, [&] { config::Store store(dir.file(), {.validate = validator}); });
    write(dir.file(), R"({"value":1})");
    config::Store store(dir.file(), {.validate = validator});
    expect_error(config::ErrorCode::validation, [&] { store.set("/value", -1); });
    expect_error(config::ErrorCode::validation, [&] { store.merge(json{{"value", -1}}); });
    expect_error(config::ErrorCode::validation, [&] { store.update([](json &data) { data["value"] = -1; }); });
    write(dir.file("bad"), R"({"value":-1})");
    expect_error(config::ErrorCode::validation, [&] { store.merge_file(dir.file("bad")); });
    write(dir.file(), R"({"value":-1})");
    expect_error(config::ErrorCode::validation, [&] { store.reload(); });
    EXPECT_EQ(store.get<int>("/value"), 1);
}
TEST(Sources, ValidationCanReadCommittedState)
{
    TempDirectory dir;
    config::Store *current = nullptr;
    config::Store store(dir.file(), {.validate = [&](const json &) {
                            if (current)
                                EXPECT_EQ(current->get<int>("/value"), 1);
                        }});
    store.set("/value", 1);
    current = &store;
    store.set("/value", 2);
    current = nullptr;
    EXPECT_EQ(store.get<int>("/value"), 2);
}
