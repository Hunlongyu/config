#include <algorithm>
#include <config/config.hpp>
#include <config/version.h>

int config_peer();
int main()
{
    return std::max(1, 2) == config_peer() && config::version() == CONFIG_TEST_VERSION ? 0 : 1;
}
