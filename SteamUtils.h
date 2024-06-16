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

    bool isLoaded = false;

    bool initTextLengthSet = false;
    uint32 initTextLength = 0;

    typedef bool(WINAPI* SteamAPI_Init)();

    SteamAPI_Init SteamAPIInit = NULL;

    typedef ISteamUtils*(__stdcall* GetISteamUtils)(void);

    GetISteamUtils ISteamUtilsConstructor = NULL;

    typedef void(WINAPI* SteamAPI_ReleaseCurrentThreadMemory)();

    SteamAPI_ReleaseCurrentThreadMemory ReleaseMemory = NULL;

    typedef void(WINAPI* SteamAPI_RunCallbacks)(void);

    SteamAPI_RunCallbacks fpSteamAPI_RunCallbacks = NULL;

    void DetourSteamAPI_RunCallbacks()
    {
        if (fpSteamAPI_RunCallbacks == NULL) {
            return;
        }

        if (!isLoaded) {
            fpSteamAPI_RunCallbacks();
        }

        auto utils = ISteamUtilsConstructor();
        auto textLength = utils->GetEnteredGamepadTextLength();

        logger::info("Text length: {0}", textLength);

        if (!initTextLengthSet) {
            logger::info("Using {0} as init text length.", textLength);
            initTextLength = textLength;
            initTextLengthSet = true;
        }

        if (textLength != initTextLength) {
            fpSteamAPI_RunCallbacks();
            return;
        }

        ReleaseMemory();
    }

    static void Init() {
        logger::info("Init Steam Keyboard workaround.");

        auto handle = LoadLibrary(L"steam_api64.dll");

        if (handle == NULL) {
            logger::info("Steam Api is NULL.");
            return;
        }

        logger::info("Steam Api handle found.");

        SteamAPIInit = (SteamAPI_Init)GetProcAddress(handle, "SteamAPI_Init");

        if (SteamAPIInit == NULL) {
            logger::info("SteamAPI_Init is NULL.");
            return;
        }

        if (!SteamAPIInit()) {
            logger::info("SteamAPI_Init returned false.");
            return;
        }

        ReleaseMemory = (SteamAPI_ReleaseCurrentThreadMemory)GetProcAddress(handle, "SteamAPI_ReleaseCurrentThreadMemory");

        if (ReleaseMemory == NULL) {
            logger::info("SteamAPI_ReleaseCurrentThreadMemory is NULL.");
            return;
        }

        ISteamUtilsConstructor = (GetISteamUtils)GetProcAddress(handle, "SteamAPI_SteamUtils_v010");

        if (ISteamUtilsConstructor == NULL) {
            logger::info("GetISteamUtils is NULL.");
            return;
        }

        steamUtils = ISteamUtilsConstructor();

        if (steamUtils == NULL) {
            logger::info("SteamUtils not found.");
            return;
        }

        if (!steamUtils->IsSteamInBigPictureMode()) {
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
        logger::info("Set workaround as loaded.");
        isLoaded = true;
    }
}