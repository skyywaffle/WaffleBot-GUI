#ifndef AUDIOFILE_H
#define AUDIOFILE_H
#include "sndfile.h"
#include <vector>

struct AudioFile {
    SF_INFO info;
    SNDFILE *file;
    std::vector<short> buffer;
};

#endif // AUDIOFILE_H
