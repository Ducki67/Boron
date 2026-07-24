#include "pch.h"
#include "../../Engine/Public/NetDriver.h"
#include "../../Erbium/Plugins/CrashReporter/Public/CrashReporter.h"
#include "../../FortniteGame/Public/FortInventory.h"
#include "../../FortniteGame/Public/FortPlayerControllerAthena.h"
#include "../Public/Configuration.h"
#include "../Public/Finders.h"
#include "../Public/GUI.h"
#include "../Public/Misc.h"
#include "../Public/Utils.h"
#include <chrono>
#include <iostream>
#include <thread>
#include <io.h>
#include <fcntl.h>
#pragma comment(lib, "libcurl/libcurl.lib")
#pragma comment(lib, "libcurl/zlib.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Wldap32.lib")
#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "Normaliz.lib")

static void SetupConsoleTee()
{
    if (_fileno(stdout) < 0)
        return;

    HANDLE realConsole = CreateFileA("CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);

    HANDLE readPipe = nullptr, writePipe = nullptr;
    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
    if (!CreatePipe(&readPipe, &writePipe, &sa, 1 << 20))
        return;

    int fd = _open_osfhandle((intptr_t)writePipe, _O_TEXT);
    if (fd == -1)
    {
        CloseHandle(readPipe);
        CloseHandle(writePipe);
        return;
    }

    _dup2(fd, _fileno(stdout));
    _dup2(fd, _fileno(stderr));
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);

    struct TeeContext { HANDLE Read; HANDLE Console; };
    auto ctx = new TeeContext{ readPipe, realConsole };

    CreateThread(nullptr, 0, [](LPVOID param) -> DWORD {
        auto c = (TeeContext*)param;
        HANDLE logFile = CreateFileA("Boron_Console.txt", GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        char buffer[8192];
        DWORD bytesRead = 0;
        while (ReadFile(c->Read, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0)
        {
            DWORD written = 0;
            if (c->Console && c->Console != INVALID_HANDLE_VALUE)
                WriteFile(c->Console, buffer, bytesRead, &written, nullptr);
            if (logFile != INVALID_HANDLE_VALUE)
                WriteFile(logFile, buffer, bytesRead, &written, nullptr);
        }
        if (logFile != INVALID_HANDLE_VALUE)
            CloseHandle(logFile);
        return 0;
    }, ctx, 0, nullptr);
}

void Main()
{
    if constexpr (!FConfig::bGUI)
        AllocConsole();

    if constexpr (!FConfig::bGUI || !FConfig::bUseStdoutLog)
    {
        if (!FConfig::bGUI || GetConsoleWindow())
        {
            FILE* s;
            freopen_s(&s, "CONOUT$", "w", stdout);
            freopen_s(&s, "CONOUT$", "w+", stderr);
            freopen_s(&s, "CONIN$", "r", stdin);
        }
    }

    if constexpr (FConfig::bSaveConsoleLog && (!FConfig::bUseStdoutLog || !FConfig::bGUI))
        SetupConsoleTee();

    if constexpr (FConfig::bCustomCrashReporter)
        FCrashReporter::Register();

    printf("Initializing SDK...\n");
    SDK::Init();

    if constexpr (FConfig::bGUI)
    {
        if constexpr (FConfig::bUseStdoutLog)
        {
            FILE* s;
            freopen_s(&s, "stdout.log", "w", stdout);
            freopen_s(&s, "stdout.log", "w+", stderr);
        }

        CreateThread(0, 0, (LPTHREAD_START_ROUTINE)GUI::Init, 0, 0, 0);
    }

    if (wcscmp(FConfig::Playlist, L"/DurianPlaylist/Playlist/Playlist_Durian.Playlist_Durian") == 0)
        FConfig::bEnableIris = false;

    if (VersionInfo.EngineVersion >= 5.0)
    {
        UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), FString(L"log LogFortUIDirector None"), nullptr);
        UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), FString(L"log LogFortUIManager None"), nullptr);
    }
    if (VersionInfo.FortniteVersion == 20.40)
    {
        UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), FString(L"log LogSpecialRelevancyHealthComponent None"), nullptr);
    }
    if (VersionInfo.EngineVersion >= 5.1)
    {
        UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), FString(L"net.AllowEncryption 0"), nullptr);

        auto DefaultCurieGlobals = FindClass("CurieGlobals")->GetDefaultObj();

        if (DefaultCurieGlobals)
        {
            uint32 Offset = DefaultCurieGlobals->GetOffset("bEnableCurie");

            // if (Offset != -1)
            //     *(bool*)(uintptr_t(DefaultCurieGlobals) + Offset) = false;
        }
    }
    if (VersionInfo.EngineVersion >= 5.3 && FConfig::bEnableIris)
    {
        UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), FString(L"log LogIris None"), nullptr);
        UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), FString(L"log LogIrisRpc None"), nullptr);
        UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), FString(L"log LogIrisBridge None"), nullptr);
        /*auto IrisBool = Memcury::Scanner::FindPattern("83 3D ? ? ? ? ? 0F 8E ? ? ? ? 49 8B B9").RelativeOffset(2, 1).Get();
        if (IrisBool)
            *(uint32_t*)IrisBool = true;
        else
        {
            IrisBool = Memcury::Scanner::FindPattern("44 39 25 ? ? ? ? 0F 9F C0 45 84 FF").RelativeOffset(3).Get();

            if (IrisBool)
                *(uint32_t*)IrisBool = true;
        }*/
        auto IrisBool = FindCVar<uint32_t>(L"net.Iris.UseIrisReplication");

        if (IrisBool)
            *IrisBool = true;

        if (VersionInfo.FortniteVersion >= 29)
        {
            auto ReplicationBridgeConfig = UObjectReplicationBridgeConfig::GetDefaultObj();

            auto FortInventoryName = FName(L"/Script/FortniteGame.FortInventory");
            bool clearedInv = false;
            int clearedExtra = 0;
            int cfgN = ReplicationBridgeConfig ? ReplicationBridgeConfig->FilterConfigs.Num() : -1;
            for (int i = 0; i < cfgN; i++)
            {
                auto& FilterConfig = ReplicationBridgeConfig->FilterConfigs.Get(i, FObjectReplicationBridgeFilterConfig::Size());

                if (FilterConfig.ClassName == FortInventoryName)
                {
                    FilterConfig.DynamicFilterName = FName(0);
                    clearedInv = true;
                    continue;
                }

                // CH5: pickups replicate fine for ~1-2s then become impossible to interact with
                // (drop+grab instantly works; floor/chest/ammo loot never does). Dormancy was ruled
                // out, so dump every dynamic filter and clear the ones covering pickups/loot — a
                // dynamically-filtered pickup stops replicating and the client can't complete pickup.
                if (VersionInfo.EngineVersion >= 5.4)
                {
                    auto cls = FilterConfig.ClassName.ToString();
                    printf("[Boron][Iris] filter[%d] class=%s dyn=%s\n", i, cls.c_str(), FilterConfig.DynamicFilterName.ToString().c_str());

                    if (strstr(cls.c_str(), "Pickup") || strstr(cls.c_str(), "LootTier") || strstr(cls.c_str(), "SearchableContainer"))
                    {
                        FilterConfig.DynamicFilterName = FName(0);
                        clearedExtra++;
                        printf("[Boron][Iris]   -> CLEARED filter on %s\n", cls.c_str());
                    }
                }
            }
            printf("[Boron][Iris] filters: configs=%d clearedInventory=%d clearedPickupLike=%d\n", cfgN, clearedInv, clearedExtra);
        }
        // UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), FString(L"net.Iris.UseIrisReplication 1"), nullptr);
    }
    if (VersionInfo.EngineVersion >= 5.4)
    {
        // sprint fix
        auto SprintCVar = FindCVar<uint32_t>(L"Fort.MME.TacticalSprint");
        auto HurdleCVar = FindCVar<uint32_t>(L"Fort.MME.Hurdle");
        auto SlideCVar = FindCVar<uint32_t>(L"Fort.MME.Sliding");
        auto MantleCVar = FindCVar<uint32_t>(L"Fort.MME.Clambering");

        // if (SprintCVar)
        //     *SprintCVar = false;

        // if (HurdleCVar)
        //     *HurdleCVar = false;

        // if (SprintCVar)
        //     *SprintCVar = false;

        // if (HurdleCVar)
        //     *HurdleCVar = false;

        if (SlideCVar)
            *SlideCVar = false;

        if (MantleCVar)
            *MantleCVar = false;
        UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), FString(L"Fort.MME.TacticalSprint 0"), nullptr);
        // UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), FString(L"Fort.MME.Hurdle 0"), nullptr);
        UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), FString(L"Fort.MME.Sliding 0"), nullptr);
        UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), FString(L"Fort.MME.Clambering 0"), nullptr);
    }
    UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), FString(L"log LogSpecialEventScript VeryVerbose"), nullptr);

#ifdef CLIENT
    Misc::InitClient();

    return;
#endif

    if constexpr (DiscordWebhookConfig::WebhookURL && *DiscordWebhookConfig::WebhookURL)
        curl_global_init(CURL_GLOBAL_ALL);

    sprintf_s(GUI::windowTitle,
              VersionInfo.EngineVersion >= 5.0 ? "Boron (FN %.2f, UE %.1f): Setting up"
                                               : (VersionInfo.FortniteVersion >= 5.00 || VersionInfo.FortniteVersion < 1.2 ? "Boron (FN %.2f, UE %.2f): Setting up" : "Boron (FN %.1f, UE %.2f): Setting up"),
              VersionInfo.FortniteVersion, VersionInfo.EngineVersion);
    SetConsoleTitleA(GUI::windowTitle);

    // if constexpr (!FConfiguration::bGUI)
    //     Sleep(2000);

    printf("Hooking & finding offsets... (this may take a while)\n");

    FindNullsAndRetTrues();

    for (auto& NullFunc : NullFuncs)
        if (NullFunc != 0)
        {
            Hooking::Patch<uint8_t>(NullFunc, 0xc3);
        }

    for (auto& RetTrueFunc : RetTrueFuncs)
    {
        if (RetTrueFunc == 0)
            continue;

        Hooking::Patch<uint32_t>(RetTrueFunc, 0xc0ffc031);
        Hooking::Patch<uint8_t>(RetTrueFunc + 4, 0xc3);
    }

    auto GameSessionPatch = FindGameSessionPatch();
    if (GameSessionPatch)
        Hooking::Patch<uint8_t>(GameSessionPatch, 0x85);

    for (auto& HookFunc : _HookFuncs)
        HookFunc();

    auto GIsClientAddr = FindGIsClient();
    auto GIsServerAddr = FindGIsServer();
    printf("[Boron] GIsClient=0x%llX GIsServer=0x%llX (FN %.2f)\n",
           (unsigned long long)GIsClientAddr, (unsigned long long)GIsServerAddr, VersionInfo.FortniteVersion);

    if (GIsClientAddr)
        *(bool*)GIsClientAddr = false;
    else
        printf("[Boron] WARNING: GIsClient not found -- skipping (add a signature in Finders.cpp)\n");

    if (VersionInfo.EngineVersion > 4.20) // 3.6 and below have a crash on ALandscapeProxy
    {
        if (GIsServerAddr)
            *(bool*)GIsServerAddr = true;
        else
            printf("[Boron] WARNING: GIsServer not found -- skipping (add a signature in Finders.cpp)\n");
    }

    srand((uint32_t)time(0));

    UWorld::GetWorld()->OwningGameInstance->LocalPlayers.Remove(0);
    const wchar_t* terrainOpen = L"open Athena_Terrain";

    if (wcsstr(FConfig::Playlist, L"/MoleGame/Playlists/Playlist_MoleGame"))
    {
        UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), FString(L"Mole.WorstCasePlayerCount 1"), nullptr);
        terrainOpen = L"open Mole_UnderBase_Parent";
    }
    else if (VersionInfo.FortniteVersion >= 12.00 && wcsstr(FConfig::Playlist, L"/Game/Athena/Playlists/Creative/Playlist_PlaygroundV2.Playlist_PlaygroundV2"))
        terrainOpen = L"open Creative_NoApollo_Terrain";
    // temp for now
    else if (VersionInfo.FortniteVersion == 31.41 && wcsstr(FConfig::Playlist, L"/BlastBerry/Playlists/Playlist_SunflowerSolo.Playlist_SunflowerSolo"))
        terrainOpen = L"open BlastBerry_Terrain";

    /* else if (VersionInfo.FortniteVersion == 32.00 && wcsstr(FConfig::Playlist, L"/BlastBerry/Playlists/Playlist_PunchBerrySolo.Playlist_PunchBerrySolo"))
        terrainOpen = L"open PunchBerry_Terrain";*/
    else
    {
        if (VersionInfo.FortniteVersion >= 27.00)
        {
            if (VersionInfo.FortniteVersion >= 28.00)
                terrainOpen = L"open Helios_Terrain";
        }
        else if (VersionInfo.FortniteVersion >= 23.00)
            terrainOpen = L"open Asteria_Terrain";
        else if (VersionInfo.FortniteVersion >= 19.00)
            terrainOpen = L"open Artemis_Terrain";
        else if (VersionInfo.FortniteVersion >= 11.00)
            terrainOpen = L"open Apollo_Terrain";
    }

    UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), FString(terrainOpen), nullptr);

    auto EncryptionPatch = FindEncryptionPatch();
    if (EncryptionPatch)
        Hooking::Patch<uint8_t>(EncryptionPatch, 0x74);
    else
    {
        // UE5.4+ (30.20/31.41/32.11) inlined the encryption check so the byte-patch sigs miss.
        // net.AllowEncryption 0 disables encryption engine-wide and is version-agnostic (same job).
        printf("Encryption byte-patch not found, falling back to net.AllowEncryption 0 (UE5.4+)\n");
        UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), FString(L"net.AllowEncryption 0"), nullptr);
    }

    for (auto& HookFunc : _PostLoadHookFuncs)
        HookFunc();

    Misc::bHookedAll = true;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        std::thread(Main).detach();
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
