#include <spdlog/sinks/basic_file_sink.h>
#include "SteamUtils.h"
#include "isteamutils.h"

namespace logger = SKSE::log;
using namespace RE;
using namespace RE::BSScript;
using namespace SKSE;
using namespace SKSE::stl;

void SetupLog() {
    auto logsFolder = SKSE::log::log_directory();
    if (!logsFolder) SKSE::stl::report_and_fail("SKSE log_directory not provided, logs disabled.");
    auto pluginName = SKSE::PluginDeclaration::GetSingleton()->GetName();
    auto logFilePath = *logsFolder / std::format("{}.log", pluginName);
    
    auto log = std::make_shared<spdlog::logger>(
        "Global", std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath.string(), true));
    log->set_level(spdlog::level::trace);
    log->flush_on(spdlog::level::trace);

    spdlog::set_default_logger(std::move(log));
}

typedef bool(__stdcall* SteamAPI_Init)();

typedef ISteamUtils*(__stdcall* GetISteamUtils)();

void OnMessage(SKSE::MessagingInterface::Message* message) {
    if (message->type == SKSE::MessagingInterface::kInputLoaded) {
        SteamWorkaround::Init();
    } else if (message->type == SKSE::MessagingInterface::kPostLoadGame) {
        SteamWorkaround::SetIsLoaded();
    }
}

SKSEPluginLoad(const LoadInterface* skse) {
    Init(skse);

    SetupLog();
    logger::info("Setup log...");

    GetMessagingInterface()->RegisterListener(OnMessage);

    return true;
}