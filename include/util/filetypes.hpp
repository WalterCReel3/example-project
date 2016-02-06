#ifndef __UTIL_FILETYPES_HPP__
#define __UTIL_FILETYPES_HPP__
namespace util { 

enum Whence {
    SeekSet,
    SeekCur,
    SeekEnd
};

enum OpenMode {
    OpenReadOnly,
    OpenWriteOnly,
    OpenReadWrite
};

}
#endif
