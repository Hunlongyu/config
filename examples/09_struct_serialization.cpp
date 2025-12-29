#include <config/config.hpp>
#include <iostream>
#include <vector>

// Define a struct
struct Item {
    std::string name;
    std::string value;
};

// Define a nested struct
struct Config {
    std::string app_name;
    int version;
    std::vector<Item> items;
};

// Macro to map struct fields to JSON keys
CONFIG_STRUCT(Item, name, value)
CONFIG_STRUCT(Config, app_name, version, items)

int main() {
    // 1. Initialize ConfigStore
    // We use a temporary file for this example
    auto& store = config::get_store("example_struct.json", config::Path::Relative);

    // 2. Prepare data
    std::cout << "Setting up configuration data..." << std::endl;
    store.set("app_name", "StructApp");
    store.set("version", 1);
    
    std::vector<Item> items = {{"item1", "value1"}, {"item2", "value2"}};
    store.set("items", items);
    
    // Save to disk (optional, set() usually auto-saves)
    store.save();
    
    // 3. Read back as a struct using root key ""
    std::cout << "Reading configuration into struct..." << std::endl;
    try {
        auto loaded_config = store.get<Config>("");
        
        std::cout << "Loaded Config:" << std::endl;
        std::cout << "App Name: " << loaded_config.app_name << std::endl;
        std::cout << "Version: " << loaded_config.version << std::endl;
        std::cout << "Items:" << std::endl;
        for (const auto& item : loaded_config.items) {
            std::cout << "  - " << item.name << ": " << item.value << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
