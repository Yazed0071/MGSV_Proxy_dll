// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <windows.h>
#include <unknwn.h>
#include <filesystem>
#include <string>
#include <vector>
#include <shellapi.h>
#include <fstream>

HMODULE realDInput8 = nullptr;

// Logger for debugging
void Log(const std::string& msg) {
    std::ofstream log("dinput8_proxy.log", std::ios::app);
    if (log.is_open()) {
        log << msg << std::endl;
    }
}

// Load the original dinput8.dll from system folder
void LoadRealDInput8() {
    if (realDInput8) return;

    char systemDir[MAX_PATH];
    GetSystemDirectoryA(systemDir, MAX_PATH);
    std::string realPath = std::string(systemDir) + "\\dinput8.dll";

    realDInput8 = LoadLibraryA(realPath.c_str());
    if (!realDInput8) {
        Log("Failed to load real dinput8.dll");
        MessageBoxA(nullptr, "Failed to load real dinput8.dll", "Proxy DLL", MB_ICONERROR);
        ExitProcess(1);
    }
    else {
        Log("Loaded real dinput8.dll");
    }
}

// Load all .dll files next to this DLL (except dinput8.dll)
void LoadNearbyDLLs(const std::filesystem::path& baseDir) {
    for (const auto& entry : std::filesystem::directory_iterator(baseDir)) {
        auto filename = entry.path().filename().wstring();

        if (entry.path().extension() == L".dll" &&
            filename != L"dinput8.dll" &&
            filename != L"steam_api64.dll") {
            Log("Loading nearby DLL: " + entry.path().string());
            LoadLibraryW(entry.path().c_str());
        }
        else {
            Log("Skipping DLL: " + entry.path().string());
        }
    }
}


// Load all .dll files from mod/hooks/
void LoadModHooks(const std::filesystem::path& baseDir) {
    std::filesystem::path hookDir = baseDir / "mod" / "hooks";

    if (!std::filesystem::exists(hookDir)) {
        Log("No mod/hooks/ directory found.");
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(hookDir)) {
        if (entry.path().extension() == ".dll") {
            Log("Loading hook: " + entry.path().string());
            LoadLibraryW(entry.path().c_str());
        }
    }
}

// Initialization thread (safe from DllMain restrictions)
DWORD WINAPI InitThread(LPVOID) {
    char modulePath[MAX_PATH];
    GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
    std::filesystem::path baseDir = std::filesystem::path(modulePath).parent_path();

    LoadNearbyDLLs(baseDir);
    LoadModHooks(baseDir);
    return 0;
}

// Entry point for the proxy function
extern "C" __declspec(dllexport)
HRESULT WINAPI DirectInput8Create(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf,
    LPVOID* ppvOut, LPUNKNOWN punkOuter) {
    LoadRealDInput8();

    auto originalFunc = (decltype(&DirectInput8Create))GetProcAddress(realDInput8, "DirectInput8Create");
    if (!originalFunc) {
        Log("Failed to get DirectInput8Create from real DLL.");
        return E_FAIL;
    }

    Log("Forwarding DirectInput8Create call.");
    return originalFunc(hinst, dwVersion, riidltf, ppvOut, punkOuter);
}

// DllMain
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
    }
    return TRUE;
}