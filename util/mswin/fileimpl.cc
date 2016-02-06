#include <Windows.h>
#include <stdexcept>
#include <sys/types.h>
#include <posix/errors.hpp>
#include <util/file.hpp>
#include <util/format.hpp>

namespace util
{

static inline DWORD make_move_method(Whence whence)
{
    int move_method = 0;
    switch (whence) {
    case SeekCur:
        move_method = FILE_CURRENT;
        break;
    case SeekSet:
        move_method = FILE_BEGIN;
        break;
    case SeekEnd:
        move_method = FILE_END;
        break;
    }
    return move_method;
}

static inline DWORD make_access_flags(OpenMode open_mode)
{
    DWORD flags = 0;
    switch (open_mode) {
    case OpenReadOnly:
        flags = GENERIC_READ;
        break;
    case OpenWriteOnly:
        flags = GENERIC_WRITE;
        break;
    case OpenReadWrite:
        flags = GENERIC_READ | GENERIC_WRITE;
        break;
    }
    return flags;
}

static HANDLE open_file(const char* filename, DWORD access_flags) {
    HANDLE ret = ::CreateFile(filename, access_flags, 0, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (ret == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Could not open file");
    }
    return ret;
}


FileImpl::FileImpl(const std::string& filename, OpenMode flags)
    : _handle(open_file(filename.c_str(), make_access_flags(flags)))
{
}

FileImpl::~FileImpl()
{
    if (_handle != INVALID_HANDLE_VALUE) {
        ::CloseHandle(_handle);
    }
}

ssize_t FileImpl::read(void* buf, size_t count)
{
    DWORD ret;
    BOOL res = ::ReadFile(_handle, buf, count, &ret, NULL);
    if (res == FALSE) {
        throw std::runtime_error("Could not read from file");
    }
    return ret;
}

off_t FileImpl::seek(off_t offset, Whence whence)
{
    DWORD res = ::SetFilePointer(_handle, offset, NULL, make_move_method(whence));
    if (res == INVALID_SET_FILE_POINTER) {
        throw std::runtime_error("Could not seek on file");
    }
    return res;
}

} // namespace util

// vim: set sts=2 sw=2 expandtab:
