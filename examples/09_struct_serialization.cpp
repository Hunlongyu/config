#include <config/config.hpp>
#include <iostream>
#include <vector>

struct Item
{
    std::string name;
    std::string value;
};

struct Config
{
    std::string app_name;
    int version;
    std::vector<Item> items;
};

// Strict binding: all fields must be present in JSON
CONFIG_STRUCT(Item, name, value)
CONFIG_STRUCT(Config, app_name, version, items)

// Tolerant binding: missing fields fall back to struct defaults
struct ServerConfig
{
    std::string host = "localhost";
    int port         = 8080;
    bool tls         = false;
};
CONFIG_STRUCT_WITH_DEFAULT(ServerConfig, host, port, tls)

int main()
{
    auto &store = config::get_store("example_struct.json", config::Path::Relative);

    // 1. Write individual fields (SaveStrategy::Auto saves on each set)
    store.set("app_name", "StructApp");
    store.set("version", 1);
    std::vector<Item> items = {{"item1", "value1"}, {"item2", "value2"}};
    store.set("items", items);

    // 2. Read back as a struct via root key ""
    try
    {
        auto cfg = store.get_root<Config>();
        std::cout << "App: " << cfg.app_name << " v" << cfg.version << "\n";
        for (const auto &item : cfg.items)
            std::cout << "  " << item.name << ": " << item.value << "\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    // 3. Round-trip: write a whole struct back to root
    Config updated{"StructApp", 2, {{"item3", "value3"}}};
    store.set_root(updated);

    // 4. CONFIG_STRUCT_WITH_DEFAULT: only "host" is set, port and tls use defaults
    auto &srv = config::get_store("example_server.json", config::Path::Relative);
    srv.set("host", std::string("example.com"));
    auto sc = srv.get_root<ServerConfig>();
    std::cout << "Server: " << sc.host << ":" << sc.port << (sc.tls ? " (TLS)" : "") << "\n";

    return 0;
}
