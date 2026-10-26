#include "AtlasShortcut.h"

#include <array>
#include <algorithm>

namespace cmm
{
    bool IsModifierKey(uint16_t key)
    {
        return key == VK_SHIFT || key == VK_LSHIFT || key == VK_RSHIFT ||
            key == VK_CONTROL || key == VK_LCONTROL || key == VK_RCONTROL ||
            key == VK_MENU || key == VK_LMENU || key == VK_RMENU;
    }

    bool MatchesShortcut(const Shortcut& shortcut, uint16_t key, bool alt, bool ctrl, bool shift)
    {
        return shortcut.key != 0 && shortcut.key == key && shortcut.alt == alt &&
            shortcut.ctrl == ctrl && shortcut.shift == shift;
    }

    std::string ShortcutText(const Shortcut& shortcut)
    {
        if (!shortcut.key) return "Not assigned";
        std::string text;
        if (shortcut.ctrl) text += "Ctrl+";
        if (shortcut.alt) text += "Alt+";
        if (shortcut.shift) text += "Shift+";
        if ((shortcut.key >= 'A' && shortcut.key <= 'Z') || (shortcut.key >= '0' && shortcut.key <= '9'))
            text.push_back(static_cast<char>(shortcut.key));
        else if (shortcut.key >= VK_F1 && shortcut.key <= VK_F24)
            text += "F" + std::to_string(shortcut.key - VK_F1 + 1);
        else
        {
            const UINT scan = MapVirtualKeyW(shortcut.key, MAPVK_VK_TO_VSC);
            std::array<wchar_t, 64> name{};
            const LONG parameter = static_cast<LONG>(scan << 16);
            const int count = GetKeyNameTextW(parameter, name.data(), static_cast<int>(name.size()));
            if (count > 0)
            {
                const int bytes = WideCharToMultiByte(CP_UTF8, 0, name.data(), count, nullptr, 0, nullptr, nullptr);
                std::string converted(static_cast<size_t>(bytes), '\0');
                if (bytes > 0) WideCharToMultiByte(CP_UTF8, 0, name.data(), count, converted.data(), bytes, nullptr, nullptr);
                text += converted;
            }
            else text += "Key " + std::to_string(shortcut.key);
        }
        return text;
    }

    std::pair<float, float> ClampLauncherPosition(float x, float y, float size,
        float displayWidth, float displayHeight)
    {
        constexpr float margin = 0.0f;
        const float safeSize = std::max(1.0f, size);
        return {
            std::clamp(x, margin, std::max(margin, displayWidth - safeSize - margin)),
            std::clamp(y, margin, std::max(margin, displayHeight - safeSize - margin))};
    }
}
