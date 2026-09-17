#include <pch.h>

// Patches
#include <ReloadFix.h>

namespace Main
{
    // Init Bool
    static bool isInit = false;

    bool InitPlugin(const F4SE::LoadInterface* a_f4se)
    {
        if (isInit)
            return true;

        static std::once_flag once;
        std::call_once(once, [&]() {
            // Init F4SE
            F4SE::Init(a_f4se);

            // Init Mod
            REX::INFO("Reload Fix AE Initializing...");

            // Load the Config
            const auto config = REX::FTomlSettingStore::GetSingleton();
            config->Init("Data/F4SE/Plugins/ReloadFixAE.toml", "Data/F4SE/Plugins/ReloadFixAECustom.toml");
            config->Load();

            // Get the Trampoline and Allocate
            auto& trampoline = REL::GetTrampoline();
            trampoline.create(64);

            // Listen for Messages
            auto MessagingInterface = F4SE::GetMessagingInterface();
            MessagingInterface->RegisterListener(ReloadFix::F4SEMessageListener);
            REX::INFO("Started Listening for F4SE Message Callbacks.");

            // Install the Mod
            if (ReloadFix::Install())
                REX::INFO("Reload Fix AE Patch Initialized!");
            else
                REX::ERROR("Reload Fix AE Patch failed!");

            // Finished
            isInit = true;
        });

        return isInit;
    }

    // F4SE_PLUGIN_QUERY(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info)
    // {
    //     if (const auto data = F4SE::PluginVersionData::GetSingleton())
    //     {
    //         a_info->infoVersion = F4SE::PluginInfo::kVersion;
    //         a_info->name = data->GetPluginName().data();
    //         a_info->version = data->GetPluginVersion().pack();
    //     }

    //     const auto ver = a_f4se->RuntimeVersion();
    //     if (ver < REL::Version(F4SE::RUNTIME_1_10_163))
    //         return false;

    //     return true;
    // }

    // F4SE_PLUGIN_LOAD(const F4SE::LoadInterface* a_f4se)
	// {
    //     // OG does not support PreLoading
	// 	return InitPlugin(a_f4se);
	// }

    F4SE_PLUGIN_PRELOAD(const F4SE::LoadInterface* a_f4se)
    {
        return InitPlugin(a_f4se);
    }
}
