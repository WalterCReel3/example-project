#ifndef __WREEL_FILE_HPP__
#define __WREEL_FILE_HPP__

#include <util/filetypes.hpp>
#ifdef WIN32
#include <util/mswin/fileimpl.hpp>
#else
#include <util/posix/fileimpl.hpp>
#endif

namespace util
{

class File
{
public:
    File(const std::string& filename, OpenMode open_mode);

public:
    ssize_t read(void* buf, size_t count);
    off_t seek(off_t offset, Whence whence);

private:
    std::string _filename;
    FileImpl _file_impl;
};

} // namespace util

// vim: set sts=2 sw=2 expandtab:

#endif
