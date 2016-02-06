#ifndef __UTIL_DELETER_HPP
#define __UTIL_DELETER_HPP
namespace util
{

template <typename T>
void deleter(T* const ptr)
{
    delete ptr;
}

} // namespace util

// vim: set sts=2 sw=2 expandtab:
#endif
