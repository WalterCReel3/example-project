#include <util/io.hpp>

namespace util
{

std::string read_file(const char *path)
{
    std::string filename = path;
    std::string file_buffer;
    util::File input(filename, O_RDONLY);
    util::read_all(input, file_buffer);

    return file_buffer;
}

} // namespace util
