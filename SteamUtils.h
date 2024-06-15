#include "pch.h"
#include "isteamutils.h"
#include <MinHook.h>

template <typename T>
inline MH_STATUS MH_CreateHookEx(LPVOID pTarget, LPVOID pDetour, T** ppOriginal) {
    return MH_CreateHook(pTarget, pDetour, reinterpret_cast<LPVOID*>(ppOriginal));
}

template <typename T>
inline MH_STATUS MH_CreateHookApiEx(LPCWSTR pszModule, LPCSTR pszProcName, LPVOID pDetour, T** ppOriginal) {
    return MH_CreateHookApi(pszModule, pszProcName, pDetour, reinterpret_cast<LPVOID*>(ppOriginal));
}

namespace logger = SKSE::log;

namespace SteamWorkaround {
    ISteamUtils* steamUtils = NULL;

    bool isInitialized = false;
    bool isLoaded = false;

    const int loadCyclesMax = 300;
    int loadCycles = 0;

    typedef void(WINAPI* SteamAPI_RunCallbacks)(void);

    SteamAPI_RunCallbacks fpSteamAPI_RunCallbacks = NULL;

    void DetourSteamAPI_RunCallbacks()
    {
        if (steamUtils == NULL && fpSteamAPI_RunCallbacks != NULL) {
            fpSteamAPI_RunCallbacks();
            return;
        }

        if (fpSteamAPI_RunCallbacks == NULL) {
            return;
        }

        if (isInitialized || steamUtils->GetEnteredGamepadTextLength() > 1) {
            isInitialized = true;
            fpSteamAPI_RunCallbacks();
            return;
        }

        if (loadCycles < loadCyclesMax) {
            loadCycles++;
        }

        if (!isLoaded && loadCycles >= loadCyclesMax) {
            isLoaded = true;        
        }

        if (!isLoaded) {
            fpSteamAPI_RunCallbacks();
        }
    }

    typedef bool(__stdcall* SteamAPI_Init)();

    typedef ISteamUtils*(__stdcall* GetISteamUtils)();

    static void Init() {
        logger::info("Init Steam Keyboard workaround.");

        auto handle = LoadLibrary(L"steam_api64.dll");

        if (handle == NULL) {
            logger::info("Steam Api is NULL.");
            return;
        }

        logger::info("Steam Api handle found.");

        auto steamAPI_Init = (SteamAPI_Init)GetProcAddress(handle, "SteamAPI_Init");

        if (steamAPI_Init == NULL) {
            logger::info("SteamAPI_Init is NULL.");
            return;
        }

        if (!steamAPI_Init()) {
            logger::info("SteamAPI_Init returned false.");
            return;
        }

        auto getISteamUtils = (GetISteamUtils)GetProcAddress(handle, "SteamAPI_SteamUtils_v010");

        if (getISteamUtils == NULL) {
            logger::info("GetISteamUtils is NULL.");
            return;
        }

        steamUtils = getISteamUtils();

        if (steamUtils == NULL) {
            logger::info("SteamUtils not found.");
            return;
        }

        if (!steamUtils->IsSteamInBigPictureMode())
        {
            logger::info("Game is not running in Big Picture / Game Mode.");
            return;
        }

        if (MH_Initialize() != MH_OK) {
            logger::info("Could not initialize MinHook.");
            return;
        }

        if (MH_CreateHookApiEx(L"steam_api64.dll", "SteamAPI_RunCallbacks", &DetourSteamAPI_RunCallbacks,
                               &fpSteamAPI_RunCallbacks) != MH_OK) {
            logger::info("Could not hook SteamAPI_RunCallbacks.");
            return;
        }

        if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
            logger::info("Could not enable hooks.");
            return;
        }

        logger::info("MinHook initialization finished.");
    }

    static void SetIsLoaded() {
        logger::info("Set workaroundas loaded.");
        isLoaded = true;
    }
}