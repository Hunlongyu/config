#include <iostream>
#include <fstream>
#include <config/config.h>

int main() {
    using namespace config;
    config_store store("error_and_validation", SavePolicy::ManualSave, Path::CurrentDir);

    try { auto v = store.get<int>("missing"); (void)v; }
    catch (const config_exception& e) { std::cout << e.what() << "\n"; }

    store.set<std::string>("s", "text");
    try { auto v2 = store.get<int>("s"); (void)v2; }
    catch (const std::exception& e) { std::cout << e.what() << "\n"; }

    try { store.set<int>("/invalid~", 1); }
    catch (const config_exception& e) { std::cout << e.what() << "\n"; }

    store.save();
    auto path = store.get_file_path();
    {
        std::ofstream ofs(path);
        ofs << "{ invalid json }";
        ofs.flush();
    }
    try { store.reload(); }
    catch (const config_exception& e) { std::cout << e.what() << "\n"; }
    return 0;
}