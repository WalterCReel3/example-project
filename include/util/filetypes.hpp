#pragma once
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
