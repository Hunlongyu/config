#include <iostream>
#include <config/config.h>

int main() {
    using namespace config;
    config_store store("change_listeners", SavePolicy::ManualSave, Path::CurrentDir);
    auto id1 = store.connect("name", [](const nlohmann::json& oldv, const nlohmann::json& newv){
        std::cout << "name:" << oldv.dump() << "->" << newv.dump() << "\n";
    });
    auto id2 = store.connect("/a/b", [](const nlohmann::json& oldv, const nlohmann::json& newv){
        std::cout << "/a/b:" << oldv.dump() << "->" << newv.dump() << "\n";
    });

    store.set<std::string>("name", "alpha");
    store.set<std::string>("name", "beta");
    store.set<int>("/a/b", 1);
    store.set<int>("/a/b", 2);
    store.remove("/a/b");
    store.disconnect(id1);
    store.disconnect(id2);
    store.save();
    return 0;
}