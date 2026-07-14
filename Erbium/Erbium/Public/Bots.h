#pragma once

#include "pch.h"
#include <cmath>
#include <cstdlib>
#include <unordered_map>
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
    };

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
                static auto BossWeapon = FindObject<UFortWorldItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Assault_AutoHigh_Athena_SR_Ore_T03.WID_Assault_AutoHigh_Athena_SR_Ore_T03");
                if (BossWeapon)
                {
                    static auto InvOffset = Bot->Controller->GetOffset("Inventory");
                    AFortInventory* Inv = (InvOffset != -1) ? GetFromOffset<AFortInventory*>(Bot->Controller, InvOffset) : nullptr;
                    if (Inv)
                    {
                        auto Stats = AFortInventory::GetStats((UFortWeaponItemDefinition*)BossWeapon);
                        int Clip = (Stats && Stats->ClipSize > 0) ? Stats->ClipSize : 30;
                        if (auto Ammo = BossWeapon->GetAmmoWorldItemDefinition_BP())
                            if ((UFortWorldItemDefinition*)Ammo != BossWeapon)
                                Inv->GiveItem(Ammo, 999);
                        if (auto WItem = Inv->GiveItem(BossWeapon, 1, Clip))
                            Bot->EquipWeaponDefinition((UFortWeaponItemDefinition*)BossWeapon, WItem->ItemEntry.ItemGuid);
                    }
                }
            }

            if (auto Def = GetWeaponDef(Bot->CurrentWeapon))
                State.lastWeaponDef = Def;

            static auto DropOffset = Bot->GetOffset("bShouldDropItemsOnDeath");
            if (DropOffset != -1)
                GetFromOffset<bool>(Bot, DropOffset) = true;

            if (Bot->IsDBNO() || Bot->GetHealth() <= 0.f)
            {
                if (!State.dropped && State.lastWeaponDef)
                {
                    AFortInventory::SpawnPickup(State.lastLoc, State.lastWeaponDef, 1, 0);
                    State.dropped = true;
                }
                if (State.firing)
                {
                    Bot->PawnStopFire((uint8)0);
                    State.firing = false;
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
                State.fireTimer = 0;

                State.patrolTimer++;
                if (State.patrolYaw < 0.f || State.patrolTimer % 150 == 0)
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
                Bot->bIsTargeting = true;
                Bot->OnRep_IsTargeting();

                State.fireTimer++;
                uint32 phase = State.fireTimer % 80;
                if (phase == 1 && !State.firing)
                {
                    Bot->PawnStartFire((uint8)0);
                    State.firing = true;
                }
                else if (phase == 40 && State.firing)
                {
                    Bot->PawnStopFire((uint8)0);
                    State.firing = false;
                }
            }
            else if (State.firing)
            {
                Bot->PawnStopFire((uint8)0);
                State.firing = false;
                State.fireTimer = 0;
            }
        }

        for (auto it = St.begin(); it != St.end();)
        {
            if (!it->second.seen && !it->second.dropped && it->second.lastWeaponDef)
            {
                AFortInventory::SpawnPickup(it->second.lastLoc, it->second.lastWeaponDef, 1, 0);
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
