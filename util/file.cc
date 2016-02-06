#include <util/file.hpp>

namespace util
{

File::File(const std::string& filename, OpenMode flags)
    : _filename(filename)
    , _file_impl(filename, flags)
{
}

ssize_t File::read(void* buf, size_t count)
{
    return _file_impl.read(buf, count);
}

off_t File::seek(off_t offset, Whence whence)
{
    return _file_impl.seek(offset, whence);
}

} // namespace util

// vim: set sts=2 sw=2 expandtab:
