#pragma once
#include "../../pch.h"
#include "FortWeapon.h"
#include "GameplayTagContainer.h"

class UFortWeaponModItemDefinition : public UFortItemDefinition
{
public:
    UCLASS_COMMON_MEMBERS(UFortWeaponModItemDefinition);

    DEFINE_PROP(ModSlot, FGameplayTag);
};

class UFortWeaponModFunctionLibrary : public UObject
{
public:
    UCLASS_COMMON_MEMBERS(UFortWeaponModFunctionLibrary);

    DEFINE_STATIC_FUNC(TryAddWeaponMod, bool);
    DEFINE_STATIC_FUNC(TryRemoveWeaponMod, bool);
    DEFINE_STATIC_FUNC(CanApplyModToWeapon, bool);
    DEFINE_STATIC_FUNC(ModAllowedOnWeapon, bool);
    DEFINE_STATIC_FUNC(ApplyWeaponModToPickup, bool);
};

namespace WeaponMods
{
    enum EModCategory
    {
        ModCategory_Optic = 0,
        ModCategory_Barrel = 1,
        ModCategory_Underbarrel = 2,
        ModCategory_Magazine = 3,
        ModCategory_Count = 4,
        ModCategory_Unknown = -1
    };

    struct FStoredMods
    {
        int32 A;
        int32 B;
        int32 C;
        int32 D;
        const UFortWeaponModItemDefinition* Mods[8];
        int32 Count;
    };

    struct FAlias
    {
        const char* Name;
        const char* Match;
    };

    static inline const FAlias Aliases[] = {
        { "reddot", "RedDot" },     { "red", "RedDot" },        { "rd", "RedDot" },
        { "holo", "Holo" },         { "holographic", "Holo" },  { "h13", "Holo" },
        { "p2x", "P2X" },           { "acog", "P2X" },          { "2x", "P2X" },
        { "sniper", "Optic_Sniper" },{ "scope", "Optic_Sniper" },{ "snipe", "Optic_Sniper" }, { "ss", "Optic_Sniper" },
        { "ironsights", "IronSights" }, { "iron", "IronSights" }, { "sights", "IronSights" },
        { "suppressor", "Suppressor" }, { "supp", "Suppressor" }, { "silencer", "Suppressor" }, { "sil", "Suppressor" },
        { "muzzle", "MuzzleBrake" }, { "brake", "MuzzleBrake" }, { "mb", "MuzzleBrake" },
        { "vert", "VertForegrip" }, { "vertical", "VertForegrip" }, { "vfg", "VertForegrip" }, { "vgrip", "VertForegrip" },
        { "angled", "AngledForegrip" }, { "afg", "AngledForegrip" }, { "agrip", "AngledForegrip" }, { "angle", "AngledForegrip" },
        { "speedgrip", "SpeedForegrip" }, { "sfg", "SpeedForegrip" }, { "sgrip", "SpeedForegrip" },
        { "laser", "Underbarrel_Laser" }, { "las", "Underbarrel_Laser" }, { "laz", "Underbarrel_Laser" },
        { "speed", "Magazine_Speed" }, { "speedmag", "Magazine_Speed" }, { "fast", "Magazine_Speed" }, { "smag", "Magazine_Speed" },
        { "drum", "Magazine_Drum" }, { "drummag", "Magazine_Drum" }, { "dmag", "Magazine_Drum" }, { "big", "Magazine_Drum" },
    };

    static inline FStoredMods Store[512]{};
    static inline int32 StoreCount = 0;
    static inline std::vector<const UFortWeaponModItemDefinition*> Discovered;
    static inline bool bDiscovered = false;

    inline std::string Lower(const std::string& In)
    {
        std::string Out = In;
        std::transform(Out.begin(), Out.end(), Out.begin(), [](unsigned char c) { return (char)tolower(c); });
        return Out;
    }

    inline bool IsSupported(const AFortWeapon* Weapon)
    {
        return Weapon && FFortWeaponModSlot::StaticStruct() && Weapon->HasWeaponModSlots();
    }

    inline void Discover()
    {
        if (bDiscovered)
            return;

        bDiscovered = true;

        auto ModClass = FindClass("FortWeaponModItemDefinition");

        if (!ModClass)
        {
            printf("[Boron][Mods] FortWeaponModItemDefinition class not present on this build\n");
            return;
        }

        for (int i = 0; i < TUObjectArray::Num(); i++)
        {
            auto Obj = TUObjectArray::GetObjectByIndex(i);

            if (Obj && !Obj->IsDefaultObject() && Obj->IsA(ModClass))
                Discovered.push_back((const UFortWeaponModItemDefinition*)Obj);
        }

        printf("[Boron][Mods] discovered %d weapon mod definitions\n", (int)Discovered.size());

        for (auto Mod : Discovered)
            printf("[Boron][Mods]   %s\n", Mod->Name.ToString().c_str());
    }

    inline int Category(const UFortWeaponModItemDefinition* Mod)
    {
        if (!Mod)
            return ModCategory_Unknown;

        if (Mod->HasModSlot())
        {
            auto Tag = Lower(Mod->ModSlot.TagName.ToString().c_str());

            if (Tag.find("optic") != std::string::npos || Tag.find("sight") != std::string::npos)
                return ModCategory_Optic;
            if (Tag.find("barrel") != std::string::npos && Tag.find("underbarrel") == std::string::npos)
                return ModCategory_Barrel;
            if (Tag.find("underbarrel") != std::string::npos || Tag.find("grip") != std::string::npos)
                return ModCategory_Underbarrel;
            if (Tag.find("magazine") != std::string::npos || Tag.find("mag") != std::string::npos)
                return ModCategory_Magazine;
        }

        auto Name = Lower(Mod->Name.ToString().c_str());

        if (Name.find("optic") != std::string::npos || Name.find("sight") != std::string::npos)
            return ModCategory_Optic;
        if (Name.find("underbarrel") != std::string::npos)
            return ModCategory_Underbarrel;
        if (Name.find("barrel") != std::string::npos)
            return ModCategory_Barrel;
        if (Name.find("magazine") != std::string::npos)
            return ModCategory_Magazine;

        return ModCategory_Unknown;
    }

    inline const char* CategoryName(int Cat)
    {
        switch (Cat)
        {
        case ModCategory_Optic: return "Optic";
        case ModCategory_Barrel: return "Barrel";
        case ModCategory_Underbarrel: return "Underbarrel";
        case ModCategory_Magazine: return "Magazine";
        default: return "Other";
        }
    }

    inline const UFortWeaponModItemDefinition* FindByMatch(const std::string& Match)
    {
        auto Want = Lower(Match);

        for (auto Mod : Discovered)
        {
            if (Lower(Mod->Name.ToString().c_str()).find(Want) != std::string::npos)
                return Mod;
        }

        return nullptr;
    }

    inline void Rediscover()
    {
        bDiscovered = false;
        Discovered.clear();
        Discover();
    }

    inline const UFortWeaponModItemDefinition* ResolveIn(const std::string& Want)
    {
        for (auto& Alias : Aliases)
        {
            if (Want == Alias.Name)
            {
                if (auto Mod = FindByMatch(Alias.Match))
                    return Mod;
            }
        }

        return FindByMatch(Want);
    }

    inline const UFortWeaponModItemDefinition* Resolve(const std::string& Input)
    {
        Discover();

        auto Want = Lower(Input);

        if (auto Mod = ResolveIn(Want))
            return Mod;

        auto Before = (int)Discovered.size();
        Rediscover();

        if ((int)Discovered.size() != Before)
            printf("[Boron][Mods] rescan on miss: %d -> %d definitions\n", Before, (int)Discovered.size());

        return ResolveIn(Want);
    }

    inline FStoredMods* FindStore(const FGuid& Guid)
    {
        for (int i = 0; i < StoreCount; i++)
            if (Store[i].A == Guid.A && Store[i].B == Guid.B && Store[i].C == Guid.C && Store[i].D == Guid.D)
                return &Store[i];

        return nullptr;
    }

    inline void RemoveStore(const FGuid& Guid)
    {
        for (int i = 0; i < StoreCount; i++)
            if (Store[i].A == Guid.A && Store[i].B == Guid.B && Store[i].C == Guid.C && Store[i].D == Guid.D)
            {
                Store[i] = Store[--StoreCount];
                return;
            }
    }

    inline void PushStore(const FGuid& Guid, const UFortWeaponModItemDefinition* Mod)
    {
        if (!Mod)
            return;

        auto Entry = FindStore(Guid);

        if (!Entry)
        {
            if (StoreCount >= 512)
                return;

            Entry = &Store[StoreCount++];
            Entry->A = Guid.A;
            Entry->B = Guid.B;
            Entry->C = Guid.C;
            Entry->D = Guid.D;
            Entry->Count = 0;
        }

        auto Cat = Category(Mod);

        for (int i = 0; i < Entry->Count; i++)
            if (Category(Entry->Mods[i]) == Cat && Cat != ModCategory_Unknown)
            {
                Entry->Mods[i] = Mod;
                return;
            }

        if (Entry->Count < 8)
            Entry->Mods[Entry->Count++] = Mod;
    }

    inline bool IsCompatible(const UFortWeaponModItemDefinition* Mod, const std::string& WeaponName)
    {
        if (!Mod)
            return false;

        auto ModName = Mod->Name.ToString().c_str();
        auto Weapon = WeaponName;

        bool bShotgun = Weapon.find("Shotgun") != std::string::npos;
        bool bSMG = Weapon.find("SMG") != std::string::npos;
        bool bPistol = Weapon.find("Pistol") != std::string::npos;
        bool bPump = Weapon.find("Shotgun_Pump") != std::string::npos || Weapon.find("Shotgun_Standard") != std::string::npos;

        std::string Mods = ModName;

        if (Mods.find("Sniper") != std::string::npos || Mods.find("P2X") != std::string::npos)
            if (bShotgun || bSMG || bPistol)
                return false;

        if (Mods.find("Drum") != std::string::npos && bPump)
            return false;

        return true;
    }

    inline bool HasNative()
    {
        static int Cached = -1;

        if (Cached == -1)
        {
            auto Cls = FindClass("FortWeaponModFunctionLibrary");
            auto Obj = Cls ? UFortWeaponModFunctionLibrary::GetDefaultObj() : nullptr;
            Cached = (Obj && Obj->GetFunction("TryAddWeaponMod")) ? 1 : 0;
            printf("[Boron][Mods] native TryAddWeaponMod available=%d\n", Cached);
        }

        return Cached == 1;
    }

    inline bool NativeAdd(AFortWeapon* Weapon, const UFortWeaponModItemDefinition* Mod)
    {
        if (!HasNative() || !Weapon || !Mod)
            return false;

        return UFortWeaponModFunctionLibrary::TryAddWeaponMod((UFortWeaponModItemDefinition*)Mod, Weapon);
    }

    inline void NotifyRep(AFortWeapon* Weapon)
    {
        if (!Weapon)
            return;

        if (auto OnRepFn = Weapon->GetFunction("OnRep_ReplicatedWeaponModSlots"))
        {
            TArray<FFortWeaponModSlot> Previous{};
            Weapon->ProcessEvent(OnRepFn, &Previous);
        }

        Weapon->ForceNetUpdate();
    }

    inline int WriteSlots(AFortWeapon* Weapon, const UFortWeaponModItemDefinition* const* Mods, int Count)
    {
        if (!IsSupported(Weapon))
            return 0;

        auto SlotSize = FFortWeaponModSlot::Size();

        if (SlotSize <= 0 || SlotSize > 128)
            return 0;

        auto& Slots = Weapon->WeaponModSlots;

        Slots.NumElements = 0;

        uint8 Buf[128]{};
        auto& Slot = *(FFortWeaponModSlot*)Buf;
        int Written = 0;

        for (int i = 0; i < Count; i++)
        {
            if (!Mods[i])
                continue;

            memset(Buf, 0, sizeof(Buf));
            Slot.WeaponMod = (const UFortItemDefinition*)Mods[i];

            if (FFortWeaponModSlot::HasbIsDynamic())
                Slot.bIsDynamic = true;

            Slots.Add(Slot, SlotSize);
            Written++;
        }

        NotifyRep(Weapon);
        return Written;
    }

    inline void SeedFromWeapon(AFortWeapon* Weapon)
    {
        if (!IsSupported(Weapon) || FindStore(Weapon->ItemEntryGuid))
            return;

        auto SlotSize = FFortWeaponModSlot::Size();

        if (SlotSize <= 0)
            return;

        auto& Slots = Weapon->WeaponModSlots;

        for (int i = 0; i < Slots.Num(); i++)
        {
            auto& S = Slots.Get(i, SlotSize);

            if (S.WeaponMod)
                PushStore(Weapon->ItemEntryGuid, (const UFortWeaponModItemDefinition*)S.WeaponMod);
        }
    }

    inline bool Apply(AFortWeapon* Weapon, const UFortWeaponModItemDefinition* Mod, bool bForce, std::string* OutReason)
    {
        if (!Weapon || !Mod)
        {
            if (OutReason)
                *OutReason = "no weapon or mod";
            return false;
        }

        if (!IsSupported(Weapon))
        {
            if (OutReason)
                *OutReason = "this build has no weapon mods";
            return false;
        }

        std::string WeaponName = Weapon->HasWeaponData() && Weapon->WeaponData ? Weapon->WeaponData->Name.ToString().c_str() : "";

        if (!bForce && !IsCompatible(Mod, WeaponName))
        {
            if (OutReason)
                *OutReason = "not compatible with this weapon";
            return false;
        }

        SeedFromWeapon(Weapon);
        PushStore(Weapon->ItemEntryGuid, Mod);

        auto Entry = FindStore(Weapon->ItemEntryGuid);

        if (!Entry)
        {
            if (OutReason)
                *OutReason = "mod store full";
            return false;
        }

        bool bNative = NativeAdd(Weapon, Mod);
        int Written = bNative ? Weapon->WeaponModSlots.Num() : WriteSlots(Weapon, Entry->Mods, Entry->Count);

        if (bNative)
            NotifyRep(Weapon);

        printf("[Boron][Mods] apply %s (%s) to %s -> %d slot(s) native=%d\n", Mod->Name.ToString().c_str(), CategoryName(Category(Mod)), WeaponName.c_str(), Written, (int)bNative);

        return bNative || Written > 0;
    }

    inline void ClearAll(AFortWeapon* Weapon)
    {
        if (!IsSupported(Weapon))
            return;

        if (auto Entry = FindStore(Weapon->ItemEntryGuid))
            if (HasNative())
                for (int i = 0; i < Entry->Count; i++)
                    if (Entry->Mods[i])
                        UFortWeaponModFunctionLibrary::TryRemoveWeaponMod((UFortWeaponModItemDefinition*)Entry->Mods[i], Weapon);

        RemoveStore(Weapon->ItemEntryGuid);
        Weapon->WeaponModSlots.NumElements = 0;
        NotifyRep(Weapon);

        printf("[Boron][Mods] cleared mods on %s\n", Weapon->HasWeaponData() && Weapon->WeaponData ? Weapon->WeaponData->Name.ToString().c_str() : "weapon");
    }

    inline void Reapply(AFortWeapon* Weapon)
    {
        if (!IsSupported(Weapon))
            return;

        auto Entry = FindStore(Weapon->ItemEntryGuid);

        if (!Entry || Entry->Count <= 0)
            return;

        int Native = 0;

        for (int i = 0; i < Entry->Count; i++)
            if (NativeAdd(Weapon, Entry->Mods[i]))
                Native++;

        int Written = Native == Entry->Count ? Native : WriteSlots(Weapon, Entry->Mods, Entry->Count);

        if (Native > 0)
            NotifyRep(Weapon);

        printf("[Boron][Mods] reapplied %d mod(s) on equip of %s native=%d\n", Written, Weapon->HasWeaponData() && Weapon->WeaponData ? Weapon->WeaponData->Name.ToString().c_str() : "weapon", Native);
    }

    inline bool HasPickupNative()
    {
        static int Cached = -1;

        if (Cached == -1)
        {
            auto Cls = FindClass("FortWeaponModFunctionLibrary");
            auto Obj = Cls ? UFortWeaponModFunctionLibrary::GetDefaultObj() : nullptr;
            Cached = (Obj && Obj->GetFunction("ApplyWeaponModToPickup")) ? 1 : 0;
            printf("[Boron][Mods] native ApplyWeaponModToPickup available=%d\n", Cached);
        }

        return Cached == 1;
    }

    inline int RarityBudget(int Rarity)
    {
        switch (Rarity)
        {
        case 0:
            return rand() % 100 < 30 ? 1 : 0;
        case 1:
            return rand() % 100 < 55 ? 1 : 0;
        case 2:
            return 1 + (rand() % 100 < 35 ? 1 : 0);
        case 3:
            return 1 + (rand() % 100 < 65 ? 1 : 0);
        case 4:
            return 2 + (rand() % 100 < 55 ? 1 : 0);
        default:
            return 3 + (rand() % 100 < 40 ? 1 : 0);
        }
    }

    inline bool IsDefaultMod(const UFortItemDefinition* Mod)
    {
        return Mod && Lower(Mod->Name.ToString().c_str()).find("default") != std::string::npos;
    }

    inline int EntryModCount(FFortItemEntry* Entry)
    {
        if (!Entry || !FFortItemEntry::HasWeaponModSlots())
            return 0;

        auto SlotSize = FFortWeaponModSlot::Size();

        if (SlotSize <= 0)
            return 0;

        auto& Slots = Entry->GetWeaponModSlots();
        int Count = 0;

        for (int i = 0; i < Slots.Num(); i++)
        {
            auto Mod = Slots.Get(i, SlotSize).WeaponMod;

            if (Mod && !IsDefaultMod(Mod))
                Count++;
        }

        return Count;
    }

    inline int CarryEntryMods(FFortItemEntry* Src, FFortItemEntry* Dst)
    {
        if (!Src || !Dst || !FFortItemEntry::HasWeaponModSlots())
            return 0;

        auto SlotSize = FFortWeaponModSlot::Size();

        if (SlotSize <= 0)
            return 0;

        auto& From = Src->GetWeaponModSlots();
        auto& To = Dst->GetWeaponModSlots();
        int Limit = From.Num() < To.Num() ? From.Num() : To.Num();
        int Carried = 0;

        for (int i = 0; i < Limit; i++)
        {
            auto& S = From.Get(i, SlotSize);

            if (!S.WeaponMod)
                continue;

            To.Get(i, SlotSize).WeaponMod = S.WeaponMod;
            Carried++;
        }

        return Carried;
    }

    inline int ReapplyEntryMods(FFortItemEntry* Src, UObject* Pickup)
    {
        if (!Src || !Pickup || !HasPickupNative() || !FFortItemEntry::HasWeaponModSlots())
            return 0;

        auto SlotSize = FFortWeaponModSlot::Size();

        if (SlotSize <= 0)
            return 0;

        auto& Slots = Src->GetWeaponModSlots();
        int Applied = 0;

        for (int i = 0; i < Slots.Num(); i++)
        {
            auto& S = Slots.Get(i, SlotSize);

            if (S.WeaponMod && !IsDefaultMod(S.WeaponMod) && UFortWeaponModFunctionLibrary::ApplyWeaponModToPickup(Pickup, (UFortWeaponModItemDefinition*)S.WeaponMod))
                Applied++;
        }

        return Applied;
    }
    inline bool WriteEntrySlot(FFortItemEntry* Entry, const UFortWeaponModItemDefinition* Mod)
    {
        if (!Entry || !Mod || !FFortItemEntry::HasWeaponModSlots() || !FFortWeaponModSlot::HasWeaponMod())
            return false;

        auto SlotSize = FFortWeaponModSlot::Size();

        if (SlotSize <= 0)
            return false;

        auto& Slots = Entry->GetWeaponModSlots();
        int Cat = Category(Mod);

        for (int i = 0; i < Slots.Num(); i++)
        {
            auto& S = Slots.Get(i, SlotSize);

            if (S.WeaponMod && Category((const UFortWeaponModItemDefinition*)S.WeaponMod) == Cat)
            {
                S.WeaponMod = (const UFortItemDefinition*)Mod;
                return true;
            }
        }

        for (int i = 0; i < Slots.Num(); i++)
        {
            auto& S = Slots.Get(i, SlotSize);

            if (!S.WeaponMod)
            {
                S.WeaponMod = (const UFortItemDefinition*)Mod;
                return true;
            }
        }

        return false;
    }
    inline int RollForPickup(UObject* Pickup, const UFortItemDefinition* Def, int Rarity)
    {
        if (!Pickup || !Def || !HasPickupNative())
            return 0;

        Discover();

        if (Discovered.empty())
            return 0;

        int Budget = RarityBudget(Rarity);

        if (Budget <= 0)
            return 0;

        std::string WeaponName = Def->Name.ToString().c_str();

        int Order[ModCategory_Count];

        for (int i = 0; i < ModCategory_Count; i++)
            Order[i] = i;

        for (int i = ModCategory_Count - 1; i > 0; i--)
        {
            int j = rand() % (i + 1);
            int Tmp = Order[i];
            Order[i] = Order[j];
            Order[j] = Tmp;
        }

        int Applied = 0;

        for (int i = 0; i < ModCategory_Count && Applied < Budget; i++)
        {
            std::vector<const UFortWeaponModItemDefinition*> Pool;

            for (auto Mod : Discovered)
                if (Category(Mod) == Order[i] && IsCompatible(Mod, WeaponName))
                    Pool.push_back(Mod);

            if (Pool.empty())
                continue;

            auto Mod = Pool[rand() % Pool.size()];

            if (UFortWeaponModFunctionLibrary::ApplyWeaponModToPickup(Pickup, (UFortWeaponModItemDefinition*)Mod))
            {
                bool Wrote = WriteEntrySlot(&((AFortPickupAthena*)Pickup)->PrimaryPickupItemEntry, Mod);
                static int wn = 0;

                if (Utils::LogBudget(wn, 12, "[Mods] entry write"))
                    printf("[Boron][Mods] %s <- %s entryWrite=%d\n", WeaponName.c_str(), Mod->Name.ToString().c_str(), Wrote);

                Applied++;
            }
        }

        static int pn = 0;

        if (Applied > 0 && Utils::LogBudget(pn, 20, "[Mods] pickup roll"))
            printf("[Boron][Mods] pickup %s rarity=%d rolled %d/%d mod(s)\n", WeaponName.c_str(), Rarity, Applied, Budget);

        return Applied;
    }

    inline int StoreFromEntry(FFortItemEntry* Src, const FGuid& Guid)
    {
        if (!Src || !FFortItemEntry::HasWeaponModSlots() || !FFortWeaponModSlot::HasWeaponMod())
            return 0;

        auto SlotSize = FFortWeaponModSlot::Size();

        if (SlotSize <= 0)
            return 0;

        RemoveStore(Guid);

        auto& Slots = Src->GetWeaponModSlots();
        int Stored = 0;

        for (int i = 0; i < Slots.Num(); i++)
        {
            auto& S = Slots.Get(i, SlotSize);

            if (S.WeaponMod && !IsDefaultMod(S.WeaponMod))
            {
                PushStore(Guid, (const UFortWeaponModItemDefinition*)S.WeaponMod);
                Stored++;
            }
        }

        return Stored;
    }

    inline int ApplyStoredToPickup(UObject* Pickup, const FGuid& Guid)
    {
        auto Entry = FindStore(Guid);

        if (!Entry || Entry->Count <= 0 || !Pickup || !HasPickupNative())
            return 0;

        int Applied = 0;

        for (int i = 0; i < Entry->Count; i++)
            if (Entry->Mods[i] && UFortWeaponModFunctionLibrary::ApplyWeaponModToPickup(Pickup, (UFortWeaponModItemDefinition*)Entry->Mods[i]))
            {
                WriteEntrySlot(&((AFortPickupAthena*)Pickup)->PrimaryPickupItemEntry, Entry->Mods[i]);
                Applied++;
            }

        return Applied;
    }
    inline void ApplyToSpawnedPickup(AFortPickupAthena* Pickup, FFortItemEntry* Source)
    {
        if (VersionInfo.EngineVersion < 5.4 || !Pickup || !Pickup->PrimaryPickupItemEntry.ItemDefinition)
            return;

        auto Def = (UFortItemDefinition*)Pickup->PrimaryPickupItemEntry.ItemDefinition;

        if (!Def->Cast<UFortWeaponItemDefinition>() || Def->Cast<UFortWeaponMeleeItemDefinition>())
            return;

        int FromStore = Source->HasItemGuid() ? ApplyStoredToPickup(Pickup, Source->ItemGuid) : 0;

        if (FromStore > 0)
        {
            RemoveStore(Source->ItemGuid);

            static int cn = 0;

            if (Utils::LogBudget(cn, 20, "[Mods] keep on drop"))
                printf("[Boron][Mods] %s kept %d mod(s) on drop (store)\n", Def->Name.ToString().c_str(), FromStore);

            return;
        }

        int Existing = EntryModCount(Source);

        if (Existing > 0)
        {
            CarryEntryMods(Source, &Pickup->PrimaryPickupItemEntry);

            int Reapplied = ReapplyEntryMods(Source, Pickup);
            static int cn2 = 0;

            if (Utils::LogBudget(cn2, 20, "[Mods] keep on drop entry"))
                printf("[Boron][Mods] %s kept %d/%d mod(s) on drop (entry)\n", Def->Name.ToString().c_str(), Reapplied, Existing);

            return;
        }

        RollForPickup(Pickup, Def, Def->HasRarity() ? (int)Def->Rarity : 0);
    }

    inline int ApplyRandom(AFortWeapon* Weapon, bool bForceOptic)
    {
        Discover();

        if (!IsSupported(Weapon))
            return 0;

        ClearAll(Weapon);

        int Applied = 0;

        if (bForceOptic)
        {
            std::vector<const UFortWeaponModItemDefinition*> Optics;

            for (auto Mod : Discovered)
                if (Category(Mod) == ModCategory_Optic && Lower(Mod->Name.ToString().c_str()).find("ironsights") == std::string::npos)
                    Optics.push_back(Mod);

            if (!Optics.empty())
            {
                std::string Reason;

                if (Apply(Weapon, Optics[rand() % Optics.size()], true, &Reason))
                    Applied++;
            }
        }

        for (int Cat = bForceOptic ? ModCategory_Barrel : ModCategory_Optic; Cat < ModCategory_Count; Cat++)
        {
            if (rand() % 5 == 0)
                continue;

            std::vector<const UFortWeaponModItemDefinition*> Pool;

            for (auto Mod : Discovered)
                if (Category(Mod) == Cat)
                    Pool.push_back(Mod);

            if (Pool.empty())
                continue;

            for (size_t k = Pool.size() - 1; k > 0; k--)
            {
                size_t j = rand() % (k + 1);
                auto Tmp = Pool[k];
                Pool[k] = Pool[j];
                Pool[j] = Tmp;
            }

            for (auto Mod : Pool)
            {
                std::string Reason;

                if (Apply(Weapon, Mod, bForceOptic, &Reason))
                {
                    Applied++;
                    break;
                }
            }
        }

        return Applied;
    }
}
