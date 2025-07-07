#ifndef AUDIOHANDLING_H
#define AUDIOHANDLING_H
#include "sndfile.h"
#include "AudioFile.h"

class Macro;

void mix_click(std::vector<short> &outBuffer, const std::vector<short> &click, int insertIndex, int channels);
std::vector<AudioFile> getAudioFiles(const char *folderName);
AudioFile getNoiseFile(const char *folderName);
#endif // AUDIOHANDLING_H
