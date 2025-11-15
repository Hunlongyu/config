#include <iostream>
#include <vector>
#include <config/config.h>

int main() {
    using namespace config;
    config_store store("obfuscation_policies", SavePolicy::ManualSave, Path::CurrentDir);
    store.set<std::string>("plain", "text", Obfuscate::None);
    store.set_obfuscated<std::string>("base", "secret_base", Obfuscate::Base64);
    store.set_obfuscated<std::string>("xor", "secret_xor", Obfuscate::XorCipher);
    store.set_obfuscated<std::string>("shift", "secret_shift", Obfuscate::CharShift);
    store.set_obfuscated<std::string>("combo", "secret_combo", Obfuscate::Combined);

    std::cout << std::boolalpha << store.is_obfuscated("plain") << "\n";
    std::cout << std::boolalpha << store.is_obfuscated("base") << "\n";
    auto keys = store.get_obfuscated_keys();
    for (const auto& k : keys) std::cout << k << "\n";
    std::cout << store.dump(2) << "\n";
    store.save();
    return 0;
}