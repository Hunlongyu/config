#include <iostream>
#include <thread>
#include <chrono>
#include <config/config.h>

int main() {
    using namespace config;
    config_store autoS("save_auto", SavePolicy::AutoSave, Path::CurrentDir);
    autoS.set<int>("v", 1);
    std::cout << std::boolalpha << autoS.is_dirty() << "\n";

    config_store manualS("save_manual", SavePolicy::ManualSave, Path::CurrentDir);
    manualS.set<int>("v", 2);
    std::cout << std::boolalpha << manualS.is_dirty() << "\n";
    manualS.save();
    std::cout << std::boolalpha << manualS.is_dirty() << "\n";

    config_store batchS("save_batch", SavePolicy::BatchSave, Path::CurrentDir);
    batchS.set<int>("v", 3);
    std::cout << std::boolalpha << batchS.is_dirty() << "\n";
    batchS.save();
    std::cout << std::boolalpha << batchS.is_dirty() << "\n";

    config_store timedS("save_timed", SavePolicy::TimedSave, Path::CurrentDir);
    timedS.set_save_interval(std::chrono::milliseconds(200));
    timedS.set<int>("v", 4);
    std::cout << std::boolalpha << timedS.is_dirty() << "\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << std::boolalpha << timedS.is_dirty() << "\n";
    return 0;
}