#include <posix/errors.hpp>

#define POSIX_ERROR_CASE(EID, ENAME) \
case EID: throw ENAME()

namespace posix
{

void __dispatch_exception(int e)
{
    switch (e) {
        POSIX_ERROR_CASE(EPERM, operation_not_permitted);
        POSIX_ERROR_CASE(ENOENT, no_such_file);
        POSIX_ERROR_CASE(ESRCH, no_such_process);
        POSIX_ERROR_CASE(EINTR, interrupted);
        POSIX_ERROR_CASE(EIO, io_error);
        POSIX_ERROR_CASE(ENXIO, no_such_device_or_address);
        POSIX_ERROR_CASE(E2BIG, argument_list_too_long);
        POSIX_ERROR_CASE(ENOEXEC, exec_format_error);
        POSIX_ERROR_CASE(EBADF, bad_file_descriptor);
        POSIX_ERROR_CASE(ECHILD, no_child_process);
        POSIX_ERROR_CASE(EAGAIN, try_again);
        POSIX_ERROR_CASE(ENOMEM, out_of_memory);
        POSIX_ERROR_CASE(EACCES, permission_denied);
        POSIX_ERROR_CASE(EFAULT, bad_address);
        // POSIX_ERROR_CASE(ENOTBLK, block_device_required);
        POSIX_ERROR_CASE(EBUSY, resource_busy);
        // POSIX_ERROR_CASE(EEXISTS, file_exists);
        POSIX_ERROR_CASE(EXDEV, cross_device_link);
        POSIX_ERROR_CASE(ENODEV, no_such_device);
        POSIX_ERROR_CASE(ENOTDIR, not_a_directory);
        POSIX_ERROR_CASE(EINVAL, invalid_argument);
        POSIX_ERROR_CASE(EISDIR, is_a_directory);
        POSIX_ERROR_CASE(ENFILE, file_table_overflow);
        POSIX_ERROR_CASE(EMFILE, too_many_open_files);
        POSIX_ERROR_CASE(ENOTTY, not_a_terminal);
        // POSIX_ERROR_CASE(ETXTBSY, text_file_busy);
        POSIX_ERROR_CASE(EFBIG, file_too_large);
        POSIX_ERROR_CASE(ENOSPC, no_space_left_on_device);
        POSIX_ERROR_CASE(ESPIPE, illegal_seek);
        POSIX_ERROR_CASE(EROFS, read_only_filesystem);
        POSIX_ERROR_CASE(EMLINK, too_many_links);
        POSIX_ERROR_CASE(EPIPE, broken_pipe);
        POSIX_ERROR_CASE(EDEADLK, deadlock_would_occurr);
        POSIX_ERROR_CASE(ENAMETOOLONG, name_too_long);
        POSIX_ERROR_CASE(ENOLCK, no_record_locks_available);
        POSIX_ERROR_CASE(ENOSYS, function_not_implemented);
        POSIX_ERROR_CASE(ENOTEMPTY, directory_not_empty);
        POSIX_ERROR_CASE(ELOOP, too_many_symlinks);
        // POSIX_ERROR_CASE(EWOULDBLOCK, would_block); // is EAGAIN on linux
        // POSIX_ERROR_CASE(ENOMSG, no_message);
        POSIX_ERROR_CASE(ENOPROTOOPT, invalid_protocol_option);
        POSIX_ERROR_CASE(ENOTSOCK, not_a_socket);
        POSIX_ERROR_CASE(EAFNOSUPPORT, address_family_not_supported);
        POSIX_ERROR_CASE(EADDRINUSE, address_in_use);
        POSIX_ERROR_CASE(EALREADY, already_connecting);
        POSIX_ERROR_CASE(ECONNREFUSED, connection_refused);
        POSIX_ERROR_CASE(EISCONN, already_connected);
        POSIX_ERROR_CASE(ENETUNREACH, network_unreachable);
        POSIX_ERROR_CASE(ENOTCONN, not_connected);
        POSIX_ERROR_CASE(ECONNRESET, connection_reset);
        POSIX_ERROR_CASE(EDESTADDRREQ, no_peer_address);
        POSIX_ERROR_CASE(ENOBUFS, no_buffer_space);
        POSIX_ERROR_CASE(EOPNOTSUPP, operation_not_supported);
    }
}

} // namespace posix

// vim: set sts=2 sw=2 expandtab:
