#include <fcntl.h>
#include <unistd.h>
#include <posix/errors.hpp>
#include <util/file.hpp>
#include <util/format.hpp>

namespace util
{

static inline int make_posix_whence(Whence whence)
{
    int lseek_whence = -1;
    switch (whence) {
    case SeekCur:
        lseek_whence = SEEK_CUR;
        break;
    case SeekSet:
        lseek_whence = SEEK_SET;
        break;
    case SeekEnd:
        lseek_whence = SEEK_END;
        break;
    }
    return lseek_whence;
}

static inline int make_posix_flags(OpenMode open_mode)
{
  int flags = -1;
  switch (open_mode) {
  case OpenReadOnly:
      flags = O_RDONLY;
      break;
  case OpenWriteOnly:
      flags = O_WRONLY;
      break;
  case OpenReadWrite:
      flags = O_RDWR;
      break;
  }
  return flags;
}

FileImpl::FileImpl(const std::string& filename, OpenMode open_mode)
    : _fd(posix::wrap(::open(filename.c_str(), make_posix_flags(open_mode)))
{
}

FileImpl::~FileImpl()
{
    if (_fd != -1) {
        ::close(_fd);
    }
}

ssize_t FileImpl::read(void* buf, size_t count)
{
    ssize_t res = posix::wrap(::read(_fd, buf, count));
    return res;
}

off_t FileImpl::seek(off_t offset, Whence whence)
{
    off_t res = posix::wrap(::lseek(_fd, 0, make_posix_whence(whence)));
    return res;
}

} // namespace util

// vim: set sts=2 sw=2 expandtab:
