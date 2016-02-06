#ifndef __UTIL_FILEIMPL_HPP__
#define __UTIL_FILEIMPL_HPP__

#include <util/filetypes.hpp>
#include <sys/types.h>
#include <string>
#include <fcntl.h>

namespace util {

class FileImpl
{
public:
    FileImpl(const std::string& filename, int flags);
    ~FileImpl();

public:
    ssize_t read(void* buf, size_t count);
    off_t seek(off_t offset, Whence whence);

private:
    int _fd;
};

}

#endif
