#include <config/config.hpp>
#include <string>

struct Server
{
    std::string host = "localhost";
    int port         = 80;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Server, host, port)
};
int main()
{
    config::Store store("struct.json");
    store.set("/server", Server{"example", 8080});
    store.reload();
    return store.get<Server>("/server").port == 8080 ? 0 : 1;
}
