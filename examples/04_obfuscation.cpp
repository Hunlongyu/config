#include <config/config.hpp>
#include <iostream>

int main()
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    auto &store = config::get_store("obfuscated.json");

    std::cout << "=== 各种混淆方式 ===" << std::endl;

    // 原始明文
    std::string secret = "MySecretPassword123!";

    // 不混淆
    store.set("password_none", secret, config::Obfuscate::None);

    // Base64 编码
    store.set("password_base64", secret, config::Obfuscate::Base64);

    // 十六进制编码
    store.set("password_hex", secret, config::Obfuscate::Hex);

    // ROT13
    store.set("password_rot13", secret, config::Obfuscate::ROT13);

    // 反转
    store.set("password_reverse", secret, config::Obfuscate::Reverse);

    // 组合混淆
    store.set("password_combined", secret, config::Obfuscate::Combined);

    std::cout << "原始密码: " << secret << std::endl;
    std::cout << "\n混淆后的存储格式（查看 obfuscated.json）：" << std::endl;
    std::cout << "- None: 明文存储" << std::endl;
    std::cout << "- Base64: 标准 Base64 编码" << std::endl;
    std::cout << "- Hex: 十六进制编码" << std::endl;
    std::cout << "- ROT13: 字母位移" << std::endl;
    std::cout << "- Reverse: 字符串反转" << std::endl;
    std::cout << "- Combined: Base64 + Reverse" << std::endl;

    std::cout << "\n=== 读取混淆数据（自动解密） ===" << std::endl;

    auto pwd_none     = store.get<std::string>("password_none");
    auto pwd_base64   = store.get<std::string>("password_base64");
    auto pwd_hex      = store.get<std::string>("password_hex");
    auto pwd_rot13    = store.get<std::string>("password_rot13");
    auto pwd_reverse  = store.get<std::string>("password_reverse");
    auto pwd_combined = store.get<std::string>("password_combined");

    std::cout << "None:     " << pwd_none << std::endl;
    std::cout << "Base64:   " << pwd_base64 << std::endl;
    std::cout << "Hex:      " << pwd_hex << std::endl;
    std::cout << "ROT13:    " << pwd_rot13 << std::endl;
    std::cout << "Reverse:  " << pwd_reverse << std::endl;
    std::cout << "Combined: " << pwd_combined << std::endl;

    // 验证所有方式都能正确还原
    bool all_match = (pwd_none == secret) && (pwd_base64 == secret) && (pwd_hex == secret) && (pwd_rot13 == secret) &&
                     (pwd_reverse == secret) && (pwd_combined == secret);

    std::cout << "\n所有混淆方式验证: " << (all_match ? "✓ 通过" : "✗ 失败") << std::endl;

    std::cout << "\n=== 实际应用场景 ===" << std::endl;

    // API 密钥
    store.set("api/key", "sk_live_1234567890abcdef", config::Obfuscate::Base64);
    store.set("api/secret", "secret_key_xyz", config::Obfuscate::Combined);

    // 数据库密码
    store.set("database/password", "db_pass_123", config::Obfuscate::Combined);

    // 用户令牌
    store.set("user/token", "user_token_abc", config::Obfuscate::Hex);

    std::cout << "已存储混淆的敏感配置" << std::endl;
    std::cout << "查看 obfuscated.json 可以看到数据已混淆" << std::endl;

    system("pause");
    return 0;
}
