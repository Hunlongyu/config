#include <iostream>
#include <vector>
#include <config/config.h>

int main() {
    using namespace config;
    config_store store("json_pointer_crud", SavePolicy::ManualSave, Path::CurrentDir);
    store.set<int>("/a/b/c", 123);
    std::cout << store.get<int>("/a/b/c") << "\n";
    std::cout << std::boolalpha << store.contains("/a/b/c") << "\n";

    store.set<std::vector<int>>("/arr", std::vector<int>{1, 2, 3});
    store.remove("/arr/1");
    std::cout << store.dump(2) << "\n";
    store.remove("/a/b/c");
    std::cout << std::boolalpha << store.contains("/a/b/c") << "\n";
    store.save();
    return 0;
}