#ifndef __UTIL_FILEIMPL_HPP__
#define __UTIL_FILEIMPL_HPP__

#include <Windows.h>
#include <sys/types.h>
#include <string>

typedef LONG_PTR ssize_t;

namespace util {

class FileImpl
{
public:
    FileImpl(const std::string& filename, OpenMode flags);
    ~FileImpl();

public:
    ssize_t read(void* buf, size_t count);
    off_t seek(off_t offset, Whence whence);

private:
    HANDLE _handle;
};

}

#endif
