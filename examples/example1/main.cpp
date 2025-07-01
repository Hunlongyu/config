#include <config/config.h>
#include <iostream>

int main()
{
    auto cfg    = config::get_store("test", config::save_policy::auto_save, config::Path::appdata);

    std::cout << cfg->get_file_path() << std::endl;

    cfg->set("debug/timeout", 30);

    cfg->set("/user/passworld", "123", config::Obfuscate::base64);
    std::cout << cfg->get<std::string>("/user/passworld") << std::endl;

    cfg->set_obfuscated("/user/age", 18);
    std::cout << cfg->get<int>("/user/age") << std::endl;

    cfg->set_obfuscated("/user/phone", "18365289000");
    std::cout << cfg->get<std::string>("/user/phone") << std::endl;

    auto id = cfg->connect("debug/timeout", [](const config::json &old_val, const config::json &new_val) {
        std::cout << "old_val: " << old_val.dump() << " new_val: " << new_val.dump() << std::endl;
    });

    cfg->set("debug/timeout", 60);

    cfg->disconnect(id);

    std::cout << cfg->to_string() << std::endl;

    cfg->set("debug/timeout", 90);

    std::cout << cfg->to_string() << std::endl;
    return 0;
}