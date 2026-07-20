#include "pch.h"
#include "../Public/FortGameMode.h"
#include "../../Engine/Public/AbilitySystemComponent.h"
#include "../../Engine/Public/CurveTable.h"
#include "../../Engine/Public/DataTableFunctionLibrary.h"
#include "../../Engine/Public/NetDriver.h"
#include "../../Erbium/Public/Configuration.h"
#include "../../Erbium/Public/Events.h"
#include "../../Erbium/Public/Finders.h"
#include "../../Erbium/Public/GUI.h"
#include "../../Erbium/Public/LateGame.h"
#include "../../Erbium/Public/Misc.h"
#include "../Public/BattleRoyaleGamePhaseLogic.h"
#include "../Public/BuildingFoundation.h"
#include "../Public/BuildingItemCollectorActor.h"
#include "../Public/FortAthenaCreativePortal.h"
#include "../Public/FortKismetLibrary.h"
#include "../Public/FortLootPackage.h"
#include "../Public/FortPhysicsPawn.h"
#include "../Public/FortPlayerControllerAthena.h"
#include "../Public/FortSafeZoneIndicator.h"
#include "../Public/LevelStreamingDynamic.h"
#include <cmath>
#include "../Public/FortAthenaSpawningPolicyManager.h"
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

void ShowFoundation(const ABuildingFoundation* Foundation)
{
    if (!Foundation)
        return;

    /*Foundation->StreamingData.BoundingBox = Foundation->StreamingBoundingBox;
    Foundation->StreamingData.FoundationLocation = Foundation->GetTransform().Translation;
    Foundation->SetDynamicFoundationEnabled(true);*/
    // Foundation->SetDynamicFoundationTransform(Foundation->GetTransform());

    if (Foundation->HasbServerStreamedInLevel())
    {
        //Foundation->bServerStreamedInLevel = true;
        //Foundation->OnRep_ServerStreamedInLevel();
    }

    Foundation->SetDynamicFoundationEnabled(true);
}

bool bIsLargeTeamGame = false;

static UFortPlaylistAthena* FindLoadedPlaylist()
{
    std::wstring pathW = FConfig::Playlist;
    auto slash = pathW.find_last_of(L'/');
    auto dot = pathW.find_last_of(L'.');
    std::wstring nameW = (dot != std::wstring::npos && (slash == std::wstring::npos || dot > slash))
                             ? pathW.substr(slash + 1, dot - slash - 1)
                             : (slash == std::wstring::npos ? pathW : pathW.substr(slash + 1));
    auto WantIdx = FName(nameW.c_str()).ComparisonIndex;

    TArray<UFortPlaylistAthena*> Playlists;
    Utils::GetAll<UFortPlaylistAthena>(Playlists);
    int found = Playlists.Num();

    UFortPlaylistAthena* Result = nullptr;
    for (int i = 0; i < Playlists.Num(); i++)
    {
        auto pl = Playlists[i];
        if (!pl || !pl->HasPlaylistName())
            continue;
        if (!Result)
            Result = pl;
        if (pl->PlaylistName.ComparisonIndex == WantIdx)
        {
            Result = pl;
            break;
        }
    }
    printf("[Boron][Playlist] FindLoadedPlaylist: %d loaded, want '%ls' -> %p\n", found, nameW.c_str(), (void*)Result);
    Playlists.Free();
    return Result;
}

void SetupPlaylist(AFortGameMode* GameMode, AFortGameStateAthena* GameState)
{
    auto Playlist = FindObject<UFortPlaylistAthena>(FConfig::Playlist);

    if (!Playlist)
        Playlist = FindObject<UFortPlaylistAthena>(L"/Game/Athena/Playlists/Playlist_DefaultSolo.Playlist_DefaultSolo");

    if (!Playlist)
        Playlist = FindLoadedPlaylist();

    printf("[Boron][Playlist] SetupPlaylist -> Playlist=%p\n", (void*)Playlist);

    if (Playlist)
    {
        if (GameRuleConfig::bForceRespawns)
        {
            if (Playlist->HasbRespawnInAir())
                Playlist->bRespawnInAir = true;
            if (Playlist->HasRespawnHeight())
            {
                Playlist->RespawnHeight.Curve.CurveTable = nullptr;
                Playlist->RespawnHeight.Curve.RowName = FName();
                Playlist->RespawnHeight.Value = GameRuleConfig::RespawnHightGamemode;
            }
            if (Playlist->HasRespawnTime())
            {
                Playlist->RespawnTime.Curve.CurveTable = nullptr;
                Playlist->RespawnTime.Curve.RowName = FName();
                Playlist->RespawnTime.Value = GameRuleConfig::RespawnTimeGamemode;
            }
            Playlist->RespawnType = 1;
            // if (Playlist->HasbForceRespawnLocationInsideOfVolume())
            //      Playlist->bForceRespawnLocationInsideOfVolume = true;
        }
        if (GameRuleConfig::bForceRespawns || GameRuleConfig::bJoinInProgress)
        {
            if (Playlist->HasbAllowJoinInProgress())
                Playlist->bAllowJoinInProgress = true;
            if (Playlist->HasJoinInProgressMatchType())
                Playlist->JoinInProgressMatchType = UKismetTextLibrary::Conv_StringToText(FString(L"Creative"));
        }
        // if (VersionInfo.FortniteVersion >= 16)
        {
            if (Playlist->HasGarbageCollectionFrequency())
                Playlist->GarbageCollectionFrequency = 9999999999999999.f; // easier than hooking collectgarbage
            if (GameMode->HasPlaylistHotfixOriginalGCFrequency())
                GameMode->PlaylistHotfixOriginalGCFrequency = 9999999999999999.f;
            if (GameMode->HasbDisableGCOnServerDuringMatch())
                GameMode->bDisableGCOnServerDuringMatch = true;
            if (GameMode->HasbPlaylistHotfixChangedGCDisabling())
                GameMode->bPlaylistHotfixChangedGCDisabling = true;
        }
        if (GameState->HasCurrentPlaylistInfo())
        {
            // if (VersionInfo.EngineVersion >= 4.27)
            GameState->CurrentPlaylistInfo.BasePlaylist = Playlist;
            GameState->CurrentPlaylistInfo.PlaylistReplicationKey++;
            GameState->CurrentPlaylistInfo.MarkArrayDirty();
            GameState->OnRep_CurrentPlaylistInfo();
        }
        else if (GameState->HasCurrentPlaylistData())
        {
            GameState->CurrentPlaylistData = Playlist;
            GameState->OnRep_CurrentPlaylistData();
        }

        GameMode->CurrentPlaylistId = Playlist->PlaylistId;
        if (GameState->HasCurrentPlaylistId())
            GameState->CurrentPlaylistId = Playlist->PlaylistId;
        if (GameMode->HasCurrentPlaylistName())
            GameMode->CurrentPlaylistName = Playlist->PlaylistName;

        if (GameMode->GameSession->HasMaxPlayers())
            GameMode->GameSession->MaxPlayers = Playlist->MaxPlayers;

        if (GameState->HasAirCraftBehavior() && Playlist->HasAirCraftBehavior())
            GameState->AirCraftBehavior = Playlist->AirCraftBehavior;
        if (GameState->HasCachedSafeZoneStartUp() && Playlist->HasSafeZoneStartUp())
            GameState->CachedSafeZoneStartUp = Playlist->SafeZoneStartUp;

        if (GameMode->HasbEnableDBNO())
            GameMode->bEnableDBNO = Playlist->MaxSquadSize > 1;

        bIsLargeTeamGame = Playlist->bIsLargeTeamGame;

        if (Playlist)
        {
            auto AdditionalPlaylistLevelsStreamed__Off = GameState->GetOffset("AdditionalPlaylistLevelsStreamed");

            if (AdditionalPlaylistLevelsStreamed__Off != -1)
            {
                TArray<FPlaylistStreamedLevelData>& AdditionalPlaylistLevels = *(TArray<FPlaylistStreamedLevelData>*)(__int64(GameState) + AdditionalPlaylistLevelsStreamed__Off - 0x10);

                AdditionalPlaylistLevels.Free();

                auto AdditionalLevelStruct = FAdditionalLevelStreamed::StaticStruct();
                if (Playlist->HasAdditionalLevels())
                    for (auto& Level : Playlist->AdditionalLevels)
                    {
                        bool Success = false;
                        // ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(UWorld::GetWorld(), Level, FVector(), FRotator(), &Success, FString(), nullptr);
                        if (AdditionalLevelStruct)
                        {
                            auto level = (FAdditionalLevelStreamed*)malloc(FAdditionalLevelStreamed::Size());
                            memset((PBYTE)level, 0, FAdditionalLevelStreamed::Size());
                            level->bIsServerOnly = false;
                            level->LevelName = Level.ObjectID.AssetPathName;
                            if (Success)
                                GameState->AdditionalPlaylistLevelsStreamed.Add(*level, FAdditionalLevelStreamed::Size());
                            free(level);
                        }
                        else
                            GetFromOffset<TArray<FName>>(GameState, AdditionalPlaylistLevelsStreamed__Off).Add(Level.ObjectID.AssetPathName);
                    }

                if (Playlist->HasAdditionalLevelsServerOnly())
                    for (auto& Level : Playlist->AdditionalLevelsServerOnly)
                    {
                        bool Success = false;
                        // ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(UWorld::GetWorld(), Level, FVector(), FRotator(), &Success, FString(), nullptr);

                        if (AdditionalLevelStruct)
                        {

                            auto level = (FAdditionalLevelStreamed*)malloc(FAdditionalLevelStreamed::Size());
                            memset((PBYTE)level, 0, FAdditionalLevelStreamed::Size());
                            level->bIsServerOnly = true;
                            level->LevelName = Level.ObjectID.AssetPathName;
                            if (Success)
                                GameState->AdditionalPlaylistLevelsStreamed.Add(*level, FAdditionalLevelStreamed::Size());
                            free(level);
                        }
                        else
                            GetFromOffset<TArray<FName>>(GameState, AdditionalPlaylistLevelsStreamed__Off).Add(Level.ObjectID.AssetPathName);
                    }
            }
        }

        if (GameState->HasAdditionalPlaylistLevelsStreamed())
             GameState->OnRep_AdditionalPlaylistLevelsStreamed();
    }
    else
    {
        GameState->CurrentPlaylistId = GameMode->CurrentPlaylistId = 0;

        if (GameMode->GameSession->HasMaxPlayers())
            GameMode->GameSession->MaxPlayers = 100;
    }
}

void (*VendWobble__FinishedFuncOG)(UObject* Context, FFrame& Stack);
void VendWobble__FinishedFunc(UObject* Context, FFrame& Stack)
{
    auto CollectorActor = (ABuildingItemCollectorActor*)Context;
    auto PlayerController = CollectorActor->ControllingPlayer;

    if (!PlayerController)
        return VendWobble__FinishedFuncOG(Context, Stack);

    auto Collection = CollectorActor->ItemCollections.Search([&](FCollectorUnitInfo& Coll) { return Coll.InputItem == CollectorActor->ClientPausedActiveInputItem; }, FCollectorUnitInfo::Size());

    if (!Collection)
        return VendWobble__FinishedFuncOG(Context, Stack);

    CollectorActor->ClientPausedActiveInputItem = nullptr;

    float Cost = Collection->InputCount.Evaluate();

    auto VMLoc = CollectorActor->K2_GetActorLocation();
    auto& SpawnLocation = CollectorActor->LootSpawnLocation;
    auto Loc = VMLoc + (CollectorActor->GetActorForwardVector() * SpawnLocation.X) + (CollectorActor->GetActorRightVector() * SpawnLocation.Y) + (CollectorActor->GetActorUpVector() * SpawnLocation.Z);

    for (int i = 0; i < Collection->OutputItemEntry.Num(); i++)
    {
        auto& Item = Collection->OutputItemEntry.Get(i, FFortItemEntry::Size());

        AFortInventory::SpawnPickup(Loc, Item);
        if (CollectorActor->HasPickupSpawned())
            CollectorActor->PickupSpawned.Process();
    }

    /*if (Cost == 0)
    {
        CollectorActor->DoVendDeath();
        CollectorActor->K2_DestroyActor();
    }*/

    return VendWobble__FinishedFuncOG(Context, Stack);
}

std::unordered_map<int, float> WeightMap;
float Sum = 0;
float Weight;
float TotalWeight;

class AFortAthenaLivingWorldStaticPointProvider : public AActor
{
public:
    UCLASS_COMMON_MEMBERS(AFortAthenaLivingWorldStaticPointProvider);

    DEFINE_PROP(FiltersTags, FGameplayTagContainer);
    DEFINE_PROP(SpawnPoints, TArray<FTransform>);
    DEFINE_PROP(bStartEnabled, bool);
    DEFINE_PROP(bRandomizeStartPoint, bool);
    DEFINE_PROP(bRandomizePointRotation, bool);
};

class UFortVehicleItemDefinition : public UObject
{
public:
    UCLASS_COMMON_MEMBERS(UFortVehicleItemDefinition);

    DEFINE_PROP(VehicleMinSpawnPercent, FScalableFloat);
    DEFINE_PROP(VehicleMaxSpawnPercent, FScalableFloat);
};

class AFortAthenaVehicleSpawner : public AActor
{
public:
    UCLASS_COMMON_MEMBERS(AFortAthenaVehicleSpawner);

    DEFINE_PROP(CachedFortVehicleItemDef, UFortVehicleItemDefinition*);
    DEFINE_PROP(bForceSpawnAlways, bool);

    DEFINE_FUNC(GetVehicleClass, UClass*);
};

void AFortGameMode::ReadyToStartMatch_(UObject* Context, FFrame& Stack, bool* Ret)
{
    Stack.IncrementCode();

    static auto FrontendMode = FindClass("FortGameModeFrontend");

    if (VersionInfo.EngineVersion >= 5.4)
    {
        static bool rtsmEnterLogged = false;
        if (!rtsmEnterLogged)
        {
            rtsmEnterLogged = true;
            printf("[Boron][RTSM] ENTER: Context=%p FrontendMode=%p IsFrontend=%d\n",
                   (void*)Context,
                   (void*)FrontendMode,
                   (Context && FrontendMode) ? (int)Context->IsA(FrontendMode) : -1);
        }
    }

    // If FindClass fails on this version (CH5 find-object quirks) FrontendMode is null;
    // the guard keeps us from early-returning on a bad IsA. No-op where it's already valid.
    if (FrontendMode && Context->IsA(FrontendMode))
    {
        *Ret = callOGWithRet(((AFortGameMode*)Context), Stack.GetCurrentNativeFunction(), ReadyToStartMatch);
        return;
    }
    auto GameMode = Context->Cast<AFortGameMode>();

    auto GameState = GameMode->GameState;

#ifdef MANUAL_SERVER_SETUP
    if (FConfig::bGUI && !GUI::bServerSetupRequested)
    {
        *Ret = false;
        return;
    }
#endif

    static bool setup = false;

    // CH5 (UE5.4+, 30.20/31.41/32.11): WarmupRequiredPlayerCount defaults to 1, so the
    // original gate below never fires and the listen server is never created. Force the
    // run-once path there. Pre-5.4 keeps the exact original gate -> older builds untouched.
    bool shouldSetup = GameMode->HasWarmupRequiredPlayerCount() ? GameMode->WarmupRequiredPlayerCount != 1 : !setup;
    if (VersionInfo.EngineVersion >= 5.4)
    {
        if (!setup)
            printf("[Boron][RTSM] first setup call: hasWarmupCount=%d warmupCount=%d\n",
                   (int)GameMode->HasWarmupRequiredPlayerCount(),
                   GameMode->HasWarmupRequiredPlayerCount() ? GameMode->WarmupRequiredPlayerCount : -1);
        shouldSetup = !setup;
    }

    if (shouldSetup)
    {
        setup = true;

        auto World = UWorld::GetWorld();
        if (VersionInfo.EngineVersion >= 5.4 && World->NetDriver)
            printf("[Boron][RTSM] NetDriver already exists (FWI listen path) -- skipping listen setup\n");
        else
        {
            auto Engine = UEngine::GetEngine();
            auto NetDriverName = FName(L"GameNetDriver");

            if (GameMode->HasbEnableReplicationGraph())
                GameMode->bEnableReplicationGraph = true;

            UNetDriver* NetDriver = nullptr;
            if (VersionInfo.FortniteVersion >= 16.00)
            {
                auto GetWorldContextFn = FindGetWorldContext();
                auto CreateNetDriverFn = FindCreateNetDriverWorldContext();
                if (!GetWorldContextFn || !CreateNetDriverFn)
                {
                    printf("[Boron] NetDriver setup aborted: GetWorldContext=0x%llX CreateNetDriverWorldContext=0x%llX missing for FN %.2f - add a signature in Finders.cpp\n", (unsigned long long)GetWorldContextFn, (unsigned long long)CreateNetDriverFn, VersionInfo.FortniteVersion);
                    setup = false;
                    *Ret = false;
                    return;
                }
                void* WorldCtx = ((void* (*)(UEngine*, UWorld*))GetWorldContextFn)(Engine, World);
                World->NetDriver = NetDriver = ((UNetDriver * (*)(UEngine*, void*, FName, int)) CreateNetDriverFn)(Engine, WorldCtx, NetDriverName, 0);
            }
            else
            {
                auto CreateNetDriverFn = FindCreateNetDriver();
                if (!CreateNetDriverFn)
                {
                    printf("[Boron] NetDriver setup aborted: CreateNetDriver missing for FN %.2f - add a signature in Finders.cpp\n", VersionInfo.FortniteVersion);
                    setup = false;
                    *Ret = false;
                    return;
                }
                World->NetDriver = NetDriver = ((UNetDriver * (*)(UEngine*, UWorld*, FName)) CreateNetDriverFn)(Engine, World, NetDriverName);
            }
            if ((uintptr_t)NetDriver < 0x10000)
            {
                printf("[Boron] NetDriver setup aborted: create returned bad pointer %p for FN %.2f - wrong finder hit (see MEMORY.md CreateNamedNetDriver_Local gotcha)\n", (void*)NetDriver, VersionInfo.FortniteVersion);
                World->NetDriver = nullptr;
                setup = false;
                *Ret = false;
                return;
            }
            if (VersionInfo.FortniteVersion >= 20)
                NetDriver->NetServerMaxTickRate = 30;

            NetDriver->NetDriverName = NetDriverName;
            NetDriver->World = World;

            if (VersionInfo.EngineVersion >= 5.3 && FConfig::bEnableIris)
            {
                *(bool*)(__int64(&NetDriver->ReplicationDriver) + 0x11) = true;
            }

            NetDriver->NetDriverName = NetDriverName;
            NetDriver->World = World;

            for (int i = 0; i < World->LevelCollections.Num(); i++)
            {
                auto& LevelCollection = World->LevelCollections.Get(i, FLevelCollection::Size());

                LevelCollection.NetDriver = NetDriver;
            }

            auto URL = (FURL*)malloc(FURL::Size());
            memset((PBYTE)URL, 0, FURL::Size());
            URL->Port = FConfig::Port;

            auto InitListenFn = FindInitListen();
            auto SetWorldFn = FindSetWorld();

            if (InitListenFn && SetWorldFn)
            {
                auto InitListen = (bool (*)(UNetDriver*, UWorld*, FURL*, bool, FString&))InitListenFn;
                auto SetWorld = (void (*)(UNetDriver*, UWorld*))SetWorldFn;

                SetWorld(NetDriver, World);
                FString Err;
                if (InitListen(NetDriver, World, URL, false, Err))
                {
                    printf("[Boron] InitListen OK -- GameNetDriver listening on port %d\n", FConfig::Port);
                    SetWorld(NetDriver, World);
                }
                else
                    printf("Failed to listen!\n");
            }
            else
                printf("[Boron] Listen aborted: InitListen=0x%llX SetWorld=0x%llX missing for FN %.2f - add a signature in Finders.cpp\n", (unsigned long long)InitListenFn, (unsigned long long)SetWorldFn, VersionInfo.FortniteVersion);

            free(URL);
        }

        if (GameMode->HasWarmupRequiredPlayerCount())
            GameMode->WarmupRequiredPlayerCount = 1;

        if (VersionInfo.FortniteVersion > 4.0)
            SetupPlaylist(GameMode, GameState);

        auto Playlist = FindObject<UFortPlaylistAthena>(FConfig::Playlist);

        if (!Playlist)
            Playlist = FindObject<UFortPlaylistAthena>(L"/Game/Athena/Playlists/Playlist_DefaultSolo.Playlist_DefaultSolo");

        // misc C1 poi things
        if (VersionInfo.FortniteVersion >= 6 && VersionInfo.FortniteVersion < 7)
        {
            if (VersionInfo.FortniteVersion > 6.10)
                ShowFoundation(VersionInfo.FortniteVersion <= 6.21 ? FindObject<ABuildingFoundation>(L"/Game/Athena/Maps/Athena_POI_Foundations.Athena_POI_Foundations.PersistentLevel.LF_Lake1")
                                                                   : FindObject<ABuildingFoundation>(L"/Game/Athena/Maps/Athena_POI_Foundations.Athena_POI_Foundations.PersistentLevel.LF_Lake2"));
            else
                ShowFoundation(FindObject<ABuildingFoundation>(L"/Game/Athena/Maps/Athena_POI_Foundations.Athena_POI_Foundations.PersistentLevel.LF_Athena_StreamingTest12"));

            ShowFoundation(VersionInfo.FortniteVersion <= 6.10 ? FindObject<ABuildingFoundation>(L"/Game/Athena/Maps/Athena_POI_Foundations.Athena_POI_Foundations.PersistentLevel.LF_Athena_StreamingTest13")
                                                               : FindObject<ABuildingFoundation>(L"/Game/Athena/Maps/Athena_POI_Foundations.Athena_POI_Foundations.PersistentLevel.LF_FloatingIsland"));

            auto IslandScripting = TUObjectArray::FindFirstObject("BP_IslandScripting_C");
            if (IslandScripting)
            {
                auto UpdateMapOffset = IslandScripting->GetOffset("UpdateMap");
                if (UpdateMapOffset != -1)
                {
                    *(bool*)(__int64(IslandScripting) + UpdateMapOffset) = true;
                    IslandScripting->ProcessEvent(IslandScripting->GetFunction("OnRep_UpdateMap"), nullptr);
                }
            }
        }
        else if (VersionInfo.FortniteVersion >= 7 && VersionInfo.FortniteVersion < 8)
        {
            ShowFoundation(FindObject<ABuildingFoundation>("/Game/Athena/Maps/Athena_POI_Foundations.Athena_POI_Foundations.PersistentLevel.LF_Athena_POI_25x36"));
            ShowFoundation(FindObject<ABuildingFoundation>("/Game/Athena/Maps/Athena_POI_Foundations.Athena_POI_Foundations.PersistentLevel.ShopsNew"));
        }
        else if (VersionInfo.FortniteVersion >= 8 && VersionInfo.FortniteVersion < 10)
            ShowFoundation(FindObject<ABuildingFoundation>(L"/Game/Athena/Maps/Athena_POI_Foundations.Athena_POI_Foundations.PersistentLevel.LF_Athena_POI_50x53_Volcano"));
        else if (VersionInfo.FortniteVersion >= 10.20 && VersionInfo.FortniteVersion < 11)
            ShowFoundation(FindObject<ABuildingFoundation>(L"/Game/Athena/Maps/Athena_POI_Foundations.Athena_POI_Foundations.PersistentLevel.LF_Athena_POI_50x53_Volcano"));

        if (VersionInfo.FortniteVersion >= 7 && VersionInfo.FortniteVersion <= 10)
            ShowFoundation(FindObject<ABuildingFoundation>(L"/Game/Athena/Maps/Athena_POI_Foundations.Athena_POI_Foundations.PersistentLevel.SLAB_2"));
        else if (VersionInfo.EngineVersion == 4.23) // rest of S10
            ShowFoundation(FindObject<ABuildingFoundation>(L"/Game/Athena/Maps/Athena_POI_Foundations.Athena_POI_Foundations.PersistentLevel.SLAB_4"));

        bool bEvent = false;
        if (Playlist && Playlist->HasGameplayTagContainer())
        {
            for (int i = 0; i < Playlist->GameplayTagContainer.GameplayTags.Num(); i++)
            {
                auto& PlaylistTag = Playlist->GameplayTagContainer.GameplayTags.Get(i, FGameplayTag::Size());

                if (PlaylistTag.TagName.ToString() == "Athena.Playlist.SpecialEvent" || PlaylistTag.TagName.ToString() == "Athena.Playlist.Concert")
                {
                    bEvent = true;
                    if (VersionInfo.FortniteVersion == 7.30)
                        ShowFoundation(FindObject<ABuildingFoundation>("/Game/Athena/Maps/Athena_POI_Foundations.Athena_POI_Foundations.PersistentLevel.PleasentParkFestivus"));

                    break;
                }
            }
        }

        if (VersionInfo.FortniteVersion == 12.41)
        {
            ShowFoundation(FindObject<ABuildingFoundation>("/Game/Athena/Apollo/Maps/Apollo_POI_Foundations.Apollo_POI_Foundations.PersistentLevel.LF_Athena_POI_19x19_2"));
            ShowFoundation(FindObject<ABuildingFoundation>("/Game/Athena/Apollo/Maps/Apollo_POI_Foundations.Apollo_POI_Foundations.PersistentLevel.BP_Jerky_Head6_18"));
            ShowFoundation(FindObject<ABuildingFoundation>("/Game/Athena/Apollo/Maps/Apollo_POI_Foundations.Apollo_POI_Foundations.PersistentLevel.BP_Jerky_Head5_14"));
            ShowFoundation(FindObject<ABuildingFoundation>("/Game/Athena/Apollo/Maps/Apollo_POI_Foundations.Apollo_POI_Foundations.PersistentLevel.BP_Jerky_Head3_8"));
            ShowFoundation(FindObject<ABuildingFoundation>("/Game/Athena/Apollo/Maps/Apollo_POI_Foundations.Apollo_POI_Foundations.PersistentLevel.BP_Jerky_Head_2"));
            ShowFoundation(FindObject<ABuildingFoundation>("/Game/Athena/Apollo/Maps/Apollo_POI_Foundations.Apollo_POI_Foundations.PersistentLevel.BP_Jerky_Head4_11"));
        }

        if (VersionInfo.FortniteVersion == 7.30 && !bEvent)
            ShowFoundation(FindObject<ABuildingFoundation>("/Game/Athena/Maps/Athena_POI_Foundations.Athena_POI_Foundations.PersistentLevel.PleasentParkDefault"));

        if (VersionInfo.FortniteVersion == 17.50)
        {
            // ShowFoundation(FindObject<ABuildingFoundation>(L"/Game/Athena/Apollo/Maps/Apollo_Mother.Apollo_Mother.PersistentLevel.farmbase_2"));
            // ShowFoundation(FindObject<ABuildingFoundation>(L"/Game/Athena/Apollo/Maps/Apollo_Mother.Apollo_Mother.PersistentLevel.Farm_Phase_03"));
        }

        if (VersionInfo.EngineVersion >= 4.27 && std::floor(VersionInfo.FortniteVersion) != 20) // on 20 it does some weird stuff
        {
            auto MeshNetworkSubsystem = TUObjectArray::FindFirstObject("MeshNetworkSubsystem");

            if (MeshNetworkSubsystem)
                *(uint8_t*)(__int64(MeshNetworkSubsystem) + MeshNetworkSubsystem->GetOffset("NodeType")) = 2;
        }

        auto AIDirectorClass = GameMode->HasWarmupRequiredPlayerCount() ? FindClass("AthenaAIDirector") : FindObject<UClass>("/Game/AIDirector/AIDirector_Fortnite.AIDirector_Fortnite_C");
        if (!AIDirectorClass)
            AIDirectorClass = FindClass("FortAIDirector");

        if (!GameMode->AIDirector)
        {
            GameMode->AIDirector = UWorld::SpawnActor(AIDirectorClass, FVector{}, GameMode);
            if (GameMode->AIDirector)
                GameMode->AIDirector->Call(GameMode->AIDirector->GetFunction("Activate"));
        }

        if (GameMode->HasServerBotManager())
        {
            if (auto BotManager = (UFortServerBotManagerAthena*)UGameplayStatics::SpawnObject(UFortServerBotManagerAthena::StaticClass(), GameMode))
            {
                GameMode->ServerBotManager = BotManager;
                BotManager->CachedGameState = GameState;
                BotManager->CachedGameMode = GameMode;
            }
            else
            {
                printf("BotManager is nullptr!\n");
            }
        }

        if (!GameMode->AIGoalManager)
        {
            auto GoalManagerClass = GameMode->HasWarmupRequiredPlayerCount() ? FindClass("FortAIGoalManager") : FindObject<UClass>("/Game/AI/GoalSelection/AIGoalManager.AIGoalManager_C");

            GameMode->AIGoalManager = UWorld::SpawnActor(GoalManagerClass, FVector{}, GameMode);
        }

        if (GameMode->HasSpawningPolicyManager() && !GameMode->SpawningPolicyManager)
        {
            GameMode->SpawningPolicyManager = UWorld::SpawnActor<AFortAthenaSpawningPolicyManager>(AFortAthenaSpawningPolicyManager::StaticClass(), {});
            GameMode->SpawningPolicyManager->GameStateAthena = GameState;
            GameMode->SpawningPolicyManager->GameModeAthena = GameMode;
        }

        auto MissionManagerClass = GameMode->HasWarmupRequiredPlayerCount() ? nullptr : FindObject<UClass>("/Game/Blueprints/MissionManager.MissionManager_C");

        if (MissionManagerClass)
        {
            GameState->MissionManager = UWorld::SpawnActor(MissionManagerClass, FVector{}, GameState);
            GameState->OnRep_MissionManager();

            auto MissionInfo = FindObject<UFortMissionInfo>(L"/Game/Missions/Primary/EvacuateTheSurvivors/EvacuteTheSurvivors.EvacuteTheSurvivors");

            if (!MissionInfo)
                MissionInfo = FindObject<UFortMissionInfo>(L"/SaveTheWorld/Missions/Primary/EvacuateTheSurvivors/EvacuteTheSurvivors.EvacuteTheSurvivors");

            if (MissionInfo)
            {
                MissionInfo->bStartPlayingOnLoad = true; // bad hack, we should find a better way to do this later
                // startplayingmission

                UFortMissionLibrary::LoadMission(UWorld::GetWorld(), MissionInfo);
            }
            // we need to spawn bluglo manager too?
        }

        /* if (VersionInfo.EngineVersion == 4.16)
         {
             if (!UWorld::GetWorld()->NavigationSystem)
             {
                 UWorld::GetWorld()->NavigationSystem = UGameplayStatics::SpawnObject(FindClass("FortNavSystem"), UWorld::GetWorld());
                 auto OnWorldInitDone = (void(*)(UObject*, char))(ImageBase + 0x1f6fc40);
                 OnWorldInitDone(UWorld::GetWorld()->NavigationSystem, 1);
             }
         }*/

        // if (!GameMode->HasWarmupRequiredPlayerCount())
        //     UWorld::SpawnActor(FindClass("FortPlayerStart"), FVector{0, 0, 3000});

        *Ret = false;
        return;
    }

    if (!GameMode->bWorldIsReady)
    {
        static auto WarmupStartClass = FindClass("PlayerStart");
        TArray<AActor*> Starts;
        Utils::GetAll(WarmupStartClass, Starts);
        auto StartsNum = Starts.Num();
        Starts.Free();

        if (StartsNum == 0 || !Misc::bHookedAll)
        {
            *Ret = false;
            return;
        }

        TArray<AFortAthenaMapInfo*> AllMapInfos;
        Utils::GetAll<AFortAthenaMapInfo>(AllMapInfos);

        if (AllMapInfos.Num() > 0 && !GameState->MapInfo)
        {
            *Ret = false;
            return;
        }
        AllMapInfos.Free();

        if ((VersionInfo.FortniteVersion >= 3.5 && VersionInfo.FortniteVersion <= 4.0))
            SetupPlaylist(GameMode, GameState);
        // else if (VersionInfo.EngineVersion >= 4.22 && VersionInfo.EngineVersion < 4.26)
        //     GameState->OnRep_CurrentPlaylistInfo();

        if (VersionInfo.FortniteVersion >= 25.20 && GameState->HasMapInfo() && GameState->MapInfo)
        {
            auto GamePhaseLogic = UFortGameStateComponent_BattleRoyaleGamePhaseLogic::Get(GameState);

            if (GamePhaseLogic)
            {
                auto InitializeFlightPath = (void (*)(AFortAthenaMapInfo*, AFortGameStateAthena*, UFortGameStateComponent_BattleRoyaleGamePhaseLogic*, bool, double, float, float))FindInitializeFlightPath();
                if (InitializeFlightPath)
                    InitializeFlightPath(GameState->MapInfo, GameState, GamePhaseLogic, false, 0.f, 0.f, 360.f);

                GamePhaseLogic->InitializeSafeZoneLocations();
            }
        }

        auto Playlist = VersionInfo.FortniteVersion >= 3.5 && GameMode->HasWarmupRequiredPlayerCount()
                            ? (GameMode->GameState->HasCurrentPlaylistInfo() ? GameMode->GameState->CurrentPlaylistInfo.BasePlaylist : GameMode->GameState->CurrentPlaylistData)
                            : nullptr;

        if (Playlist && Playlist->HasbSkipWarmup())
            UFortGameStateComponent_BattleRoyaleGamePhaseLogic::bSkipWarmup = Playlist->bSkipWarmup;
        if (Playlist && Playlist->HasbSkipAircraft())
            UFortGameStateComponent_BattleRoyaleGamePhaseLogic::bSkipAircraft = Playlist->bSkipAircraft;

        if (Playlist && Playlist->HasGameplayTagContainer())
        {

            for (int i = 0; i < Playlist->GameplayTagContainer.GameplayTags.Num(); i++)
            {
                auto& PlaylistTag = Playlist->GameplayTagContainer.GameplayTags.Get(i, FGameplayTag::Size());

                if (PlaylistTag.TagName.ToString() == "Athena.Playlist.SpecialEvent")
                {
                    for (auto& Event : Events::EventsArray)
                    {
                        if (Event.EventVersion != VersionInfo.FortniteVersion)
                            continue;

                        UObject* LoaderObject = nullptr;
                        if (Event.LoaderClass)
                            if (const UClass* LoaderClass = FindObject<UClass>(Event.LoaderClass))
                            {
                                TArray<AActor*> AllLoaders;
                                Utils::GetAll(LoaderClass, AllLoaders);
                                LoaderObject = AllLoaders.Num() > 0 ? AllLoaders[0] : nullptr;
                                AllLoaders.Free();
                            }

                        if (Event.LoaderFuncPath != nullptr && LoaderObject)
                            if (const UFunction* LoaderFunction = FindObject<UFunction>(Event.LoaderFuncPath))
                            {
                                int Param = 1;
                                LoaderObject->ProcessEvent(const_cast<UFunction*>(LoaderFunction), &Param);
                                printf("[Events] Loaded event level!\n");
                            }
                            else
                                printf("[Events] Failed to load event level!\n");

                        if (GameMode->HasSafeZoneLocations())
                            GameMode->SafeZoneLocations.Free();
                        else
                            UFortGameStateComponent_BattleRoyaleGamePhaseLogic::bEnableZones = false;
                        break;
                    }

                    break;
                }
            }
        }

        auto AbilitySet = VersionInfo.FortniteVersion > 8.30 ? FindObject<UFortAbilitySet>(L"/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GAS_AthenaPlayer.GAS_AthenaPlayer")
                                                             : FindObject<UFortAbilitySet>(L"/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GAS_DefaultPlayer.GAS_DefaultPlayer");
        AbilitySet->AddToRoot();
        AbilitySets.Add(AbilitySet);

        if (VersionInfo.FortniteVersion >= 20)
        {
            auto TacticalSprintAbility = FindObject<UFortAbilitySet>(L"/TacticalSprintGame/Gameplay/AS_TacticalSprint.AS_TacticalSprint");

            if (!TacticalSprintAbility)
                TacticalSprintAbility = FindObject<UFortAbilitySet>(L"/TacticalSprint/Gameplay/AS_TacticalSprint.AS_TacticalSprint");
            TacticalSprintAbility->AddToRoot();
            AbilitySets.Add(TacticalSprintAbility);

            auto AscenderAbility = FindObject<UFortAbilitySet>(L"/Ascender/Gameplay/Ascender/AS_Ascender.AS_Ascender");
            AscenderAbility->AddToRoot();
            AbilitySets.Add(AscenderAbility);

            auto DoorBashAbility = FindObject<UFortAbilitySet>(L"/DoorBashContent/Gameplay/AS_DoorBash.AS_DoorBash");
            DoorBashAbility->AddToRoot();
            AbilitySets.Add(DoorBashAbility);

            auto HillScrambleAbility = FindObject<UFortAbilitySet>(L"/HillScramble/Gameplay/AS_HillScramble.AS_HillScramble");
            HillScrambleAbility->AddToRoot();
            AbilitySets.Add(HillScrambleAbility);

            auto SlideImpulseAbility = FindObject<UFortAbilitySet>(L"/SlideImpulse/Gameplay/AS_SlideImpulse.AS_SlideImpulse");
            SlideImpulseAbility->AddToRoot();
            AbilitySets.Add(SlideImpulseAbility);

            if (std::floor(VersionInfo.FortniteVersion) == 21)
            {
                auto RealitySaplingAbility = FindObject<UFortAbilitySet>(L"/RealitySeedGameplay/Environment/Foliage/GAS_Athena_RealitySapling.GAS_Athena_RealitySapling");
                AbilitySets.Add(RealitySaplingAbility);
            }
        }

        for (auto& Set : AbilitySets)
            if (Set)
                Set->AddToRoot();

        if (Playlist && Playlist->HasModifierList())
            for (int i = 0; i < Playlist->ModifierList.Num(); i++)
            {
                auto Modifier = Playlist->ModifierList.Get(i, FSoftObjectPtr::Size()).Get();

                if (!Modifier)
                    continue;

                for (int j = 0; j < Modifier->PersistentAbilitySets.Num(); j++)
                {
                    auto& DeliveryInfo = Modifier->PersistentAbilitySets.Get(j, FFortAbilitySetDeliveryInfo::Size());

                    if (!DeliveryInfo.DeliveryRequirements.bApplyToPlayerPawns)
                        continue;

                    for (int k = 0; k < DeliveryInfo.AbilitySets.Num(); k++)
                    {
                        auto AbilitySet = DeliveryInfo.AbilitySets.Get(k, FSoftObjectPtr::Size()).Get();

                        AbilitySets.Add(AbilitySet);
                    }
                }
            }

        /*if (floor(VersionInfo.FortniteVersion) != 20)
        {
            UFortLootPackage::SpawnFloorLootForContainer(FindObject<UClass>(L"/Game/Athena/Environments/Blueprints/Tiered_Athena_FloorLoot_Warmup.Tiered_Athena_FloorLoot_Warmup_C"));
            UFortLootPackage::SpawnFloorLootForContainer(FindObject<UClass>(L"/Game/Athena/Environments/Blueprints/Tiered_Athena_FloorLoot_01.Tiered_Athena_FloorLoot_01_C"));
        }

        auto ConsumableSpawners = Utils::GetAll<ABGAConsumableSpawner>();

        for (auto& Spawner : ConsumableSpawners)
            UFortLootPackage::SpawnConsumableActor(Spawner);*/

        if (VersionInfo.EngineVersion >= 4.27)
        {
            if (GameState->HasDefaultParachuteDeployTraceForGroundDistance())
                GameState->DefaultParachuteDeployTraceForGroundDistance = 10000;
        }

        if (VersionInfo.FortniteVersion >= 27)
        {
            // fix grind rails
            auto GameData = FindObject<UCurveTable>("/GrindRail/DataTables/GrindRailGameData.GrindRailGameData");

            if (GameData)
            {
                static FName UseGrindingMME = FName(L"Default.GrindRails.UseGrindingMME");

                for (const auto& [RowName, RowPtr] : GameData->RowMap)
                {
                    if (RowName != UseGrindingMME)
                        continue;

                    FSimpleCurve* Row = (FSimpleCurve*)RowPtr;

                    if (!Row)
                        continue;

                    for (auto& Key : Row->Keys)
                        Key.Value = 0.f;
                }
            }
        }
        if (GameState->HasMapInfo() && GameState->MapInfo)
        {
            if (VersionInfo.FortniteVersion >= 3.4)
            {
                GameData = Playlist ? Playlist->GameData : nullptr;
                if (!GameData)
                    GameData = FindObject<UCurveTable>(L"/Game/Athena/Balance/DataTables/AthenaGameData.AthenaGameData");

                for (int i = 0; i < 6; i++)
                {
                    float Weight;
                    UDataTableFunctionLibrary::EvaluateCurveTableRow(GameState->MapInfo->VendingMachineRarityCount.Curve.CurveTable, GameState->MapInfo->VendingMachineRarityCount.Curve.RowName, (float)i, nullptr,
                                                                     &Weight, FString());

                    WeightMap[i] = Weight;
                    Sum += Weight;
                }

                UDataTableFunctionLibrary::EvaluateCurveTableRow(GameState->MapInfo->VendingMachineRarityCount.Curve.CurveTable, GameState->MapInfo->VendingMachineRarityCount.Curve.RowName, 0.f, nullptr, &Weight,
                                                                 FString());

                TotalWeight = std::accumulate(WeightMap.begin(), WeightMap.end(), 0.0f, [&](float acc, const std::pair<int, float>& p) { return acc + p.second; });
            }

            if (VersionInfo.FortniteVersion >= 3.3 && VersionInfo.FortniteVersion < 17 && GameState->MapInfo->LlamaClass)
            {
                auto PickSupplyDropLocation = (FVector * (*)(AFortAthenaMapInfo*, FVector*, FVector*, float, bool, float, float)) FindPickSupplyDropLocation();

                if (PickSupplyDropLocation)
                {
                    FFortSafeZoneDefinition& SafeZoneDefinition = GameState->MapInfo->SafeZoneDefinition;

                    auto LlamaMin = GameState->MapInfo->LlamaQuantityMin.Evaluate();
                    auto LlamaMax = GameState->MapInfo->LlamaQuantityMax.Evaluate();
                    auto LlamaCount = UKismetMathLibrary::RandomIntegerInRange((int)LlamaMin, (int)LlamaMax);
                    auto Radius = GameState->MapInfo->HasSafeZoneDefinition() ? SafeZoneDefinition.Radius.Evaluate(0) : 0;

                    if (Radius == 0)
                        Radius = 120000;
                    auto Center = GameState->MapInfo->GetMapCenter();
                    Center.Z = 10000;

                    for (int i = 0; i < LlamaCount; i++)
                    {
                        FVector Loc(0, 0, 0);
                        PickSupplyDropLocation(GameState->MapInfo, &Loc, &Center, Radius, 0, -1, -1);

                        if (Loc.X != 0 || Loc.Y != 0 || Loc.Z != 0)
                        {
                            FRotator Rot{};
                            Rot.Yaw = (float)rand() * 0.010986663f;

                            auto NewLlama = UWorld::SpawnActorUnfinished(GameState->MapInfo->LlamaClass, Loc, Rot);

                            static auto FindGroundLocationAt = NewLlama->GetFunction("FindGroundLocationAt");
                            auto GroundLoc = NewLlama->Call<FVector>(FindGroundLocationAt, Loc);

                            UWorld::FinishSpawnActor(NewLlama, GroundLoc, Rot);
                        }
                    }
                }
            }
        }

        GameMode->DefaultPawnClass = FindObject<UClass>(L"/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C");

        if (VersionInfo.EngineVersion == 4.16 && VersionInfo.FortniteVersion < 1.9)
        {
            auto sRef = Memcury::Scanner::FindStringRef(L"CollectGarbageInternal() is flushing async loading").Get();
            uint64_t CollectGarbage = 0;

            if (sRef)
            {
                for (int i = 0; i < 1000; i++)
                {
                    auto Ptr = (uint8_t*)(sRef - i);

                    if (*Ptr == 0x48 && *(Ptr + 1) == 0x89 && *(Ptr + 2) == 0x5C)
                    {
                        CollectGarbage = uint64_t(Ptr);
                        break;
                    }
                    else if (*Ptr == 0x40 && *(Ptr + 1) == 0x55)
                    {
                        CollectGarbage = uint64_t(Ptr);
                        break;
                    }
                    else if (*Ptr == 0x48 && *(Ptr + 1) == 0x8B && *(Ptr + 2) == 0xC4)
                    {
                        CollectGarbage = uint64_t(Ptr);
                        break;
                    }
                }

                Hooking::Patch<uint8_t>(CollectGarbage, 0xC3);
            }
        }
        else if (VersionInfo.EngineVersion <= 4.20)
        {
            auto pattern = VersionInfo.FortniteVersion > 3.2 ? Memcury::Scanner::FindPattern("E8 ? ? ? ? EB 26 40 38 3D ? ? ? ?") : Memcury::Scanner::FindPattern("E8 ? ? ? ? F0 FF 0D ? ? ? ? 0F B6 C3");

            if (pattern.IsValid())
                Hooking::Patch<uint8_t>(pattern.RelativeOffset(1).Get(), 0xC3);
        }

        if (GameState->HasAllPlayerBuildableClassesIndexLookup())
            for (auto& [Class, Handle] : GameState->AllPlayerBuildableClassesIndexLookup)
                AFortGameStateAthena::BuildingClassMap[Handle] = Class;

        if constexpr (DiscordWebhookConfig::WebhookURL && *DiscordWebhookConfig::WebhookURL)
        {
            auto curl = curl_easy_init();

            curl_easy_setopt(curl, CURLOPT_URL, DiscordWebhookConfig::WebhookURL);
            curl_slist* headers = curl_slist_append(NULL, "Content-Type: application/json");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

            char version[6];

            sprintf_s(version, VersionInfo.FortniteVersion >= 5.00 || VersionInfo.FortniteVersion < 1.2 ? "%.2f" : "%.1f", VersionInfo.FortniteVersion);

            auto payload = UEAllocatedString("{\"embeds\": [{\"title\": \"Server is joinable!\", \"fields\": [{\"name\":\"Version\",\"value\":\"") + version + "\"}, {\"name\":\"Playlist\",\"value\":\"" +
                           (Playlist ? Playlist->PlaylistName.ToString() : "Playlist_DefaultSolo") + "\"}], \"color\": " +
                           "\"7237230\", \"footer\": {\"text\":\"Erbium\", "
                           "\"icon_url\":\"https://cdn.discordapp.com/attachments/1341168629378584698/1436803905119064105/"
                           "L0WnFa.png.png?ex=6910ef69&is=690f9de9&hm=01a0888b46647959b38ee58df322048ab49e2a5a678e52d4502d9c5e3978d805&\"}, \"timestamp\":\"" +
                           iso8601() + "\"}] }";

            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());

            curl_easy_perform(curl);

            curl_easy_cleanup(curl);
        }

        // for some reason it doesnt like when u do it earlier
        if (!Playlist && VersionInfo.FortniteVersion <= 4)
            if (GameMode->GameSession->HasMaxPlayers())
                GameMode->GameSession->MaxPlayers = 100;

        GUI::gsStatus = Joinable;
        sprintf_s(GUI::windowTitle,
                  VersionInfo.EngineVersion >= 5.0 ? "Erbium (FN %.2f, UE %.1f): Joinable"
                                                   : (VersionInfo.FortniteVersion >= 5.00 || VersionInfo.FortniteVersion < 1.2 ? "Erbium (FN %.2f, UE %.2f): Joinable" : "Erbium (FN %.1f, UE %.2f): Joinable"),
                  VersionInfo.FortniteVersion, VersionInfo.EngineVersion);
        SetConsoleTitleA(GUI::windowTitle);
        GameMode->bWorldIsReady = true;
    }

    if (VersionInfo.EngineVersion >= 4.24 && GameMode->IsA<AFortGameModeAthena>())
    {
        int ReadyPlayers = 0;
        TArray<AFortPlayerControllerAthena*> PlayerList;
        Utils::GetAll<AFortPlayerControllerAthena>(PlayerList);

        for (auto& PlayerController : PlayerList)
        {
            auto PlayerState = PlayerController->PlayerState;

            if (!PlayerState->bIsSpectator && PlayerController->bReadyToStartMatch)
                ReadyPlayers++;
        }

        PlayerList.Free();

        auto VolumeManager = GameState->HasVolumeManager() ? GameState->VolumeManager : nullptr;

        bool bAllLevelsFinishedStreaming = true;
        if (GameState->HasAdditionalPlaylistLevelsStreamed())
        {
            TArray<FPlaylistStreamedLevelData>& AdditionalPlaylistLevels = *(TArray<FPlaylistStreamedLevelData>*)(__int64(GameState) + GameState->GetOffset("AdditionalPlaylistLevelsStreamed") - 0x10);
            for (int i = 0; i < AdditionalPlaylistLevels.Num(); i++)
            {
                auto& AdditionalPlaylistLevel = AdditionalPlaylistLevels.Get(i, FPlaylistStreamedLevelData::Size());

                if (!AdditionalPlaylistLevel.bIsFinishedStreaming || !AdditionalPlaylistLevel.StreamingLevel || !AdditionalPlaylistLevel.StreamingLevel->LoadedLevel->bIsVisible)
                {
                    bAllLevelsFinishedStreaming = false;
                    break;
                }
            }
        }

        static auto WaitingToStart = FName(L"WaitingToStart");
        *Ret = GameMode->bWorldIsReady && (GameState->HasbPlaylistDataIsLoaded() ? GameState->bPlaylistDataIsLoaded : true) && GameMode->MatchState == WaitingToStart && bAllLevelsFinishedStreaming &&
               (!VolumeManager || !(VolumeManager->HasbInSpawningStartup() ? VolumeManager->bInSpawningStartup : GameState->bInSpawningStartup)) &&
               ReadyPlayers >= (GameMode->HasWarmupRequiredPlayerCount() ? GameMode->WarmupRequiredPlayerCount : 1);
    }
    else
        *Ret = callOGWithRet(GameMode, Stack.GetCurrentNativeFunction(), ReadyToStartMatch);

    if (VersionInfo.FortniteVersion >= 11.00 && VersionInfo.FortniteVersion < 25.20 && !*Ret)
    {
        auto Time = (float)UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());
        auto WarmupDuration = 60.f;

        if (GameState->HasWarmupCountdownEndTime()) // gamephaselogic builds
        {
            GameState->WarmupCountdownStartTime = Time;
            GameState->WarmupCountdownEndTime = Time + WarmupDuration;
            GameMode->WarmupCountdownDuration = WarmupDuration;
            GameMode->WarmupEarlyCountdownDuration = WarmupDuration;
        }
    }
    return;
}

auto SpawnDefaultPawnForIdx = 0;
uint64_t ApplyCharacterCustomization;

void AFortGameMode::SpawnDefaultPawnFor(UObject* Context, FFrame& Stack, AActor** Ret)
{
    AFortPlayerControllerAthena* NewPlayer;
    AActor* StartSpot;
    Stack.StepCompiledIn(&NewPlayer);
    Stack.StepCompiledIn(&StartSpot);
    Stack.IncrementCode();
    auto GameMode = (AFortGameMode*)Context;

    if (VersionInfo.EngineVersion >= 5.4)
    {
        static bool once = false;
        if (!once) { once = true; printf("[Boron][ExecProbe] SpawnDefaultPawnFor FIRED\n"); }
    }

    if (!NewPlayer || !StartSpot)
        return;

    auto GameState = GameMode->GameState;
    AFortPlayerPawnAthena* Pawn = nullptr;

    Pawn = (AFortPlayerPawnAthena*)UWorld::SpawnActor(GameMode->GetDefaultPawnClassForController(NewPlayer), StartSpot->GetTransform(), NewPlayer, 3);

    while (!Pawn)
    {
        auto PlayerStart = GameMode->ChoosePlayerStart(NewPlayer);
        if (PlayerStart)
            Pawn = (AFortPlayerPawnAthena*)UWorld::SpawnActor(GameMode->GetDefaultPawnClassForController(NewPlayer), PlayerStart->GetTransform(), NewPlayer, 3);
    }
    // they only stripped it on athena for some reason
    /*static auto FortGMSpawnDefaultPawnFor = (AFortPlayerPawnAthena * (*)(AFortGameMode*, AFortPlayerControllerAthena*, AActor*))
    DefaultObjImpl("FortGameMode")->Vft[SpawnDefaultPawnForIdx]; Pawn = FortGMSpawnDefaultPawnFor(GameMode, NewPlayer, StartSpot);

    if (!Pawn)
    {
        auto Transform = StartSpot->GetTransform();
        Transform.Translation.Z += 200.f;
        Pawn = GameMode->SpawnDefaultPawnAtTransform(NewPlayer, Transform);
    }*/

    *Ret = Pawn;

    auto Num = NewPlayer->WorldInventory ? NewPlayer->WorldInventory->Inventory.ReplicatedEntries.Num() : 0;
    if (Num == 0)
    {
        if (VersionInfo.FortniteVersion <= 1.91 && VersionInfo.FortniteVersion != 1.1 && VersionInfo.FortniteVersion != 1.11 && NewPlayer->HasStrongMyHero())
        {
            static auto HeroCharPartsOffset = NewPlayer->StrongMyHero->GetOffset("CharacterParts");
            auto& HeroCharParts = GetFromOffset<TArray<UObject*>>(NewPlayer->StrongMyHero, HeroCharPartsOffset);
            static auto CharacterPartsOffset = NewPlayer->PlayerState->GetOffset("CharacterParts");
            auto& CharacterParts = GetFromOffset<const UObject* [0x6]>(NewPlayer->PlayerState, CharacterPartsOffset);

            if (HeroCharParts.Num() > 0)
            {
                for (auto& Part : HeroCharParts)
                {
                    static auto PartTypeOffset = Part->GetOffset("CharacterPartType");
                    CharacterParts[GetFromOffset<uint8>(Part, PartTypeOffset)] = Part;
                }
            }
            else
            {

                static auto Head = FindObject<UObject>(L"/Game/Characters/CharacterParts/Female/Medium/Heads/F_Med_Head1.F_Med_Head1");
                static auto Body = FindObject<UObject>(L"/Game/Characters/CharacterParts/Female/Medium/Bodies/F_Med_Soldier_01.F_Med_Soldier_01");
                static auto Backpack = FindObject<UObject>(L"/Game/Characters/CharacterParts/Backpacks/NoBackpack.NoBackpack");

                CharacterParts[0] = Head;
                CharacterParts[1] = Body;
                CharacterParts[3] = Backpack;
            }
        }

        if (NewPlayer->HasXPComponent())
        {
            if (NewPlayer->XPComponent->HasbRegisteredWithQuestManager())
            {
                NewPlayer->XPComponent->bRegisteredWithQuestManager = true;
                NewPlayer->XPComponent->OnRep_bRegisteredWithQuestManager();
            }

            if (NewPlayer->PlayerState->HasSeasonLevelUIDisplay())
            {
                NewPlayer->PlayerState->SeasonLevelUIDisplay = NewPlayer->XPComponent->CurrentLevel;
                NewPlayer->PlayerState->OnRep_SeasonLevelUIDisplay();
            }
            // NewPlayer->XPComponent->OnProfileUpdated();
        }

        const UObject* BattleBusDef = nullptr;
        const UClass* SupplyDropClass = nullptr;
        if (VersionInfo.FortniteVersion == 18.40)
            BattleBusDef = FindObject<UObject>(L"/Game/Athena/Items/Cosmetics/BattleBuses/BBID_HeadbandBus.BBID_HeadbandBus");
        else if (VersionInfo.FortniteVersion == 1.11 || VersionInfo.FortniteVersion == 7.30 || VersionInfo.FortniteVersion == 11.31 || VersionInfo.FortniteVersion == 15.10 || VersionInfo.FortniteVersion == 19.01 ||
                 VersionInfo.FortniteVersion == 28.01)
        {
            BattleBusDef = FindObject<UObject>(L"/Game/Athena/Items/Cosmetics/BattleBuses/BBID_WinterBus.BBID_WinterBus");

            if (VersionInfo.FortniteVersion == 1.11)
                SupplyDropClass = FindObject<UClass>(L"/Game/Athena/SupplyDrops/B_AthenaSupplyDrop_Gift.B_AthenaSupplyDrop_Gift_C");
            else
                SupplyDropClass = FindObject<UClass>(L"/Game/Athena/SupplyDrops/AthenaSupplyDrop_Holiday.AthenaSupplyDrop_Holiday_C");
        }
        else if (VersionInfo.FortniteVersion == 23.10)
        {
            BattleBusDef = FindObject<UObject>(L"/Game/Athena/Items/Cosmetics/BattleBuses/BBID_BattleBus_Booster_Winter.BBID_BattleBus_Booster_Winter");
            SupplyDropClass = FindObject<UClass>(L"/Game/Athena/SupplyDrops/AthenaSupplyDrop_Holiday.AthenaSupplyDrop_Holiday_C");
        }
        else if (VersionInfo.FortniteVersion == 5.10 || VersionInfo.FortniteVersion == 9.41 || VersionInfo.FortniteVersion == 14.20 || VersionInfo.FortniteVersion == 18.00 || VersionInfo.FortniteVersion == 22.00 ||
                 VersionInfo.FortniteVersion == 26.20)
        {
            if (VersionInfo.FortniteVersion == 5.10)
                BattleBusDef = FindObject<UObject>(L"/Game/Athena/Items/Cosmetics/BattleBuses/BBID_BirthdayBus.BBID_BirthdayBus");
            else if (VersionInfo.FortniteVersion == 9.41)
                BattleBusDef = FindObject<UObject>(L"/Game/Athena/Items/Cosmetics/BattleBuses/BBID_BirthdayBus2nd.BBID_BirthdayBus2nd");
            else if (VersionInfo.FortniteVersion == 14.20)
                BattleBusDef = FindObject<UObject>(L"/Game/Athena/Items/Cosmetics/BattleBuses/BBID_BirthdayBus3rd.BBID_BirthdayBus3rd");
            else if (VersionInfo.FortniteVersion == 18.00)
                BattleBusDef = FindObject<UObject>(L"/Game/Athena/Items/Cosmetics/BattleBuses/BBID_BirthdayBus4th.BBID_BirthdayBus4th");
            else if (VersionInfo.FortniteVersion == 22.00)
                BattleBusDef = FindObject<UObject>(L"/Game/Athena/Items/Cosmetics/BattleBuses/BBID_BirthdayBus5th.BBID_BirthdayBus5th");
            else if (VersionInfo.FortniteVersion == 26.20)
                BattleBusDef = FindObject<UObject>(L"/Game/Athena/Items/Cosmetics/BattleBuses/BBID_BirthdayBus6th.BBID_BirthdayBus6th");

            SupplyDropClass = FindObject<UClass>(L"/Game/Athena/SupplyDrops/AthenaSupplyDrop_BDay.AthenaSupplyDrop_BDay_C");
        }
        else if (VersionInfo.FortniteVersion == 6.20 || VersionInfo.FortniteVersion == 6.21 || VersionInfo.FortniteVersion == 11.10 || VersionInfo.FortniteVersion == 14.40 || VersionInfo.FortniteVersion == 18.21)
            BattleBusDef = FindObject<UObject>(L"/Game/Athena/Items/Cosmetics/BattleBuses/BBID_HalloweenBus.BBID_HalloweenBus");
        else if (VersionInfo.FortniteVersion == 26.30)
            BattleBusDef = FindObject<UObject>(L"/Game/Athena/Items/Cosmetics/BattleBuses/BBID_HalloweenBus_Booster.BBID_HalloweenBus_Booster");
        else if (VersionInfo.FortniteVersion == 14.30)
            BattleBusDef = FindObject<UObject>(L"/Game/Athena/Items/Cosmetics/BattleBuses/BBID_BusUpgrade1.BBID_BusUpgrade1");
        else if (VersionInfo.FortniteVersion == 14.50)
            BattleBusDef = FindObject<UObject>(L"/Game/Athena/Items/Cosmetics/BattleBuses/BBID_BusUpgrade2.BBID_BusUpgrade2");
        else if (VersionInfo.FortniteVersion == 14.60)
            BattleBusDef = FindObject<UObject>(L"/Game/Athena/Items/Cosmetics/BattleBuses/BBID_BusUpgrade3.BBID_BusUpgrade3");
        else if (VersionInfo.FortniteVersion >= 12.30 && VersionInfo.FortniteVersion <= 12.61)
        {
            BattleBusDef = FindObject<UObject>(L"/Game/Athena/Items/Cosmetics/BattleBuses/BBID_DonutBus.BBID_DonutBus");
            SupplyDropClass = FindObject<UClass>(L"/Game/Athena/SupplyDrops/AthenaSupplyDrop_Donut.AthenaSupplyDrop_Donut_C");
        }
        else if (VersionInfo.FortniteVersion == 9.30)
            BattleBusDef = FindObject<UObject>(L"/Game/Athena/Items/Cosmetics/BattleBuses/BBID_WorldCupBus.BBID_WorldCupBus");
        else if (VersionInfo.FortniteVersion == 21.00)
            BattleBusDef = FindObject<UObject>(L"/Game/Athena/Items/Cosmetics/BattleBuses/BBID_CelebrationBus.BBID_CelebrationBus");
        else if (std::floor(VersionInfo.FortniteVersion) == 27)
            BattleBusDef = FindObject<UObject>(L"/Game/Athena/Items/Cosmetics/BattleBuses/BBID_DefaultBus.BBID_DefaultBus");

        if (BattleBusDef)
        {
            if (GameState->HasDefaultBattleBus())
                GameState->DefaultBattleBus = BattleBusDef;

            TArray<AFortAthenaAircraft*> Aircrafts;
            Utils::GetAll<AFortAthenaAircraft>(Aircrafts);
            for (auto& Aircraft : Aircrafts)
            {
                Aircraft->DefaultBusSkin = BattleBusDef;

                if (Aircraft->SpawnedCosmeticActor)
                {
                    static auto Offset = Aircraft->SpawnedCosmeticActor->GetOffset("ActiveSkin");

                    GetFromOffset<const UObject*>(Aircraft->SpawnedCosmeticActor, Offset) = BattleBusDef;
                }
            }
            Aircrafts.Free();
        }

        if (GameState->HasMapInfo() && GameState->MapInfo)
        {
            if (SupplyDropClass)
            {
                if (GameState->MapInfo->HasSupplyDropInfoList())
                    for (auto& Info : GameState->MapInfo->SupplyDropInfoList)
                        Info->SupplyDropClass = SupplyDropClass;
                else
                    GameState->MapInfo->SupplyDropClass = SupplyDropClass;
            }
        }
    }
    else
    {
        // NewPlayer->WorldInventory->Inventory.ReplicatedEntries.ResetNum();
        // NewPlayer->WorldInventory->Inventory.ItemInstances.ResetNum();

        /*for (int i = 0; i < NewPlayer->WorldInventory->Inventory.ItemInstances.Num(); i++)
        {
            auto& Entry = NewPlayer->WorldInventory->Inventory.ItemInstances[i]->ItemEntry;

            if (AFortInventory::IsPrimaryQuickbar(Entry.ItemDefinition) || Entry.ItemDefinition->IsA(AmmoClass) || Entry.ItemDefinition->IsA(ResourceClass))
            {
                NewPlayer->WorldInventory->Inventory.ItemInstances.Remove(i);
                i--;
            }
        }

        NewPlayer->WorldInventory->Update(nullptr);*/
    }
}

AActor* AFortGameMode::SpawnDefaultPawnFor_Native(AFortGameMode* GameMode, AFortPlayerControllerAthena* NewPlayer, AActor* StartSpot)
{
    static bool once = false;
    if (!once)
    {
        once = true;
        printf("[Boron][ExecProbe] SpawnDefaultPawnFor_Native FIRED (address hook)\n");
    }

    if (!GameMode || !NewPlayer)
        return SpawnDefaultPawnFor_NativeOG ? SpawnDefaultPawnFor_NativeOG(GameMode, NewPlayer, StartSpot) : nullptr;

    AFortPlayerPawnAthena* Pawn = nullptr;

    if (SpawnDefaultPawnFor_NativeOG)
        Pawn = (AFortPlayerPawnAthena*)SpawnDefaultPawnFor_NativeOG(GameMode, NewPlayer, StartSpot);

    if (!Pawn)
    {
        auto PawnClass = GameMode->GetDefaultPawnClassForController(NewPlayer);

        if (StartSpot)
            Pawn = (AFortPlayerPawnAthena*)UWorld::SpawnActor(PawnClass, StartSpot->GetTransform(), NewPlayer, 3);

        for (int tries = 0; !Pawn && tries < 8; tries++)
        {
            auto PlayerStart = GameMode->ChoosePlayerStart(NewPlayer);
            if (!PlayerStart)
                break;
            Pawn = (AFortPlayerPawnAthena*)UWorld::SpawnActor(PawnClass, PlayerStart->GetTransform(), NewPlayer, 3);
        }
    }

    if (VersionInfo.EngineVersion >= 5.4 && Pawn)
    {
        if (!NewPlayer->MyFortPawn)
            NewPlayer->MyFortPawn = Pawn;

        if (!NewPlayer->Pawn)
            NewPlayer->Pawn = Pawn;

        printf("[Boron][Cosmetics] pre-registered pawn at spawn: MyFortPawn=%p Pawn=%p\n",
               (void*)NewPlayer->MyFortPawn, (void*)NewPlayer->Pawn);
    }

    printf("[Boron][Pawn] SpawnDefaultPawnFor_Native -> Pawn=%p StartSpot=%p native=%s\n",
           (void*)Pawn, (void*)StartSpot, SpawnDefaultPawnFor_NativeOG ? "yes" : "no");

    return Pawn;
}

void AFortGameMode::HandlePostSafeZonePhaseChanged(AFortGameMode* GameMode, int NewSafeZonePhase_Inp)
{
    if (!GameMode->SafeZoneIndicator)
        return;

    auto NewSafeZonePhase = NewSafeZonePhase_Inp >= 0 ? NewSafeZonePhase_Inp : ((GameMode->HasSafeZonePhase() ? GameMode->SafeZonePhase : GameMode->SafeZoneIndicator->CurrentPhase) + 1);
    auto GameState = (AFortGameStateAthena*)GameMode->GameState;

    float TimeSeconds = (float)UGameplayStatics::GetTimeSeconds(GameState);

    if (VersionInfo.FortniteVersion >= 21.10)
    {
        if (HandlePostSafeZonePhaseChangedOG)
            HandlePostSafeZonePhaseChangedOG(GameMode, NewSafeZonePhase_Inp);

        return;
    }

    constexpr static std::array<float, 8> LateGameDurations{
        0.f, 120.f, 90.f, 60.f, 50.f, 35.f, 30.f, 40.f,
    };

    constexpr static std::array<float, 8> LateGameHoldDurations{
        0.f, 90.f, 75.f, 60.f, 45.f, 30.f, 0.f, 0.f,
    };

    static auto DurationsOffset = 0;
    if (DurationsOffset == 0)
    {
        DurationsOffset = 0x258;

        if (VersionInfo.FortniteVersion >= 18)
            DurationsOffset = 0x248;
        else if (VersionInfo.FortniteVersion < 15.20)
            DurationsOffset = 0x1f8;
    }

    auto SafeZoneDefinition = &GameState->MapInfo->SafeZoneDefinition;
    TArray<float>& Durations = *(TArray<float>*)(SafeZoneDefinition + DurationsOffset);
    TArray<float>& HoldDurations = *(TArray<float>*)(SafeZoneDefinition + DurationsOffset - 0x10);

    if (VersionInfo.FortniteVersion >= 13.00)
    {
        static bool bSetDurations = false;
        if (!bSetDurations)
        {
            bSetDurations = true;

            auto GameData = GameMode->HasAthenaGameDataTable() ? GameMode->AthenaGameDataTable : GameState->AthenaGameDataTable;

            auto ShrinkTime = FName(L"Default.SafeZone.ShrinkTime");
            auto HoldTime = FName(L"Default.SafeZone.WaitTime");

            for (int i = 0; i < Durations.Num(); i++)
            {
                UDataTableFunctionLibrary::EvaluateCurveTableRow(GameData, ShrinkTime, (float)i, nullptr, &Durations[i], FString());
            }
            for (int i = 0; i < HoldDurations.Num(); i++)
            {
                UDataTableFunctionLibrary::EvaluateCurveTableRow(GameData, HoldTime, (float)i, nullptr, &HoldDurations[i], FString());
            }
        }

        if (!LategameConfig::bLateGame || GameMode->SafeZonePhase > LategameConfig::LateGameZone)
        {
            auto Duration = Durations[NewSafeZonePhase];
            auto HoldDuration = HoldDurations[NewSafeZonePhase];

            GameMode->SafeZoneIndicator->SafeZoneStartShrinkTime = TimeSeconds + HoldDuration;
            GameMode->SafeZoneIndicator->SafeZoneFinishShrinkTime = GameMode->SafeZoneIndicator->SafeZoneStartShrinkTime + Duration;
        }
    }

    HandlePostSafeZonePhaseChangedOG(GameMode, NewSafeZonePhase_Inp);

    /*if (FConfiguration::bLateGame && GameMode->SafeZonePhase > FConfiguration::LateGameZone)
    {
        auto newIdx = GameMode->SafeZonePhase - FConfiguration::LateGameZone + 1;
        auto Duration = newIdx >= LateGameDurations.size() ? 0.f : LateGameDurations[newIdx];
        auto HoldDuration = newIdx >= LateGameHoldDurations.size() ? 0.f : LateGameHoldDurations[newIdx];

        GameMode->SafeZoneIndicator->SafeZoneStartShrinkTime = TimeSeconds + HoldDuration;
        GameMode->SafeZoneIndicator->SafeZoneFinishShrinkTime = GameMode->SafeZoneIndicator->SafeZoneStartShrinkTime + Duration;
    }*/

    if (LategameConfig::bLateGame && GameMode->SafeZonePhase < LategameConfig::LateGameZone)
    {
        GameMode->SafeZoneIndicator->SafeZoneStartShrinkTime = TimeSeconds;
        GameMode->SafeZoneIndicator->SafeZoneFinishShrinkTime = GameMode->SafeZoneIndicator->SafeZoneStartShrinkTime + 0.15f;
        return;
    }
    else if (LategameConfig::bLateGame && GameMode->SafeZonePhase == LategameConfig::LateGameZone)
    {
        // auto Duration = Durations[FConfiguration::LateGameZone];
        // auto HoldDuration = HoldDurations[FConfiguration::LateGameZone];

        if (LategameConfig::bLateGame && LategameConfig::bLateGameLongZone)
            GameMode->SafeZoneIndicator->SafeZoneStartShrinkTime = 676767.f;
        if (VersionInfo.FortniteVersion >= 13)
            GameMode->SafeZoneIndicator->SafeZoneFinishShrinkTime = GameMode->SafeZoneIndicator->SafeZoneStartShrinkTime + Durations[LategameConfig::LateGameZone];
    }

    if (LategameConfig::bLateGame && (SafeZoneLoc.X != 0 || SafeZoneLoc.Y != 0 || SafeZoneLoc.Z != 0))
    {
        GameMode->SafeZoneIndicator->NextCenter = SafeZoneLoc;
        GameMode->SafeZoneIndicator->LastCenter = SafeZoneLoc;
    }

    if (NewSafeZonePhase > (LategameConfig::bLateGame ? LategameConfig::LateGameZone : 1))
    {
        for (auto& UncastedPlayer : GameMode->AlivePlayers)
        {
            auto PlayerController = (AFortPlayerControllerAthena*)UncastedPlayer;

            PlayerController->GetQuestManager(1)->SendStatEvent(PlayerController, EFortQuestObjectiveStatEvent::GetStormPhase(), 1, false);
        }
    }
}

uint64_t NotifyGameMemberAdded_ = 0;
int16_t WorldPlayerId = 0;
static void HandleStartingNewPlayer_Impl(AFortGameMode* GameMode, AFortPlayerControllerAthena* NewPlayer)
{
    auto GameState = (AFortGameStateAthena*)GameMode->GameState;
    AFortPlayerStateAthena* PlayerState = (AFortPlayerStateAthena*)NewPlayer->PlayerState;

    if (VersionInfo.FortniteVersion <= 2.5)
    {
        NewPlayer->QuickBars = UWorld::SpawnActor<AFortQuickBars>(FVector{});
        NewPlayer->QuickBars->SetOwner(NewPlayer);
    }

    if (PlayerState->HasSquadId())
    {
        PlayerState->SquadId = PlayerState->TeamIndex - 3;
        PlayerState->OnRep_SquadId();
    }

    if (GameState->HasGameMemberInfoArray())
    {
        auto Member = (FGameMemberInfo*)malloc(FGameMemberInfo::Size());
        memset((PBYTE)Member, 0, FGameMemberInfo::Size());

        Member->MostRecentArrayReplicationKey = -1;
        Member->ReplicationID = -1;
        Member->ReplicationKey = -1;
        Member->TeamIndex = PlayerState->TeamIndex;
        Member->SquadId = PlayerState->SquadId;
        Member->MemberUniqueId = PlayerState->HasUniqueID() ? PlayerState->UniqueID : PlayerState->UniqueId;

        auto& NewMember = GameState->GameMemberInfoArray.Members.Add(*Member, FGameMemberInfo::Size());
        GameState->GameMemberInfoArray.MarkItemDirty(NewMember);

        auto NotifyGameMemberAdded = (void (*)(AFortGameStateAthena*, uint8_t, uint8_t, FUniqueNetIdRepl*))NotifyGameMemberAdded_;
        if (NotifyGameMemberAdded)
            NotifyGameMemberAdded(GameState, Member->SquadId, Member->TeamIndex, &Member->MemberUniqueId);

        free(Member);
    }

    // if (NewPlayer->HasbBuildFree())
    //     NewPlayer->bBuildFree = FConfiguration::bInfiniteMats;

    if (!NewPlayer->WorldInventory)
    {
        NewPlayer->WorldInventory = UWorld::SpawnActor<AFortInventory>(NewPlayer->WorldInventoryClass, FVector{}, FRotator{}, NewPlayer);
        NewPlayer->WorldInventory->InventoryType = 0;
    }

    if (wcsstr(FConfig::Playlist, L"/Game/Athena/Playlists/Creative/Playlist_PlaygroundV2.Playlist_PlaygroundV2"))
        AFortAthenaCreativePortal::Create(NewPlayer);

    PlayerState->WorldPlayerId = WorldPlayerId;
}

void AFortGameMode::HandleStartingNewPlayer_(UObject* Context, FFrame& Stack)
{
    AFortPlayerControllerAthena* NewPlayer;
    Stack.StepCompiledIn(&NewPlayer);
    Stack.IncrementCode();
    auto GameMode = (AFortGameMode*)Context;

    HandleStartingNewPlayer_Impl(GameMode, NewPlayer);

    return callOG(GameMode, Stack.GetCurrentNativeFunction(), HandleStartingNewPlayer, NewPlayer);
}

void AFortGameMode::HandleStartingNewPlayer_Native(AFortGameMode* GameMode, AFortPlayerControllerAthena* NewPlayer)
{
    static bool once = false;
    if (!once) { once = true; printf("[Boron][ExecProbe] HandleStartingNewPlayer_Native FIRED (address hook)\n"); }

    if (!GameMode || !NewPlayer)
    {
        if (HandleStartingNewPlayer_NativeOG)
            HandleStartingNewPlayer_NativeOG(GameMode, NewPlayer);
        return;
    }

    HandleStartingNewPlayer_Impl(GameMode, NewPlayer);

    if (HandleStartingNewPlayer_NativeOG)
        HandleStartingNewPlayer_NativeOG(GameMode, NewPlayer);
}

uint8_t AFortGameMode::PickTeam(AFortGameMode* GameMode, uint8_t PreferredTeam, AFortPlayerControllerAthena* Controller)
{
    if (!GameMode->HasWarmupRequiredPlayerCount())
        return 0;

    uint8_t ret = CurrentTeam;
    auto Playlist = VersionInfo.FortniteVersion >= 3.5 && GameMode->HasWarmupRequiredPlayerCount()
                        ? (GameMode->GameState->HasCurrentPlaylistInfo() ? GameMode->GameState->CurrentPlaylistInfo.BasePlaylist : GameMode->GameState->CurrentPlaylistData)
                        : nullptr;

    if (wcscmp(FConfig::Playlist, L"/DurianPlaylist/Playlist/Playlist_Durian.Playlist_Durian") == 0)
    {
        CurrentTeam++;
        return ret;
    }
    printf("Picked team %d %d\n", ret, Playlist ? Playlist->MaxSquadSize : 1);
    if (bIsLargeTeamGame)
    {
        if (CurrentTeam == 4)
            CurrentTeam = 3;
        else
            CurrentTeam = 4;
    }
    else
    {
        if (++PlayersOnCurTeam >= (Playlist ? Playlist->MaxSquadSize : 1))
        {
            CurrentTeam++;
            PlayersOnCurTeam = 0;
        }
    }

    return ret;
}

bool AFortGameMode::StartAircraftPhase(AFortGameMode* GameMode, char a2)
{
    auto Ret = StartAircraftPhaseOG(GameMode, a2);

    auto GameState = (AFortGameStateAthena*)GameMode->GameState;

    auto Playlist = VersionInfo.FortniteVersion >= 3.5 && GameMode->HasWarmupRequiredPlayerCount()
                        ? (GameMode->GameState->HasCurrentPlaylistInfo() ? GameMode->GameState->CurrentPlaylistInfo.BasePlaylist : GameMode->GameState->CurrentPlaylistData)
                        : nullptr;
    if constexpr (DiscordWebhookConfig::WebhookURL && *DiscordWebhookConfig::WebhookURL)
    {
        auto curl = curl_easy_init();

        curl_easy_setopt(curl, CURLOPT_URL, DiscordWebhookConfig::WebhookURL);
        curl_slist* headers = curl_slist_append(NULL, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        char version[6];

        sprintf_s(version, VersionInfo.FortniteVersion >= 5.00 || VersionInfo.FortniteVersion < 1.2 ? "%.2f" : "%.1f", VersionInfo.FortniteVersion);

        auto payload = UEAllocatedString("{\"embeds\": [{\"title\": \"Match has started!\", \"fields\": [{\"name\":\"Version\",\"value\":\"") + version + "\"}, {\"name\":\"Playlist\",\"value\":\"" +
                       (Playlist ? Playlist->PlaylistName.ToString() : "Playlist_DefaultSolo") + "\"},{\"name\":\"Players\",\"value\":\"" + std::to_string(GameMode->AlivePlayers.Num()).c_str() +
                       "\"}], \"color\": " +
                       "\"7237230\", \"footer\": {\"text\":\"Erbium\", "
                       "\"icon_url\":\"https://cdn.discordapp.com/attachments/1341168629378584698/1436803905119064105/"
                       "L0WnFa.png.png?ex=6910ef69&is=690f9de9&hm=01a0888b46647959b38ee58df322048ab49e2a5a678e52d4502d9c5e3978d805&\"}, \"timestamp\":\"" +
                       iso8601() + "\"}] }";

        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());

        curl_easy_perform(curl);

        curl_easy_cleanup(curl);
    }
    GUI::gsStatus = StartedMatch;
    sprintf_s(GUI::windowTitle,
              VersionInfo.EngineVersion >= 5.0 ? "Erbium (FN %.2f, UE %.1f): Match started"
                                               : (VersionInfo.FortniteVersion >= 5.00 || VersionInfo.FortniteVersion < 1.2 ? "Erbium (FN %.2f, UE %.2f): Match started" : "Erbium (FN %.1f, UE %.2f): Match started"),
              VersionInfo.FortniteVersion, VersionInfo.EngineVersion);
    SetConsoleTitleA(GUI::windowTitle);

    // credit to heliato
    if (GameRuleConfig::bJoinInProgress || (Playlist && (Playlist->HasbAllowJoinInProgress() ? Playlist->bAllowJoinInProgress : false)))
        *(bool*)(uint64_t(&GameMode->WarmupRequiredPlayerCount) - 4) = false;

    if (LategameConfig::bLateGame && VersionInfo.FortniteVersion < 25.20)
    {
        auto Aircraft = GameState->HasAircrafts() ? (GameState->Aircrafts.Num() > 0 ? GameState->Aircrafts[0] : nullptr) : GameState->Aircraft;

        if (!Aircraft)
            return Ret;

        FVector Loc;
        bool bScuffed = false;
        if (GameMode->SafeZoneLocations.Num() < 4)
        {
            bScuffed = true;

            TArray<ABuildingFoundation*> Foundations;
            Utils::GetAll<ABuildingFoundation>(Foundations);
            auto Foundation = Foundations[rand() % Foundations.Num()];

            Foundations.Free();

            SafeZoneLoc = Loc = Foundation->K2_GetActorLocation();

            // FConfiguration::bLateGame = false;
            // printf("LateGame is not supported on this version!\n");
            // return Ret;
        }
        else
        {
            Loc = GameMode->SafeZoneLocations.Get(LategameConfig::LateGameZone + (VersionInfo.FortniteVersion >= 24 ? 3 : 0) - 1, FVector::Size());
        }

        Loc.Z = 17500.f;

        if (GameState->HasDefaultParachuteDeployTraceForGroundDistance())
        {
            GameState->DefaultParachuteDeployTraceForGroundDistance = 2500.f;
        }

        float LGFlightSpeed = 0.f;
        float LGFlightTime = 7.f;
        FVector LGStartLoc = Loc;

        if (LategameConfig::bLateGameMovingBus)
        {
            auto Forward = Aircraft->GetActorForwardVector();
            Forward.Z = 0.f;
            Forward.Normalize();

            float LGRadius = 75000.f;
            LGFlightTime = 35.f;
            LGFlightSpeed = (LGRadius * 2.f) / LGFlightTime;

            LGStartLoc = Loc - Forward * LGRadius;
            LGStartLoc.Z = 17500.f;
        }

        if (Aircraft->HasFlightInfo())
        {
            Aircraft->FlightInfo.FlightSpeed = LGFlightSpeed;

            Aircraft->FlightInfo.FlightStartLocation = LGStartLoc;

            Aircraft->FlightInfo.TimeTillFlightEnd = LGFlightTime;
            Aircraft->FlightInfo.TimeTillDropEnd = LGFlightTime;
            Aircraft->FlightInfo.TimeTillDropStart = 0.f;
        }
        else
        {
            Aircraft->FlightSpeed = LGFlightSpeed;

            Aircraft->FlightStartLocation = LGStartLoc;

            if (Aircraft->HasTimeTillFlightEnd())
            {
                Aircraft->TimeTillFlightEnd = LGFlightTime;
                Aircraft->TimeTillDropEnd = LGFlightTime;
                Aircraft->TimeTillDropStart = 0.f;
            }
        }
        Aircraft->DropStartTime = (float)UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());
        Aircraft->DropEndTime = (float)UGameplayStatics::GetTimeSeconds(UWorld::GetWorld()) + LGFlightTime;
        Aircraft->FlightStartTime = (float)UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());
        Aircraft->FlightEndTime = (float)UGameplayStatics::GetTimeSeconds(UWorld::GetWorld()) + LGFlightTime;
        // GameState->bAircraftIsLocked = false;
        // GameState->SafeZonesStartTime = (float)UGameplayStatics::GetTimeSeconds(UWorld::GetWorld()) + 7.6f;
    }

    return Ret;
}

void AFortGameMode::OnAircraftExitedDropZone_(UObject* Context, FFrame& Stack)
{
    AFortAthenaAircraft* Aircraft;
    Stack.StepCompiledIn(&Aircraft);
    Stack.IncrementCode();

    auto GameMode = (AFortGameMode*)Context;
    auto GameState = (AFortGameStateAthena*)GameMode->GameState;

    if (LategameConfig::bLateGame)
    {
        static auto CompClass = FindClass("FortControllerComponent_Aircraft");

        if (CompClass)
        {
            for (auto& Player : GameMode->AlivePlayers)
            {
                if (((AFortPlayerControllerAthena*)Player)->IsInAircraft())
                {
                    ((AFortPlayerControllerAthena*)Player)->GetAircraftComponent()->ServerAttemptAircraftJump(FRotator{});
                }
            }
        }
        else
        {
            for (auto& Player : GameMode->AlivePlayers)
            {
                if (((AFortPlayerControllerAthena*)Player)->IsInAircraft())
                {
                    ((AFortPlayerControllerAthena*)Player)->ServerAttemptAircraftJump(FRotator{});
                }
            }
        }
    }

    if (LategameConfig::bLateGame)
    {
        GameState->GamePhase = 4;
        GameState->GamePhaseStep = 7;
        GameState->OnRep_GamePhase(3);
    }

    callOG(GameMode, Stack.GetCurrentNativeFunction(), OnAircraftExitedDropZone, Aircraft);
}

TArray<FFortSafeZonePhaseInfo> Phases;

AFortSafeZoneIndicator* SetupSafeZoneIndicator(AFortGameMode* GameMode)
{
    // thanks heliato
    auto GameState = (AFortGameStateAthena*)GameMode->GameState;

    if (!GameMode->SafeZoneIndicator)
    {
        AFortSafeZoneIndicator* SafeZoneIndicator = UWorld::SpawnActor<AFortSafeZoneIndicator>(GameMode->SafeZoneIndicatorClass, FVector{});

        if (SafeZoneIndicator)
        {
            FFortSafeZoneDefinition& SafeZoneDefinition = GameState->MapInfo->SafeZoneDefinition;
            float SafeZoneCount = SafeZoneDefinition.Count.Evaluate();

            auto& Array = SafeZoneIndicator->HasSafeZonePhases() ? SafeZoneIndicator->SafeZonePhases : Phases;

            if (Array.IsValid())
                Array.Free();

            const float Time = (float)UGameplayStatics::GetTimeSeconds(GameState);

            for (float i = 0; i < SafeZoneCount; i++)
            {
                auto PhaseInfo = (FFortSafeZonePhaseInfo*)malloc(FFortSafeZonePhaseInfo::Size());
                memset((PBYTE)PhaseInfo, 0, FFortSafeZonePhaseInfo::Size());

                PhaseInfo->Radius = SafeZoneDefinition.Radius.Evaluate(i);
                PhaseInfo->WaitTime = SafeZoneDefinition.WaitTime.Evaluate(i);
                PhaseInfo->ShrinkTime = SafeZoneDefinition.ShrinkTime.Evaluate(i);
                PhaseInfo->PlayerCap = (int)SafeZoneDefinition.PlayerCapSolo.Evaluate(i);

                UDataTableFunctionLibrary::EvaluateCurveTableRow(GameState->AthenaGameDataTable, FName(L"Default.SafeZone.Damage"), i, nullptr, &PhaseInfo->DamageInfo.Damage, FString());
                if (i == 0.f)
                    PhaseInfo->DamageInfo.Damage = 0.01f;
                PhaseInfo->DamageInfo.bPercentageBasedDamage = true;
                PhaseInfo->TimeBetweenStormCapDamage = GameMode->TimeBetweenStormCapDamage.Evaluate(i);
                PhaseInfo->StormCapDamagePerTick = GameMode->StormCapDamagePerTick.Evaluate(i);
                PhaseInfo->StormCampingIncrementTimeAfterDelay = GameMode->StormCampingIncrementTimeAfterDelay.Evaluate(i);
                PhaseInfo->StormCampingInitialDelayTime = GameMode->StormCampingInitialDelayTime.Evaluate(i);
                PhaseInfo->MegaStormGridCellThickness = (int)SafeZoneDefinition.MegaStormGridCellThickness.Evaluate(i);

                if (FFortSafeZonePhaseInfo::HasUsePOIStormCenter())
                    PhaseInfo->UsePOIStormCenter = false;

                if (GameMode->SafeZoneLocations.GetData() && GameMode->SafeZoneLocations.Num() > i)
                    PhaseInfo->Center = GameMode->SafeZoneLocations.Get((int)i, FVector::Size());

                Array.Add(*PhaseInfo, FFortSafeZonePhaseInfo::Size());
                free(PhaseInfo);

                if (SafeZoneIndicator->HasPhaseCount())
                    SafeZoneIndicator->PhaseCount++;
            }

            SafeZoneIndicator->OnRep_PhaseCount();

            SafeZoneIndicator->SafeZoneStartShrinkTime = Time + Array[0].WaitTime;
            SafeZoneIndicator->SafeZoneFinishShrinkTime = SafeZoneIndicator->SafeZoneStartShrinkTime + Array[0].ShrinkTime;

            SafeZoneIndicator->CurrentPhase = 0;
            SafeZoneIndicator->OnRep_CurrentPhase();
        }

        GameMode->SafeZoneIndicator = SafeZoneIndicator;
        GameState->SafeZoneIndicator = SafeZoneIndicator;
        GameState->OnRep_SafeZoneIndicator();
    }

    return GameMode->SafeZoneIndicator;
}

void StartNewSafeZonePhase(AFortGameMode* GameMode, int NewSafeZonePhase, bool bInitial = false)
{
    auto GameState = (AFortGameStateAthena*)GameMode->GameState;
    float TimeSeconds = (float)UGameplayStatics::GetTimeSeconds(GameState);
    auto& Array = GameMode->SafeZoneIndicator->HasSafeZonePhases() ? GameMode->SafeZoneIndicator->SafeZonePhases : Phases;

    if (Array.IsValidIndex(NewSafeZonePhase))
    {
        if (Array.IsValidIndex(NewSafeZonePhase - 1))
        {
            auto& PreviousPhaseInfo = Array.Get(NewSafeZonePhase - 1, FFortSafeZonePhaseInfo::Size());

            GameMode->SafeZoneIndicator->PreviousCenter = PreviousPhaseInfo.Center;
            GameMode->SafeZoneIndicator->PreviousRadius = PreviousPhaseInfo.Radius;
        }

        auto& PhaseInfo = Array.Get(NewSafeZonePhase, FFortSafeZonePhaseInfo::Size());

        GameMode->SafeZoneIndicator->NextCenter = PhaseInfo.Center;
        GameMode->SafeZoneIndicator->NextRadius = PhaseInfo.Radius;
        GameMode->SafeZoneIndicator->NextMegaStormGridCellThickness = PhaseInfo.MegaStormGridCellThickness;

        if (Array.IsValidIndex(NewSafeZonePhase + 1))
        {
            auto& NextPhaseInfo = Array.Get(NewSafeZonePhase + 1, FFortSafeZonePhaseInfo::Size());

            GameMode->SafeZoneIndicator->FutureReplicator->NextNextCenter = NextPhaseInfo.Center;
            GameMode->SafeZoneIndicator->FutureReplicator->NextNextRadius = NextPhaseInfo.Radius;

            GameMode->SafeZoneIndicator->NextNextCenter = NextPhaseInfo.Center;
            GameMode->SafeZoneIndicator->NextNextRadius = NextPhaseInfo.Radius;
            GameMode->SafeZoneIndicator->NextNextMegaStormGridCellThickness = NextPhaseInfo.MegaStormGridCellThickness;
        }

        GameMode->SafeZoneIndicator->SafeZoneStartShrinkTime = LategameConfig::bLateGame && LategameConfig::bLateGameLongZone ? 676767.f : TimeSeconds + PhaseInfo.WaitTime;
        GameMode->SafeZoneIndicator->SafeZoneFinishShrinkTime = GameMode->SafeZoneIndicator->SafeZoneStartShrinkTime + PhaseInfo.ShrinkTime;

        GameMode->SafeZoneIndicator->CurrentDamageInfo = PhaseInfo.DamageInfo;
        GameMode->SafeZoneIndicator->OnRep_CurrentDamageInfo();

        GameMode->SafeZoneIndicator->CurrentPhase = NewSafeZonePhase;
        GameMode->SafeZoneIndicator->OnRep_CurrentPhase();

        GameMode->SafeZoneIndicator->OnSafeZonePhaseChanged.Process();

        auto& SafeZoneState = *(uint8_t*)(__int64(&GameMode->SafeZoneIndicator->FutureReplicator) - 0x4);
        SafeZoneState = 2;

        GameMode->SafeZoneIndicator->OnSafeZoneStateChange(2, false);
        if (GameMode->SafeZoneIndicator->HasSafezoneStateChangedDelegate())
            GameMode->SafeZoneIndicator->SafezoneStateChangedDelegate.Process(GameMode->SafeZoneIndicator, 2);

        if (!bInitial)
            for (auto& UncastedPlayer : GameMode->AlivePlayers)
            {
                auto PlayerController = (AFortPlayerControllerAthena*)UncastedPlayer;

                PlayerController->GetQuestManager(1)->SendStatEvent(PlayerController, EFortQuestObjectiveStatEvent::GetStormPhase(), 1, false);
            }
    }
}

void (*SpawnInitialSafeZoneOG)(AFortGameMode* GameMode);
void SpawnInitialSafeZone(AFortGameMode* GameMode)
{
    // return;
    GameMode->bSafeZoneActive = true;
    auto SafeZoneIndicator = SetupSafeZoneIndicator(GameMode);

    SafeZoneIndicator->OnSafeZonePhaseChanged.Bind(GameMode, FName(L"HandlePostSafeZonePhaseChanged"));
    GameMode->OnSafeZoneIndicatorSpawned.Process(SafeZoneIndicator);

    StartNewSafeZonePhase(GameMode, LategameConfig::bLateGame ? (LategameConfig::LateGameZone + (VersionInfo.FortniteVersion >= 24 ? 3 : 0)) : 1, true);

    // return SpawnInitialSafeZoneOG(GameMode);
}

void (*UpdateSafeZonesPhaseOG)(AFortGameMode* GameMode);
void UpdateSafeZonesPhase(AFortGameMode* GameMode)
{
    auto& Array = GameMode->SafeZoneIndicator && GameMode->SafeZoneIndicator->HasSafeZonePhases() ? GameMode->SafeZoneIndicator->SafeZonePhases : Phases;
    if (GameMode->bSafeZoneActive && UGameplayStatics::GetTimeSeconds(GameMode) >= GameMode->SafeZoneIndicator->SafeZoneFinishShrinkTime && !GameMode->bSafeZonePaused &&
        Array.IsValidIndex(GameMode->SafeZoneIndicator->CurrentPhase + 1))
        StartNewSafeZonePhase(GameMode, GameMode->SafeZoneIndicator->CurrentPhase + 1);

    return UpdateSafeZonesPhaseOG(GameMode);
}

void GetPhaseInfo(UObject* Context, FFrame& Stack, bool* Ret)
{
    auto& OutSafeZonePhase = Stack.StepCompiledInRef<FFortSafeZonePhaseInfo>();
    int32 InPhaseToGet;
    Stack.StepCompiledIn(&InPhaseToGet);
    Stack.IncrementCode();
    auto SafeZoneIndicator = (AFortSafeZoneIndicator*)Context;
    auto& Array = SafeZoneIndicator->HasSafeZonePhases() ? SafeZoneIndicator->SafeZonePhases : Phases;

    if (Array.IsValidIndex(InPhaseToGet))
    {
        OutSafeZonePhase = Array[InPhaseToGet];

        *Ret = true;
        return;
    }
    *Ret = false;
}

class AFortNavMesh : public AActor
{
public:
    UCLASS_COMMON_MEMBERS(AFortNavMesh);

    DEFINE_PROP(HotSpotManager, const UObject*);
};
void (*OnWorldInitDoneOG)(UNavigationSystem* NavSys, char Mode);
void OnWorldInitDone(UNavigationSystem* NavSys, char Mode)
{
    printf("OnWorldInitDone\n");
    /*NavSys->bAutoCreateNavigationData = true;
    NavSys->bAllowClientSideNavigation = true;
    NavSys->bSupportRebuilding = true;

    OnWorldInitDoneOG(NavSys, Mode);

    auto AllBounds = Utils::GetAll(FindClass("NavMeshBoundsVolume"));
    auto AllNavmeshes = Utils::GetAll<AFortNavMesh>();
    auto HotSpotMgr = TUObjectArray::FindFirstObject("FortAIHotSpotManager");

    //auto Test = (void(*)(UNavigationSystem*)) (ImageBase + 0x1F5C290);
    //Test(NavSys);

    NavSys->OnNavigationBoundsUpdated(AllBounds[0]);
    AllNavmeshes[0]->HotSpotManager = HotSpotMgr;
    //printf("NavGraphData: %llx, AllBounds.Num() = %d\n", NavSys->NavGraphData, AllBounds.Num());
    AllBounds.Free();
    AllNavmeshes.Free();*/
}

void AFortGameMode::FinishWorldInitialization(AFortGameMode* _this, AActor* WorldManager)
{
    auto GameMode = (AFortGameModeAthena*)_this;
    auto GameState = (AFortGameStateAthena*)GameMode->GameState;

    printf("[GameMode] FinishWorldInitialization\n");
    FinishWorldInitializationOG(_this, WorldManager);

    // CH5 (UE5.4+): ReadyToStartMatch is called natively and bypasses its ExecHook, so the
    // listen setup there never runs. FinishWorldInitialization is an address-detour hook that
    // DOES fire on CH5 (this Athena-specific function) -> bring the listen server up here, once.
    // Pre-5.4 is untouched (still handled by ReadyToStartMatch_).
    if (VersionInfo.EngineVersion >= 5.4)
    {
        static UWorld* listenDoneWorld = nullptr;
        auto CurWorld = UWorld::GetWorld();
        if (listenDoneWorld != CurWorld && GameMode && CurWorld && !CurWorld->NetDriver)
        {
            listenDoneWorld = CurWorld;

            auto Engine = UEngine::GetEngine();
            auto NetDriverName = FName(L"GameNetDriver");

            if (GameMode->HasbEnableReplicationGraph())
                GameMode->bEnableReplicationGraph = true;

            auto GetWorldContextFn = FindGetWorldContext();
            auto CreateNamedLocalFn = FindCreateNamedNetDriverLocal();

            printf("[Boron][FWI-Listen] World=%p Engine=%p GetWorldCtx=0x%llX CreateNamedNetDriver_Local=0x%llX\n",
                   (void*)CurWorld, (void*)Engine,
                   (unsigned long long)GetWorldContextFn, (unsigned long long)CreateNamedLocalFn);

            if (!GetWorldContextFn || !CreateNamedLocalFn)
            {
                printf("[Boron][FWI-Listen] aborted: finders missing for FN %.2f (GetWorldCtx=%d CreateNamedLocal=%d)\n",
                       VersionInfo.FortniteVersion, GetWorldContextFn != 0, CreateNamedLocalFn != 0);
                listenDoneWorld = nullptr;
            }
            else
            {
                void* WorldCtx = ((void* (*)(UEngine*, UWorld*))GetWorldContextFn)(Engine, CurWorld);
                printf("[Boron][FWI-Listen] WorldCtx=%p\n", WorldCtx);

                UNetDriver* NetDriver = nullptr;
                if ((uintptr_t)WorldCtx < 0x10000 || (uintptr_t)WorldCtx >= 0x7FFFFFFFFFFFull)
                    printf("[Boron][FWI-Listen] aborted: GetWorldContext returned a bad pointer -> FindGetWorldContext sig wrong for FN %.2f\n", VersionInfo.FortniteVersion);
                else
                {
                    // CreateNamedNetDriver_Local(Engine, Context, Name, Definition) builds + registers the
                    // driver into Context->ActiveNetDrivers (returns bool). Then read the driver back out:
                    // FWorldContext::ActiveNetDrivers @ 0x208 (TArray<FNamedNetDriver>); FNamedNetDriver{ UNetDriver* @ 0x0 } size 0x10.
                    ((char (*)(UEngine*, void*, FName, FName))CreateNamedLocalFn)(Engine, WorldCtx, NetDriverName, NetDriverName);

                    uint8_t* AndData = *(uint8_t**)((uint8_t*)WorldCtx + 0x208);
                    int32_t AndNum = *(int32_t*)((uint8_t*)WorldCtx + 0x210);
                    if (AndData)
                        for (int i = AndNum - 1; i >= 0 && !NetDriver; i--)
                            NetDriver = *(UNetDriver**)(AndData + (size_t)i * 0x10);

                    printf("[Boron][FWI-Listen] CreateNamedNetDriver_Local -> ActiveNetDrivers.Num=%d NetDriver=%p\n", AndNum, (void*)NetDriver);
                }

                CurWorld->NetDriver = NetDriver;

                if (!NetDriver)
                {
                    printf("[Boron][FWI-Listen] no NetDriver produced (WorldCtx=%p)\n", WorldCtx);
                    listenDoneWorld = nullptr;
                }
                else
                {
                    if (VersionInfo.FortniteVersion >= 20)
                        NetDriver->NetServerMaxTickRate = 30;

                    NetDriver->NetDriverName = NetDriverName;
                    NetDriver->World = CurWorld;

                    if (VersionInfo.EngineVersion >= 5.3 && FConfig::bEnableIris)
                        *(bool*)(__int64(&NetDriver->ReplicationDriver) + 0x11) = true;

                    for (int i = 0; i < CurWorld->LevelCollections.Num(); i++)
                    {
                        auto& LevelCollection = CurWorld->LevelCollections.Get(i, FLevelCollection::Size());
                        LevelCollection.NetDriver = NetDriver;
                    }

                    auto URL = (FURL*)malloc(FURL::Size());
                    memset((PBYTE)URL, 0, FURL::Size());
                    URL->Port = FConfig::Port;

                    auto InitListenFn = FindInitListen();
                    auto SetWorldFn = FindSetWorld();
                    printf("[Boron][FWI-Listen] InitListen=0x%llX SetWorld=0x%llX\n",
                           (unsigned long long)InitListenFn, (unsigned long long)SetWorldFn);

                    if (!InitListenFn || !SetWorldFn)
                        printf("[Boron][FWI-Listen] aborted: InitListen/SetWorld finder missing on FN %.2f\n", VersionInfo.FortniteVersion);
                    else
                    {
                        auto InitListen = (bool (*)(UNetDriver*, UWorld*, FURL*, bool, FString&))InitListenFn;
                        auto SetWorld = (void (*)(UNetDriver*, UWorld*))SetWorldFn;

                        SetWorld(NetDriver, CurWorld);
                        FString Err;
                        if (InitListen(NetDriver, CurWorld, URL, false, Err))
                        {
                            printf("[Boron][FWI-Listen] InitListen OK -- GameNetDriver listening on port %d\n", FConfig::Port);
                            SetWorld(NetDriver, CurWorld);
                        }
                        else
                            printf("[Boron][FWI-Listen] Failed to listen!\n");
                    }

                    free(URL);
                }
            }
        }
    }

    if (VersionInfo.EngineVersion >= 5.4 && GameState)
    {
        static UWorld* playlistDoneWorld = nullptr;
        auto PlaylistWorld = UWorld::GetWorld();
        if (playlistDoneWorld != PlaylistWorld && PlaylistWorld)
        {
            playlistDoneWorld = PlaylistWorld;

            printf("[Boron][Playlist] CH5 FWI: calling SetupPlaylist (GameState=%p World=%p)\n", (void*)GameState, (void*)PlaylistWorld);
            SetupPlaylist(GameMode, GameState);

            auto PawnClass = FindObject<UClass>(L"/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C");
            if (PawnClass && GameMode->HasDefaultPawnClass())
                GameMode->DefaultPawnClass = PawnClass;

            if (GameMode->HasbWorldIsReady())
                GameMode->bWorldIsReady = true;

            printf("[Boron][Playlist] CH5 FWI: DefaultPawnClass=%p bWorldIsReady=1 warmup-count deferred until first client\n", (void*)PawnClass);

            // CH5: generate the flight path so the real Battle Bus can spawn (MapInfo->FlightInfos starts
            // empty on CH5). No-op until FindInitializeFlightPath() returns a verified 31.41 address, in
            // which case StartAircraftPhase spawns the bus instead of using the drop-in fallback.
            auto InitFlightPathFn = FindInitializeFlightPath();
            printf("[Boron][Aircraft] CH5 FWI: InitializeFlightPath finder=0x%llX MapInfo=%p\n", (unsigned long long)InitFlightPathFn, (void*)(GameState->HasMapInfo() ? GameState->MapInfo : nullptr));
            if (InitFlightPathFn && GameState->HasMapInfo() && GameState->MapInfo)
            {
                auto GamePhaseLogic = UFortGameStateComponent_BattleRoyaleGamePhaseLogic::Get(GameState);
                if (GamePhaseLogic)
                {
                    ((void (*)(AFortAthenaMapInfo*, AFortGameStateAthena*, UFortGameStateComponent_BattleRoyaleGamePhaseLogic*, bool, double, float, float))InitFlightPathFn)(
                        GameState->MapInfo, GameState, GamePhaseLogic, false, 0.0, 0.f, 360.f);
                    GamePhaseLogic->InitializeSafeZoneLocations();
                    printf("[Boron][Aircraft] CH5 FWI: InitializeFlightPath + InitializeSafeZoneLocations done -> FlightInfos.Num=%d zones=%d\n",
                           GameState->MapInfo->FlightInfos.Num(),
                           UFortGameStateComponent_BattleRoyaleGamePhaseLogic::SafeZoneLocations.Num());
                }
            }
        }
    }
    if (VersionInfo.EngineVersion >= 4.22 && VersionInfo.EngineVersion < 4.26)
        GameState->OnRep_CurrentPlaylistInfo();

    auto AddToTierData = [&](const UDataTable* Table, TArray<FFortLootTierData*>& TempArr)
    {
        if (!Table)
            return;

        Table->AddToRoot();
        if (VersionInfo.FortniteVersion >= 20)
        {
            if (auto CompositeTable = Table->Cast<UCompositeDataTable>())
                for (auto& ParentTable : CompositeTable->ParentTables)
                    if (ParentTable)
                        for (auto& [Key, Val] : *(TMap<int32, FFortLootTierData*>*)(__int64(ParentTable) + 0x30))
                            TempArr.Add(Val);

            for (auto& [Key, Val] : *(TMap<int32, FFortLootTierData*>*)(__int64(Table) + 0x30))
            {
                bool bFound = false;

                for (auto& TierData : TempArr)
                    if (TierData->TierGroup == Val->TierGroup && TierData->LootPackage == Val->LootPackage)
                    {
                        TierData = Val;
                        bFound = true;
                        break;
                    }

                if (!bFound)
                    TempArr.Add(Val);
            }
        }
        else
        {
            if (auto CompositeTable = Table->Cast<UCompositeDataTable>())
                for (auto& ParentTable : CompositeTable->ParentTables)
                    if (ParentTable)
                        for (auto& [Key, Val] : (TMap<FName, FFortLootTierData*>)ParentTable->RowMap)
                            TempArr.Add(Val);

            for (auto& [Key, Val] : (TMap<FName, FFortLootTierData*>)Table->RowMap)
            {
                bool bFound = false;

                for (auto& TierData : TempArr)
                    if (TierData->TierGroup == Val->TierGroup && TierData->LootPackage == Val->LootPackage)
                    {
                        TierData = Val;
                        bFound = true;
                        break;
                    }

                if (!bFound)
                    TempArr.Add(Val);
            }
        }
    };

    auto AddToPackages = [&](const UDataTable* Table, std::unordered_map<int32, FFortLootPackageData*>& TempArr)
    {
        if (!Table)
            return;

        Table->AddToRoot();
        if (VersionInfo.FortniteVersion >= 20)
        {
            if (auto CompositeTable = Table->Cast<UCompositeDataTable>())
                for (auto& ParentTable : CompositeTable->ParentTables)
                    if (ParentTable)
                        for (auto& [Key, Val] : *(TMap<int32, FFortLootPackageData*>*)(__int64(ParentTable) + 0x30))
                            TempArr[Key] = Val;

            for (auto& [Key, Val] : *(TMap<int32, FFortLootPackageData*>*)(__int64(Table) + 0x30))
                TempArr[Key] = Val;
        }
        else
        {
            if (auto CompositeTable = Table->Cast<UCompositeDataTable>())
                for (auto& ParentTable : CompositeTable->ParentTables)
                    if (ParentTable)
                        for (auto& [Key, Val] : (TMap<FName, FFortLootPackageData*>)ParentTable->RowMap)
                            TempArr[Key.ComparisonIndex] = Val;

            for (auto& [Key, Val] : (TMap<FName, FFortLootPackageData*>)Table->RowMap)
            {
                TempArr[Key.ComparisonIndex] = Val;
            }
        }
    };

    auto Playlist = FindObject<UFortPlaylistAthena>(FConfig::Playlist);

    if (!Playlist)
        Playlist = FindObject<UFortPlaylistAthena>(L"/Game/Athena/Playlists/Playlist_DefaultSolo.Playlist_DefaultSolo");

    TArray<FFortLootTierData*> LootTierDataTempArr;
    auto LootTierData = Playlist ? Playlist->LootTierData.Get() : nullptr;
    if (!LootTierData)
        LootTierData = FindObject<UDataTable>(GameMode->HasWarmupRequiredPlayerCount() ? L"/Game/Items/Datatables/AthenaLootTierData_Client.AthenaLootTierData_Client"
                                                                                       : L"/Game/Items/Datatables/LootTierData_Client.LootTierData_Client");
    if (LootTierData)
        AddToTierData(LootTierData, LootTierDataTempArr);

    for (auto& Val : LootTierDataTempArr)
        TierDataMap[Val->TierGroup.ComparisonIndex].Add(Val);

    std::unordered_map<int32, FFortLootPackageData*> LootPackageTempArr;
    auto LootPackages = Playlist ? Playlist->LootPackages.Get() : nullptr;
    if (!LootPackages)
        LootPackages = FindObject<UDataTable>(GameMode->HasWarmupRequiredPlayerCount() ? L"/Game/Items/Datatables/AthenaLootPackages_Client.AthenaLootPackages_Client"
                                                                                       : L"/Game/Items/Datatables/LootPackages_Client.LootPackages_Client");
    if (LootPackages)
        AddToPackages(LootPackages, LootPackageTempArr);

    for (auto& [_, Val] : LootPackageTempArr)
        LootPackageMap[Val->LootPackageID.ComparisonIndex].Add(Val);

    auto GameFeatureDataClass = FindClass("FortGameFeatureData");
    if (GameFeatureDataClass)
        for (int i = 0; i < TUObjectArray::Num(); i++)
        {
            auto Object = TUObjectArray::GetObjectByIndex(i);

            if (!Object || !Object->Class || Object->IsDefaultObject())
                continue;

            if (Object->IsA(GameFeatureDataClass))
            {
                static auto DefaultLootTableDataOffset = Object->GetOffset("DefaultLootTableData");
                static auto PlaylistOverrideLootTableDataOffset = Object->GetOffset("PlaylistOverrideLootTableData");

                auto& LootTableData = GetFromOffset<FFortGameFeatureLootTableData>(Object, DefaultLootTableDataOffset);
                auto& LootTableDataUE53 = GetFromOffset<FFortGameFeatureLootTableData_UE53>(Object, DefaultLootTableDataOffset);
                auto& PlaylistOverrideLootTableData = GetFromOffset<TMap<FGameplayTag, FFortGameFeatureLootTableData>>(Object, PlaylistOverrideLootTableDataOffset);
                auto& PlaylistOverrideLootTableDataLWC = GetFromOffset<TMap<int32, FFortGameFeatureLootTableData>>(Object, PlaylistOverrideLootTableDataOffset);
                auto& PlaylistOverrideLootTableDataUE53 = GetFromOffset<TMap<int32, FFortGameFeatureLootTableData_UE53>>(Object, PlaylistOverrideLootTableDataOffset);
                auto LTDFeatureData = VersionInfo.EngineVersion >= 5.3 ? LootTableDataUE53.LootTierData.Get() : LootTableData.LootTierData.Get();
                auto LootPackageData = VersionInfo.EngineVersion >= 5.3 ? LootTableDataUE53.LootPackageData.Get() : LootTableData.LootPackageData.Get();

                if (LTDFeatureData)
                {
                    TArray<FFortLootTierData*> LTDTempData;

                    AddToTierData(LTDFeatureData, LTDTempData);

                    if (Playlist)
                    {
                        if (VersionInfo.EngineVersion >= 5.3)
                        {
                            /*for (auto& Tag : Playlist->GameplayTagContainer.GameplayTags)
                                for (auto& Override : PlaylistOverrideLootTableDataUE52)
                                    if (Tag.TagName.ComparisonIndex == Override.First)
                                        AddToTierData(Override.Second.LootTierData.Get(), LTDTempData);*/
                        }
                        else if (VersionInfo.FortniteVersion < 20.00)
                        {
                            for (auto& Tag : Playlist->GameplayTagContainer.GameplayTags)
                                for (auto& Override : PlaylistOverrideLootTableData)
                                    if (Tag.TagName == Override.First.TagName)
                                        AddToTierData(Override.Second.LootTierData.Get(), LTDTempData);
                        }
                        else
                            for (auto& Tag : Playlist->GameplayTagContainer.GameplayTags)
                                for (auto& Override : PlaylistOverrideLootTableDataLWC)
                                    if (Tag.TagName.ComparisonIndex == Override.First)
                                        AddToTierData(Override.Second.LootTierData.Get(), LTDTempData);
                    }

                    // for (auto& [_, Val] : LTDTempData)
                    //     TierDataAllGroups.Add(Val);

                    for (auto& Val : LTDTempData)
                        TierDataMap[Val->TierGroup.ComparisonIndex].Add(Val);
                }

                if (LootPackageData)
                {
                    std::unordered_map<int32, FFortLootPackageData*> LPTempData;

                    AddToPackages(LootPackageData, LPTempData);

                    if (Playlist)
                    {
                        if (VersionInfo.EngineVersion >= 5.3)
                        {
                            /*for (auto& Tag : Playlist->GameplayTagContainer.GameplayTags)
                                for (auto& Override : PlaylistOverrideLootTableDataUE52)
                                    if (Tag.TagName.ComparisonIndex == Override.First)
                                        AddToPackages(Override.Second.LootPackageData.Get(), LPTempData);*/
                        }
                        else if (VersionInfo.FortniteVersion < 20.00)
                        {
                            for (auto& Tag : Playlist->GameplayTagContainer.GameplayTags)
                                for (auto& Override : PlaylistOverrideLootTableData)
                                    if (Tag.TagName == Override.First.TagName)
                                        AddToPackages(Override.Second.LootPackageData.Get(), LPTempData);
                        }
                        else
                            for (auto& Tag : Playlist->GameplayTagContainer.GameplayTags)
                                for (auto& Override : PlaylistOverrideLootTableDataLWC)
                                    if (Tag.TagName.ComparisonIndex == Override.First)
                                        AddToPackages(Override.Second.LootPackageData.Get(), LPTempData);
                    }

                    for (auto& [_, Val] : LPTempData)
                        LootPackageMap[Val->LootPackageID.ComparisonIndex].Add(Val);
                }
            }
        }

    if (_this->HasOnPlaylistLootTablesAppliedDelegate())
    {
        *(bool*)(__int64(&_this->OnPlaylistLootTablesAppliedDelegate) + 0x10) = true;
        _this->OnPlaylistLootTablesAppliedDelegate.Process();
    }

    auto FloorLootWarmupC = FindObject<UClass>(L"/Game/Athena/Environments/Blueprints/Tiered_Athena_FloorLoot_Warmup.Tiered_Athena_FloorLoot_Warmup_C");
    auto FloorLoot01C = FindObject<UClass>(L"/Game/Athena/Environments/Blueprints/Tiered_Athena_FloorLoot_01.Tiered_Athena_FloorLoot_01_C");

    if (VersionInfo.EngineVersion >= 5.4)
        printf("[Boron][Loot] tables: TierGroups=%zu LootPackages=%zu | FloorLoot BP: Warmup=%p Floor01=%p (null BP => path wrong for 31.41)\n",
               TierDataMap.size(), LootPackageMap.size(), (void*)FloorLootWarmupC, (void*)FloorLoot01C);

    UFortLootPackage::SpawnFloorLootForContainer(FloorLootWarmupC);
    UFortLootPackage::SpawnFloorLootForContainer(FloorLoot01C);

    if (VersionInfo.EngineVersion >= 5.4)
    {
        bCH5FloorLootTickEnabled = true;
        printf("[Boron][Loot] CH5 periodic floor-loot rescan armed (Floor01C=%p at init)\n", (void*)FloorLoot01C);

        if (GameMode->GameState && GameMode->GameState->HasAllPlayerBuildableClassesIndexLookup())
            for (auto& [BClass, BHandle] : GameMode->GameState->AllPlayerBuildableClassesIndexLookup)
                AFortGameStateAthena::BuildingClassMap[BHandle] = BClass;

        printf("[Boron][Build] FWI BuildingClassMap=%zu entries\n", AFortGameStateAthena::BuildingClassMap.size());
    }

    if (VersionInfo.EngineVersion >= 5.4)
    {
        TArray<AFortPickupAthena*> Pickups;
        Utils::GetAll<AFortPickupAthena>(Pickups);
        printf("[Boron][Loot] after SpawnFloorLoot: %d pickups now in world\n", Pickups.Num());
        Pickups.Free();
    }

    TArray<ABGAConsumableSpawner*> ConsumableSpawners{};
    Utils::GetAll<ABGAConsumableSpawner>(ConsumableSpawners);

    for (auto& Spawner : ConsumableSpawners)
        UFortLootPackage::SpawnConsumableActor(Spawner);

    ConsumableSpawners.Free();

    if (AFortAthenaLivingWorldStaticPointProvider::StaticClass())
    {
        TArray<AFortAthenaLivingWorldStaticPointProvider*> Spawners;
        Utils::GetAll<AFortAthenaLivingWorldStaticPointProvider>(Spawners);
        UEAllocatedMap<FName, const UClass*> VehicleSpawnerMap = {
            { FName(L"Athena.Vehicle.SpawnLocation.Motorcycle.Dirtbike"),       FindObject<UClass>(L"/Dirtbike/Vehicle/Motorcycle_DirtBike_Vehicle.Motorcycle_DirtBike_Vehicle_C")                   },
            { FName(L"Athena.Vehicle.SpawnLocation.Motorcycle.Sportbike"),      FindObject<UClass>(L"/Sportbike/Vehicle/Motorcycle_Sport_Vehicle.Motorcycle_Sport_Vehicle_C")                        },
            { FName(L"Athena.Vehicle.SpawnLocation.Valet.BasicCar.Taxi"),       FindObject<UClass>(L"/Valet/TaxiCab/Valet_TaxiCab_Vehicle.Valet_TaxiCab_Vehicle_C")                                  },
            { FName(L"Athena.Vehicle.SpawnLocation.Valet.BasicCar.Modded"),     FindObject<UClass>(L"/ModdedBasicCar/Vehicle/Valet_BasicCar_Vehicle_SuperSedan.Valet_BasicCar_Vehicle_SuperSedan_C") },
            { FName(L"Athena.Vehicle.SpawnLocation.Valet.BasicTruck.Upgraded"), FindObject<UClass>(L"/Valet/BasicTruck/Valet_BasicTruck_Vehicle_Upgrade.Valet_BasicTruck_Vehicle_Upgrade_C")         },
            { FName(L"Athena.Vehicle.SpawnLocation.Valet.BigRig.Upgraded"),     FindObject<UClass>(L"/Valet/BigRig/Valet_BigRig_Vehicle_Upgrade.Valet_BigRig_Vehicle_Upgrade_C")                     },
            { FName(L"Athena.Vehicle.SpawnLocation.Valet.SportsCar.Upgraded"),  FindObject<UClass>(L"/Valet/SportsCar/Valet_SportsCar_Vehicle_Upgrade.Valet_SportsCar_Vehicle_Upgrade_C")            },
            { FName(L"Athena.Vehicle.SpawnLocation.Valet.BasicCar.Upgraded"),   FindObject<UClass>(L"/Valet/BasicCar/Valet_BasicCar_Vehicle_Upgrade.Valet_BasicCar_Vehicle_Upgrade_C")               }
        };

        for (auto& Spawner : Spawners)
        {
            const UClass* VehicleClass = nullptr;
            for (int i = 0; i < Spawner->FiltersTags.GameplayTags.Num(); i++)
            {
                auto& Tag = Spawner->FiltersTags.GameplayTags.Get(i, FGameplayTag::Size());

                if (VehicleSpawnerMap.contains(Tag.TagName))
                {
                    VehicleClass = VehicleSpawnerMap[Tag.TagName];
                    break;
                }
            }

            if (VehicleClass)
            {
                auto Vehicle = UWorld::SpawnActor<AFortAthenaVehicle>(VehicleClass, Spawner->K2_GetActorLocation(), Spawner->K2_GetActorRotation());

                if (auto Car = Vehicle->Cast<AFortDagwoodVehicle>())
                    Car->SetFuel(100.f);
                // printf("Spawned a %s\n", Spawner->Name.ToString().c_str());
            }
            else
            {
                for (auto& Tag : Spawner->FiltersTags.GameplayTags)
                    printf("Fix: Tag: %s\n", Tag.TagName.ToString().c_str());
            }
        }
        Spawners.Free();
    }
    // not an else here because they still use spawners for boats, and fully on s27
    if (VersionInfo.EngineVersion >= 4.23 && std::floor(VersionInfo.FortniteVersion) != 20 && std::floor(VersionInfo.FortniteVersion) != 21 &&
        std::floor(VersionInfo.FortniteVersion) != 22) // its auto on s20, s21, and s22
    {
        TArray<AFortAthenaVehicleSpawner*> Spawners{};
        Utils::GetAll<AFortAthenaVehicleSpawner>(Spawners);

        for (auto& Spawner : Spawners)
        {
            auto VehicleClass = Spawner->GetVehicleClass();

            if (Spawner->HasCachedFortVehicleItemDef() && (!Spawner->HasbForceSpawnAlways() || !Spawner->bForceSpawnAlways))
            {
                auto VehicleDef = Spawner->CachedFortVehicleItemDef;
                if (!VehicleDef)
                    continue;

                double Min = std::clamp(VehicleDef->VehicleMinSpawnPercent.Evaluate() * 0.01f, 0.0f, 1.0f);
                double Max = std::clamp(VehicleDef->VehicleMaxSpawnPercent.Evaluate() * 0.01f, 0.0f, 1.0f);

                auto SpawnPercent = Min + (Max - Min) * (rand() / (float)RAND_MAX);
                auto bShouldSpawn = (rand() / (float)RAND_MAX) <= SpawnPercent;

                if (!bShouldSpawn)
                    continue;
            }

            auto Vehicle = UWorld::SpawnActor<AFortAthenaVehicle>(Spawner->GetVehicleClass(), Spawner->K2_GetActorLocation(), Spawner->K2_GetActorRotation());

            if (auto Car = Vehicle->Cast<AFortDagwoodVehicle>())
                Car->SetFuel(100.f);
        }

        Spawners.Free();
    }

    if (VersionInfo.FortniteVersion > 3.4)
    {
        TArray<ABuildingItemCollectorActor*> Collectors{};
        Utils::GetAll<ABuildingItemCollectorActor>(Collectors);
        for (auto& CollectorActor : Collectors)
        {
            if (Sum > Weight)
            {
            PickNum:
                auto RandomNum = (float)rand() / (RAND_MAX / TotalWeight);

                int Rarity = 0;
                bool found = false;

                for (auto& Element : WeightMap)
                {
                    float Weight = Element.second;

                    if (Weight == 0)
                        continue;

                    if (RandomNum <= Weight)
                    {
                        Rarity = Element.first;

                        found = true;
                        break;
                    }

                    RandomNum -= Weight;
                }

                if (!found)
                    goto PickNum;

                if (Rarity == 0)
                {
                    CollectorActor->K2_DestroyActor();
                    continue;
                }

                int AttemptsToGetItem = 0;
                for (int i = 0; i < CollectorActor->ItemCollections.Num(); i++)
                {
                    if (AttemptsToGetItem > 10)
                    {
                        AttemptsToGetItem = 0;
                        goto PickNum;
                    }

                    auto& Collection = CollectorActor->ItemCollections.Get(i, FCollectorUnitInfo::Size());

                    if (Collection.bUseDefinedOutputItem)
                        continue;

                    TArray<FFortItemEntry*> LootDrops{};

                    UFortLootPackage::ChooseLootForContainer(LootDrops, CollectorActor->DefaultItemLootTierGroupName, Rarity);

                    if (Collection.OutputItemEntry.Num() > 0)
                    {
                        Collection.OutputItemEntry.ResetNum();
                        Collection.OutputItem = nullptr;
                    }

                    for (auto& LootDrop : LootDrops)
                    {
                        if (!Collection.OutputItem && AFortInventory::IsPrimaryQuickbar(LootDrop->ItemDefinition))
                            Collection.OutputItem = LootDrop->ItemDefinition;

                        Collection.OutputItemEntry.Add(*LootDrop, FFortItemEntry::Size());
                        free(LootDrop);
                    }

                    if (!Collection.OutputItem)
                    {
                        i--;
                        AttemptsToGetItem++;

                        continue;
                    }

                    AttemptsToGetItem = 0;
                }

                CollectorActor->StartingGoalLevel = Rarity;
            }
            else
                CollectorActor->K2_DestroyActor();
        }
        Collectors.Free();

        Hooking::ExecHook((UFunction*)FindObject<UFunction>(L"/Game/Athena/Items/Gameplay/VendingMachine/B_Athena_VendingMachine.B_Athena_VendingMachine_C:VendWobble__FinishedFunc"), VendWobble__FinishedFunc,
                          VendWobble__FinishedFuncOG);
    }
    // Hooking::ExecHook((UFunction*)FindObject<UFunction>(L"/Game/Athena/Items/Consumables/Parents/GA_Athena_MedConsumable_Parent.GA_Athena_MedConsumable_Parent_C:Triggered_4C02BFB04B18D9E79F84848FFE6D2C32"),
    // AFortPlayerPawnAthena::Athena_MedConsumable_Triggered, AFortPlayerPawnAthena::Athena_MedConsumable_TriggeredOG);
}

void PlayerCanRestart(UObject* Context, FFrame& Stack, bool* Ret)
{
    AFortPlayerControllerAthena* Controller;

    Stack.StepCompiledIn(&Controller);
    Stack.IncrementCode();

    *Ret = true;
}

void AFortGameMode::TickCH5FloorLoot()
{
    if (VersionInfo.EngineVersion < 5.4 || !bCH5FloorLootTickEnabled)
        return;

    static int tick = 0;
    if ((tick++ % 600) != 0)
        return;

    static std::vector<const UClass*> LootClasses;
    static std::unordered_set<unsigned long long> DoneLocs;
    static int wave = 0;
    wave++;

    static const UClass* Floor01C = nullptr;
    if (!Floor01C)
    {
        Floor01C = FindObject<UClass>(L"/Game/Athena/Environments/Blueprints/Tiered_Athena_FloorLoot_01.Tiered_Athena_FloorLoot_01_C");
        if (Floor01C)
        {
            LootClasses.push_back(Floor01C);
            printf("[Boron][Loot] wave %d: resolved Tiered_Athena_FloorLoot_01_C=%p\n", wave, (void*)Floor01C);
        }
    }

    if (wave <= 24 && (wave % 4) == 1)
    {
        static std::unordered_set<std::string> DiagSeen;

        for (int i = 0; i < TUObjectArray::Num(); i++)
        {
            auto Obj = TUObjectArray::GetObjectByIndex(i);

            if (!Obj || !Obj->IsA(UClass::StaticClass()))
                continue;

            auto nm = Obj->Name.ToString();

            if (!strstr(nm.c_str(), "FloorLoot") || strstr(nm.c_str(), "Warmup"))
            {
                if ((strstr(nm.c_str(), "ItemSpawn") || strstr(nm.c_str(), "LootSpawn") || strstr(nm.c_str(), "GameModePickup") || strstr(nm.c_str(), "Forager")) &&
                    DiagSeen.insert(std::string(nm.c_str())).second)
                    printf("[Boron][Loot] loot-ish class loaded: %s\n", nm.c_str());

                continue;
            }

            if (std::find(LootClasses.begin(), LootClasses.end(), (const UClass*)Obj) != LootClasses.end())
                continue;

            printf("[Boron][Loot] wave %d: new floor-loot class %s (%p)\n", wave, nm.c_str(), (void*)Obj);
            LootClasses.push_back((const UClass*)Obj);
        }

        TArray<ABGAConsumableSpawner*> CSpawners;
        Utils::GetAll<ABGAConsumableSpawner>(CSpawners);

        static int lastCs = -1;
        if (CSpawners.Num() != lastCs)
        {
            lastCs = CSpawners.Num();
            printf("[Boron][Loot] BGAConsumableSpawner count=%d\n", lastCs);
        }
        CSpawners.Free();
    }

    {
        TArray<ABuildingContainer*> AllContainers;
        Utils::GetAll<ABuildingContainer>(AllContainers);

        static std::unordered_set<const void*> SeenClasses;
        static std::unordered_set<const void*> AnchorClasses;
        for (auto& C : AllContainers)
        {
            if (!C || !C->Class)
                continue;

            if (SeenClasses.insert((const void*)C->Class).second)
            {
                auto cn = C->Class->Name.ToString();
                printf("[Boron][Loot] container class seen: %s (containers now=%d)\n", cn.c_str(), AllContainers.Num());

                if (strstr(cn.c_str(), "Tiered_Chest") || strstr(cn.c_str(), "Tiered_Ammo"))
                    AnchorClasses.insert((const void*)C->Class);
            }
        }

        static FName FloorTG{};
        static bool tgTried = false;
        if (!tgTried)
        {
            tgTried = true;
            if (auto WarmupCDO = (const ABuildingContainer*)DefaultObjImpl("Tiered_Athena_FloorLoot_Warmup_C"))
            {
                FloorTG = WarmupCDO->SearchLootTierGroup;
                printf("[Boron][Loot] synthetic tier group=%s idx=%d\n", FloorTG.ToString().c_str(), FloorTG.ComparisonIndex);
            }
            else
                printf("[Boron][Loot] synthetic tier group: warmup CDO not found\n");
        }

        static std::unordered_set<unsigned long long> SynthDone;
        int synth = 0;

        if (FloorTG.ComparisonIndex)
            for (auto& C : AllContainers)
            {
                if (synth >= 150)
                    break;

                if (!C || !AnchorClasses.count((const void*)C->Class))
                    continue;

                auto Loc = C->K2_GetActorLocation();
                auto qx = (unsigned long long)(long long)llround(Loc.X / 100.) & 0x1FFFFF;
                auto qy = (unsigned long long)(long long)llround(Loc.Y / 100.) & 0x1FFFFF;
                auto qz = (unsigned long long)(long long)llround(Loc.Z / 100.) & 0x1FFFFF;
                if (!SynthDone.insert((qx << 42) | (qy << 21) | qz).second)
                    continue;

                auto ang = (float)rand() * 0.00019175345f;
                Loc.X += cosf(ang) * 250.f;
                Loc.Y += sinf(ang) * 250.f;
                Loc.Z += 40.f;

                auto TG = FloorTG;
                UFortLootPackage::SpawnLoot(TG, Loc);
                synth++;
            }

        if (synth)
            printf("[Boron][Loot] synthetic floor loot: +%d (total anchors done=%zu)\n", synth, SynthDone.size());

        AllContainers.Free();
    }

    for (auto Cls : LootClasses)
    {
        TArray<ABuildingContainer*> Containers;
        Utils::GetAll<ABuildingContainer>(Cls, Containers);

        int fresh = 0;
        for (auto& Container : Containers)
        {
            if (!Container)
                continue;

            auto Loc = Container->K2_GetActorLocation();
            auto qx = (unsigned long long)(long long)llround(Loc.X / 100.) & 0x1FFFFF;
            auto qy = (unsigned long long)(long long)llround(Loc.Y / 100.) & 0x1FFFFF;
            auto qz = (unsigned long long)(long long)llround(Loc.Z / 100.) & 0x1FFFFF;
            auto Key = (qx << 42) | (qy << 21) | qz;

            if (!DoneLocs.insert(Key).second)
                continue;

            Container->K2_DestroyActor();
            fresh++;
        }
        Containers.Free();

        if (fresh)
            printf("[Boron][Loot] wave %d: class=%s newContainers=%d totalDone=%zu\n",
                   wave, Cls->Name.ToString().c_str(), fresh, DoneLocs.size());
    }
}

void AFortGameMode::TickCH5PickupDummies()
{
    if (VersionInfo.EngineVersion < 5.4 || !bCH5FloorLootTickEnabled)
        return;

    static int tick = 0;
    if ((tick++ % 300) != 150)
        return;

    TArray<AFortPickupAthena*> Pickups;
    Utils::GetAll<AFortPickupAthena>(Pickups);

    int fixed = 0;
    AFortPickupAthena* firstFixed = nullptr;
    for (auto& P : Pickups)
    {
        if (!P || !P->HasPrimaryPickupDummyItem() || P->PrimaryPickupDummyItem)
            continue;

        auto Def = (UFortItemDefinition*)P->PrimaryPickupItemEntry.ItemDefinition;
        if (!Def)
            continue;

        auto Dummy = (UFortWorldItem*)Def->CreateTemporaryItemInstanceBP(P->PrimaryPickupItemEntry.Count, P->PrimaryPickupItemEntry.Level);
        if (!Dummy)
            continue;

        Dummy->ItemEntry.LoadedAmmo = P->PrimaryPickupItemEntry.LoadedAmmo;
        P->PrimaryPickupDummyItem = (UObject*)Dummy;
        if (!firstFixed)
            firstFixed = P;
        fixed++;
    }

    static int logged = 0;
    if (fixed && logged++ < 25)
        printf("[Boron][Pickup] dummy fixup: %d dummies created, sample dummy now=%p\n",
               fixed, firstFixed ? (void*)firstFixed->PrimaryPickupDummyItem : nullptr);

    Pickups.Free();
}

void AFortGameMode::Hook()
{
    Hooking::ExecHook(GetDefaultObj()->GetFunction("ReadyToStartMatch"), ReadyToStartMatch_, ReadyToStartMatch_OG);
    Hooking::Hook(FindFinishWorldInitialization(), FinishWorldInitialization, FinishWorldInitializationOG);
    // if (VersionInfo.EngineVersion == 4.16)
    //     Hooking::Hook(Memcury::Scanner::FindPattern("40 55 53 56 41 56 48 8D 6C 24 ? 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 45 ? 48 8B 01 48 8B F1").Get(),
    //     OnWorldInitDone, OnWorldInitDoneOG);
}

void AFortGameMode::PostLoadHook()
{
    ApplyCharacterCustomization = FindApplyCharacterCustomization();
    NotifyGameMemberAdded_ = FindNotifyGameMemberAdded();

    if (VersionInfo.EngineVersion >= 5.4)
        printf("[Boron][Cosmetics] ApplyCharacterCustomization=0x%llX NotifyGameMemberAdded=0x%llX\n",
               (unsigned long long)ApplyCharacterCustomization, (unsigned long long)NotifyGameMemberAdded_);

    auto spdf = GetDefaultObj()->GetFunction("SpawnDefaultPawnFor");
    SpawnDefaultPawnForIdx = spdf->GetVTableIndex();

    if (VersionInfo.EngineVersion >= 5.4)
    {
        auto AthenaCDO = AFortGameModeAthena::GetDefaultObj();
        auto SpawnPawnAddr = (AthenaCDO && SpawnDefaultPawnForIdx != (uint32_t)-1) ? (uintptr_t)AthenaCDO->Vft[SpawnDefaultPawnForIdx] : 0;
        printf("[Boron][Pawn] SpawnDefaultPawnFor: idx=%u athenaVftAddr=0x%llX (exec bypassed on 5.4+, using address hook)\n",
               SpawnDefaultPawnForIdx, (unsigned long long)SpawnPawnAddr);
        if (SpawnPawnAddr)
            Hooking::Hook(SpawnPawnAddr, SpawnDefaultPawnFor_Native, SpawnDefaultPawnFor_NativeOG);
    }
    else
        Hooking::ExecHook(spdf, SpawnDefaultPawnFor);

    auto hsnp = GetDefaultObj()->GetFunction("HandleStartingNewPlayer");
    if (VersionInfo.EngineVersion >= 5.4)
    {
        auto AthenaCDO = AFortGameModeAthena::GetDefaultObj();
        auto hsnpIdx = hsnp->GetVTableIndex();
        auto HsnpAddr = (AthenaCDO && hsnpIdx != (uint32_t)-1) ? (uintptr_t)AthenaCDO->Vft[hsnpIdx] : 0;
        printf("[Boron][Pawn] HandleStartingNewPlayer: idx=%u athenaVftAddr=0x%llX (address hook)\n",
               hsnpIdx, (unsigned long long)HsnpAddr);
        if (HsnpAddr)
            Hooking::Hook(HsnpAddr, HandleStartingNewPlayer_Native, HandleStartingNewPlayer_NativeOG);
    }
    else
        Hooking::ExecHook(hsnp, HandleStartingNewPlayer_, HandleStartingNewPlayer_OG);
    Hooking::Hook(FindPickTeam(), PickTeam, PickTeamOG);
    if (VersionInfo.FortniteVersion < 25.20)
    {
        Hooking::Hook(FindStartAircraftPhase(), StartAircraftPhase, StartAircraftPhaseOG);
        Hooking::Hook(FindHandlePostSafeZonePhaseChanged(), HandlePostSafeZonePhaseChanged, HandlePostSafeZonePhaseChangedOG);
    }
    Hooking::ExecHook(AFortGameModeAthena::GetDefaultObj()->GetFunction("OnAircraftExitedDropZone"), OnAircraftExitedDropZone_, OnAircraftExitedDropZone_OG);

    if (VersionInfo.FortniteVersion >= 21.10)
    {
        if (VersionInfo.FortniteVersion < 25.20)
        {
            Hooking::Hook(FindSpawnInitialSafeZone(), SpawnInitialSafeZone, SpawnInitialSafeZoneOG);
            Hooking::Hook(FindUpdateSafeZonesPhase(), UpdateSafeZonesPhase, UpdateSafeZonesPhaseOG);
        }
        Hooking::ExecHook((UFunction*)FindObject<UFunction>(L"/Script/FortniteGame.FortSafeZoneIndicator.GetPhaseInfo"), GetPhaseInfo);
    }

    // if (VersionInfo.FortniteVersion >= 15)
    //     Hooking::ExecHook(AFortGameModeAthena::GetDefaultObj()->GetFunction("PlayerCanRestart"), PlayerCanRestart);
}
