#include <stdio.h>
#include <stdlib.h>

int main([[maybe_unused]] const int argc, [[maybe_unused]] char **argv)
{
    memstat();

    auto cwd = getcwd();
    auto pid = create_process("/bin/blank.elf", cwd);
    waitpid(pid, nullptr);

    memstat();

    return 0;
}