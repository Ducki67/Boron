#pragma once

#include "pch.h"
#include <cmath>
#include <cstdlib>
#include <unordered_map>
#include <vector>
#include "Configuration.h"
#include "Utils.h"
#include "../../FortniteGame/Public/FortPlayerPawnAthena.h"
#include "../../FortniteGame/Public/FortPlayerControllerAthena.h"
#include "../../FortniteGame/Public/FortInventory.h"
#include "../../FortniteGame/Public/FortGameMode.h"

namespace BossAI
{
    struct BotState
    {
        bool seen = false;
        bool firing = false;
        uint32 fireTimer = 0;
        bool dropped = false;
        FVector lastLoc{};
        UFortItemDefinition* lastWeaponDef = nullptr;
        float patrolYaw = -1.f;
        uint32 patrolTimer = 0;
        bool gaveWeapon = false;
        int weaponTries = 0;
        bool targeting = false;
        bool boss = false;
        AFortInventory* inv = nullptr;
        std::vector<UFortItemDefinition*> dropDefs;
        std::vector<int> dropCounts;
        float aimX = 0.f;
        float aimY = 0.f;
        float aimZ = 0.f;
    };

    // Curated loot pools, built once from the object array by name (no offsets/RE).
    // Guns  = varied real weapons for henchmen; Mythics = boss weapons; KeyCard = vault key.
    struct LootPools
    {
        bool built = false;
        std::vector<UFortWorldItemDefinition*> Guns;
        std::vector<UFortWorldItemDefinition*> Startup;
        std::vector<UFortWorldItemDefinition*> Mythics;
        UFortItemDefinition* KeyCard = nullptr;
    };

    inline LootPools& Loot()
    {
        static LootPools L;
        return L;
    }

    inline void BuildLoot()
    {
        auto& L = Loot();
        if (L.built)
            return;
        L.built = true;

        for (int i = 0; i < TUObjectArray::Num(); i++)
        {
            auto Obj = TUObjectArray::GetObjectByIndex(i);
            if (!Obj || !Obj->Class || Obj->IsDefaultObject())
                continue;

            auto Path = UKismetSystemLibrary::GetPathName(Obj).ToString();

            if (!L.KeyCard && Obj->IsA<UFortItemDefinition>() &&
                (Path.find("KeyCard") != std::string::npos || Path.find("Keycard") != std::string::npos || Path.find("Key_Card") != std::string::npos))
                L.KeyCard = (UFortItemDefinition*)Obj;

            if (!Obj->IsA<UFortWeaponRangedItemDefinition>())
                continue;
            if (Path.find("/Athena/") == std::string::npos)
                continue;
            if (Path.find("Grenade") != std::string::npos || Path.find("Consumable") != std::string::npos ||
                Path.find("Athena_C_") != std::string::npos || Path.find("_Ammo") != std::string::npos)
                continue;

            auto Item = (UFortWorldItemDefinition*)Obj;
            if (Path.find("Mythic") != std::string::npos)
            {
                L.Mythics.push_back(Item);
                continue;
            }
            if (Path.find("WID_Assault") != std::string::npos || Path.find("WID_Shotgun") != std::string::npos ||
                Path.find("WID_PDW") != std::string::npos || Path.find("WID_SMG") != std::string::npos ||
                Path.find("WID_Pistol") != std::string::npos || Path.find("WID_Sniper") != std::string::npos)
                L.Guns.push_back(Item);
        }

        printf("[BossAI] Loot pools: %d guns, %d mythics, keycard=%s\n", (int)L.Guns.size(), (int)L.Mythics.size(), L.KeyCard ? "yes" : "no");
    }

    inline bool IsValidPtr(const void* P)
    {
        auto V = (uintptr_t)P;
        return V >= 0x10000 && V < 0x7FFFFFFFFFFFull;
    }

    inline bool IsLiveActor(AActor* Actor)
    {
        if (!IsValidPtr(Actor) || Actor->bActorIsBeingDestroyed)
            return false;

        auto Item = TUObjectArray::GetItemByIndex(Actor->Index);
        if (!Item || (Item->Flags & ((1 << 29) | (1 << 21))))
            return false;

        return true;
    }

    inline void GiveAndEquip(AFortInventory* Inv, AFortPlayerPawnAthena* Bot, UFortWorldItemDefinition* Def)
    {
        if (!IsLiveActor((AActor*)Inv) || !IsLiveActor((AActor*)Bot) || !IsValidPtr(Def))
            return;
        auto Stats = AFortInventory::GetStats((UFortWeaponItemDefinition*)Def);
        int Clip = (Stats && Stats->ClipSize > 0) ? Stats->ClipSize : 30;
        if (auto Ammo = Def->GetAmmoWorldItemDefinition_BP())
            if ((UFortWorldItemDefinition*)Ammo != Def)
                Inv->GiveItem(Ammo, 999);
        if (auto WItem = Inv->GiveItem(Def, 1, Clip))
            Bot->EquipWeaponDefinition((UFortWeaponItemDefinition*)Def, WItem->ItemEntry.ItemGuid);
    }

    inline std::unordered_map<void*, BotState>& States()
    {
        static std::unordered_map<void*, BotState> Map;
        return Map;
    }

    inline float RandRange(float Min, float Max)
    {
        return Min + ((float)rand() / (float)RAND_MAX) * (Max - Min);
    }

    inline float ModLocation(float Base, float Sep, float Max)
    {
        auto Num = RandRange(0.f, 3.f);

        if (Num < 1.f)
            return RandRange(-Base, Base);

        if (Num < 2.f)
            return RandRange(Base + Sep, Max);

        return RandRange(-Max, -Base - Sep);
    }

    inline std::vector<AFortPlayerPawnAthena*>& Live()
    {
        static std::vector<AFortPlayerPawnAthena*> Pawns;
        return Pawns;
    }

    inline bool SafeProcessEvent(UObject* Obj, UFunction* Fn, void* Params)
    {
        __try
        {
            Obj->ProcessEvent(Fn, Params);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    inline UObject* FindCDO(UClass* Cls)
    {
        if (!Cls)
            return nullptr;
        for (int i = 0; i < TUObjectArray::Num(); i++)
        {
            auto Obj = TUObjectArray::GetObjectByIndex(i);
            if (Obj && Obj->IsDefaultObject() && Obj->Class == Cls)
                return (UObject*)Obj;
        }
        return nullptr;
    }

    inline UFortItemDefinition* DroppableVersion(UFortItemDefinition* Def)
    {
        if (!Def)
            return Def;
        auto nm = Def->Name.ToString();
        auto pos = nm.find("_NPC");
        if (pos == std::string::npos)
            return Def;

        auto want = nm.substr(0, pos);
        for (int i = 0; i < TUObjectArray::Num(); i++)
        {
            auto Obj = TUObjectArray::GetObjectByIndex(i);
            if (Obj && Obj->Class && !Obj->IsDefaultObject() && Obj->IsA<UFortItemDefinition>() && Obj->Name.ToString() == want)
                return (UFortItemDefinition*)Obj;
        }
        return Def;
    }

    inline bool GiveStartupInventory(AFortPlayerPawnAthena* Bot, AFortInventory* Inv)
    {
        if (!Bot || !Bot->Controller || !Inv)
            return false;

        static auto StartupOff = Bot->Controller->GetOffset("StartupInventory");
        if (StartupOff == -1)
            return false;

        auto Startup = GetFromOffset<UObject*>(Bot->Controller, StartupOff);
        if (!IsValidPtr(Startup))
            return false;

        static auto ItemsOff = Startup->GetOffset("Items");
        if (ItemsOff == -1)
            return false;

        auto& Items = GetFromOffset<TArray<FItemAndCount>>(Startup, ItemsOff);
        if (Items.Num() == 0)
            return false;

        bool equipped = false;
        int given = 0;
        for (int i = 0; i < Items.Num(); i++)
        {
            auto& IC = Items.Get(i, FItemAndCount::Size());
            auto Def = IC.GetItem();
            if (!IsValidPtr(Def))
                continue;

            int cnt = IC.GetCount();
            if (!equipped && Def->Cast<UFortWeaponRangedItemDefinition>())
            {
                GiveAndEquip(Inv, Bot, (UFortWorldItemDefinition*)Def);
                equipped = true;
            }
            else
            {
                Inv->GiveItem(Def, cnt > 0 ? cnt : 1);
            }
            given++;

            static int silog = 0;
            if (silog++ < 40)
                printf("[Boron][Bots] startup item bot=%p [%s] x%d\n", (void*)Bot, Def->Name.ToString().c_str(), cnt);
        }

        static int slog = 0;
        if (slog++ < 12)
            printf("[Boron][Bots] startup loadout bot=%p items=%d equipped=%d\n", (void*)Bot, given, (int)equipped);

        return given > 0;
    }

    inline void ApplyBotLoadout(AFortPlayerPawnAthena* Bot, UObject* SpawnerCDO)
    {
        if (!Bot || !Bot->Controller || !SpawnerCDO)
            return;

        static auto InvCompOff = SpawnerCDO->GetOffset("InventoryComponent");
        if (InvCompOff == -1)
            return;

        auto InvCompClass = GetFromOffset<UClass*>(SpawnerCDO, InvCompOff);
        auto InvCompCDO = FindCDO(InvCompClass);
        if (!InvCompCDO)
            return;

        static auto ItemsOff = InvCompCDO->GetOffset("Items");
        if (ItemsOff == -1)
            return;

        auto& Items = GetFromOffset<TArray<FItemAndCount>>(InvCompCDO, ItemsOff);
        if (Items.Num() == 0)
            return;

        static auto InvOff = Bot->Controller->GetOffset("Inventory");
        AFortInventory* Inv = (InvOff != -1) ? GetFromOffset<AFortInventory*>(Bot->Controller, InvOff) : nullptr;
        if (!Inv && InvOff != -1)
        {
            Inv = UWorld::SpawnActor<AFortInventory>(AFortInventory::StaticClass(), FVector{ 0, 0, -99999 }, FRotator{}, Bot->Controller);
            if (Inv)
            {
                Inv->InventoryType = 0;
                if (auto OnRepOwnerFn = Inv->GetFunction("OnRep_Owner"))
                    Inv->ProcessEvent(OnRepOwnerFn, nullptr);
                GetFromOffset<AFortInventory*>(Bot->Controller, InvOff) = Inv;
            }
        }
        if (!Inv)
            return;

        bool equipped = false;
        int given = 0;
        for (int i = 0; i < Items.Num(); i++)
        {
            auto& IC = Items.Get(i, FItemAndCount::Size());
            auto Def = IC.GetItem();
            if (!Def)
                continue;

            int cnt = IC.GetCount();
            if (!equipped && Def->Cast<UFortWeaponRangedItemDefinition>())
            {
                GiveAndEquip(Inv, Bot, (UFortWorldItemDefinition*)Def);
                equipped = true;
            }
            else
            {
                Inv->GiveItem(Def, cnt > 0 ? cnt : 1);
            }
            given++;

            static int ilog = 0;
            if (ilog++ < 40)
                printf("[Boron][Bots] loadout item bot=%p [%s] x%d\n", (void*)Bot, Def->Name.ToString().c_str(), cnt);
        }

        static int llog = 0;
        if (llog++ < 12)
            printf("[Boron][Bots] loadout bot=%p items=%d equipped=%d\n", (void*)Bot, given, (int)equipped);
    }

    inline void ApplyBotCosmetics(AFortPlayerPawnAthena* Bot, UObject* SpawnerCDO)
    {
        if (!Bot || !Bot->Controller)
            return;

        UAthenaCharacterItemDefinition* CID = nullptr;

        static auto LoadoutOff = Bot->Controller->GetOffset("CosmeticLoadoutBC");
        if (LoadoutOff != -1)
            CID = *(UAthenaCharacterItemDefinition**)((char*)Bot->Controller + LoadoutOff + 0x40);

        if (!IsValidPtr(CID) && SpawnerCDO)
        {
            static auto CosCompOff = SpawnerCDO->GetOffset("CosmeticComponent");
            if (CosCompOff != -1)
            {
                auto CosCompClass = GetFromOffset<UClass*>(SpawnerCDO, CosCompOff);
                auto CosCompCDO = FindCDO(CosCompClass);
                auto LoadoutClass = FindClass("FortAthenaAISpawnerDataComponent_CosmeticLoadout");
                if (CosCompCDO && LoadoutClass && CosCompCDO->IsA(LoadoutClass))
                    CID = *(UAthenaCharacterItemDefinition**)((char*)CosCompCDO + 0x28 + 0x40);
            }
        }

        if (!IsValidPtr(CID))
        {
            static int cn = 0;
            if (cn++ < 8)
                printf("[Boron][Bots] cosmetics bot=%p no character\n", (void*)Bot);
            return;
        }

        static auto ChoosePartFn = Bot->GetFunction("ServerChoosePart");
        if (!ChoosePartFn)
            return;

        struct { uint8_t Part; uint8_t pad[7]; const UObject* ChosenCharacterPart; } cp{};
        int applied = 0;

        auto Choose = [&](const UCustomCharacterPart* Part)
        {
            if (!Part || !Part->HasCharacterPartType() || Part->CharacterPartType >= 7)
                return;
            cp.Part = Part->CharacterPartType;
            cp.ChosenCharacterPart = Part;
            Bot->ProcessEvent(ChoosePartFn, &cp);
            applied++;
        };

        if (CID->HasBaseCharacterParts())
            for (auto& PartSoft : CID->BaseCharacterParts)
                Choose(PartSoft.Get());

        if (auto Hero = CID->GetHeroDefinition())
            for (auto& SpecSoft : Hero->Specializations)
                if (auto Spec = (UFortHeroSpecialization*)SpecSoft.Get())
                    for (auto& PartSoft : Spec->CharacterParts)
                        Choose(PartSoft.Get());

        static int clog = 0;
        if (clog++ < 12)
            printf("[Boron][Bots] cosmetics bot=%p character=%s parts=%d\n", (void*)Bot,
                   CID->Name.ToString().c_str(), applied);
    }

    inline void SpawnAIOnPaths(const UClass* SpawnerClass, const std::string& Name, bool bAllPaths)
    {
        if (!SpawnerClass)
            return;

        auto World = UWorld::GetWorld();
        auto GameMode = World ? (AFortGameModeAthena*)World->AuthorityGameMode : nullptr;
        auto BotManager = GameMode ? GameMode->ServerBotManager : nullptr;
        if (!BotManager)
            return;

        auto PathClass = FindClass("FortAthenaPatrolPath");
        if (!PathClass)
            return;

        TArray<AActor*> Found;
        Utils::GetAll<AActor>(PathClass, Found);

        std::vector<AActor*> Matches;
        for (int i = 0; i < Found.Num(); i++)
        {
            auto Path = Found[i];
            if (!Path)
                continue;

            static auto TagsOffset = Path->GetOffset("GameplayTags");
            if (TagsOffset == -1)
                continue;

            auto& Tags = GetFromOffset<FGameplayTagContainer>(Path, TagsOffset).GameplayTags;
            if (Tags.Num() == 0)
                continue;

            auto TagName = Tags.Get(0, FGameplayTag::Size()).TagName.ToString();
            if (TagName.length() >= Name.length() && TagName.compare(TagName.length() - Name.length(), Name.length(), Name) == 0)
                Matches.push_back(Path);
        }
        Found.Free();

        if (Matches.empty())
            return;

        UObject* SpawnerCDO = nullptr;
        for (int i = 0; i < TUObjectArray::Num(); i++)
        {
            auto Obj = TUObjectArray::GetObjectByIndex(i);
            if (Obj && Obj->IsDefaultObject() && Obj->Class == SpawnerClass)
            {
                SpawnerCDO = (UObject*)Obj;
                break;
            }
        }

        auto CreateFn = SpawnerCDO ? SpawnerCDO->GetFunction("CreateComponentListFromClass") : nullptr;
        auto SpawnFn = BotManager->GetFunction("SpawnAI");
        if (!CreateFn || !SpawnFn)
        {
            printf("[Boron][Bots] spawn %s aborted: cdo=%p CreateComponentListFromClass=%p SpawnAI=%p\n", Name.c_str(), (void*)SpawnerCDO, (void*)CreateFn, (void*)SpawnFn);
            return;
        }

        struct { UClass* SpawnerDataClass; UObject* OuterObject; UObject* ReturnValue; } CreateParams{ (UClass*)SpawnerClass, (UObject*)World, nullptr };
        if (!SafeProcessEvent(SpawnerCDO, CreateFn, &CreateParams))
        {
            printf("[Boron][Bots] spawn %s aborted: CreateComponentListFromClass faulted\n", Name.c_str());
            return;
        }
        if (!CreateParams.ReturnValue)
        {
            printf("[Boron][Bots] spawn %s aborted: null component list\n", Name.c_str());
            return;
        }

        if (bAllPaths)
        {
            for (size_t i = Matches.size() - 1; i > 0; i--)
            {
                size_t j = (size_t)(rand() % (int)(i + 1));
                auto Temp = Matches[i];
                Matches[i] = Matches[j];
                Matches[j] = Temp;
            }
        }
        else
        {
            auto Pick = Matches[rand() % Matches.size()];
            Matches.clear();
            Matches.push_back(Pick);
        }

        int spawned = 0;
        for (size_t i = 0; i < Matches.size(); i++)
        {
            static auto PointsOffset = Matches[i]->GetOffset("PatrolPoints");
            if (PointsOffset == -1)
                break;

            auto& Points = GetFromOffset<TArray<AActor*>>(Matches[i], PointsOffset);
            if (Points.Num() == 0 || !Points[0])
                continue;

            auto LocV = Points[0]->K2_GetActorLocation();
            auto RotV = Points[0]->K2_GetActorRotation();

            int vsz = FVector::Size();
            int rsz = FRotator::Size();
            int listOff = vsz + rsz;
            int retOff = listOff + 8;

            uint8 Parms[96] = {};
            if (vsz == 0xc)
            {
                *(float*)(Parms + 0x0) = (float)LocV.X;
                *(float*)(Parms + 0x4) = (float)LocV.Y;
                *(float*)(Parms + 0x8) = (float)LocV.Z;
                *(float*)(Parms + 0xc + 0x0) = (float)RotV.Pitch;
                *(float*)(Parms + 0xc + 0x4) = (float)RotV.Yaw;
                *(float*)(Parms + 0xc + 0x8) = (float)RotV.Roll;
            }
            else
            {
                *(double*)(Parms + 0x0) = (double)LocV.X;
                *(double*)(Parms + 0x8) = (double)LocV.Y;
                *(double*)(Parms + 0x10) = (double)LocV.Z;
                *(double*)(Parms + 0x18 + 0x0) = (double)RotV.Pitch;
                *(double*)(Parms + 0x18 + 0x8) = (double)RotV.Yaw;
                *(double*)(Parms + 0x18 + 0x10) = (double)RotV.Roll;
            }
            *(UObject**)(Parms + listOff) = CreateParams.ReturnValue;

            bool ok = SafeProcessEvent((UObject*)BotManager, SpawnFn, Parms);
            AActor* spawnedPawn = *(AActor**)(Parms + retOff);
            if (ok && spawnedPawn)
            {
                spawned++;
                ApplyBotLoadout((AFortPlayerPawnAthena*)spawnedPawn, SpawnerCDO);
                ApplyBotCosmetics((AFortPlayerPawnAthena*)spawnedPawn, SpawnerCDO);
            }

            static int slog = 0;
            if (slog++ < 8)
                printf("[Boron][Bots] SpawnAI %s path=%zu ok=%d pawn=%p list=%p vsz=%d loc=(%.0f,%.0f,%.0f)\n",
                       Name.c_str(), i, (int)ok, (void*)spawnedPawn, (void*)CreateParams.ReturnValue, vsz,
                       LocV.X, LocV.Y, LocV.Z);
        }

        printf("[Boron][Bots] spawn %s: %d/%d paths\n", Name.c_str(), spawned, (int)Matches.size());
    }

    inline void RequestAISpawn()
    {
        if (VersionInfo.FortniteVersion < 14.0 || VersionInfo.FortniteVersion >= 15.0)
            return;

        {
            auto World = UWorld::GetWorld();
            auto GameMode = World ? (AFortGameModeAthena*)World->AuthorityGameMode : nullptr;
            auto GameState = World ? (AFortGameStateAthena*)World->GameState : nullptr;
            void* Nav = World ? (void*)World->NavigationSystem : nullptr;

            void* AISys = nullptr;
            if (World)
            {
                static auto AISysOffset = World->GetOffset("AISystem");
                if (AISysOffset != -1)
                    AISys = GetFromOffset<void*>(World, AISysOffset);
            }

            int phase = -1;
            if (GameState)
            {
                static auto PhaseOffset = GameState->GetOffset("GamePhase");
                if (PhaseOffset != -1)
                    phase = (int)GetFromOffset<uint8>(GameState, PhaseOffset);
            }

            printf("[Boron][Bots] S14 prereqs: world=%p gm=%p botmgr=%p aidirector=%p nav=%p aisystem=%p gamephase=%d\n",
                   (void*)World, (void*)GameMode, (void*)(GameMode ? GameMode->ServerBotManager : nullptr),
                   (void*)(GameMode ? GameMode->AIDirector : nullptr), Nav, AISys, phase);
        }

        struct Entry { const char* Path; const char* Name; bool All; };
        static const Entry Entries[] = {
            { "/Gasket/AISpawnerData/TomatoAlpha/BP_AIBotSpawnerData_Gasket_TomatoAlpha.BP_AIBotSpawnerData_Gasket_TomatoAlpha_C", "TomatoAlpha", false },
            { "/Gasket/AISpawnerData/DateAlpha/BP_AIBotSpawnerData_Gasket_DateAlpha.BP_AIBotSpawnerData_Gasket_DateAlpha_C", "DateAlpha", false },
            { "/Gasket/AISpawnerData/Wasabi/BP_AIBotSpawnerData_Gasket_Wasabi.BP_AIBotSpawnerData_Gasket_Wasabi_C", "Wasabi", false },
            { "/Gasket/AISpawnerData/MangAlpha/BP_AIBotSpawnerData_Gasket_MangAlpha.BP_AIBotSpawnerData_Gasket_MangAlpha_C", "MangAlpha", false },
            { "/Gasket/AISpawnerData/Date/BP_AIBotSpawnerData_Gasket_Date.BP_AIBotSpawnerData_Gasket_Date_C", "Date", true },
            { "/Gasket/AISpawnerData/Mang_Launcher/BP_AIBotSpawnerData_Gasket_MangLauncher.BP_AIBotSpawnerData_Gasket_MangLauncher_C", "MangLauncher", true },
            { "/Gasket/AISpawnerData/Mang_Crossbow/BP_AIBotSpawnerData_Gasket_Mang_Crossbow.BP_AIBotSpawnerData_Gasket_Mang_Crossbow_C", "Mang", true },
        };

        for (auto& E : Entries)
        {
            auto Class = FindObject<UClass>(E.Path);
            if (!Class)
            {
                printf("[Boron][Bots] spawner class missing: %s\n", E.Name);
                continue;
            }
            SpawnAIOnPaths(Class, E.Name, E.All);
        }

        if (auto StarkBot = FindObject<UClass>("/Gasket/AISpawnerData/Tomato/BP_AIBotSpawnerData_Gasket_Tomato.BP_AIBotSpawnerData_Gasket_Tomato_C"))
        {
            SpawnAIOnPaths(StarkBot, "Tomato", true);
            for (int i = 1; i <= 17; i++)
            {
                char Buf[32];
                sprintf_s(Buf, "TomatoQuinnJet%02d", i);
                SpawnAIOnPaths(StarkBot, Buf, true);
            }
        }
        else
            printf("[Boron][Bots] spawner class missing: Tomato\n");
    }

    inline UFortItemDefinition* GetWeaponDef(AActor* Weapon)
    {
        if (!IsValidPtr(Weapon))
            return nullptr;
        static auto WeaponDataOffset = Weapon->GetOffset("WeaponData");
        if (WeaponDataOffset == -1)
            return nullptr;
        auto Def = GetFromOffset<UFortItemDefinition*>(Weapon, WeaponDataOffset);
        return IsValidPtr(Def) ? Def : nullptr;
    }

    inline void Tick()
    {
        if (!GameRuleConfig::bBossAI)
            return;
        static uint32 tickCount = 0;
        if (++tickCount % 3 != 0)
            return;

        static bool bRequestedSpawns = false;
        if (!bRequestedSpawns && VersionInfo.FortniteVersion >= 14.0)
        {
            auto SpawnWorld = UWorld::GetWorld();
            auto SpawnGS = SpawnWorld ? (AFortGameStateAthena*)SpawnWorld->GameState : nullptr;
            int spawnPhase = 0;
            if (SpawnGS)
            {
                static auto PhOff = SpawnGS->GetOffset("GamePhase");
                if (PhOff != -1)
                    spawnPhase = (int)GetFromOffset<uint8>(SpawnGS, PhOff);
            }
            if (spawnPhase >= 3)
            {
                bRequestedSpawns = true;
                RequestAISpawn();
            }
        }

        auto& St = States();
        auto& Pawns = Live();

        static uint32 aiTick = 0;
        if ((aiTick++ % 10) == 0)
        {
            Pawns.clear();

            TArray<AFortPlayerPawnAthena*> Scanned;
            Utils::GetAll<AFortPlayerPawnAthena>(Scanned);
            for (int i = 0; i < Scanned.Num(); i++)
                if (IsLiveActor(Scanned[i]))
                    Pawns.push_back(Scanned[i]);
            Scanned.Free();
        }
        else
        {
            for (size_t i = 0; i < Pawns.size();)
            {
                if (!IsLiveActor(Pawns[i]))
                    Pawns.erase(Pawns.begin() + i);
                else
                    ++i;
            }
        }

        if (Pawns.size() <= 1)
            return;

        for (auto& Pair : St)
            Pair.second.seen = false;

        const float AggroRange = 6000.f;
        const float FireRange = 4500.f;
        const float StopRange = 350.f;

        for (int i = 0; i < (int)Pawns.size(); i++)
        {
            auto Bot = Pawns[i];
            if (!Bot || !Bot->Controller || !IsLiveActor((AActor*)Bot->Controller))
                continue;
            if (Bot->Controller->IsA<AFortPlayerControllerAthena>())
                continue;

            auto& State = St[Bot];
            State.seen = true;
            State.lastLoc = Bot->K2_GetActorLocation();

            static auto InvOffset = Bot->Controller->GetOffset("Inventory");
            if (!State.inv && InvOffset != -1)
                State.inv = GetFromOffset<AFortInventory*>(Bot->Controller, InvOffset);
            if (!IsLiveActor((AActor*)State.inv))
                State.inv = nullptr;

            if (!State.gaveWeapon)
            {
                State.weaponTries++;

                auto CurDef = GetWeaponDef(Bot->CurrentWeapon);
                bool bNativeRanged = CurDef && CurDef->Cast<UFortWeaponRangedItemDefinition>();

                if (bNativeRanged)
                {
                    State.gaveWeapon = true;
                }
                else if (State.inv)
                {
                    auto WeaponEntry = State.inv->Inventory.ReplicatedEntries.Search([](FFortItemEntry& Entry) {
                        return Entry.ItemDefinition && Entry.ItemDefinition->Cast<UFortWeaponRangedItemDefinition>();
                    }, FFortItemEntry::Size());

                    if (WeaponEntry)
                    {
                        State.gaveWeapon = true;
                        Bot->EquipWeaponDefinition(WeaponEntry->ItemDefinition, WeaponEntry->ItemGuid);
                    }
                    else if (State.weaponTries >= 40)
                    {
                        State.gaveWeapon = true;

                        auto MeleeBoss = State.inv->Inventory.ReplicatedEntries.Search([](FFortItemEntry& Entry) {
                            return Entry.ItemDefinition && Entry.ItemDefinition->Name.ToString().find("Wasabi") != std::string::npos;
                        }, FFortItemEntry::Size());

                        if (!MeleeBoss)
                        {
                            bool gotReal = VersionInfo.FortniteVersion < 14.0 && GiveStartupInventory(Bot, State.inv);
                            if (!gotReal)
                            {
                                BuildLoot();
                                auto& L = Loot();
                                auto& Pool = !L.Startup.empty() ? L.Startup : L.Guns;

                                if (!Pool.empty())
                                    GiveAndEquip(State.inv, Bot, Pool[rand() % Pool.size()]);
                            }
                        }
                    }
                }
                else if (State.weaponTries >= 40 && InvOffset != -1)
                {
                    auto NewInv = UWorld::SpawnActor<AFortInventory>(AFortInventory::StaticClass(), FVector{ 0, 0, -99999 }, FRotator{}, Bot->Controller);
                    if (NewInv)
                    {
                        NewInv->InventoryType = 0;
                        if (auto OnRepOwnerFn = NewInv->GetFunction("OnRep_Owner"))
                            NewInv->ProcessEvent(OnRepOwnerFn, nullptr);
                        GetFromOffset<AFortInventory*>(Bot->Controller, InvOffset) = NewInv;
                        State.inv = NewInv;

                        State.gaveWeapon = true;
                        bool gotReal = VersionInfo.FortniteVersion < 14.0 && GiveStartupInventory(Bot, NewInv);
                        if (!gotReal)
                        {
                            BuildLoot();
                            auto& L = Loot();
                            auto& Pool = !L.Startup.empty() ? L.Startup : L.Guns;

                            if (!Pool.empty())
                                GiveAndEquip(NewInv, Bot, Pool[rand() % Pool.size()]);
                        }
                    }
                }

                if (State.gaveWeapon)
                {
                    static int logged = 0;
                    if (logged++ < 16)
                        printf("[Boron][Bots] bot=%p inv=%p tries=%d nativeWeapon=%s\n", (void*)Bot, (void*)State.inv, State.weaponTries,
                               bNativeRanged && CurDef ? CurDef->Name.ToString().c_str() : "pool/none");
                }
            }

            if (auto Def = GetWeaponDef(Bot->CurrentWeapon))
                State.lastWeaponDef = Def;

            if (State.inv && (State.patrolTimer % 20) == 0)
            {
                State.dropDefs.clear();
                State.dropCounts.clear();

                auto& Entries = State.inv->Inventory.ReplicatedEntries;
                for (int e = 0; e < Entries.Num(); e++)
                {
                    auto& Entry = Entries.Get(e, FFortItemEntry::Size());

                    if (!Entry.ItemDefinition)
                        continue;
                    if (Entry.ItemDefinition->Cast<UFortAmmoItemDefinition>())
                        continue;

                    auto EntryName = Entry.ItemDefinition->Name.ToString();
                    if (Entry.ItemDefinition->Cast<UFortWeaponMeleeItemDefinition>())
                    {
                        if (EntryName.find("Wasabi") != std::string::npos)
                        {
                            static auto Claws = FindObject<UFortItemDefinition>(L"/HighTower/Items/Wasabi/Claws/CoreBR/WID_HighTower_Wasabi_Claws_CoreBR.WID_HighTower_Wasabi_Claws_CoreBR");
                            if (Claws)
                            {
                                State.dropDefs.push_back((UFortItemDefinition*)Claws);
                                State.dropCounts.push_back(1);
                            }

                            static auto Fish = FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Consumables/Flopper/Effective/WID_Athena_Flopper_Effective.WID_Athena_Flopper_Effective");
                            if (Fish)
                            {
                                State.dropDefs.push_back((UFortItemDefinition*)Fish);
                                State.dropCounts.push_back(2);
                            }
                        }
                        continue;
                    }

                    State.dropDefs.push_back((UFortItemDefinition*)Entry.ItemDefinition);
                    State.dropCounts.push_back((int)Entry.Count);
                }
            }

            static auto DropOffset = Bot->GetOffset("bShouldDropItemsOnDeath");
            if (DropOffset != -1)
                GetFromOffset<bool>(Bot, DropOffset) = true;

            if (Bot->IsDBNO())
            {
                if (State.firing)
                {
                    Bot->PawnStopFire((uint8)0);
                    State.firing = false;
                }
                if (State.targeting)
                {
                    Bot->bIsTargeting = false;
                    Bot->OnRep_IsTargeting();
                    State.targeting = false;
                }
                continue;
            }
            State.dropped = false;

            auto BotLoc = State.lastLoc;

            AFortPlayerPawnAthena* Target = nullptr;
            float BestDist = AggroRange;
            for (int j = 0; j < (int)Pawns.size(); j++)
            {
                auto Other = Pawns[j];
                if (!Other || Other == Bot || !Other->Controller || Other->IsDBNO())
                    continue;
                if (!IsLiveActor((AActor*)Other->Controller))
                    continue;
                if (!Other->Controller->IsA<AFortPlayerControllerAthena>())
                    continue;

                auto Dist = (float)Bot->GetDistanceTo(Other);
                if (Dist < BestDist)
                {
                    BestDist = Dist;
                    Target = Other;
                }
            }

            if (!Target)
            {
                if (State.firing)
                {
                    Bot->PawnStopFire((uint8)0);
                    State.firing = false;
                }
                if (State.targeting)
                {
                    Bot->bIsTargeting = false;
                    Bot->OnRep_IsTargeting();
                    State.targeting = false;
                }
                State.fireTimer = 0;

                State.patrolTimer++;
                if (State.patrolYaw < 0.f || State.patrolTimer % 40 == 0)
                    State.patrolYaw = (float)(rand() % 360);

                float prad = State.patrolYaw * 0.0174532925f;
                FVector PatrolDir{};
                PatrolDir.X = cosf(prad);
                PatrolDir.Y = sinf(prad);
                PatrolDir.Z = 0.f;
                Bot->AddMovementInput(PatrolDir, 0.55f, true);

                if (VersionInfo.EngineVersion < 5.0)
                {
                    FRotator PatrolRot{};
                    *(float*)((char*)&PatrolRot + 0x4) = State.patrolYaw;

                    static UFunction* PatrolRotFn = nullptr;
                    if (!PatrolRotFn)
                        PatrolRotFn = Bot->Controller->GetFunction("SetControlRotation");
                    if (PatrolRotFn)
                        Bot->Controller->ProcessEvent(PatrolRotFn, &PatrolRot);
                }
                continue;
            }

            auto TargetLoc = Target->K2_GetActorLocation();

            if ((State.fireTimer % 17) == 0)
            {
                float Spread = 1.f + (BestDist / 1500.f);
                State.aimX = ModLocation(30.f, 18.f, 30.f) * Spread;
                State.aimY = ModLocation(34.f, 19.f, 34.f) * Spread;
                State.aimZ = ModLocation(16.f, 17.f, 34.f) * Spread;
            }

            float dx = (float)(TargetLoc.X + State.aimX - BotLoc.X);
            float dy = (float)(TargetLoc.Y + State.aimY - BotLoc.Y);
            float dz = (float)(TargetLoc.Z + State.aimZ - BotLoc.Z);
            float len = sqrtf(dx * dx + dy * dy + dz * dz);
            if (len < 1.f)
                continue;

            if (VersionInfo.EngineVersion < 5.0)
            {
                float yaw = atan2f(dy, dx) * 57.2957795f;
                float pitch = atan2f(dz, sqrtf(dx * dx + dy * dy)) * 57.2957795f;

                FRotator LookRot{};
                *(float*)((char*)&LookRot + 0x0) = pitch;
                *(float*)((char*)&LookRot + 0x4) = yaw;

                static UFunction* SetControlRotationFn = nullptr;
                if (!SetControlRotationFn)
                    SetControlRotationFn = Bot->Controller->GetFunction("SetControlRotation");
                if (SetControlRotationFn)
                    Bot->Controller->ProcessEvent(SetControlRotationFn, &LookRot);
            }

            if (BestDist > StopRange)
            {
                FVector Dir{};
                Dir.X = dx / len;
                Dir.Y = dy / len;
                Dir.Z = dz / len;
                Bot->AddMovementInput(Dir, 1.f, true);
            }

            bool bHasLOS = true;
            if (BestDist < FireRange)
            {
                static UFunction* LOSFn = nullptr;
                static bool bLoggedLOS = false;
                if (!LOSFn)
                    LOSFn = Bot->Controller->GetFunction("LineOfSightTo");
                if (!bLoggedLOS)
                {
                    bLoggedLOS = true;
                    printf("[Boron][Bots] LineOfSightTo=%p\n", (void*)LOSFn);
                }

                if (LOSFn)
                {
                    struct { AActor* Other; FVector ViewPoint; bool bAlternateChecks; bool ReturnValue; } LOSParams{};
                    LOSParams.Other = Target;
                    Bot->Controller->ProcessEvent(LOSFn, &LOSParams);
                    bHasLOS = LOSParams.ReturnValue;
                }
            }

            if (BestDist < FireRange && Bot->CurrentWeapon && bHasLOS)
            {
                if (!State.targeting)
                {
                    Bot->bIsTargeting = true;
                    Bot->OnRep_IsTargeting();
                    State.targeting = true;
                }

                State.fireTimer++;
                uint32 phase = State.fireTimer % 28;
                if (phase == 1 && !State.firing)
                {
                    Bot->PawnStartFire((uint8)0);
                    State.firing = true;
                }
                else if (phase == 7 && State.firing)
                {
                    Bot->PawnStopFire((uint8)0);
                    State.firing = false;
                }
            }
            else
            {
                if (State.targeting)
                {
                    Bot->bIsTargeting = false;
                    Bot->OnRep_IsTargeting();
                    State.targeting = false;
                }
                if (State.firing)
                {
                    Bot->PawnStopFire((uint8)0);
                    State.firing = false;
                    State.fireTimer = 0;
                }
            }
        }

        for (auto it = St.begin(); it != St.end();)
        {
            if (!it->second.seen && !it->second.dropped)
            {
                int dropped = 0;
                static int dlog = 0;
                bool logThis = dlog++ < 16;

                if (it->second.lastWeaponDef)
                {
                    auto Drop = DroppableVersion(it->second.lastWeaponDef);
                    AFortInventory::SpawnPickup(it->second.lastLoc, Drop, 1, 0);
                    dropped++;
                    if (logThis)
                        printf("[Boron][Bots]   drop equipped [%s]\n", Drop->Name.ToString().c_str());

                    if (it->second.lastWeaponDef->Cast<UFortWeaponRangedItemDefinition>())
                    {
                        auto Ammo = ((UFortWorldItemDefinition*)it->second.lastWeaponDef)->GetAmmoWorldItemDefinition_BP();
                        if (Ammo && (UFortItemDefinition*)Ammo != it->second.lastWeaponDef)
                        {
                            AFortInventory::SpawnPickup(it->second.lastLoc, (UFortItemDefinition*)Ammo, 30, 0);
                            dropped++;
                        }
                    }
                }

                for (size_t d = 0; d < it->second.dropDefs.size(); d++)
                {
                    auto Def = it->second.dropDefs[d];

                    if (!Def || Def == it->second.lastWeaponDef)
                        continue;

                    auto Drop = DroppableVersion(Def);
                    AFortInventory::SpawnPickup(it->second.lastLoc, Drop, it->second.dropCounts[d] > 0 ? it->second.dropCounts[d] : 1, 0);
                    dropped++;
                    if (logThis)
                        printf("[Boron][Bots]   drop item [%s]\n", Drop->Name.ToString().c_str());

                    if (Drop->Cast<UFortWeaponRangedItemDefinition>())
                    {
                        auto ExtraAmmo = ((UFortWorldItemDefinition*)Drop)->GetAmmoWorldItemDefinition_BP();
                        if (ExtraAmmo && (UFortItemDefinition*)ExtraAmmo != Drop)
                        {
                            auto DropName = Drop->Name.ToString();
                            int AmmoCount = DropName.find("Launcher") != std::string::npos ? 6 : 30;
                            AFortInventory::SpawnPickup(it->second.lastLoc, (UFortItemDefinition*)ExtraAmmo, AmmoCount, 0);
                            dropped++;
                            if (logThis)
                                printf("[Boron][Bots]   drop ammo [%s] x%d\n", ((UFortItemDefinition*)ExtraAmmo)->Name.ToString().c_str(), AmmoCount);
                        }
                    }
                }

                if (logThis)
                    printf("[Boron][Bots] death drop: %d items at (%.0f,%.0f,%.0f)\n", dropped,
                           it->second.lastLoc.X, it->second.lastLoc.Y, it->second.lastLoc.Z);

                it->second.dropped = true;
            }
            if (!it->second.seen)
                it = St.erase(it);
            else
                ++it;
        }
    }
}
