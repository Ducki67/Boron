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
        bool targeting = false;
        bool boss = false;
    };

    // Curated loot pools, built once from the object array by name (no offsets/RE).
    // Guns  = varied real weapons for henchmen; Mythics = boss weapons; KeyCard = vault key.
    struct LootPools
    {
        bool built = false;
        std::vector<UFortWorldItemDefinition*> Guns;
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

    inline void GiveAndEquip(AFortInventory* Inv, AFortPlayerPawnAthena* Bot, UFortWorldItemDefinition* Def)
    {
        if (!Inv || !Bot || !Def)
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

    inline UFortItemDefinition* GetWeaponDef(AActor* Weapon)
    {
        if (!Weapon)
            return nullptr;
        static auto WeaponDataOffset = Weapon->GetOffset("WeaponData");
        if (WeaponDataOffset == -1)
            return nullptr;
        return GetFromOffset<UFortItemDefinition*>(Weapon, WeaponDataOffset);
    }

    inline void Tick()
    {
        if (!GameRuleConfig::bBossAI)
            return;
        // some nerdy shit:
        // Manual tick: run the AI at ~1/3 of the server tick rate (~10Hz) so the
        // per-tick GetAllActorsOfClass scan + ProcessEvent calls don't tank the framerate.
        static uint32 tickCount = 0;
        if (++tickCount % 3 != 0)
            return;

        auto& St = States();

        TArray<AFortPlayerPawnAthena*> Pawns;
        Utils::GetAll<AFortPlayerPawnAthena>(Pawns);
        if (Pawns.Num() <= 1)
        {
            Pawns.Free();
            return;
        }

        for (auto& Pair : St)
            Pair.second.seen = false;

        const float AggroRange = 6000.f;
        const float FireRange = 4500.f;
        const float StopRange = 350.f;

        for (int i = 0; i < Pawns.Num(); i++)
        {
            auto Bot = Pawns[i];
            if (!Bot || !Bot->Controller)
                continue;
            if (Bot->Controller->IsA<AFortPlayerControllerAthena>())
                continue;

            auto& State = St[Bot];
            State.seen = true;
            State.lastLoc = Bot->K2_GetActorLocation();

            if (!State.gaveWeapon)
            {
                State.gaveWeapon = true;
                static auto InvOffset = Bot->Controller->GetOffset("Inventory");
                AFortInventory* Inv = (InvOffset != -1) ? GetFromOffset<AFortInventory*>(Bot->Controller, InvOffset) : nullptr;
                if (!Inv && InvOffset != -1)
                {
                    Inv = UWorld::SpawnActor<AFortInventory>(AFortInventory::StaticClass(), FVector{ 0, 0, -99999 }, FRotator{}, Bot->Controller);
                    if (Inv)
                    {
                        Inv->InventoryType = 0;
                        if (auto OnRepOwnerFn = Inv->GetFunction("OnRep_Owner"))
                            Inv->ProcessEvent(OnRepOwnerFn, nullptr);
                        GetFromOffset<AFortInventory*>(Bot->Controller, InvOffset) = Inv;
                    }
                }
                if (Inv)
                {
                    BuildLoot();
                    auto& L = Loot();

                    
                    State.boss = !L.Mythics.empty() && ((((uintptr_t)Bot) >> 5) % 5 == 0);

                    if (State.boss)
                    {
                        GiveAndEquip(Inv, Bot, L.Mythics[rand() % L.Mythics.size()]);
                        if (L.KeyCard)
                            Inv->GiveItem(L.KeyCard, 1);
                    }
                    else if (!L.Guns.empty())
                    {
                        GiveAndEquip(Inv, Bot, L.Guns[rand() % L.Guns.size()]);
                    }
                    else
                    {
                        
                        auto WeaponEntry = Inv->Inventory.ReplicatedEntries.Search([](FFortItemEntry& Entry) {
                            return Entry.ItemDefinition && Entry.ItemDefinition->Cast<UFortWeaponRangedItemDefinition>();
                        }, FFortItemEntry::Size());
                        if (WeaponEntry)
                            Bot->EquipWeaponDefinition(WeaponEntry->ItemDefinition, WeaponEntry->ItemGuid);
                    }
                }
            }

            if (auto Def = GetWeaponDef(Bot->CurrentWeapon))
                State.lastWeaponDef = Def;

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
            for (int j = 0; j < Pawns.Num(); j++)
            {
                auto Other = Pawns[j];
                if (!Other || Other == Bot || !Other->Controller || Other->IsDBNO())
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
            float dx = (float)(TargetLoc.X - BotLoc.X);
            float dy = (float)(TargetLoc.Y - BotLoc.Y);
            float dz = (float)(TargetLoc.Z - BotLoc.Z);
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

            if (BestDist < FireRange && Bot->CurrentWeapon)
            {
                if (!State.targeting)
                {
                    Bot->bIsTargeting = true;
                    Bot->OnRep_IsTargeting();
                    State.targeting = true;
                }

                State.fireTimer++;
                uint32 phase = State.fireTimer % 16; // ~1.6s cycle at 10Hz
                if (phase == 1 && !State.firing)
                {
                    Bot->PawnStartFire((uint8)0);
                    State.firing = true;
                }
                else if (phase == 10 && State.firing) // ~1.0s burst, ~0.6s pause
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
            if (!it->second.seen && !it->second.dropped && it->second.lastWeaponDef)
            {
                AFortInventory::SpawnPickup(it->second.lastLoc, it->second.lastWeaponDef, 1, 0);
                if (it->second.boss && Loot().KeyCard)
                    AFortInventory::SpawnPickup(it->second.lastLoc, Loot().KeyCard, 1, 0);
                it->second.dropped = true;
            }
            if (!it->second.seen)
                it = St.erase(it);
            else
                ++it;
        }

        Pawns.Free();
    }
}
