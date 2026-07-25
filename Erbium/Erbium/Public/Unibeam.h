#pragma once

#include "pch.h"
#include <unordered_map>
#include <string>
#include "Configuration.h"
#include "Utils.h"

namespace Unibeam
{
    struct Drive
    {
        float elapsed = 0.f;
        float since = 0.f;
        int pulses = 0;
    };

    inline constexpr float ChargeDelay = 2.0f;
    inline constexpr float PulseInterval = 0.05f;
    inline constexpr float MaxLife = 2.6f;

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
        if (VersionInfo.FortniteVersion < 14.0 || VersionInfo.FortniteVersion >= 15.0)
            return;
        if (!IsLiveObject(Ability) || !Ability->Class)
            return;

        auto Name = Ability->Class->Name.ToString();
        if (Name.find("RepulsorCannon") == std::string::npos)
            return;

        auto TF = Ability->GetFunction("Trace");
        if (!TF)
            return;

        TraceFn() = TF;
        EndFn() = Ability->GetFunction("CastMontageNotifyEndAbility");
        Active()[(void*)Ability] = Drive{};

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
                if (EndFn())
                {
                    __try
                    {
                        Inst->ProcessEvent(EndFn(), nullptr);
                    }
                    __except (EXCEPTION_EXECUTE_HANDLER) {}

                    static int e = 0;
                    if (e++ < 16)
                        printf("[Boron][Unibeam] end inst=%p elapsed=%.2f pulses=%d\n", (void*)Inst, D.elapsed, D.pulses);
                }
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
