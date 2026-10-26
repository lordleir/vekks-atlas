#include <Windows.h>
#include <Nexus.h>

#include <iostream>

int wmain(int argc, wchar_t** argv)
{
    if (argc != 2) { std::cerr << "Expected DLL path.\n"; return 1; }
    const HMODULE module = LoadLibraryW(argv[1]);
    if (!module) { std::cerr << "Could not load addon DLL.\n"; return 1; }
    const auto getDefinition = reinterpret_cast<AddonDefinition_t* (*)()>(GetProcAddress(module, "GetAddonDef"));
    if (!getDefinition) { std::cerr << "GetAddonDef export missing.\n"; FreeLibrary(module); return 1; }
    const auto* definition = getDefinition();
    const bool valid = definition && definition->Load && definition->Unload && definition->Flags == AF_None;
    if (!valid) std::cerr << "Addon definition still disables hot-loading or lacks lifecycle callbacks.\n";
    FreeLibrary(module);
    return valid ? 0 : 1;
}
