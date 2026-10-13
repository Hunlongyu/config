#include "test_support.hpp"
using namespace config_test;

TEST(Persistence, MissingEraseAndFindHaveNoSideEffects)
{
    TempDirectory dir;
    auto backend    = std::make_shared<ControlledBackend>();
    int validations = 0;
    config::Store store(
        dir.file(),
        {.defaults = {{"default", 7}}, .validate = [&](const json &) { ++validations; }, .backend = backend});
    int events                     = 0;
    auto subscription              = store.subscribe([&](const auto &) { ++events; });
    const auto initial_validations = validations;
    backend->fail                  = true;
    EXPECT_FALSE(store.erase("/absent"));
    EXPECT_FALSE(store.erase("/default"));
    EXPECT_EQ(store.find<int>("/default"), std::optional<int>{7});
    EXPECT_EQ(store.find<int>("/absent"), std::nullopt);
    EXPECT_EQ(store.revision(), 0u);
    EXPECT_FALSE(store.dirty());
    EXPECT_EQ(validations, initial_validations);
    EXPECT_EQ(backend->writes, 0);
    EXPECT_EQ(events, 0);
    EXPECT_FALSE(std::filesystem::exists(dir.file()));
}

TEST(Persistence, MissingErasePreservesCleanAndDirtyManualState)
{
    TempDirectory dir;
    auto backend = std::make_shared<ControlledBackend>();
    config::Store store(dir.file(), {.save = config::SaveMode::manual, .backend = backend});
    auto check_noop = [&] {
        const auto revision = store.revision();
        const auto dirty    = store.dirty();
        const auto writes   = backend->writes.load();
        const auto data     = store.snapshot();
        EXPECT_FALSE(store.erase("/absent"));
        EXPECT_EQ(store.revision(), revision);
        EXPECT_EQ(store.dirty(), dirty);
        EXPECT_EQ(backend->writes, writes);
        EXPECT_EQ(store.snapshot(), data);
    };
    check_noop();
    store.set("/value", 1);
    check_noop();
    store.save();
    check_noop();
    EXPECT_TRUE(store.erase("/value"));
    EXPECT_TRUE(store.dirty());
    EXPECT_EQ(backend->writes, 1);
    check_noop();
}

TEST(Persistence, ErasePublishesOnlyAfterSuccessfulWrite)
{
    TempDirectory dir;
    auto backend = std::make_shared<ControlledBackend>();
    config::Store store(dir.file(), {.backend = backend});
    store.set("/value", nullptr);
    const auto revision = store.revision();
    const auto disk     = read(dir.file());
    int events          = 0;
    auto subscription   = store.subscribe([&](const auto &) { ++events; });
    backend->fail       = true;
    EXPECT_FALSE(store.erase("/absent"));
    expect_error(config::ErrorCode::write, [&] { store.erase("/value"); });
    EXPECT_TRUE(store.contains("/value"));
    EXPECT_EQ(store.revision(), revision);
    EXPECT_FALSE(store.dirty());
    EXPECT_EQ(read(dir.file()), disk);
    EXPECT_EQ(events, 0);
    backend->fail = false;
    EXPECT_TRUE(store.erase("/value"));
    EXPECT_EQ(store.revision(), revision + 1);
    EXPECT_FALSE(store.dirty());
    EXPECT_EQ(json::parse(read(dir.file())).at("values"), json::object());
    EXPECT_EQ(events, 1);
    const auto writes = backend->writes.load();
    EXPECT_FALSE(store.erase("/value"));
    EXPECT_EQ(backend->writes, writes);
    EXPECT_EQ(store.revision(), revision + 1);
    EXPECT_EQ(events, 1);
}

TEST(Persistence, AutomaticCommitsFileBeforeNotifying)
{
    TempDirectory dir;
    config::Store store(dir.file());
    int calls         = 0;
    auto subscription = store.subscribe([&](const config::Change &change) {
        EXPECT_EQ(json::parse(read(dir.file())).at("values"), change.after);
        EXPECT_EQ(store.snapshot(), change.after);
        ++calls;
    });
    store.set("/value", 7);
    EXPECT_EQ(calls, 1);
    EXPECT_FALSE(store.dirty());
}
TEST(Persistence, ManualSaveAndReload)
{
    TempDirectory dir;
    config::Store store(dir.file(), {.save = config::SaveMode::manual});
    store.set("/value", 1);
    EXPECT_FALSE(std::filesystem::exists(dir.file()));
    EXPECT_TRUE(store.dirty());
    store.save();
    EXPECT_FALSE(store.dirty());
    store.set("/value", 2);
    store.reload();
    EXPECT_EQ(store.get<int>("/value"), 1);
    EXPECT_FALSE(store.dirty());
}
TEST(Persistence, MissingFileStartsEmptyButCorruptFileThrows)
{
    TempDirectory dir;
    {
        config::Store empty(dir.file());
        EXPECT_TRUE(empty.snapshot().empty());
    }
    write(dir.file(), "{invalid");
    expect_error(config::ErrorCode::parse, [&] { config::Store corrupt(dir.file()); });
}
TEST(Persistence, CorruptReloadPreservesMemoryRevisionAndEvents)
{
    TempDirectory dir;
    config::Store store(dir.file());
    store.set("/stable", 7);
    const auto version = store.revision();
    int calls          = 0;
    auto sub           = store.subscribe([&](auto &) { ++calls; });
    write(dir.file(), "{invalid");
    expect_error(config::ErrorCode::parse, [&] { store.reload(); });
    EXPECT_EQ(store.get<int>("/stable"), 7);
    EXPECT_EQ(store.revision(), version);
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(read(dir.file()), "{invalid");
}
TEST(Persistence, NonObjectRootRejectedEverywhere)
{
    TempDirectory dir;
    write(dir.file(), "[]");
    expect_error(config::ErrorCode::type, [&] { config::Store store(dir.file()); });
    write(dir.file(), "{}");
    config::Store store(dir.file());
    write(dir.file(), "[]");
    expect_error(config::ErrorCode::type, [&] { store.reload(); });
    expect_error(config::ErrorCode::type, [&] { store.merge_file(dir.file()); });
    EXPECT_TRUE(store.snapshot().is_object());
}
TEST(Persistence, AutoWriteFailureRollsBackEveryMutation)
{
    TempDirectory dir;
    auto backend = std::make_shared<ControlledBackend>();
    config::Store store(dir.file(), {.backend = backend});
    store.set("/stable", 1);
    const auto file    = read(dir.file());
    const auto version = store.revision();
    int calls          = 0;
    auto sub           = store.subscribe([&](auto &) { ++calls; });
    backend->fail      = true;
    expect_error(config::ErrorCode::write, [&] { store.set("/stable", 2); });
    expect_error(config::ErrorCode::write, [&] { store.erase("/stable"); });
    expect_error(config::ErrorCode::write, [&] { store.merge(json{{"new", 2}}); });
    expect_error(config::ErrorCode::write, [&] { store.update([](json &data) { data.clear(); }); });
    write(dir.file("overlay.json"), R"({"imported":3})");
    expect_error(config::ErrorCode::write, [&] { store.merge_file(dir.file("overlay.json")); });
    EXPECT_EQ(store.snapshot(), json({{"stable", 1}}));
    EXPECT_EQ(read(dir.file()), file);
    EXPECT_EQ(store.revision(), version);
    EXPECT_EQ(calls, 0);
}
TEST(Persistence, InvalidUtf8FailsBeforeReplacingFile)
{
    TempDirectory dir;
    config::Store store(dir.file());
    store.set("/stable", 7);
    const auto file = read(dir.file());
    expect_error(config::ErrorCode::format, [&] { store.set("/bad", std::string(1, static_cast<char>(0xff))); });
    EXPECT_EQ(read(dir.file()), file);
    EXPECT_FALSE(store.contains("/bad"));
}
TEST(Persistence, ReplaceFailureLeavesDirectoryAndNoTemporaryFiles)
{
    TempDirectory dir;
    config::Store store(dir.file(), {.save = config::SaveMode::manual});
    store.set("/value", 1);
    std::filesystem::create_directory(dir.file());
    write(dir.file() / "keep", "old");
    expect_error(config::ErrorCode::write, [&] { store.save(); });
    EXPECT_EQ(read(dir.file() / "keep"), "old");
    EXPECT_TRUE(store.dirty());
    EXPECT_EQ(std::distance(std::filesystem::directory_iterator(dir.path()), std::filesystem::directory_iterator()), 1);
}
TEST(Persistence, ParentCreationFailureIsReported)
{
    TempDirectory dir;
    write(dir.file("blocker"), "keep");
    EXPECT_THROW(config::Store store(dir.file("blocker") / "config.json"), config::Error);
}
TEST(Persistence, UnicodeFilePathsAndCompactFormat)
{
    TempDirectory dir;
    const auto file = dir.path() / std::filesystem::path(u8"配置") / std::filesystem::path(u8"设置.json");
    {
        config::Store store(file, {.format = config::Format::compact});
        store.set("/value", "你好🌍");
    }
    EXPECT_EQ(read(file).find('\n'), std::string::npos);
    config::Store store(file);
    EXPECT_EQ(store.get<std::string>("/value"), "你好🌍");
}
TEST(Persistence, FilesystemPathValuesPersistAsUtf8)
{
    TempDirectory dir;
    const std::filesystem::path value(u8"目录/设置🌍.json");
    const std::string expected = "目录/设置🌍.json";
    {
        config::Store store(dir.file());
        store.set("/path", value);
        EXPECT_EQ(store.get<std::string>("/path"), expected);
        EXPECT_EQ(store.get<std::filesystem::path>("/path"), value);
        EXPECT_EQ(json::parse(read(dir.file())).at("values").at("path"), expected);
    }
    config::Store reopened(dir.file());
    EXPECT_EQ(reopened.get<std::string>("/path"), expected);
    EXPECT_EQ(reopened.get<std::filesystem::path>("/path"), value);
}
TEST(Persistence, AllEncodingsRoundTripWithoutChangingReservedFields)
{
    TempDirectory dir;
    config::Store store(dir.file(), {.save = config::SaveMode::manual});
    const std::vector<std::string> values = {"", "f", "fo", "foo", "你好🌍", std::string("a\0b", 3)};
    for (int encoding = 0; encoding <= 5; ++encoding)
        for (std::size_t i = 0; i < values.size(); ++i)
            store.set("/e" + std::to_string(encoding) + "/v" + std::to_string(i), values[i],
                      static_cast<config::Encoding>(encoding));
    store.set("/__obfuscate_meta__", json{{"business", true}});
    store.set("/$config", "business field");
    const auto expected = store.snapshot();
    store.save();
    store.reload();
    EXPECT_EQ(store.snapshot(), expected);
}
TEST(Persistence, ParentReplacementAndArrayRemovalMaintainEncodingRules)
{
    TempDirectory dir;
    config::Store store(dir.file());
    store.set("/object/secret", "old", config::Encoding::base64);
    store.set("/object", json{{"plain", "new"}});
    store.set("/array", json::array({"one", "two", "three"}));
    store.set("/array/0", "one", config::Encoding::base64);
    store.set("/array/2", "three", config::Encoding::hex);
    store.erase("/array/1");
    store.reload();
    EXPECT_EQ(store.get<json>("/array"), json::array({"one", "three"}));
    const auto rules = json::parse(read(dir.file())).at("encoding");
    EXPECT_EQ(rules.size(), 2u);
    EXPECT_TRUE(rules.contains("/array/1"));
    EXPECT_FALSE(rules.contains("/object/secret"));
}
TEST(Persistence, ReloadReplacesOldEncodingRules)
{
    TempDirectory dir;
    config::Store store(dir.file());
    store.set("/value", "encoded", config::Encoding::base64);
    write(dir.file(), R"({"value":"plain"})");
    store.reload();
    store.save();
    EXPECT_TRUE(json::parse(read(dir.file())).at("encoding").empty());
}
TEST(Persistence, MalformedEncodingAndEnvelopeFailWithoutCommit)
{
    TempDirectory dir;
    config::Store store(dir.file());
    store.set("/stable", 1);
    for (const auto &bad : {json{{"$config", "config/v1"}, {"values", {{"v", "!"}}}, {"encoding", {{"/v", 1}}}},
                            json{{"$config", "config/v1"}, {"values", {{"v", "xyz"}}}, {"encoding", {{"/v", 2}}}},
                            json{{"$config", "config/v1"}, {"values", {{"v", "x"}}}, {"encoding", {{"/v", 99}}}},
                            json{{"$config", "config/v1"}, {"values", json::object()}, {"encoding", {{"/absent", 1}}}},
                            json{{"$config", "config/v1"}, {"values", json::object()}}})
    {
        write(dir.file(), bad.dump());
        expect_error(config::ErrorCode::format, [&] { store.reload(); });
        EXPECT_EQ(store.get<int>("/stable"), 1);
    }
}
TEST(Persistence, LegacyEncodingImportIsExplicit)
{
    TempDirectory dir;
    const auto text = R"({"value":"YWxwaGE=","__obfuscate_meta__":{"/value":1}})";
    write(dir.file(), text);
    {
        config::Store plain(dir.file());
        EXPECT_TRUE(plain.contains("/__obfuscate_meta__"));
        EXPECT_EQ(plain.get<std::string>("/value"), "YWxwaGE=");
    }
    {
        config::Store imported(dir.file(), {.legacy_encoding = true});
        EXPECT_EQ(imported.get<std::string>("/value"), "alpha");
        imported.save();
    }
    config::Store migrated(dir.file());
    EXPECT_EQ(migrated.get<std::string>("/value"), "alpha");
}

#if defined(_WIN32)
TEST(Persistence, FileSharingDenialPreservesMemoryAndDisk)
{
    TempDirectory dir;
    config::Store store(dir.file());
    store.set("/value", 1);
    const auto before = read(dir.file());
    struct Handle
    {
        HANDLE value;
        ~Handle()
        {
            if (value != INVALID_HANDLE_VALUE)
                CloseHandle(value);
        }
    } handle{CreateFileW(dir.file().c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr)};
    ASSERT_NE(handle.value, INVALID_HANDLE_VALUE);
    expect_error(config::ErrorCode::read, [&] { store.reload(); });
    expect_error(config::ErrorCode::write, [&] { store.set("/value", 2); });
    EXPECT_EQ(store.get<int>("/value"), 1);
    CloseHandle(handle.value);
    handle.value = INVALID_HANDLE_VALUE;
    EXPECT_EQ(read(dir.file()), before);
    EXPECT_EQ(std::distance(std::filesystem::directory_iterator(dir.path()), std::filesystem::directory_iterator()), 1);
}
#endif
