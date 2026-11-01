#pragma once
#include <string>
#include <string_view>
namespace cmm
{
    enum class Language { English, Spanish, German };
    Language GetLanguage();
    void SetLanguage(Language language);
    const char* LanguageCode(Language language);
    Language LanguageFromCode(std::string_view code);
    const char* Tr(const char* english);
    // The visible label is localized; the ID always comes from the English key.
    std::string UiLabel(const char* english);
}
