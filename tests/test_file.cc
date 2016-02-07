#include <util/file.hpp>
#include <iostream>

int main(void)
{
    try {
        util::File file(std::string("data/testfile"), util::OpenReadOnly);
    } catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
        throw;
    }

    return 0;
}

// vim: set sts=2 sw=2 expandtab:
