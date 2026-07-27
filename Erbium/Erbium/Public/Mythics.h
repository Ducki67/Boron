#pragma once

#include "pch.h"
#include <unordered_map>
#include <string>
#include "Configuration.h"
#include "Utils.h"

namespace Mythics
{
    struct Drive
    {
        float elapsed = 0.f;
        float since = 0.f;
        int pulses = 0;
    };

    inline constexpr float ChargeDelay = 1.95f;
    inline constexpr float PulseInterval = 0.03f;
    inline constexpr float MaxLife = 2.85f;

    inline std::unordered_map<void*, Drive>& Active()
    {
        static std::unordered_map<void*, Drive> M;
        return M;
    }

    inline UFunction*& TraceFn()
    {
        static UFunction* F = nullptr;
        return F;
    }

    inline UFunction*& EndFn()
    {
        static UFunction* F = nullptr;
        return F;
    }

    inline UFunction*& RemoveGEFn()
    {
        static UFunction* F = nullptr;
        return F;
    }

    inline int& SlowOff()
    {
        static int O = -2;
        return O;
    }

    struct alignas(4) GESlowHandle
    {
        uint8 raw[8];
    };

    inline void SafePE(UObject* Obj, UFunction* Fn, void* Params)
    {
        __try
        {
            Obj->ProcessEvent(Fn, Params);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    inline bool IsLiveObject(UObject* Obj)
    {
        auto V = (uintptr_t)Obj;
        if (V < 0x10000 || V >= 0x7FFFFFFFFFFFull)
            return false;
        auto Item = TUObjectArray::GetItemByIndex(Obj->Index);
        if (!Item || (Item->Flags & ((1 << 29) | (1 << 21))))
            return false;
        return true;
    }

    inline void NotifyActivated(UObject* Ability)
    {
        if (VersionInfo.FortniteVersion < 13.0 || VersionInfo.FortniteVersion >= 15.0)
            return;
        if (!IsLiveObject(Ability) || !Ability->Class)
            return;

        auto Name = Ability->Class->Name.ToString();

        if (VersionInfo.FortniteVersion >= 13.0 && VersionInfo.FortniteVersion < 14.0)
        {
            static int a = 0;
            if (a++ < 120)
                printf("[Boron][Ability] activated class=%s\n", Name.c_str());

            if (Name.find("BottomlessChugJug") != std::string::npos)
            {
                if (auto CD = Ability->GetFunction("K2_CommitAbilityCooldown"))
                {
                    struct { uint8 Broadcast; uint8 Force; } P{ 1, 1 };
                    SafePE(Ability, CD, &P);
                    printf("[Boron][ChugJug] cooldown committed inst=%p\n", (void*)Ability);
                }
                else
                    printf("[Boron][ChugJug] no K2_CommitAbilityCooldown on %s\n", Name.c_str());
            }
            return;
        }

        if (Name.find("RepulsorCannon") == std::string::npos)
            return;

        auto TF = Ability->GetFunction("Trace");
        if (!TF)
            return;

        TraceFn() = TF;
        EndFn() = Ability->GetFunction("K2_EndAbility");
        RemoveGEFn() = Ability->GetFunction("BP_RemoveGameplayEffectFromOwnerWithHandle");
        if (SlowOff() == -2)
            SlowOff() = Ability->GetOffset("GE_Slow_Handle");
        Active()[(void*)Ability] = Drive{};

        if (auto CD = Ability->GetFunction("K2_CommitAbilityCooldown"))
        {
            struct { uint8 Broadcast; uint8 Force; } P{ 1, 1 };
            SafePE(Ability, CD, &P);
        }

        static int n = 0;
        if (n++ < 16)
            printf("[Boron][Unibeam] activated inst=%p class=%s trace=%p\n", (void*)Ability, Name.c_str(), (void*)TF);
    }

    inline void Tick(float Dt)
    {
        if (VersionInfo.FortniteVersion < 14.0 || VersionInfo.FortniteVersion >= 15.0)
            return;
        if (Active().empty())
            return;
        if (Dt <= 0.f || Dt > 1.f)
            Dt = 0.016f;

        auto TF = TraceFn();
        for (auto it = Active().begin(); it != Active().end();)
        {
            auto Inst = (UObject*)it->first;
            auto& D = it->second;

            if (!IsLiveObject(Inst))
            {
                it = Active().erase(it);
                continue;
            }

            if (D.elapsed > MaxLife)
            {
                if (RemoveGEFn() && SlowOff() >= 0)
                {
                    struct { GESlowHandle Handle; int32 Stacks; } P{};
                    P.Handle = GetFromOffset<GESlowHandle>(Inst, SlowOff());
                    P.Stacks = -1;
                    __try
                    {
                        Inst->ProcessEvent(RemoveGEFn(), &P);
                    }
                    __except (EXCEPTION_EXECUTE_HANDLER) {}
                }

                if (auto RS = Inst->GetFunction("Update_Rotation_Input_Scale"))
                {
                    struct { uint8 Limit; uint8 pad[3]; float Pitch; float Yaw; } P{ 0, {0, 0, 0}, 1.0f, 1.0f };
                    __try { Inst->ProcessEvent(RS, &P); } __except (EXCEPTION_EXECUTE_HANDLER) {}
                }

                if (EndFn())
                {
                    __try
                    {
                        Inst->ProcessEvent(EndFn(), nullptr);
                    }
                    __except (EXCEPTION_EXECUTE_HANDLER) {}
                }

                static int e = 0;
                if (e++ < 16)
                    printf("[Boron][Unibeam] end inst=%p elapsed=%.2f pulses=%d removeGE=%p slowOff=%d\n",
                           (void*)Inst, D.elapsed, D.pulses, (void*)RemoveGEFn(), SlowOff());

                it = Active().erase(it);
                continue;
            }

            D.elapsed += Dt;

            if (TF && D.elapsed >= ChargeDelay)
            {
                D.since += Dt;
                if (D.since >= PulseInterval)
                {
                    D.since -= PulseInterval;
                    __try
                    {
                        Inst->ProcessEvent(TF, nullptr);
                    }
                    __except (EXCEPTION_EXECUTE_HANDLER) {}

                    D.pulses++;
                    static int t = 0;
                    if (t++ < 24)
                        printf("[Boron][Unibeam] trace inst=%p elapsed=%.2f pulse=%d\n", (void*)Inst, D.elapsed, D.pulses);
                }
            }

            ++it;
        }
    }
}
