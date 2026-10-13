#include <config/config.hpp>
#include <fstream>
#include <iostream>

int main()
{
    std::ofstream("base.json") << R"({"server":{"host":"localhost","port":80}})";
    config::Store store("user.json", {.defaults     = {{"theme", "light"}},
                                      .layers       = {{"base.json"}, {"optional.json", true}},
                                      .env_prefix   = "CONFIG_EXAMPLE_",
                                      .env_bindings = {{"/server/port", "CONFIG_EXAMPLE_PORT"}}});
    store.set("/theme", "dark");
    store.set("/label", "example", config::Encoding::base64);
    store.reload();
    std::cout << store.snapshot().dump(2) << '\n';
    // 默认值、文件层及环境覆盖不会进入可写层。
    return store.snapshot(config::View::writable).contains("server") ? 1 : 0;
}
