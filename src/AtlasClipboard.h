#pragma once
#include <string>
#include <string_view>
namespace cmm
{
    bool CopyLinkToClipboard(std::string_view text, std::string& error);
    bool ReadLinkFromClipboard(std::string& text, std::string& error);
}
