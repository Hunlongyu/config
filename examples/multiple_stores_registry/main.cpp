#include <iostream>
#include <vector>
#include <config/config.h>

int main() {
    using namespace config;
    auto s1 = registry::get_store("store_one", SavePolicy::ManualSave, Path::CurrentDir);
    auto s2 = registry::get_store("store_two", SavePolicy::ManualSave, Path::AppData);
    s1->set<std::string>("name", "one");
    s2->set<int>("count", 2);
    s1->save();
    s2->save();

    auto names = registry::list_stores();
    for (const auto& n : names) {
        auto st = registry::get_store(n);
        std::cout << n << " => " << st->get_file_path().string() << "\n";
    }
    return 0;
}