#include "config/config.h"
#include <iostream>

int main()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // 创建一个配置存储，使用默认路径和自动保存策略
    auto store = config::get_store("obfuscate_config");

    // 设置普通值（不混淆）
    store->set("public_data", "HelloWorld");

    // 设置使用 Base64 混淆的敏感数据
    store->set_obfuscated("password", "secret123", config::Obfuscate::base64);

    // 设置使用 XOR 混淆的敏感数据
    store->set_obfuscated("api_key", "xyz789", config::Obfuscate::xor_cipher);

    // 设置使用组合混淆的敏感数据
    store->set_obfuscated("token", "abc456", config::Obfuscate::combined);

    // 读取并打印值（自动解混淆）
    std::cout << "public data: " << store->get<std::string>("public_data") << std::endl;
    std::cout << "passworld (Base64): " << store->get<std::string>("password") << std::endl;
    std::cout << "API_KEY (XOR): " << store->get<std::string>("api_key") << std::endl;
    std::cout << "token (Combined): " << store->get<std::string>("token") << std::endl;

    // 检查字段是否被混淆
    std::cout << "密码是否混淆: " << (store->is_obfuscated("password") ? "是" : "否") << std::endl;
    std::cout << "公开数据是否混淆: " << (store->is_obfuscated("public_data") ? "是" : "否") << std::endl;

    // 获取所有混淆字段
    auto obfuscated_keys = store->get_obfuscated_keys();
    std::cout << "混淆的字段: ";
    for (const auto &key : obfuscated_keys)
    {
        std::cout << key << " ";
    }
    std::cout << std::endl;

    // 打印 JSON 数据（未混淆的内部表示）
    std::cout << "配置内容:\n" << store->dump(2) << std::endl;

    return 0;
}