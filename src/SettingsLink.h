#pragma once

#include "AtlasPreferences.h"

#include <string>
#include <string_view>

namespace cmm
{
    bool EncodeSettingsLink(const Preferences& settings, std::string& code, std::string& error);
    bool DecodeSettingsLink(std::string_view code, Preferences& settings, std::string& error);
}
