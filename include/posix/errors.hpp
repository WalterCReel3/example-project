#ifndef __POSIX_ERRORS_HPP__
#define __POSIX_ERRORS_HPP__

#include <exception>
#include <errno.h>

namespace posix
{

#define POSIX_ERROR_DECL(EID,ENAME,EBASE) \
class ENAME : public EBASE { \
public: \
  virtual char const * what () const throw() { \
    return "unistd_error::" #ENAME " (" #EID ")"; \
  } \
};

POSIX_ERROR_DECL(EPERM, operation_not_permitted, std::exception);
POSIX_ERROR_DECL(ENOENT, no_such_file, std::exception);
POSIX_ERROR_DECL(ESRCH, no_such_process, std::exception);
POSIX_ERROR_DECL(EINTR, interrupted, std::exception);
POSIX_ERROR_DECL(EIO, io_error, std::exception);
POSIX_ERROR_DECL(ENXIO, no_such_device_or_address, std::exception);
POSIX_ERROR_DECL(E2BIG, argument_list_too_long, std::exception);
POSIX_ERROR_DECL(ENOEXEC, exec_format_error, std::exception);
POSIX_ERROR_DECL(EBADF, bad_file_descriptor, std::exception);
POSIX_ERROR_DECL(ECHILD, no_child_process, std::exception);
POSIX_ERROR_DECL(EAGAIN, try_again, std::exception);
POSIX_ERROR_DECL(ENOMEM, out_of_memory, std::exception);
POSIX_ERROR_DECL(EACCES, permission_denied, std::exception);
POSIX_ERROR_DECL(EFAULT, bad_address, std::exception);
POSIX_ERROR_DECL(ENOTBLK, block_device_required, std::exception);
POSIX_ERROR_DECL(EBUSY, resource_busy, std::exception);
POSIX_ERROR_DECL(EEXISTS, file_exists, std::exception);
POSIX_ERROR_DECL(EXDEV, cross_device_link, std::exception);
POSIX_ERROR_DECL(ENODEV, no_such_device, std::exception);
POSIX_ERROR_DECL(ENOTDIR, not_a_directory, std::exception);
POSIX_ERROR_DECL(EINVAL, invalid_argument, std::exception);
POSIX_ERROR_DECL(EISDIR, is_a_directory, std::exception);
POSIX_ERROR_DECL(ENFILE, file_table_overflow, std::exception);
POSIX_ERROR_DECL(EMFILE, too_many_open_files, std::exception);
POSIX_ERROR_DECL(ENOTTY, not_a_terminal, std::exception);
POSIX_ERROR_DECL(ETXTBSY, text_file_busy, std::exception);
POSIX_ERROR_DECL(EFBIG, file_too_large, std::exception);
POSIX_ERROR_DECL(ENOSPC, no_space_left_on_device, std::exception);
POSIX_ERROR_DECL(ESPIPE, illegal_seek, std::exception);
POSIX_ERROR_DECL(EROFS, read_only_filesystem, std::exception);
POSIX_ERROR_DECL(EMLINK, too_many_links, std::exception);
POSIX_ERROR_DECL(EPIPE, broken_pipe, std::exception);
// POSIX_ERROR_DECL(EDOM, , std::exception);
// POSIX_ERROR_DECL(ERANGE, , std::exception);
POSIX_ERROR_DECL(EDEADLK, deadlock_would_occurr, std::exception);
POSIX_ERROR_DECL(ENAMETOOLONG, name_too_long, std::exception);
POSIX_ERROR_DECL(ENOLCK, no_record_locks_available, std::exception);
POSIX_ERROR_DECL(ENOSYS, function_not_implemented, std::exception);
POSIX_ERROR_DECL(ENOTEMPTY, directory_not_empty, std::exception);
POSIX_ERROR_DECL(ELOOP, too_many_symlinks, std::exception);
POSIX_ERROR_DECL(EWOULDBLOCK, would_block, std::exception);
POSIX_ERROR_DECL(ENOMSG, no_message, std::exception);
POSIX_ERROR_DECL(ENOPROTOOPT, invalid_protocol_option, std::exception);
POSIX_ERROR_DECL(ENOTSOCK, not_a_socket, std::exception);
POSIX_ERROR_DECL(EAFNOSUPPORT, address_family_not_supported, std::exception);
POSIX_ERROR_DECL(EADDRINUSE, address_in_use, std::exception);
POSIX_ERROR_DECL(EALREADY, already_connecting, std::exception);
POSIX_ERROR_DECL(ECONNREFUSED, connection_refused, std::exception);
POSIX_ERROR_DECL(EISCONN, already_connected, std::exception);
POSIX_ERROR_DECL(ENETUNREACH, network_unreachable, std::exception);
POSIX_ERROR_DECL(ENOTCONN, not_connected, std::exception);
POSIX_ERROR_DECL(ECONNRESET, connection_reset, std::exception);
POSIX_ERROR_DECL(EDESTADDRREQ, no_peer_address, std::exception);
POSIX_ERROR_DECL(ENOBUFS, no_buffer_space, std::exception);
POSIX_ERROR_DECL(EOPNOTSUPP, operation_not_supported, std::exception);

void __dispatch_exception(int e);

inline int
wrap(int result)
{
    if (result < 0) {
        int e = errno;
        __dispatch_exception(e);
    }
    return result;
}

} // namespace posix

// vim: set sts=2 sw=2 expandtab:

#endif
