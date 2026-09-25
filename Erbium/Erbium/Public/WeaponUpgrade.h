#pragma once

#include "pch.h"
#include <functional>
#include "../../Engine/Public/CurveTable.h"
#include "../../Engine/Public/DataTable.h"
#include "../../FortniteGame/Public/FortKismetLibrary.h"
#include "../../FortniteGame/Public/FortPlayerPawnAthena.h"
#include "../../FortniteGame/Public/FortPlayerControllerAthena.h"
#include "../../FortniteGame/Public/FortInventory.h"

namespace WeaponUpgrade
{
    enum : uint8
    {
        DirectionNotSet = 0,
        DirectionVertical = 1,
        DirectionHorizontal = 2
    };

    enum : uint8
    {
        MaterialWood = 0,
        MaterialMetal = 1,
        MaterialBrick = 2,
        MaterialCount = 3
    };

    enum : uint8
    {
        RarityLegendary = 4
    };

    struct FRow
    {
        uint8 Base[8];
        const UFortItemDefinition* CurrentWeaponDef;
        const UFortItemDefinition* UpgradedWeaponDef;
        uint8 Cost[MaterialCount];
        uint8 Direction;
    };
    static_assert(sizeof(FRow) == 0x20);

    struct FHeld
    {
        AActor* Weapon = nullptr;
        const UFortItemDefinition* Def = nullptr;
        FFortItemEntry* Entry = nullptr;
    };

    inline UDataTable* Table()
    {
        static auto Table = (UDataTable*)FindObject<UDataTable>(L"/Game/Items/Datatables/AthenaWumbaData.AthenaWumbaData");
        return Table;
    }

    inline UDataTable* TableOf(AActor* Bench)
    {
        if (Bench)
        {
            auto Offset = Bench->GetOffset("UpgradeTable");
            if (Offset != (uint32)-1)
                if (auto Rows = *(UDataTable**)((uint8*)Bench + Offset))
                    return Rows;
        }
        return Table();
    }

    inline const FRow* FindRow(const UFortItemDefinition* Def, uint8 Direction, UDataTable* Rows = Table())
    {
        if (!Rows || !Def)
            return nullptr;
        for (auto& [Key, Val] : Rows->GetRowMap())
        {
            auto Row = (const FRow*)Val;
            if (Row && Row->CurrentWeaponDef == Def && Row->Direction == Direction && Row->UpgradedWeaponDef)
                return Row;
        }
        return nullptr;
    }

    inline const UFortItemDefinition* MaterialDef(AActor* Bench, uint8 Material)
    {
        static const char* Props[] = { "WoodItem", "MetalItem", "BrickItem" };
        static const wchar_t* Paths[] = { L"/Game/Items/ResourcePickups/WoodItemData.WoodItemData", L"/Game/Items/ResourcePickups/MetalItemData.MetalItemData",
                                          L"/Game/Items/ResourcePickups/StoneItemData.StoneItemData" };
        if (Material >= MaterialCount)
            return nullptr;
        if (Bench)
        {
            auto Offset = Bench->GetOffset(Props[Material]);
            if (Offset != (uint32)-1)
                if (auto Def = *(const UFortItemDefinition**)((uint8*)Bench + Offset))
                    return Def;
        }
        return FindObject<UFortItemDefinition>(Paths[Material]);
    }

    inline int32 FallbackCost(uint8 Cost)
    {
        static const int32 Costs[] = { 0, 10, 100, 150, 200, 10, 100, 150, 200, 10, 100, 150, 200, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20 };
        return Cost < sizeof(Costs) / sizeof(Costs[0]) ? Costs[Cost] : 0;
    }

    inline float CostLevel(uint8 Cost)
    {
        if (Cost >= 1 && Cost <= 12)
            return (float)((Cost - 1) % 4 + 1);
        if (Cost >= 13 && Cost <= 27)
            return (float)((Cost - 13) % 5);
        return 0.f;
    }

    inline int32 MaterialCost(AActor* Bench, uint8 Material, uint8 Cost)
    {
        static const char* Curves[] = { "WoodCostCurve", "MetalCostCurve", "BrickCostCurve", "HorizontalWoodCostCurve", "HorizontalMetalCostCurve", "HorizontalBrickCostCurve" };
        if (Cost == 0 || Cost > 27 || Material >= MaterialCount)
            return 0;
        if (Bench)
        {
            auto Offset = Bench->GetOffset(Curves[Material + (Cost >= 13 ? MaterialCount : 0)]);
            if (Offset != (uint32)-1)
            {
                auto& Curve = *(FScalableFloat*)((uint8*)Bench + Offset);
                if (Curve.Curve.CurveTable && Curve.Curve.RowName.IsValid())
                {
                    auto Value = Curve.Evaluate(CostLevel(Cost));
                    if (Value > 0.f)
                        return (int32)(Value + 0.5f);
                }
            }
        }
        return FallbackCost(Cost);
    }

    inline int32 CountOf(AFortInventory* Inventory, const UFortItemDefinition* Def)
    {
        int32 Count = 0;
        if (!Inventory || !Def)
            return 0;
        for (int32 i = 0; i < Inventory->Inventory.ReplicatedEntries.Num(); i++)
        {
            auto& Entry = Inventory->Inventory.ReplicatedEntries.Get(i, FFortItemEntry::Size());
            if (Entry.ItemDefinition == Def)
                Count += Entry.Count;
        }
        return Count;
    }

    inline bool Take(AFortInventory* Inventory, const UFortItemDefinition* Def, int32 Amount)
    {
        if (Amount <= 0)
            return true;
        if (CountOf(Inventory, Def) < Amount)
            return false;
        while (Amount > 0)
        {
            FFortItemEntry* Entry = nullptr;
            for (int32 i = 0; i < Inventory->Inventory.ReplicatedEntries.Num() && !Entry; i++)
            {
                auto& Candidate = Inventory->Inventory.ReplicatedEntries.Get(i, FFortItemEntry::Size());
                if (Candidate.ItemDefinition == Def && Candidate.Count > 0)
                    Entry = &Candidate;
            }
            if (!Entry)
                return false;
            if (Amount >= Entry->Count)
            {
                Amount -= Entry->Count;
                Inventory->Remove(Entry->ItemGuid);
                continue;
            }
            Entry->Count -= Amount;
            Amount = 0;
            for (int32 i = 0; i < Inventory->Inventory.ItemInstances.Num(); i++)
            {
                auto Item = Inventory->Inventory.ItemInstances[i];
                if (Item && Item->ItemEntry.ItemGuid == Entry->ItemGuid)
                {
                    Item->ItemEntry.Count = Entry->Count;
                    Item->ItemEntry.bIsDirty = true;
                }
            }
            Inventory->UpdateEntry(*Entry);
        }
        return true;
    }

    inline bool GetHeld(AFortPlayerControllerAthena* PC, FHeld& Out)
    {
        if (!PC || !PC->WorldInventory || !PC->MyFortPawn)
            return false;
        auto Weapon = PC->MyFortPawn->CurrentWeapon;
        if (!Weapon)
            return false;
        static auto DataOffset = Weapon->GetOffset("WeaponData");
        static auto GuidOffset = Weapon->GetOffset("ItemEntryGuid");
        if (DataOffset == (uint32)-1 || GuidOffset == (uint32)-1)
            return false;
        auto Def = *(const UFortItemDefinition**)((uint8*)Weapon + DataOffset);
        auto& Guid = *(FGuid*)((uint8*)Weapon + GuidOffset);
        if (!Def)
            return false;
        auto& Entries = PC->WorldInventory->Inventory.ReplicatedEntries;
        for (int32 i = 0; i < Entries.Num(); i++)
        {
            auto& Entry = Entries.Get(i, FFortItemEntry::Size());
            if (Entry.ItemGuid == Guid && Entry.ItemDefinition == Def)
            {
                Out.Weapon = Weapon;
                Out.Def = Def;
                Out.Entry = &Entry;
                return true;
            }
        }
        return false;
    }

    inline bool ReplaceHeld(AFortPlayerControllerAthena* PC, const FHeld& Held, const UFortItemDefinition* NewDef)
    {
        if (!PC || !Held.Entry || !NewDef)
            return false;
        auto OldGuid = Held.Entry->ItemGuid;
        auto LoadedAmmo = Held.Entry->LoadedAmmo;
        PC->WorldInventory->Remove(OldGuid);
        auto Item = PC->WorldInventory->GiveItem(NewDef, 1, LoadedAmmo);
        if (!Item)
            return false;
        auto& Entry = Item->ItemEntry;
        if (auto WeaponDef = NewDef->Cast<UFortWeaponItemDefinition>())
            PC->MyFortPawn->EquipWeaponDefinition(WeaponDef, Entry.ItemGuid, Entry.HasTrackerGuid() ? Entry.TrackerGuid : FGuid(), false);
        return true;
    }

    inline const UFortItemDefinition* VerticalToRarity(const UFortItemDefinition* Def, uint8 Rarity)
    {
        if (!Def)
            return nullptr;
        return (const UFortItemDefinition*)UFortKismetLibrary::GetUpgradedWeaponItemVerticalToRarity(Def, Rarity);
    }

    inline const UFortItemDefinition* NextRarity(const UFortItemDefinition* Def)
    {
        if (!Def || !Def->IsA(UFortWeaponItemDefinition::StaticClass()) || Def->Rarity >= RarityLegendary)
            return nullptr;
        auto Upgraded = VerticalToRarity(Def, (uint8)(Def->Rarity + 1));
        if (Upgraded && Upgraded != Def)
            return Upgraded;
        if (auto Row = FindRow(Def, DirectionVertical))
            return Row->UpgradedWeaponDef;
        return nullptr;
    }

    inline void UseBench(AFortPlayerControllerAthena* PC, AActor* Bench, uint8 InteractionBeingAttempted, const std::function<void()>& CallOriginal)
    {
        FHeld Held;
        uint8 Direction = InteractionBeingAttempted == 1 ? DirectionHorizontal : DirectionVertical;
        auto Row = GetHeld(PC, Held) ? FindRow(Held.Def, Direction, TableOf(Bench)) : nullptr;
        if (!Row)
        {
            CallOriginal();
            return;
        }
        auto Inventory = PC->WorldInventory;
        const UFortItemDefinition* Materials[MaterialCount];
        int32 Before[MaterialCount];
        for (uint8 i = 0; i < MaterialCount; i++)
        {
            Materials[i] = MaterialDef(Bench, i);
            Before[i] = CountOf(Inventory, Materials[i]);
        }
        auto HeldDef = Held.Def;
        CallOriginal();
        FHeld After;
        if (!GetHeld(PC, After) || After.Def != HeldDef)
            return;
        bool bNativeCharged = false;
        for (uint8 i = 0; i < MaterialCount; i++)
            bNativeCharged |= CountOf(Inventory, Materials[i]) < Before[i];
        int32 Costs[MaterialCount];
        for (uint8 i = 0; i < MaterialCount; i++)
            Costs[i] = bNativeCharged ? 0 : MaterialCost(Bench, i, Row->Cost[i]);
        for (uint8 i = 0; i < MaterialCount; i++)
        {
            if (CountOf(Inventory, Materials[i]) < Costs[i])
            {
                printf("[Boron][Bench] %s needs wood=%d metal=%d brick=%d\n", HeldDef->Name.ToString().c_str(), Costs[0], Costs[1], Costs[2]);
                return;
            }
        }
        for (uint8 i = 0; i < MaterialCount; i++)
            Take(Inventory, Materials[i], Costs[i]);
        if (GetHeld(PC, After) && After.Def == HeldDef && ReplaceHeld(PC, After, Row->UpgradedWeaponDef))
            printf("[Boron][Bench] %s -> %s dir=%d charged=%s wood=%d metal=%d brick=%d\n", HeldDef->Name.ToString().c_str(), Row->UpgradedWeaponDef->Name.ToString().c_str(), (int)Direction,
                   bNativeCharged ? "native" : "boron", Costs[0], Costs[1], Costs[2]);
    }
}
