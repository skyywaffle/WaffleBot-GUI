#include "AudioHandling.h"
#include "Action.h"
#include "AudioFile.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include "sndfile.h"
#include <vector>
#include <string>

#include "Input.h"
#include "Macro.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"

namespace fs = std::filesystem;

void mix_click(std::vector<short> &outBuffer, const std::vector<short> &click, int insertIndex, int channels) {
    for (size_t i = 0; i < click.size(); ++i) {
        size_t outIndex = insertIndex + i;
        if (outIndex < outBuffer.size()) {
            int mixed = outBuffer[outIndex] + click[i];
            // Clipping
            if (mixed > 32767)
                mixed = 32767;
            if (mixed < -32768)
                mixed = -32768;
            outBuffer[outIndex] = static_cast<short>(mixed);
        }
    }
}

std::vector<AudioFile> getAudioFiles(const char *folderName) {
    std::vector<AudioFile> files{};

    fs::path folderPath{folderName};
    if (fs::exists(folderPath) && fs::is_directory(folderPath)) {
        for (const auto &item : fs::directory_iterator(folderPath)) {
            // Construct the filename and add a new AudioFile to the files vector
            if (fs::is_regular_file(item)) {
                std::string fileString{folderName};
                fileString.append("/");
                fileString.append(item.path().filename().string());
                AudioFile audioFile{};
                audioFile.file = sf_open(fileString.c_str(), SFM_READ, &audioFile.info);

                audioFile.buffer.resize(audioFile.info.frames * audioFile.info.channels);
                sf_read_short(audioFile.file, audioFile.buffer.data(), audioFile.buffer.size());
                sf_close(audioFile.file);

                files.push_back(audioFile);
            }
        }
    }
    return files;
}

AudioFile getNoiseFile(const char *folderName) {
    fs::path folderPath{folderName};
    if (fs::exists(folderPath) && fs::is_directory(folderPath)) {
        for (const auto &item : fs::directory_iterator(folderPath)) {
            if (fs::is_regular_file(item) && (item.path().filename().string() == "noise.wav" || item.path().filename().string() == "whitenoise.wav")) {
                std::string fileString{folderName};
                fileString.append("/");
                fileString.append(item.path().filename().string());
                AudioFile audioFile{};
                audioFile.file = sf_open(fileString.c_str(), SFM_READ, &audioFile.info);

                audioFile.buffer.resize(audioFile.info.frames * audioFile.info.channels);
                sf_read_short(audioFile.file, audioFile.buffer.data(), audioFile.buffer.size());
                sf_close(audioFile.file);
                return audioFile;
            }
        }
    }
    return {};
}

void addToBuffer(std::vector<float> &inputTimes, std::vector<AudioFile> &files, std::vector<short> &buffer, int sampleRate, int channels) {
    // Add clicks to output buffer
    for (float t : inputTimes) {
        // set up random index choice
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(0, files.size() - 1);

        // Add to buffer
        sf_count_t frameIndex = static_cast<sf_count_t>(t * sampleRate);
        int sampleIndex = frameIndex * channels; // Interleaved
        mix_click(buffer, files[distrib(gen)].buffer, sampleIndex, channels);
    }
}

// Returns whether audio generation succeeded
bool MainWindow::generateAudio(Macro &macro) {
    bool isTwoPlayerMacro {macro.isTwoPlayer()};
    bool isPlatformerMacro {macro.isPlatformer()};

    int sampleRate = 44100;
    int channels = 2;
    SF_INFO audioInfo;

    // Define the total duration in seconds of the output file
    float durationSeconds = (float)macro.getFrameCount() / macro.getFps() + 2; // add an extra 2 seconds so no releases get cut off

    for (auto& clickFolder : clickFiles) {
        if (!clickFolder.empty()) {
            sampleRate = clickFolder[0].info.samplerate;
            channels = clickFolder[0].info.channels;
            audioInfo = clickFolder[0].info;
        }
    }

    sf_count_t totalFrames = static_cast<sf_count_t>(durationSeconds * sampleRate);
    std::vector<short> outputBuffer(totalFrames * channels, 0); // Silent buffer

    // Add noise if there's noise
    if (noiseFile.buffer.size() > 0 && ui->noiseCheckBox->isChecked()) {
        size_t noiseLength = noiseFile.buffer.size();
        size_t outLength = outputBuffer.size();
        for (size_t i = 0; i < outLength; ++i) {
            outputBuffer[i] = noiseFile.buffer[i % noiseLength];
        }
    }



    // Define time points where clicks and releases (and soft clicks/releases) occur (in seconds)
    std::vector<float> p1ClickTimes{};
    std::vector<float> p1ReleaseTimes{};
    std::vector<float> p1SoftClickTimes{};
    std::vector<float> p1SoftReleaseTimes{};
    std::vector<float> p1HardClickTimes{};
    std::vector<float> p1HardReleaseTimes{};
    std::vector<float> p1MicroClickTimes{};
    std::vector<float> p1MicroReleaseTimes{};

    std::vector<float> p1LeftClickTimes{};
    std::vector<float> p1LeftReleaseTimes{};
    std::vector<float> p1LeftSoftClickTimes{};
    std::vector<float> p1LeftSoftReleaseTimes{};
    std::vector<float> p1LeftHardClickTimes{};
    std::vector<float> p1LeftHardReleaseTimes{};
    std::vector<float> p1LeftMicroClickTimes{};
    std::vector<float> p1LeftMicroReleaseTimes{};

    std::vector<float> p1RightClickTimes{};
    std::vector<float> p1RightReleaseTimes{};
    std::vector<float> p1RightSoftClickTimes{};
    std::vector<float> p1RightSoftReleaseTimes{};
    std::vector<float> p1RightHardClickTimes{};
    std::vector<float> p1RightHardReleaseTimes{};
    std::vector<float> p1RightMicroClickTimes{};
    std::vector<float> p1RightMicroReleaseTimes{};

    std::vector<float> p2ClickTimes{};
    std::vector<float> p2ReleaseTimes{};
    std::vector<float> p2SoftClickTimes{};
    std::vector<float> p2SoftReleaseTimes{};
    std::vector<float> p2HardClickTimes{};
    std::vector<float> p2HardReleaseTimes{};
    std::vector<float> p2MicroClickTimes{};
    std::vector<float> p2MicroReleaseTimes{};

    std::vector<float> p2LeftClickTimes{};
    std::vector<float> p2LeftReleaseTimes{};
    std::vector<float> p2LeftSoftClickTimes{};
    std::vector<float> p2LeftSoftReleaseTimes{};
    std::vector<float> p2LeftHardClickTimes{};
    std::vector<float> p2LeftHardReleaseTimes{};
    std::vector<float> p2LeftMicroClickTimes{};
    std::vector<float> p2LeftMicroReleaseTimes{};

    std::vector<float> p2RightClickTimes{};
    std::vector<float> p2RightReleaseTimes{};
    std::vector<float> p2RightSoftClickTimes{};
    std::vector<float> p2RightSoftReleaseTimes{};
    std::vector<float> p2RightHardClickTimes{};
    std::vector<float> p2RightHardReleaseTimes{};
    std::vector<float> p2RightMicroClickTimes{};
    std::vector<float> p2RightMicroReleaseTimes{};




    // Add the times of inputs to their corresponding vectors
    float macroFps = static_cast<float>(macro.getFps());
    for (Action action : macro.getActions()) {
        float actionTime = action.getFrame() / macroFps;
        for (Input input : action.getPlayerOneInputs()) {
            if (input.isPressed()) {
                if (input.getClickType() == ClickType::NORMAL) {
                    Button button = input.getButton();
                    if (button == Button::JUMP) {
                        p1ClickTimes.push_back(actionTime);
                    }
                    else if (button == Button::LEFT) {
                        p1LeftClickTimes.push_back(actionTime);
                    }
                    else if (button == Button::RIGHT) {
                        p1RightClickTimes.push_back(actionTime);
                    }
                }
                else if (input.getClickType() == ClickType::SOFT) {
                    Button button = input.getButton();
                    if (button == Button::JUMP) {
                        p1SoftClickTimes.push_back(actionTime);
                    }
                    else if (button == Button::LEFT) {
                        p1LeftSoftClickTimes.push_back(actionTime);
                    }
                    else if (button == Button::RIGHT) {
                        p1RightSoftClickTimes.push_back(actionTime);
                    }
                }
                else if (input.getClickType() == ClickType::HARD) {
                    Button button = input.getButton();
                    if (button == Button::JUMP) {
                        p1HardClickTimes.push_back(actionTime);
                    }
                    else if (button == Button::LEFT) {
                        p1LeftHardClickTimes.push_back(actionTime);
                    }
                    else if (button == Button::RIGHT) {
                        p1RightHardClickTimes.push_back(actionTime);
                    }
                }
                else if (input.getClickType() == ClickType::MICRO) {
                    Button button = input.getButton();
                    if (button == Button::JUMP) {
                        p1MicroClickTimes.push_back(actionTime);
                    }
                    else if (button == Button::LEFT) {
                        p1LeftMicroClickTimes.push_back(actionTime);
                    }
                    else if (button == Button::RIGHT) {
                        p1RightMicroClickTimes.push_back(actionTime);
                    }
                }
            }
            else {
                if (input.getClickType() == ClickType::NORMAL) {
                    Button button = input.getButton();
                    if (button == Button::JUMP) {
                        p1ReleaseTimes.push_back(actionTime);
                    }
                    else if (button == Button::LEFT) {
                        p1LeftReleaseTimes.push_back(actionTime);
                    }
                    else if (button == Button::RIGHT) {
                        p1RightReleaseTimes.push_back(actionTime);
                    }
                }
                else if (input.getClickType() == ClickType::SOFT) {
                    Button button = input.getButton();
                    if (button == Button::JUMP) {
                        p1SoftReleaseTimes.push_back(actionTime);
                    }
                    else if (button == Button::LEFT) {
                        p1LeftSoftReleaseTimes.push_back(actionTime);
                    }
                    else if (button == Button::RIGHT) {
                        p1RightSoftReleaseTimes.push_back(actionTime);
                    }
                }
                else if (input.getClickType() == ClickType::HARD) {
                    Button button = input.getButton();
                    if (button == Button::JUMP) {
                        p1HardReleaseTimes.push_back(actionTime);
                    }
                    else if (button == Button::LEFT) {
                        p1LeftHardReleaseTimes.push_back(actionTime);
                    }
                    else if (button == Button::RIGHT) {
                        p1RightHardReleaseTimes.push_back(actionTime);
                    }
                }
                else if (input.getClickType() == ClickType::MICRO) {
                    Button button = input.getButton();
                    if (button == Button::JUMP) {
                        p1MicroReleaseTimes.push_back(actionTime);
                    }
                    else if (button == Button::LEFT) {
                        p1LeftMicroReleaseTimes.push_back(actionTime);
                    }
                    else if (button == Button::RIGHT) {
                        p1RightMicroReleaseTimes.push_back(actionTime);
                    }
                }
            }
        }

        for (Input input : action.getPlayerTwoInputs()) {
            if (input.isPressed()) {
                if (input.getClickType() == ClickType::NORMAL) {
                    Button button = input.getButton();
                    if (button == Button::JUMP) {
                        p2ClickTimes.push_back(actionTime);
                    }
                    else if (button == Button::LEFT) {
                        p2LeftClickTimes.push_back(actionTime);
                    }
                    else if (button == Button::RIGHT) {
                        p2RightClickTimes.push_back(actionTime);
                    }
                }
                else if (input.getClickType() == ClickType::SOFT) {
                    Button button = input.getButton();
                    if (button == Button::JUMP) {
                        p2SoftClickTimes.push_back(actionTime);
                    }
                    else if (button == Button::LEFT) {
                        p2LeftSoftClickTimes.push_back(actionTime);
                    }
                    else if (button == Button::RIGHT) {
                        p2RightSoftClickTimes.push_back(actionTime);
                    }
                }
                else if (input.getClickType() == ClickType::HARD) {
                    Button button = input.getButton();
                    if (button == Button::JUMP) {
                        p2HardClickTimes.push_back(actionTime);
                    }
                    else if (button == Button::LEFT) {
                        p2LeftHardClickTimes.push_back(actionTime);
                    }
                    else if (button == Button::RIGHT) {
                        p2RightHardClickTimes.push_back(actionTime);
                    }
                }
                else if (input.getClickType() == ClickType::MICRO) {
                    Button button = input.getButton();
                    if (button == Button::JUMP) {
                        p2MicroClickTimes.push_back(actionTime);
                    }
                    else if (button == Button::LEFT) {
                        p2LeftMicroClickTimes.push_back(actionTime);
                    }
                    else if (button == Button::RIGHT) {
                        p2RightMicroClickTimes.push_back(actionTime);
                    }
                }
            }
            else {
                if (input.getClickType() == ClickType::NORMAL) {
                    Button button = input.getButton();
                    if (button == Button::JUMP) {
                        p2ReleaseTimes.push_back(actionTime);
                    }
                    else if (button == Button::LEFT) {
                        p2LeftReleaseTimes.push_back(actionTime);
                    }
                    else if (button == Button::RIGHT) {
                        p2RightReleaseTimes.push_back(actionTime);
                    }
                }
                else if (input.getClickType() == ClickType::SOFT) {
                    Button button = input.getButton();
                    if (button == Button::JUMP) {
                        p2SoftReleaseTimes.push_back(actionTime);
                    }
                    else if (button == Button::LEFT) {
                        p2LeftSoftReleaseTimes.push_back(actionTime);
                    }
                    else if (button == Button::RIGHT) {
                        p2RightSoftReleaseTimes.push_back(actionTime);
                    }
                }
                else if (input.getClickType() == ClickType::HARD) {
                    Button button = input.getButton();
                    if (button == Button::JUMP) {
                        p2HardReleaseTimes.push_back(actionTime);
                    }
                    else if (button == Button::LEFT) {
                        p2LeftHardReleaseTimes.push_back(actionTime);
                    }
                    else if (button == Button::RIGHT) {
                        p2RightHardReleaseTimes.push_back(actionTime);
                    }
                }
                else if (input.getClickType() == ClickType::MICRO) {
                    Button button = input.getButton();
                    if (button == Button::JUMP) {
                        p2MicroReleaseTimes.push_back(actionTime);
                    }
                    else if (button == Button::LEFT) {
                        p2LeftMicroReleaseTimes.push_back(actionTime);
                    }
                    else if (button == Button::RIGHT) {
                        p2RightMicroReleaseTimes.push_back(actionTime);
                    }
                }
            }
        }
    }

    std::array<std::vector<float>, 48> clickTimes {
        p1ClickTimes,
        p1ReleaseTimes,
        p1SoftClickTimes,
        p1SoftReleaseTimes,
        p1HardClickTimes,
        p1HardReleaseTimes,
        p1MicroClickTimes,
        p1MicroReleaseTimes,

        p1LeftClickTimes,
        p1LeftReleaseTimes,
        p1LeftSoftClickTimes,
        p1LeftSoftReleaseTimes,
        p1LeftHardClickTimes,
        p1LeftHardReleaseTimes,
        p1LeftMicroClickTimes,
        p1LeftMicroReleaseTimes,

        p1RightClickTimes,
        p1RightReleaseTimes,
        p1RightSoftClickTimes,
        p1RightSoftReleaseTimes,
        p1RightHardClickTimes,
        p1RightHardReleaseTimes,
        p1RightMicroClickTimes,
        p1RightMicroReleaseTimes,

        p2ClickTimes,
        p2ReleaseTimes,
        p2SoftClickTimes,
        p2SoftReleaseTimes,
        p2HardClickTimes,
        p2HardReleaseTimes,
        p2MicroClickTimes,
        p2MicroReleaseTimes,

        p2LeftClickTimes,
        p2LeftReleaseTimes,
        p2LeftSoftClickTimes,
        p2LeftSoftReleaseTimes,
        p2LeftHardClickTimes,
        p2LeftHardReleaseTimes,
        p2LeftMicroClickTimes,
        p2LeftMicroReleaseTimes,

        p2RightClickTimes,
        p2RightReleaseTimes,
        p2RightSoftClickTimes,
        p2RightSoftReleaseTimes,
        p2RightHardClickTimes,
        p2RightHardReleaseTimes,
        p2RightMicroClickTimes,
        p2RightMicroReleaseTimes,
    };

    // Set soft clicks to normal, micro clicks to soft, hard clicks to normal if empty
    // dont forget releases too retard
    if (clickFiles[PLAYER_1_SOFTCLICKS].size() == 0) {
        clickFiles[PLAYER_1_SOFTCLICKS] = clickFiles[PLAYER_1_CLICKS];
    }

    if (clickFiles[PLAYER_1_SOFTRELEASES].size() == 0) {
        clickFiles[PLAYER_1_SOFTRELEASES] = clickFiles[PLAYER_1_RELEASES];
    }

    if (clickFiles[PLAYER_1_MICROCLICKS].size() == 0) {
        clickFiles[PLAYER_1_MICROCLICKS] = clickFiles[PLAYER_1_SOFTCLICKS];
    }

    if (clickFiles[PLAYER_1_MICRORELEASES].size() == 0) {
        clickFiles[PLAYER_1_MICRORELEASES] = clickFiles[PLAYER_1_SOFTRELEASES];
    }

    if (clickFiles[PLAYER_1_HARDCLICKS].size() == 0) {
        clickFiles[PLAYER_1_HARDCLICKS] = clickFiles[PLAYER_1_CLICKS];
    }

    if (clickFiles[PLAYER_1_HARDRELEASES].size() == 0) {
        clickFiles[PLAYER_1_HARDRELEASES] = clickFiles[PLAYER_1_RELEASES];
    }



    if (clickFiles[PLAYER_1_LEFTSOFTCLICKS].size() == 0) {
        clickFiles[PLAYER_1_LEFTSOFTCLICKS] = clickFiles[PLAYER_1_LEFTCLICKS];
    }

    if (clickFiles[PLAYER_1_LEFTSOFTRELEASES].size() == 0) {
        clickFiles[PLAYER_1_LEFTSOFTRELEASES] = clickFiles[PLAYER_1_LEFTRELEASES];
    }

    if (clickFiles[PLAYER_1_LEFTMICROCLICKS].size() == 0) {
        clickFiles[PLAYER_1_LEFTMICROCLICKS] = clickFiles[PLAYER_1_LEFTSOFTCLICKS];
    }

    if (clickFiles[PLAYER_1_LEFTMICRORELEASES].size() == 0) {
        clickFiles[PLAYER_1_LEFTMICRORELEASES] = clickFiles[PLAYER_1_LEFTSOFTRELEASES];
    }

    if (clickFiles[PLAYER_1_LEFTHARDCLICKS].size() == 0) {
        clickFiles[PLAYER_1_LEFTHARDCLICKS] = clickFiles[PLAYER_1_LEFTCLICKS];
    }

    if (clickFiles[PLAYER_1_LEFTHARDRELEASES].size() == 0) {
        clickFiles[PLAYER_1_LEFTHARDRELEASES] = clickFiles[PLAYER_1_LEFTRELEASES];
    }



    if (clickFiles[PLAYER_1_RIGHTSOFTCLICKS].size() == 0) {
        clickFiles[PLAYER_1_RIGHTSOFTCLICKS] = clickFiles[PLAYER_1_RIGHTCLICKS];
    }

    if (clickFiles[PLAYER_1_RIGHTSOFTRELEASES].size() == 0) {
        clickFiles[PLAYER_1_RIGHTSOFTRELEASES] = clickFiles[PLAYER_1_RIGHTRELEASES];
    }

    if (clickFiles[PLAYER_1_RIGHTMICROCLICKS].size() == 0) {
        clickFiles[PLAYER_1_RIGHTMICROCLICKS] = clickFiles[PLAYER_1_RIGHTSOFTCLICKS];
    }

    if (clickFiles[PLAYER_1_RIGHTMICRORELEASES].size() == 0) {
        clickFiles[PLAYER_1_RIGHTMICRORELEASES] = clickFiles[PLAYER_1_RIGHTSOFTRELEASES];
    }

    if (clickFiles[PLAYER_1_RIGHTHARDCLICKS].size() == 0) {
        clickFiles[PLAYER_1_RIGHTHARDCLICKS] = clickFiles[PLAYER_1_RIGHTCLICKS];
    }

    if (clickFiles[PLAYER_1_RIGHTHARDRELEASES].size() == 0) {
        clickFiles[PLAYER_1_RIGHTHARDRELEASES] = clickFiles[PLAYER_1_RIGHTRELEASES];
    }



    if (clickFiles[PLAYER_2_SOFTCLICKS].size() == 0) {
        clickFiles[PLAYER_2_SOFTCLICKS] = clickFiles[PLAYER_2_CLICKS];
    }

    if (clickFiles[PLAYER_2_SOFTRELEASES].size() == 0) {
        clickFiles[PLAYER_2_SOFTRELEASES] = clickFiles[PLAYER_2_RELEASES];
    }

    if (clickFiles[PLAYER_2_MICROCLICKS].size() == 0) {
        clickFiles[PLAYER_2_MICROCLICKS] = clickFiles[PLAYER_2_SOFTCLICKS];
    }

    if (clickFiles[PLAYER_2_MICRORELEASES].size() == 0) {
        clickFiles[PLAYER_2_MICRORELEASES] = clickFiles[PLAYER_2_SOFTRELEASES];
    }

    if (clickFiles[PLAYER_2_HARDCLICKS].size() == 0) {
        clickFiles[PLAYER_2_HARDCLICKS] = clickFiles[PLAYER_2_CLICKS];
    }

    if (clickFiles[PLAYER_2_HARDRELEASES].size() == 0) {
        clickFiles[PLAYER_2_HARDRELEASES] = clickFiles[PLAYER_2_RELEASES];
    }



    if (clickFiles[PLAYER_2_LEFTSOFTCLICKS].size() == 0) {
        clickFiles[PLAYER_2_LEFTSOFTCLICKS] = clickFiles[PLAYER_2_LEFTCLICKS];
    }

    if (clickFiles[PLAYER_2_LEFTSOFTRELEASES].size() == 0) {
        clickFiles[PLAYER_2_LEFTSOFTRELEASES] = clickFiles[PLAYER_2_LEFTRELEASES];
    }

    if (clickFiles[PLAYER_2_LEFTMICROCLICKS].size() == 0) {
        clickFiles[PLAYER_2_LEFTMICROCLICKS] = clickFiles[PLAYER_2_LEFTSOFTCLICKS];
    }

    if (clickFiles[PLAYER_2_LEFTMICRORELEASES].size() == 0) {
        clickFiles[PLAYER_2_LEFTMICRORELEASES] = clickFiles[PLAYER_2_LEFTSOFTRELEASES];
    }

    if (clickFiles[PLAYER_2_LEFTHARDCLICKS].size() == 0) {
        clickFiles[PLAYER_2_LEFTHARDCLICKS] = clickFiles[PLAYER_2_LEFTCLICKS];
    }

    if (clickFiles[PLAYER_2_LEFTHARDRELEASES].size() == 0) {
        clickFiles[PLAYER_2_LEFTHARDRELEASES] = clickFiles[PLAYER_2_LEFTRELEASES];
    }



    if (clickFiles[PLAYER_2_RIGHTSOFTCLICKS].size() == 0) {
        clickFiles[PLAYER_2_RIGHTSOFTCLICKS] = clickFiles[PLAYER_2_RIGHTCLICKS];
    }

    if (clickFiles[PLAYER_2_RIGHTSOFTRELEASES].size() == 0) {
        clickFiles[PLAYER_2_RIGHTSOFTRELEASES] = clickFiles[PLAYER_2_RIGHTRELEASES];
    }

    if (clickFiles[PLAYER_2_RIGHTMICROCLICKS].size() == 0) {
        clickFiles[PLAYER_2_RIGHTMICROCLICKS] = clickFiles[PLAYER_2_RIGHTSOFTCLICKS];
    }

    if (clickFiles[PLAYER_2_RIGHTMICRORELEASES].size() == 0) {
        clickFiles[PLAYER_2_RIGHTMICRORELEASES] = clickFiles[PLAYER_2_RIGHTSOFTRELEASES];
    }

    if (clickFiles[PLAYER_2_RIGHTHARDCLICKS].size() == 0) {
        clickFiles[PLAYER_2_RIGHTHARDCLICKS] = clickFiles[PLAYER_2_RIGHTCLICKS];
    }

    if (clickFiles[PLAYER_2_RIGHTHARDRELEASES].size() == 0) {
        clickFiles[PLAYER_2_RIGHTHARDRELEASES] = clickFiles[PLAYER_2_RIGHTRELEASES];
    }



    // Add click sounds to the output buffer
    for (int i {0}; i < clickTimes.size(); i++)
    {
        if (clickFiles[i].empty()) {
            continue; // skip adding to buffer because adding an empty vector causes a crash
        }
        addToBuffer(clickTimes[i], clickFiles[i], outputBuffer, sampleRate, channels);
    }


    // Write to new WAV file
    SF_INFO sfinfoOut = audioInfo;
    sfinfoOut.frames = totalFrames;
    std::string macroName {macro.getModifiableName()};

    SNDFILE *outFile = sf_open(macroName.append(".wav").c_str(), SFM_WRITE, &sfinfoOut);

    if (!outFile) {
        std::cerr << "Error creating output file for " << macroName << '\n';
        return false;
    }

    sf_write_short(outFile, outputBuffer.data(), outputBuffer.size());
    sf_close(outFile);

    std::cout << "Successfully generated clicks for " << macroName << '\n';

    return true;
}
