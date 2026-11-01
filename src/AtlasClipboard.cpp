#include "AtlasLocalization.h"
#include "AtlasClipboard.h"
#include <Windows.h>
#include <algorithm>
#include <cstring>
#include <vector>

namespace cmm
{
    namespace
    {
        constexpr size_t MaxClipboardCode = 1024 * 1024;
        struct CloseClipboardOnExit { ~CloseClipboardOnExit() { CloseClipboard(); } };
    }
    bool CopyLinkToClipboard(std::string_view text, std::string& error)
    {
        error.clear();
        // The share format is ASCII, so widening is exact and cannot truncate UTF-8.
        if (text.empty() || text.size() > MaxClipboardCode) { error = Tr("Invalid code length."); return false; }
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, (text.size()+1)*sizeof(wchar_t));
        if (!memory) { error = Tr("Not enough memory for the clipboard."); return false; }
        auto* buffer = static_cast<wchar_t*>(GlobalLock(memory));
        if (!buffer) { GlobalFree(memory); error = Tr("Could not prepare the clipboard."); return false; }
        for (size_t i=0;i<text.size();++i) buffer[i]=static_cast<unsigned char>(text[i]);
        buffer[text.size()]=0;
        GlobalUnlock(memory);
        if (!OpenClipboard(GetForegroundWindow())) { GlobalFree(memory); error = Tr("Clipboard busy. Click copy again."); return false; }
        CloseClipboardOnExit close;
        if (!EmptyClipboard() || !SetClipboardData(CF_UNICODETEXT, memory))
        { GlobalFree(memory); error = Tr("Could not copy the code."); return false; }
        // Windows owns memory after SetClipboardData succeeds.
        return true;
    }
    bool ReadLinkFromClipboard(std::string& text, std::string& error)
    {
        text.clear(); error.clear();
        if (!OpenClipboard(nullptr)) { error = Tr("Clipboard busy. Paste into the field using Ctrl+V."); return false; }
        CloseClipboardOnExit close;
        HANDLE memory = GetClipboardData(CF_UNICODETEXT);
        const SIZE_T size = memory ? GlobalSize(memory) : 0;
        if (!memory || size < sizeof(wchar_t))
        { error = Tr("The clipboard does not contain a valid Vekk's Atlas code."); return false; }
        const size_t capacity = size / sizeof(wchar_t);
        const size_t scanCapacity = std::min(capacity, MaxClipboardCode + 1);
        std::string copied(scanCapacity, '\0');
        const auto* buffer = static_cast<const wchar_t*>(GlobalLock(memory));
        if (!buffer) { error = Tr("Could not read the clipboard."); return false; }
        const size_t length = wcsnlen_s(buffer, scanCapacity);
        bool valid = length < scanCapacity && length <= MaxClipboardCode;
        // Allocation happened before locking the clipboard memory.
        if (valid) for (size_t i=0;i<length;++i)
        {
            if (buffer[i] > 127) { valid=false; break; }
            copied[i]=static_cast<char>(buffer[i]);
        }
        GlobalUnlock(memory);
        if (!valid) { error = Tr("The text is not a valid Atlas share code or is too long."); return false; }
        text.assign(copied.data(), length);
        return true;
    }
}
