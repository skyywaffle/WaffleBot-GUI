#include "Macro.h"
#include "Action.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <array>
#include "simdjson.h"

namespace fs = std::filesystem;
using namespace simdjson;
using namespace std::string_view_literals;

// File type is JSON, supports xdBot, MH Replay, and TASBot JSONs
Macro::Macro(const std::string& filepath) {
    m_name = fs::path(filepath).stem().string();
    simdjson_result<padded_string> macroBuffer = padded_string::load(filepath);
    m_isLoaded = false;


    determineBotType(macroBuffer);
    parseMacroJson(macroBuffer);
    // determineClickTypes();

    m_isLoaded = (m_fps > 0 && !m_actions.empty());

    if (!m_isLoaded) {
        return;
    }


    // xdBot has the wrong 2p bool if it's a 2 player macro
    if (isTwoPlayer() && getBot() == Bot::XDBOT_GDR) {
        swapPlayerOneAndTwoActions();
    }
}

ondemand::document Macro::getMacroData(const simdjson_result<padded_string>& macroBuffer) {
    // Create a new parser instance each time to avoid iterator reuse issues
    // This prevents stack overflow from simdjson's internal iterator management
    ondemand::parser fresh_parser;
    return fresh_parser.iterate(macroBuffer);
}

void Macro::determineBotType(const simdjson_result<padded_string>& macroBuffer) {
    // Use a dedicated parser for bot type determination
    ondemand::parser bot_parser;
    auto macroData = bot_parser.iterate(macroBuffer);

    if (auto botName = macroData["bot"]["name"].get_string(); !botName.error()) {
        if (botName.value() == "MH_REPLAY"sv) {
            m_bot = Bot::MH_REPLAY_GDR;
        }
        else if (botName.value() == "xdBot"sv) {
            m_bot = Bot::XDBOT_GDR;
        }
        return;
    }

    // Determine if macro is TASBot
    // TASBot has "fps" as the first key-value pair
    // Create a fresh parser instance for this check
    ondemand::parser tasbot_parser;
    auto macroData2 = tasbot_parser.iterate(macroBuffer);

    auto macroObject = macroData2.get_object();

    for (auto field : macroObject) {
        if (field.key() == "fps") {
            m_bot = Bot::TASBOT;
            return;
        }
        break;
    }

    // else the macro is not supported
    std::cerr << "ERROR: " << m_name << " is either an unsupported macro or a corrupted one.\n";
    return;
}

void Macro::parseMacroJson(const simdjson_result<padded_string>& macroBuffer) {
    // Use a dedicated parser for macro parsing
    ondemand::parser macro_parser;
    auto macroData = macro_parser.iterate(macroBuffer);

    // Parse xdBot JSON macro
    if (m_bot == Bot::XDBOT_GDR) {
        // Get framerate
        ondemand::value framerate_val;
        auto err = macroData["framerate"].get(framerate_val);
        if (err) {
            std::cerr << "Failed to get 'framerate' value: " << err << "\n";
            return;
        }

        double framerate_double;
        err = framerate_val.get(framerate_double);
        if (err) {
            std::cerr << "Failed to parse 'framerate' as double: " << err << "\n";
            return;
        }
        m_fps = static_cast<int>(framerate_double);

        // Get duration
        ondemand::value duration_val;
        err = macroData["duration"].get(duration_val);
        if (err) {
            std::cerr << "Failed to get 'duration' value: " << err << "\n";
            return;
        }

        double durationInSec;
        err = duration_val.get(durationInSec);
        if (err) {
            std::cerr << "Failed to parse 'duration' as double: " << err << "\n";
            return;
        }

        m_frameCount = static_cast<int>(std::round(durationInSec * m_fps));
    }

    // Parse MH Replay JSON
    else if (m_bot == Bot::MH_REPLAY_GDR) {
        // MH Replay macros have a "framerate" object only if physics bypass is enabled, else it's just 240fps
        bool framerateIsNot240 = true;

        // Get framerate
        ondemand::value framerate_val;
        auto err = macroData["framerate"].get(framerate_val);
        if (err) {
            m_fps = 240;
            framerateIsNot240 = false;
        }

        if (framerateIsNot240) {
            double framerate_double;
            err = framerate_val.get(framerate_double);
            if (err) {
                framerate_double = 240.0;
            }
            m_fps = static_cast<int>(framerate_double);
        }

        m_frameCount = static_cast<int>(macroData["duration"].get_int64());
    }

    // Parse TASBot macro
    else if (m_bot == Bot::TASBOT) {
        m_fps = static_cast<int>(macroData["fps"].get_double());

        auto macroArray = macroData["macro"].get_array();
        int lastFrame = 0;

        for (auto element : macroArray) {
            lastFrame = static_cast<int>(element.value()["frame"].get_int64());
        }

        m_frameCount = lastFrame;
    }

    // Process inputs with a separate parser instance
    // This prevents simdjson iterator stack overflow issues
    if (m_bot == Bot::XDBOT_GDR || m_bot == Bot::MH_REPLAY_GDR) {
        // Create a fresh parser for inputs processing
        ondemand::parser inputs_parser;
        auto inputsData = inputs_parser.iterate(macroBuffer);

        auto inputs_result = inputsData["inputs"].get_array();
        if (inputs_result.error()) {
            std::cerr << "Failed to read 'inputs' array: " << inputs_result.error() << "\n";
            return;
        }

        // Process each input individually to avoid deep stack recursion
        for (auto actionData : inputs_result.value()) {
            try {
                m_actions.emplace_back(actionData, Bot::MH_REPLAY_GDR);
            } catch (const std::exception& e) {
                std::cerr << "Error processing action: " << e.what() << "\n";
            }
        }

        // Merge different actions on the same frame (happens if player 1 and player 2 make an action on the same frame)
        for (size_t i = 1; i < m_actions.size(); i++) {
            if (m_actions[i].getFrame() == m_actions[i - 1].getFrame()) {
                // Transfer player 2's inputs to the previous action, where there are no player 2 inputs
                if (m_actions[i].getPlayerOneInputs().empty()) {
                    m_actions[i - 1].setPlayerTwoInputs(m_actions[i].getPlayerTwoInputs());

                    // Remove the now redundant action
                    m_actions.erase(m_actions.begin() + i);
                    i--; // Adjust index after erase
                }

                // Else transfer player 1's inputs to the previous action, where there are no player 1 inputs
                else if (m_actions[i].getPlayerTwoInputs().empty()) {
                    m_actions[i - 1].setPlayerOneInputs(m_actions[i].getPlayerOneInputs());

                    // Remove the now redundant action
                    m_actions.erase(m_actions.begin() + i);
                    i--; // Adjust index after erase
                }
            }
        }
    }

    // Grab inputs for TASBot
    else if (m_bot == Bot::TASBOT) {
        // Create a fresh parser for TASBot inputs
        ondemand::parser tasbot_inputs_parser;
        auto tasbotData = tasbot_inputs_parser.iterate(macroBuffer);

        auto inputs_result = tasbotData["macro"].get_array();
        if (inputs_result.error()) {
            std::cerr << "Failed to read 'inputs' array: " << inputs_result.error() << "\n";
            return;
        }

        for (auto actionData : inputs_result.value()) {
            try {
                m_actions.emplace_back(actionData, Bot::TASBOT);
            } catch (const std::exception& e) {
                std::cerr << "Error processing TASBot action: " << e.what() << "\n";
                continue; // Skip malformed actions
            }
        }
        // We don't need to consider merging different actions on same frame, already taken care of in Action.cpp
    }
}

void Macro::determineClickTypes(
    double microClickTime,
    double microClickAfterReleaseTime,
    double softClickTime,
    double softClickAfterReleaseTime,
    double hardClickTime,
    double hardClickAfterReleaseTime) {

    std::array<int, 3> p1lastButtonFrame{};
    std::array<int, 3> p1lastLastButtonFrame{};
    std::array<bool, 3> p1isFirstButtonInput{true, true, true};
    std::array<ClickType, 3> p1LastButtonType{ClickType::NORMAL, ClickType::NORMAL, ClickType::NORMAL};

    std::array<int, 3> p2lastButtonFrame{};
    std::array<int, 3> p2lastLastButtonFrame{};
    std::array<bool, 3> p2isFirstButtonInput{true, true, true};
    std::array<ClickType, 3> p2LastButtonType{ClickType::NORMAL, ClickType::NORMAL, ClickType::NORMAL};

    for (Action& action : m_actions) {
        int currentFrame = action.getFrame();

        for (Input& input : action.getPlayerOneInputs()) {
            int button = static_cast<int>(input.getButton());
            if (button >= 0 && button < 3) { // Bounds check
                if (!p1isFirstButtonInput[button]) {
                    if (input.isPressed()) {
                        double timeDeltaFromLastRelease = static_cast<double>(currentFrame - p1lastButtonFrame[button]) / m_fps;
                        double timeDeltaFromLastClick = static_cast<double>(currentFrame - p1lastLastButtonFrame[button]) / m_fps;

                        if (timeDeltaFromLastRelease < softClickAfterReleaseTime || timeDeltaFromLastClick < softClickTime) {
                            input.setClickType(ClickType::SOFT);
                        } else if (timeDeltaFromLastRelease > hardClickAfterReleaseTime || timeDeltaFromLastClick > hardClickTime) {
                            input.setClickType(ClickType::HARD);
                        } else if (timeDeltaFromLastRelease < microClickAfterReleaseTime || timeDeltaFromLastClick < microClickTime) {
                            input.setClickType(ClickType::MICRO);
                        }
                    } else {
                        input.setClickType(p1LastButtonType[button]);
                    }
                    p1lastLastButtonFrame[button] = p1lastButtonFrame[button];
                    p1lastButtonFrame[button] = currentFrame;
                } else {
                    p1lastLastButtonFrame[button] = p1lastButtonFrame[button];
                    p1lastButtonFrame[button] = currentFrame;
                    p1isFirstButtonInput[button] = false;
                }
            }
        }

        for (Input& input : action.getPlayerTwoInputs()) {
            int button = static_cast<int>(input.getButton());
            if (button >= 0 && button < 3) { // Bounds check
                if (!p2isFirstButtonInput[button]) {
                    if (input.isPressed()) {
                        double timeDeltaFromLastRelease = static_cast<double>(currentFrame - p2lastButtonFrame[button]) / m_fps;
                        double timeDeltaFromLastClick = static_cast<double>(currentFrame - p2lastLastButtonFrame[button]) / m_fps;

                        if (timeDeltaFromLastRelease < softClickAfterReleaseTime || timeDeltaFromLastClick < softClickTime) {
                            input.setClickType(ClickType::SOFT);
                        } else if (timeDeltaFromLastRelease > hardClickAfterReleaseTime || timeDeltaFromLastClick > hardClickTime) {
                            input.setClickType(ClickType::HARD);
                        } else if (timeDeltaFromLastRelease < microClickAfterReleaseTime || timeDeltaFromLastClick < microClickTime) {
                            input.setClickType(ClickType::MICRO);
                        }
                    } else {
                        input.setClickType(p2LastButtonType[button]);
                    }
                    p2lastLastButtonFrame[button] = p2lastButtonFrame[button];
                    p2lastButtonFrame[button] = currentFrame;
                } else {
                    p2lastLastButtonFrame[button] = p2lastButtonFrame[button];
                    p2lastButtonFrame[button] = currentFrame;
                    p2isFirstButtonInput[button] = false;
                }
            }
        }
    }
}

bool Macro::isTwoPlayer() {
    bool hasPlayer1 = std::any_of(m_actions.begin(), m_actions.end(),
        [](Action& a) { return !a.getPlayerOneInputs().empty(); });

    bool hasPlayer2 = std::any_of(m_actions.begin(), m_actions.end(),
        [](Action& a) { return !a.getPlayerTwoInputs().empty(); });

    return hasPlayer1 && hasPlayer2;
}

bool Macro::isPlatformer() {
    if (m_bot == Bot::MH_REPLAY_GDR || m_bot == Bot::XDBOT_GDR) {
        for (Action& action : m_actions) {
            for (Input& input : action.getPlayerOneInputs()) {
                if (input.getButton() == Button::LEFT || input.getButton() == Button::RIGHT) {
                    return true;
                }
            }
            for (Input& input : action.getPlayerTwoInputs()) {
                if (input.getButton() == Button::LEFT || input.getButton() == Button::RIGHT) {
                    return true;
                }
            }
        }
    }
    return false;
}

void Macro::swapPlayerOneAndTwoActions() {
    for (Action& action : m_actions) {
        std::swap(action.getPlayerOneInputs(), action.getPlayerTwoInputs());
    }
}
