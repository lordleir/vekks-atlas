#pragma once

#include <Windows.h>

#include <cstdint>
#include <string>
#include <utility>

namespace cmm
{
    struct Shortcut
    {
        uint16_t key{};
        bool alt{};
        bool ctrl{};
        bool shift{};
    };

    bool IsModifierKey(uint16_t key);
    bool MatchesShortcut(const Shortcut& shortcut, uint16_t key, bool alt, bool ctrl, bool shift);
    std::string ShortcutText(const Shortcut& shortcut);
    std::pair<float, float> ClampLauncherPosition(float x, float y, float size,
        float displayWidth, float displayHeight);
}
