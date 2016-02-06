#ifndef __WREEL_IO_HPP
#define __WREEL_IO_HPP

#include <sys/types.h>
#include <util/file.hpp>

namespace util
{

template<typename Buffer>
void read_all(File &file, Buffer& fbuffer)
{
    typedef typename Buffer::iterator Iterator;
    off_t bytes_read;
    char buf[8096];
    off_t total_bytes;
    off_t sz = file.seek(0, SeekEnd);

    fbuffer.reserve(sz);
    fbuffer.resize(sz, 0);

    file.seek(0, SeekSet);
    total_bytes = 0;
    for (Iterator i = fbuffer.begin(); total_bytes < sz; ) {
        bytes_read = (off_t)file.read(buf, sizeof(buf));
        if (bytes_read == 0) {
            break;
        }
        i = std::copy(buf, buf + bytes_read, i);
        total_bytes += bytes_read;
    }
}

std::string read_file(const char *path);

} // namespace util

#endif
