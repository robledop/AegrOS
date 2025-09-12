#include "echo.hpp"

extern "C" {
int main(const int argc, char **argv)
{
    Echo::print(argc, argv);

    return 0;
}
}