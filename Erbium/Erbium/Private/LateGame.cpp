#include "pch.h"
#include "../Public/LateGame.h"
#include "../Public/Utils.h"
#include "../../FortniteGame/Public/FortInventory.h"
#include "../../FortniteGame/Public/FortLootPackage.h"


#include <d3d11.h>
#include <sstream>
#include <fstream>

#include <string>
#include <algorithm> // For std::count
#include <vector>    // For using std::vector
#include <iostream>  // For printing output
#include <Windows.h>

#include "../Public/Configuration.h"
//#include "FortniteGame/Private/FortPlayerControllerAthena.cpp"

// TODO: make  this look nicer :sob:
/* amugy t�nyleg b#zdmeg! az sz#r! */


static UEAllocatedVector<FLateGameItem> LootPoolWeapons(std::initializer_list<const char*> Prefixes)
{
    UEAllocatedVector<FLateGameItem> All, RarePlus;
    for (auto& [Id, Packages] : LootPackageMap)
    {
        for (auto Package : Packages)
        {
            if (!Package || Package->Weight <= 0.f || !Package->LootPackageID.ToString().starts_with("WorldList.AthenaLoot.Weapon."))
                continue;
            auto Def = Package->ItemDefinition.Get();
            if (!Def)
                continue;
            auto Name = Def->Name.ToString();
            if (std::none_of(Prefixes.begin(), Prefixes.end(), [&](const char* Prefix) { return Name.starts_with(Prefix) || (Name.starts_with("WID_") && Name.find(Prefix + 3) != std::string::npos); }))
                continue;
            if (std::any_of(All.begin(), All.end(), [&](FLateGameItem& Item) { return Item.Item == Def; }))
                continue;
            All.push_back(FLateGameItem(1, Def));
            if (Def->Rarity >= 2)
                RarePlus.push_back(FLateGameItem(1, Def));
        }
    }
    BORON_LOG("[Boron][LateGame] loot pool %s: %d items (%d rare+)\n", *Prefixes.begin(), (int)All.size(), (int)RarePlus.size());
    return RarePlus.size() ? RarePlus : All;
}

static FLateGameItem PickLateGameItem(UEAllocatedVector<FLateGameItem>& Items, const wchar_t* Fallback)
{
    std::erase_if(Items, [](FLateGameItem& Item) { return !Item.Item; });
    if (Items.size() == 0)
        return FLateGameItem(1, FindObject<UFortItemDefinition>(Fallback));
    return Items[rand() % Items.size()];
}

FLateGameItem LateGame::GetShotgun()
{
    UEAllocatedVector<FLateGameItem> Shotguns;
    if (LategameConfig::bLateGameVersionized)
    {
        // CH1
        if (VersionInfo.FortniteVersion >= 1.2 && VersionInfo.FortniteVersion <= 4.5)
        {
            Shotguns =
            {
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Shotgun_Standard_Athena_UC_Ore_T03.WID_Shotgun_Standard_Athena_UC_Ore_T03")), // Green
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Shotgun_Standard_Athena_C_Ore_T03.WID_Shotgun_Standard_Athena_C_Ore_T03")) // Gray
            };
        }

        // CH1 - CH2 S8
        if (VersionInfo.FortniteVersion >= 5.00 && VersionInfo.FortniteVersion <= 18.40)
        {
            Shotguns =
            {
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Shotgun_Standard_Athena_VR_Ore_T03.WID_Shotgun_Standard_Athena_VR_Ore_T03")), // Epic
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Shotgun_Standard_Athena_SR_Ore_T03.WID_Shotgun_Standard_Athena_SR_Ore_T03")) // Gold
            };
        }

        // CH4 S1
        else if (VersionInfo.FortniteVersion >= 23.0 && VersionInfo.FortniteVersion <= 23.50)
        {
            Shotguns =
            {
                 FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/MusterCoreWeapons/Items/Weapons/MusterPumpShotgun/WID_Shotgun_MusterPump_Athena_VR.WID_Shotgun_MusterPump_Athena_VR")), // Epic
                 FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/MusterCoreWeapons/Items/Weapons/MusterPumpShotgun/WID_Shotgun_MusterPump_Athena_SR.WID_Shotgun_MusterPump_Athena_SR")), // Gold
                 FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/MusterCoreWeapons/Items/Weapons/MusterPumpShotgun/WID_Shotgun_MusterPump_Athena_R.WID_Shotgun_MusterPump_Athena_R")) // Rare  
            };
        }

        // CH4 S2
        else if (VersionInfo.FortniteVersion >= 24.0 && VersionInfo.FortniteVersion <= 24.40)
        {
            Shotguns =
            {
                // thunder shotty
                 FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/MusterCoreWeapons/Items/Weapons/MusterPumpShotgun/WID_Shotgun_MusterPump_Athena_VR.WID_Shotgun_MusterPump_Athena_VR")), // Epic
                 FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/MusterCoreWeapons/Items/Weapons/MusterPumpShotgun/WID_Shotgun_MusterPump_Athena_SR.WID_Shotgun_MusterPump_Athena_SR")), // Gold
                 FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/MusterCoreWeapons/Items/Weapons/MusterPumpShotgun/WID_Shotgun_MusterPump_Athena_R.WID_Shotgun_MusterPump_Athena_R")), // Rare  

                 FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/RadicalWeaponsGameplay/Weapons/RadicalShotgunPump/WID_Shotgun_RadicalPump_Athena_VR.WID_Shotgun_RadicalPump_Athena_VR")), // Epic
                 FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/RadicalWeaponsGameplay/Weapons/RadicalShotgunPump/WID_Shotgun_RadicalPump_Athena_SR.WID_Shotgun_RadicalPump_Athena_SR")), // Gold
                 FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/RadicalWeaponsGameplay/Weapons/RadicalShotgunPump/WID_Shotgun_RadicalPump_Athena_R.WID_Shotgun_RadicalPump_Athena_R")), // Rare  
                 FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/RadicalWeaponsGameplay/Weapons/RadicalShotgunPump/WID_Shotgun_RadicalPump_Athena_UR.WID_Shotgun_RadicalPump_Athena_UR")) // Mythic
            };
        }


        // CH4 S4
        else if (VersionInfo.FortniteVersion >= 26.0 && VersionInfo.FortniteVersion <= 26.30)
        {
            Shotguns =
            {
                 FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/ChronoWeaponGameplay/Items/ChronoShotgun/WID_Shotgun_Chrono_Athena_VR.WID_Shotgun_Chrono_Athena_VR")), // Epic
                 FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/ChronoWeaponGameplay/Items/ChronoShotgun/WID_Shotgun_Chrono_Athena_SR.WID_Shotgun_Chrono_Athena_SR")), // Gold
                 FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/ChronoWeaponGameplay/Items/ChronoShotgun/WID_Shotgun_Chrono_Athena_R.WID_Shotgun_Chrono_Athena_R")), // Rare  

                 FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/HopscotchWeaponsGameplay/Items/HopscotchShotgun/WID_Shotgun_HopScotch_Athena_SR.WID_Shotgun_HopScotch_Athena_SR")), // Gold
                 FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/HopscotchWeaponsGameplay/Items/HopscotchShotgun/WID_Shotgun_HopScotch_Athena_VR.WID_Shotgun_HopScotch_Athena_VR")), // Epic
                 FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/HopscotchWeaponsGameplay/Items/HopscotchShotgun/WID_Shotgun_HopScotch_Athena_R.WID_Shotgun_HopScotch_Athena_R")) // Rare
            };
        }

        // Ch4 S5 (sOG)
        else if (VersionInfo.FortniteVersion >= 27.0 && VersionInfo.FortniteVersion <= 27.11)
        {
            Shotguns =
            {
                // pump / spaz
                FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Shotgun_Standard_Athena_SR_Ore_T03.WID_Shotgun_Standard_Athena_SR_Ore_T03")), // Gold
                FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Shotgun_Standard_Athena_VR_Ore_T03.WID_Shotgun_Standard_Athena_VR_Ore_T03")), // Epic
                FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Shotgun_Standard_Athena_UC_Ore_T03.WID_Shotgun_Standard_Athena_UC_Ore_T03")), // Rare  

                // tac shotty
                FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Shotgun_HighSemiAuto_Athena_SR_Ore_T03.WID_Shotgun_HighSemiAuto_Athena_SR_Ore_T03")), // Gold
                FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Shotgun_HighSemiAuto_Athena_VR_Ore_T03.WID_Shotgun_HighSemiAuto_Athena_VR_Ore_T03")), // Epic
                FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Shotgun_SemiAuto_Athena_VR_Ore_T03.WID_Shotgun_SemiAuto_Athena_VR_Ore_T03")) // Rare
            };
        }


        // CH5
        else if (VersionInfo.FortniteVersion >= 28.00 && VersionInfo.FortniteVersion <= 28.30)
        {
            Shotguns =
            {
                // Hammer pump
                FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaShotgun_Pump/WID_Shotgun_Pump_Paprika_Athena_UR_Boss.WID_Shotgun_Pump_Paprika_Athena_UR_Boss")), // Hammer pump Boss
                FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaShotgun_Pump/WID_Shotgun_Pump_Paprika_Athena_R.WID_Shotgun_Pump_Paprika_Athena_R")), // rare
                FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaShotgun_Pump/WID_Shotgun_Pump_Paprika_Athena_VR.WID_Shotgun_Pump_Paprika_Athena_VR")), // Epic
                FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaShotgun_Pump/WID_Shotgun_Pump_Paprika_Athena_SR.WID_Shotgun_Pump_Paprika_Athena_SR")), // Gold

            };

        }

        // CH5 S2
        /*
        else if (VersionInfo.FortniteVersion >= 29.00 && VersionInfo.FortniteVersion <= 29.40)
        {
            Shotguns =
            {
                // Hammer pump
               FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaShotgun_Pump/WID_Shotgun_Pump_Paprika_Athena_UR_Boss.WID_Shotgun_Pump_Paprika_Athena_UR_Boss")), // Hammer pump Boss
               FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaShotgun_Pump/WID_Shotgun_Pump_Paprika_Athena_R.WID_Shotgun_Pump_Paprika_Athena_R")), // rare
               FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaShotgun_Pump/WID_Shotgun_Pump_Paprika_Athena_VR.WID_Shotgun_Pump_Paprika_Athena_VR")), // Epic
               FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaShotgun_Pump/WID_Shotgun_Pump_Paprika_Athena_SR.WID_Shotgun_Pump_Paprika_Athena_SR")), // Gold
            };
        }*/

        // CH5 S3
        else if (VersionInfo.FortniteVersion >= 32.00 && LategameConfig::bPullGamemodeLootPool)
        {
            static UEAllocatedVector<FLateGameItem> Pool;
            if (Pool.empty())
                Pool = LootPoolWeapons({ "WID_Shotgun_" });
            Shotguns = Pool;
        }
        else if (VersionInfo.FortniteVersion >= 33.00)
        {
            Shotguns =
            {
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/FirePetalWeaponGameplay/Gameplay/FirePetal_PumpShotgun/WID_Shotgun_Pump_FirePetal_Athena_R.WID_Shotgun_Pump_FirePetal_Athena_R")),
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/FirePetalWeaponGameplay/Gameplay/FirePetal_PumpShotgun/WID_Shotgun_Pump_FirePetal_Athena_VR.WID_Shotgun_Pump_FirePetal_Athena_VR")),
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/FirePetalWeaponGameplay/Gameplay/FirePetal_PumpShotgun/WID_Shotgun_Pump_FirePetal_Athena_SR.WID_Shotgun_Pump_FirePetal_Athena_SR")),
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/FirePetalWeaponGameplay/Gameplay/FirePetal_FlavorShotgun/WID_Shotgun_FirePetal_Flavor_Athena_R.WID_Shotgun_FirePetal_Flavor_Athena_R")),
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/FirePetalWeaponGameplay/Gameplay/FirePetal_FlavorShotgun/WID_Shotgun_FirePetal_Flavor_Athena_VR.WID_Shotgun_FirePetal_Flavor_Athena_VR")),
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/FirePetalWeaponGameplay/Gameplay/FirePetal_FlavorShotgun/WID_Shotgun_FirePetal_Flavor_Athena_SR.WID_Shotgun_FirePetal_Flavor_Athena_SR")),
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/FirePetalWeaponGameplay/Gameplay/FirePetal_AutoShotgun/WID_Shotgun_Auto_FirePetal_Athena_R.WID_Shotgun_Auto_FirePetal_Athena_R")),
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/FirePetalWeaponGameplay/Gameplay/FirePetal_AutoShotgun/WID_Shotgun_Auto_FirePetal_Athena_VR.WID_Shotgun_Auto_FirePetal_Athena_VR")),
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/FirePetalWeaponGameplay/Gameplay/FirePetal_AutoShotgun/WID_Shotgun_Auto_FirePetal_Athena_SR.WID_Shotgun_Auto_FirePetal_Athena_SR")),
            };
        }
        else if (VersionInfo.FortniteVersion >= 32.00 && VersionInfo.FortniteVersion < 33.00)
        {
            Shotguns =
            {
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Shotgun_Standard_Athena_VR_Ore_T03.WID_Shotgun_Standard_Athena_VR_Ore_T03")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Shotgun_Standard_Athena_SR_Ore_T03.WID_Shotgun_Standard_Athena_SR_Ore_T03")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Shotgun_HighSemiAuto_Athena_VR_Ore_T03.WID_Shotgun_HighSemiAuto_Athena_VR_Ore_T03")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Shotgun_HighSemiAuto_Athena_SR_Ore_T03.WID_Shotgun_HighSemiAuto_Athena_SR_Ore_T03")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Shotgun_SemiAuto_Athena_R_Ore_T03.WID_Shotgun_SemiAuto_Athena_R_Ore_T03")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Shotgun_SemiAuto_Athena_VR_Ore_T03.WID_Shotgun_SemiAuto_Athena_VR_Ore_T03")),
            };
        }
        else if (VersionInfo.FortniteVersion >= 31.00 && VersionInfo.FortniteVersion < 32.00)
        {
            Shotguns =
            {
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/FlexLegendWeaponGameplay/Gameplay/LeverShotgun/WID_Shotgun_Lever_FlexLegend_Athena_R.WID_Shotgun_Lever_FlexLegend_Athena_R")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/FlexLegendWeaponGameplay/Gameplay/LeverShotgun/WID_Shotgun_Lever_FlexLegend_Athena_VR.WID_Shotgun_Lever_FlexLegend_Athena_VR")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/FlexLegendWeaponGameplay/Gameplay/LeverShotgun/WID_Shotgun_Lever_FlexLegend_Athena_SR.WID_Shotgun_Lever_FlexLegend_Athena_SR")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaShotgun_Pump/WID_Shotgun_Pump_Paprika_Athena_R.WID_Shotgun_Pump_Paprika_Athena_R")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaShotgun_Pump/WID_Shotgun_Pump_Paprika_Athena_VR.WID_Shotgun_Pump_Paprika_Athena_VR")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaShotgun_Pump/WID_Shotgun_Pump_Paprika_Athena_SR.WID_Shotgun_Pump_Paprika_Athena_SR")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/SunRoseWeaponsGameplay/Items/Weapons/CerberusSG/WID_Shotgun_Break_Cerberus_Athena_R.WID_Shotgun_Break_Cerberus_Athena_R")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/SunRoseWeaponsGameplay/Items/Weapons/CerberusSG/WID_Shotgun_Break_Cerberus_Athena_VR.WID_Shotgun_Break_Cerberus_Athena_VR")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/SunRoseWeaponsGameplay/Items/Weapons/CerberusSG/WID_Shotgun_Break_Cerberus_Athena_SR.WID_Shotgun_Break_Cerberus_Athena_SR")),
            };
        }
        else if (VersionInfo.FortniteVersion >= 30.00)
        {
            Shotguns =
            {
                // Hammer pump
               FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaShotgun_Pump/WID_Shotgun_Pump_Paprika_Athena_UR_Boss.WID_Shotgun_Pump_Paprika_Athena_UR_Boss")), // Hammer pump Boss
               FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaShotgun_Pump/WID_Shotgun_Pump_Paprika_Athena_R.WID_Shotgun_Pump_Paprika_Athena_R")), // rare
               FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaShotgun_Pump/WID_Shotgun_Pump_Paprika_Athena_VR.WID_Shotgun_Pump_Paprika_Athena_VR")), // Epic
               FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaShotgun_Pump/WID_Shotgun_Pump_Paprika_Athena_SR.WID_Shotgun_Pump_Paprika_Athena_SR")), // Gold

               // Gatekeeper Shotty
               FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/SunRoseWeaponsGameplay/Items/Weapons/CerberusSG/WID_Shotgun_Break_Cerberus_Athena_R.WID_Shotgun_Break_Cerberus_Athena_R")), // Rare
               FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/SunRoseWeaponsGameplay/Items/Weapons/CerberusSG/WID_Shotgun_Break_Cerberus_Athena_VR.WID_Shotgun_Break_Cerberus_Athena_VR")), // Epic
               FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/SunRoseWeaponsGameplay/Items/Weapons/CerberusSG/WID_Shotgun_Break_Cerberus_Athena_SR.WID_Shotgun_Break_Cerberus_Athena_SR")), // Gold
               FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/SunRoseWeaponsGameplay/Items/Weapons/CerberusSG/WID_Shotgun_Break_Cerberus_Athena_UR.WID_Shotgun_Break_Cerberus_Athena_UR")), // Boss / mythic

               // Combat shotty
               FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/WeaponsUpdated/Gameplay/CombatShotgun/WID_Shotgun_Moonflax_Combat_Athena_UR.WID_Shotgun_Moonflax_Combat_Athena_UR")), // Boss / mythic
               FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/WeaponsUpdated/Gameplay/CombatShotgun/WID_Shotgun_Moonflax_Combat_Athena_R.WID_Shotgun_Moonflax_Combat_Athena_R")), // Rare
               FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/WeaponsUpdated/Gameplay/CombatShotgun/WID_Shotgun_Moonflax_Combat_Athena_VR.WID_Shotgun_Moonflax_Combat_Athena_VR")), // Epic
               FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/WeaponsUpdated/Gameplay/CombatShotgun/WID_Shotgun_Moonflax_Combat_Athena_SR.WID_Shotgun_Moonflax_Combat_Athena_SR")), // Gold

            };

        }
    };
    // LG V1
    if (!LategameConfig::bLateGameVersionized)
    {
        Shotguns =
        {
            FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Shotgun_Standard_Athena_VR_Ore_T03.WID_Shotgun_Standard_Athena_VR_Ore_T03")), // pump 
            FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Shotgun_Standard_Athena_SR_Ore_T03.WID_Shotgun_Standard_Athena_SR_Ore_T03")), // pump 
        };
    };

    if (VersionInfo.FortniteVersion >= 32.00 && LategameConfig::bPullGamemodeLootPool)
    {
        static UEAllocatedVector<FLateGameItem> GamemodePool;
        if (GamemodePool.empty())
            GamemodePool = LootPoolWeapons({ "WID_Shotgun_" });
        if (!GamemodePool.empty())
            Shotguns = GamemodePool;
    }

    // custom :)
    if (LategameConfig::bLateGameCustom)
    {
        Shotguns =
        {
            FLateGameItem((uint32)LategameConfig::CustomSlot1ItemCount, FindObject<UFortItemDefinition>(LategameConfig::CustomSlot1Item)),
        };
    }



    std::cout << "LATEGAME >> (Shotguns)\n";
    return PickLateGameItem(Shotguns, L"/Game/Athena/Items/Weapons/WID_Shotgun_Standard_Athena_SR_Ore_T03.WID_Shotgun_Standard_Athena_SR_Ore_T03");
}



FLateGameItem LateGame::GetAssaultRifle()
{
    UEAllocatedVector<FLateGameItem> AssaultRifles;
    if (LategameConfig::bLateGameVersionized)
    {

        // CH1
        if (VersionInfo.FortniteVersion >= 1.2 && VersionInfo.FortniteVersion <= 4.5)
        {
            AssaultRifles =
            {
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Assault_AutoHigh_Athena_SR_Ore_T03.WID_Assault_AutoHigh_Athena_SR_Ore_T03")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Assault_AutoHigh_Athena_VR_Ore_T03.WID_Assault_AutoHigh_Athena_VR_Ore_T03"))
            };

        }

        // CH1 - Ch2 S8
        if (VersionInfo.FortniteVersion >= 5.00 && VersionInfo.FortniteVersion <= 18.40)
        {
            AssaultRifles =
            {
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Assault_AutoHigh_Athena_SR_Ore_T03.WID_Assault_AutoHigh_Athena_SR_Ore_T03")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Assault_AutoHigh_Athena_VR_Ore_T03.WID_Assault_AutoHigh_Athena_VR_Ore_T03"))
            };

        }

        // CH4 S1
        else if (VersionInfo.FortniteVersion >= 23.0 && VersionInfo.FortniteVersion <= 23.50)
        {
            AssaultRifles =
            {
                 FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/MusterCoreWeapons/Items/Weapons/MusterScopedAR/WID_Assault_MusterScoped_Athena_SR.WID_Assault_MusterScoped_Athena_SR")), // Gold
                 FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/MusterCoreWeapons/Items/Weapons/MusterScopedAR/WID_Assault_MusterScoped_Athena_VR.WID_Assault_MusterScoped_Athena_VR")),// Epic
                 FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/MusterCoreWeapons/Items/Weapons/MusterScopedAR/WID_Assault_MusterScoped_Athena_R.WID_Assault_MusterScoped_Athena_R")), // Rare

                 FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Assault_AutoHigh_Athena_R_Ore_T03.WID_Assault_AutoHigh_Athena_R_Ore_T03")), // Rare
                 FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Assault_AutoHigh_Athena_VR_Ore_T03.WID_Assault_AutoHigh_Athena_VR_Ore_T03")), // Epic
                 FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Assault_AutoHigh_Athena_SR_Ore_T03.WID_Assault_AutoHigh_Athena_SR_Ore_T03")) // Gold


            };

        }

        // CH4 S2
        else if (VersionInfo.FortniteVersion >= 24.0 && VersionInfo.FortniteVersion <= 24.40)
        {
            AssaultRifles =
            {

                 FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/MusterCoreWeapons/Items/Weapons/MusterScopedAR/WID_Assault_MusterScoped_Athena_SR.WID_Assault_MusterScoped_Athena_SR")), // Gold
                 FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/MusterCoreWeapons/Items/Weapons/MusterScopedAR/WID_Assault_MusterScoped_Athena_VR.WID_Assault_MusterScoped_Athena_VR")),// Epic
                 FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/MusterCoreWeapons/Items/Weapons/MusterScopedAR/WID_Assault_MusterScoped_Athena_R.WID_Assault_MusterScoped_Athena_R")), // Rare

                 // ch4 scar
                 FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Assault_AutoHigh_Athena_R_Ore_T03.WID_Assault_AutoHigh_Athena_R_Ore_T03")), // Rare
                 FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Assault_AutoHigh_Athena_VR_Ore_T03.WID_Assault_AutoHigh_Athena_VR_Ore_T03")), // Epic
                 FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Assault_AutoHigh_Athena_SR_Ore_T03.WID_Assault_AutoHigh_Athena_SR_Ore_T03")), // Gold

                 // havoc suppressed ar
                 FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/RadicalWeaponsGameplay/Weapons/RadicalCoreAR/WID_Assault_Radical_CoreAR_Athena_R.WID_Assault_Radical_CoreAR_Athena_R")), // Rare
                 FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/RadicalWeaponsGameplay/Weapons/RadicalCoreAR/WID_Assault_Radical_CoreAR_Athena_VR.WID_Assault_Radical_CoreAR_Athena_VR")), // Epic
                 FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/RadicalWeaponsGameplay/Weapons/RadicalCoreAR/WID_Assault_Radical_CoreAR_Athena_SR.WID_Assault_Radical_CoreAR_Athena_SR")) // Gold


            };

        }

        // CH4 S4
        else if (VersionInfo.FortniteVersion >= 26.0 && VersionInfo.FortniteVersion <= 26.30)
        {
            AssaultRifles =
            {
                // twin mag ar
                 FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/HopscotchWeaponsGameplay/Items/FlipmagAR/WID_Assault_FlipMag_Athena_SR.WID_Assault_FlipMag_Athena_SR")), // Gold
                 FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/HopscotchWeaponsGameplay/Items/FlipmagAR/WID_Assault_FlipMag_Athena_VR.WID_Assault_FlipMag_Athena_VR")),// Epic
                 FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/HopscotchWeaponsGameplay/Items/FlipmagAR/WID_Assault_FlipMag_Athena_R.WID_Assault_FlipMag_Athena_R")), // Rare

                 // havoc ar
                 FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/RadicalWeaponsGameplay/Weapons/RadicalCoreAR/WID_Assault_Radical_CoreAR_Athena_R.WID_Assault_Radical_CoreAR_Athena_R")), // Rare
                 FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/RadicalWeaponsGameplay/Weapons/RadicalCoreAR/WID_Assault_Radical_CoreAR_Athena_VR.WID_Assault_Radical_CoreAR_Athena_VR")), // Epic
                 FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/RadicalWeaponsGameplay/Weapons/RadicalCoreAR/WID_Assault_Radical_CoreAR_Athena_SR.WID_Assault_Radical_CoreAR_Athena_SR")) // Gold
                 ///FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"ID here")), // Mythic / Boss



            };

        }

        // CH4 S5 / (sOG)
        else if (VersionInfo.FortniteVersion >= 27.0 && VersionInfo.FortniteVersion <= 27.11)
        {
            AssaultRifles =
            {
                // Assault Riffle
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Assault_AutoHigh_Athena_SR_Ore_T03.WID_Assault_AutoHigh_Athena_SR_Ore_T03")), // Gold
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Assault_AutoHigh_Athena_VR_Ore_T03.WID_Assault_AutoHigh_Athena_VR_Ore_T03")),// Epic
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Assault_Auto_Athena_R_Ore_T03.WID_Assault_Auto_Athena_R_Ore_T03")), // Rare

            };

        }

        // CH5
        else if (VersionInfo.FortniteVersion >= 28.00 && VersionInfo.FortniteVersion <= 28.30)
        {
            AssaultRifles =
            {
                // All here are Hitscan

            FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaAR_DPS/HitscanWIDs/WID_Assault_Paprika_DPS_Athena_HS_UR_Boss.WID_Assault_Paprika_DPS_Athena_HS_UR_Boss")), // Boss / mythic
            FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaAR_DPS/HitscanWIDs/WID_Assault_Paprika_DPS_Athena_HS_VR.WID_Assault_Paprika_DPS_Athena_HS_VR")),// gold
            FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaAR_DPS/HitscanWIDs/WID_Assault_Paprika_DPS_Athena_HS_SR.WID_Assault_Paprika_DPS_Athena_HS_SR")), // epic
            FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaAR_DPS/HitscanWIDs/WID_Assault_Paprika_DPS_Athena_HS_R.WID_Assault_Paprika_DPS_Athena_HS_R")), // blue

            FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaAR_Infantry/HitscanWIDs/WID_Assault_Paprika_Infantry_Athena_HS_UR_Boss.WID_Assault_Paprika_Infantry_Athena_HS_UR_Boss")), // Mythic enforcer AR
            FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaAR_Infantry/HitscanWIDs/WID_Assault_Paprika_Infantry_Athena_HS_SR.WID_Assault_Paprika_Infantry_Athena_HS_SR")), // gold
            FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaAR_Infantry/HitscanWIDs/WID_Assault_Paprika_Infantry_Athena_HS_VR.WID_Assault_Paprika_Infantry_Athena_HS_VR")), // epic
            FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaAR_Infantry/HitscanWIDs/WID_Assault_Paprika_Infantry_Athena_HS_R.WID_Assault_Paprika_Infantry_Athena_HS_R")), // blue

            FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaAR_Heavy/HitscanWIDs/WID_Assault_Paprika_Heavy_Athena_HS_UR_Boss.WID_Assault_Paprika_Heavy_Athena_HS_UR_Boss")), // boss / mythic
            FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaAR_Heavy/HitscanWIDs/WID_Assault_Paprika_Heavy_Athena_HS_VR.WID_Assault_Paprika_Heavy_Athena_HS_VR")), // gold
            FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaAR_Heavy/HitscanWIDs/WID_Assault_Paprika_Heavy_Athena_HS_SR.WID_Assault_Paprika_Heavy_Athena_HS_SR")), // epic
            FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaAR_Heavy/HitscanWIDs/WID_Assault_Paprika_Heavy_Athena_HS_R.WID_Assault_Paprika_Heavy_Athena_HS_R")), // blue

            FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaAR_DPS/WID_Assault_Paprika_HITSCAN.WID_Assault_Paprika_HITSCAN")), // IDK
            };

        }

        // CH5 S2
        /*
        else if (VersionInfo.FortniteVersion >= 29.00 && VersionInfo.FortniteVersion <= 29.40)
        {
            AssaultRifles =
            {
                // All here are Hitscan

            FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaAR_DPS/HitscanWIDs/WID_Assault_Paprika_DPS_Athena_HS_UR_Boss.WID_Assault_Paprika_DPS_Athena_HS_UR_Boss")), // Boss / mythic
            FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaAR_DPS/HitscanWIDs/WID_Assault_Paprika_DPS_Athena_HS_VR.WID_Assault_Paprika_DPS_Athena_HS_VR")),// gold
            FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaAR_DPS/HitscanWIDs/WID_Assault_Paprika_DPS_Athena_HS_SR.WID_Assault_Paprika_DPS_Athena_HS_SR")), // epic
            FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaAR_DPS/HitscanWIDs/WID_Assault_Paprika_DPS_Athena_HS_R.WID_Assault_Paprika_DPS_Athena_HS_R")), // blue

            FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaAR_Infantry/HitscanWIDs/WID_Assault_Paprika_Infantry_Athena_HS_UR_Boss.WID_Assault_Paprika_Infantry_Athena_HS_UR_Boss")), // Mythic enforcer AR
            FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaAR_Infantry/HitscanWIDs/WID_Assault_Paprika_Infantry_Athena_HS_SR.WID_Assault_Paprika_Infantry_Athena_HS_SR")), // gold
            FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaAR_Infantry/HitscanWIDs/WID_Assault_Paprika_Infantry_Athena_HS_VR.WID_Assault_Paprika_Infantry_Athena_HS_VR")), // epic
            FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaAR_Infantry/HitscanWIDs/WID_Assault_Paprika_Infantry_Athena_HS_R.WID_Assault_Paprika_Infantry_Athena_HS_R")), // blue

            FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaAR_Heavy/HitscanWIDs/WID_Assault_Paprika_Heavy_Athena_HS_UR_Boss.WID_Assault_Paprika_Heavy_Athena_HS_UR_Boss")), // boss / mythic
            FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaAR_Heavy/HitscanWIDs/WID_Assault_Paprika_Heavy_Athena_HS_VR.WID_Assault_Paprika_Heavy_Athena_HS_VR")), // gold
            FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaAR_Heavy/HitscanWIDs/WID_Assault_Paprika_Heavy_Athena_HS_SR.WID_Assault_Paprika_Heavy_Athena_HS_SR")), // epic
            FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaAR_Heavy/HitscanWIDs/WID_Assault_Paprika_Heavy_Athena_HS_R.WID_Assault_Paprika_Heavy_Athena_HS_R")), // blue

            FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaAR_DPS/WID_Assault_Paprika_HITSCAN.WID_Assault_Paprika_HITSCAN")), // IDK
            };

        }
        */

        // CH5 S3
        else if (VersionInfo.FortniteVersion >= 32.00 && LategameConfig::bPullGamemodeLootPool)
        {
            static UEAllocatedVector<FLateGameItem> Pool;
            if (Pool.empty())
                Pool = LootPoolWeapons({ "WID_Assault_" });
            AssaultRifles = Pool;
        }
        else if (VersionInfo.FortniteVersion >= 33.00)
        {
            AssaultRifles =
            {
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/FlipperGameplay/Items/Weapons/CoreAR/WID_Assault_CoreAR_Athena_R.WID_Assault_CoreAR_Athena_R")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/FlipperGameplay/Items/Weapons/CoreAR/WID_Assault_CoreAR_Athena_VR.WID_Assault_CoreAR_Athena_VR")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/FlipperGameplay/Items/Weapons/CoreAR/WID_Assault_CoreAR_Athena_SR.WID_Assault_CoreAR_Athena_SR")),
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/FirePetalWeaponGameplay/Gameplay/FirePetal_AR_Fast/WID_Assault_FirePetal_Fast_Athena_R.WID_Assault_FirePetal_Fast_Athena_R")),
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/FirePetalWeaponGameplay/Gameplay/FirePetal_AR_Fast/WID_Assault_FirePetal_Fast_Athena_VR.WID_Assault_FirePetal_Fast_Athena_VR")),
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/FirePetalWeaponGameplay/Gameplay/FirePetal_AR_Fast/WID_Assault_FirePetal_Fast_Athena_SR.WID_Assault_FirePetal_Fast_Athena_SR")),
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/FirePetalWeaponGameplay/Gameplay/FirePetal_AR_Mid/WID_Assault_FirePetal_Mid_Athena_R.WID_Assault_FirePetal_Mid_Athena_R")),
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/FirePetalWeaponGameplay/Gameplay/FirePetal_AR_Mid/WID_Assault_FirePetal_Mid_Athena_VR.WID_Assault_FirePetal_Mid_Athena_VR")),
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/FirePetalWeaponGameplay/Gameplay/FirePetal_AR_Mid/WID_Assault_FirePetal_Mid_Athena_SR.WID_Assault_FirePetal_Mid_Athena_SR")),
            };
        }
        else if (VersionInfo.FortniteVersion >= 32.00 && VersionInfo.FortniteVersion < 33.00)
        {
            AssaultRifles =
            {
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Assault_AutoHigh_Athena_VR_Ore_T03.WID_Assault_AutoHigh_Athena_VR_Ore_T03")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Assault_AutoHigh_Athena_SR_Ore_T03.WID_Assault_AutoHigh_Athena_SR_Ore_T03")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Assault_Auto_Athena_R_Ore_T03.WID_Assault_Auto_Athena_R_Ore_T03")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Assault_AutoDrum_Athena_R_Ore_T03.WID_Assault_AutoDrum_Athena_R_Ore_T03")),
            };
        }
        else if (VersionInfo.FortniteVersion >= 31.00 && VersionInfo.FortniteVersion < 32.00)
        {
            AssaultRifles =
            {
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaAR_DPS/WID_Assault_Paprika_DPS_Athena_R.WID_Assault_Paprika_DPS_Athena_R")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaAR_DPS/WID_Assault_Paprika_DPS_Athena_VR.WID_Assault_Paprika_DPS_Athena_VR")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaAR_DPS/WID_Assault_Paprika_DPS_Athena_SR.WID_Assault_Paprika_DPS_Athena_SR")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/WeaponsUpdated/Gameplay/CombatAR/WID_Assault_MoonFlax_CombatAR_Athena_R.WID_Assault_MoonFlax_CombatAR_Athena_R")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/WeaponsUpdated/Gameplay/CombatAR/WID_Assault_MoonFlax_CombatAR_Athena_VR.WID_Assault_MoonFlax_CombatAR_Athena_VR")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/WeaponsUpdated/Gameplay/CombatAR/WID_Assault_MoonFlax_CombatAR_Athena_SR.WID_Assault_MoonFlax_CombatAR_Athena_SR")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/WeaponsUpdated/Gameplay/StrikerBurstAR/WID_Assault_Update_StrikerBurst_Athena_R.WID_Assault_Update_StrikerBurst_Athena_R")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/WeaponsUpdated/Gameplay/StrikerBurstAR/WID_Assault_Update_StrikerBurst_Athena_VR.WID_Assault_Update_StrikerBurst_Athena_VR")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/WeaponsUpdated/Gameplay/StrikerBurstAR/WID_Assault_Update_StrikerBurst_Athena_SR.WID_Assault_Update_StrikerBurst_Athena_SR")),
            };
        }
        else if (VersionInfo.FortniteVersion >= 30.00)
        {
            AssaultRifles =
            {
                // Tac AR
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/WeaponsUpdated/Gameplay/TacticalAR/WID_Assault_SunRose_Tactical_Athena_SR.WID_Assault_SunRose_Tactical_Athena_SR")),// gold
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/WeaponsUpdated/Gameplay/TacticalAR/WID_Assault_SunRose_Tactical_Athena_VR.WID_Assault_SunRose_Tactical_Athena_VR")), // epic
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/WeaponsUpdated/Gameplay/TacticalAR/WID_Assault_SunRose_Tactical_Athena_R.WID_Assault_SunRose_Tactical_Athena_R")), // blue

                // Enforcer AR (hitscan)
                FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaAR_Infantry/HitscanWIDs/WID_Assault_Paprika_Infantry_Athena_HS_UR_Boss.WID_Assault_Paprika_Infantry_Athena_HS_UR_Boss")), // Mythic enforcer AR
                FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaAR_Infantry/HitscanWIDs/WID_Assault_Paprika_Infantry_Athena_HS_SR.WID_Assault_Paprika_Infantry_Athena_HS_SR")), // gold
                FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaAR_Infantry/HitscanWIDs/WID_Assault_Paprika_Infantry_Athena_HS_VR.WID_Assault_Paprika_Infantry_Athena_HS_VR")), // epic
                FLateGameItem(1,FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaAR_Infantry/HitscanWIDs/WID_Assault_Paprika_Infantry_Athena_HS_R.WID_Assault_Paprika_Infantry_Athena_HS_R")) // blue
            };

        }
    };
    // LG V1
    if (!LategameConfig::bLateGameVersionized)
    {
        AssaultRifles =
        {
            FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Assault_AutoHigh_Athena_SR_Ore_T03.WID_Assault_AutoHigh_Athena_SR_Ore_T03")), // scar 
            FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Assault_AutoHigh_Athena_VR_Ore_T03.WID_Assault_AutoHigh_Athena_VR_Ore_T03")), // scar
        };
    };

    if (VersionInfo.FortniteVersion >= 32.00 && LategameConfig::bPullGamemodeLootPool)
    {
        static UEAllocatedVector<FLateGameItem> GamemodePool;
        if (GamemodePool.empty())
            GamemodePool = LootPoolWeapons({ "WID_Assault_" });
        if (!GamemodePool.empty())
            AssaultRifles = GamemodePool;
    }

    if (LategameConfig::bLateGameCustom) // yea  custom
    {
        AssaultRifles =
        {
            FLateGameItem((uint32)LategameConfig::CustomSlot2ItemCount, FindObject<UFortItemDefinition>(LategameConfig::CustomSlot2Item)),
        };

    }

    std::cout << "LATEGAME >> (AssaultRifles)\n";
    return PickLateGameItem(AssaultRifles, L"/Game/Athena/Items/Weapons/WID_Assault_AutoHigh_Athena_SR_Ore_T03.WID_Assault_AutoHigh_Athena_SR_Ore_T03");
}



FLateGameItem LateGame::GetUtility()
{
    UEAllocatedVector<FLateGameItem> Snipers;
    if (LategameConfig::bLateGameVersionized)
    {

        // CH1
        if (VersionInfo.FortniteVersion >= 1.2 && VersionInfo.FortniteVersion <= 4.5)
        {
            Snipers =
            {
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Sniper_BoltAction_Scope_Athena_SR_Ore_T03.WID_Sniper_BoltAction_Scope_Athena_SR_Ore_T03")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Pistol_Scavenger_Athena_VR_Ore_T03.WID_Pistol_Scavenger_Athena_VR_Ore_T03")),
                // zapotron (BR)
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Sniper_AMR_Athena_SR_Ore_T03.WID_Sniper_AMR_Athena_SR_Ore_T03"))
            };

        }

        // CH1 -Ch2 S8
        if (VersionInfo.FortniteVersion >= 5.00 && VersionInfo.FortniteVersion <= 18.40)
        {
            Snipers =
            {
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Sniper_BoltAction_Scope_Athena_SR_Ore_T03.WID_Sniper_BoltAction_Scope_Athena_SR_Ore_T03")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Pistol_Scavenger_Athena_VR_Ore_T03.WID_Pistol_Scavenger_Athena_VR_Ore_T03"))
            };

        }

        // CH4 S1
        else if (VersionInfo.FortniteVersion >= 23.0 && VersionInfo.FortniteVersion <= 23.50)
        {
            Snipers =
            {
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/MusterCoreWeapons/Items/Weapons/MusterQuickSMG/WID_SMG_MusterQuick_Athena_SR.WID_SMG_MusterQuick_Athena_SR")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/MusterCoreWeapons/Items/Weapons/MusterQuickSMG/WID_SMG_MusterQuick_Athena_VR.WID_SMG_MusterQuick_Athena_VR")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/MusterCoreWeapons/Items/Weapons/MusterQuickSMG/WID_SMG_MusterQuick_Athena_R.WID_SMG_MusterQuick_Athena_R")),


                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/MusterCoreWeapons/Items/Weapons/SwordDMR/WID_Sniper_NoScope_ExSword_Athena_UR_EmblemBoss.WID_Sniper_NoScope_ExSword_Athena_UR_EmblemBoss")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/MusterCoreWeapons/Items/Weapons/SwordDMR/WID_Sniper_NoScope_ExSword_Athena_SR.WID_Sniper_NoScope_ExSword_Athena_SR")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/MusterCoreWeapons/Items/Weapons/SwordDMR/WID_Sniper_NoScope_ExSword_Athena_VR.WID_Sniper_NoScope_ExSword_Athena_VR")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/MusterCoreWeapons/Items/Weapons/SwordDMR/WID_Sniper_NoScope_ExSword_Athena_R.WID_Sniper_NoScope_ExSword_Athena_R"))
            };
        }


        // CH4 S2
        else if (VersionInfo.FortniteVersion >= 24.0 && VersionInfo.FortniteVersion <= 24.40)
        {
            Snipers =
            {
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/MusterCoreWeapons/Items/Weapons/MusterQuickSMG/WID_SMG_MusterQuick_Athena_SR.WID_SMG_MusterQuick_Athena_SR")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/MusterCoreWeapons/Items/Weapons/MusterQuickSMG/WID_SMG_MusterQuick_Athena_VR.WID_SMG_MusterQuick_Athena_VR")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/MusterCoreWeapons/Items/Weapons/MusterQuickSMG/WID_SMG_MusterQuick_Athena_R.WID_SMG_MusterQuick_Athena_R")),


                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/CobraDMR/DMR/WID_DMR22_Athena_SR.WID_DMR22_Athena_SR")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/CobraDMR/DMR/WID_DMR22_Athena_VR.WID_DMR22_Athena_VR")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/CobraDMR/DMR/WID_DMR22_Athena_R.WID_DMR22_Athena_R"))
            };
        }

        // CH4 S4
        else if (VersionInfo.FortniteVersion >= 26.0 && VersionInfo.FortniteVersion <= 26.30)
        {
            Snipers =
            {
                // Suppressed Sniper
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Sniper_Suppressed_Scope_Athena_R_Ore_T03.WID_Sniper_Suppressed_Scope_Athena_R_Ore_T03")), // Rare
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Sniper_Suppressed_Scope_Athena_VR_Ore_T03.WID_Sniper_Suppressed_Scope_Athena_VR_Ore_T03")), // Epic
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Sniper_Suppressed_Scope_Athena_SR_Ore_T03.WID_Sniper_Suppressed_Scope_Athena_SR_Ore_T03")), // Gold

                // Scoped Burst smg 
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/HopscotchWeaponsGameplay/Items/ScopedBurstSMG/WID_SMG_RedDot_Athena_R.WID_SMG_RedDot_Athena_R")), // Rare
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/HopscotchWeaponsGameplay/Items/ScopedBurstSMG/WID_SMG_RedDot_Athena_VR.WID_SMG_RedDot_Athena_VR")), // Epic
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/HopscotchWeaponsGameplay/Items/ScopedBurstSMG/WID_SMG_RedDot_Athena_SR.WID_SMG_RedDot_Athena_SR")), // Gold

                // Combat SMG
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/CorruptionItems/Gameplay/Items/SMG/WID_SMG_Recoil_Athena_SR.WID_SMG_Recoil_Athena_SR")), // Gold
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/CorruptionItems/Gameplay/Items/SMG/WID_SMG_Recoil_Athena_VR.WID_SMG_Recoil_Athena_VR")), // Epic
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/CorruptionItems/Gameplay/Items/SMG/WID_SMG_Recoil_Athena_R.WID_SMG_Recoil_Athena_R")) // Rare
            };
        }

        // CH4 S5 / (sOG)
        else if (VersionInfo.FortniteVersion >= 27.0 && VersionInfo.FortniteVersion <= 27.11)
        {
            Snipers =
            {
                // Bolt Sniper
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Sniper_BoltAction_Scope_Athena_R_Ore_T03.WID_Sniper_BoltAction_Scope_Athena_R_Ore_T03")), // Rare
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Sniper_BoltAction_Scope_Athena_VR_Ore_T03.WID_Sniper_BoltAction_Scope_Athena_VR_Ore_T03")), // Epic
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Sniper_BoltAction_Scope_Athena_SR_Ore_T03.WID_Sniper_BoltAction_Scope_Athena_SR_Ore_T03")), // Gold

                // Hand Cannon
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Pistol_HandCannon_Athena_VR_Ore_T03.WID_Pistol_HandCannon_Athena_VR_Ore_T03")), // Epic
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Pistol_HandCannon_Athena_SR_Ore_T03.WID_Pistol_HandCannon_Athena_SR_Ore_T03")), // Gold
            };
        }

        // CH5 S1
        else if (VersionInfo.FortniteVersion >= 28.00 && VersionInfo.FortniteVersion <= 28.30)
        {
            Snipers =
            {
#ifdef HITSCAN_WEAPONS
                // Hyper SMG (hitscan)
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaSMG_DPS/HitscanWIDs/WID_SMG_Paprika_DPS_Athena_HS_UR_Boss.WID_SMG_Paprika_DPS_Athena_HS_UR_Boss")), // hyper smg mythic
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaSMG_DPS/HitscanWIDs/WID_SMG_Paprika_DPS_Athena_HS_SR.WID_SMG_Paprika_DPS_Athena_HS_SR")), // hyper smg gold
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaSMG_DPS/HitscanWIDs/WID_SMG_Paprika_DPS_Athena_HS_VR.WID_SMG_Paprika_DPS_Athena_HS_VR")), // hyper smg epic
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaSMG_DPS/HitscanWIDs/WID_SMG_Paprika_DPS_Athena_HS_R.WID_SMG_Paprika_DPS_Athena_HS_R")), // hyper smg blue

                // Thunder smg (hitscan)
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaSMG_Burst/HitscanWIDs/WID_SMG_Paprika_Burst_Athena_HS_SR.WID_SMG_Paprika_Burst_Athena_HS_SR")), // thunder busrt gold
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaSMG_Burst/HitscanWIDs/WID_SMG_Paprika_Burst_Athena_HS_VR.WID_SMG_Paprika_Burst_Athena_HS_VR")), // thunder busrt epic
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaSMG_Burst/HitscanWIDs/WID_SMG_Paprika_Burst_Athena_HS_R.WID_SMG_Paprika_Burst_Athena_HS_R")) // thunder busrt blue
#endif
            };
        }

        // CH S2
        /*
        else if (VersionInfo.FortniteVersion >= 29.00 && VersionInfo.FortniteVersion <= 29.40)
        {
            Snipers =
            {
#ifdef HITSCAN_WEAPONS
                // Hyper SMG (hitscan)
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaSMG_DPS/HitscanWIDs/WID_SMG_Paprika_DPS_Athena_HS_UR_Boss.WID_SMG_Paprika_DPS_Athena_HS_UR_Boss")), // hyper smg mythic
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaSMG_DPS/HitscanWIDs/WID_SMG_Paprika_DPS_Athena_HS_SR.WID_SMG_Paprika_DPS_Athena_HS_SR")), // hyper smg gold
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaSMG_DPS/HitscanWIDs/WID_SMG_Paprika_DPS_Athena_HS_VR.WID_SMG_Paprika_DPS_Athena_HS_VR")), // hyper smg epic
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaSMG_DPS/HitscanWIDs/WID_SMG_Paprika_DPS_Athena_HS_R.WID_SMG_Paprika_DPS_Athena_HS_R")), // hyper smg blue

                // Thunder smg (hitscan)
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaSMG_Burst/HitscanWIDs/WID_SMG_Paprika_Burst_Athena_HS_SR.WID_SMG_Paprika_Burst_Athena_HS_SR")), // thunder busrt gold
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaSMG_Burst/HitscanWIDs/WID_SMG_Paprika_Burst_Athena_HS_VR.WID_SMG_Paprika_Burst_Athena_HS_VR")), // thunder busrt epic
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaSMG_Burst/HitscanWIDs/WID_SMG_Paprika_Burst_Athena_HS_R.WID_SMG_Paprika_Burst_Athena_HS_R")) // thunder busrt blue
#endif
            };
        }
        */

        // CH5 S3
        else if (VersionInfo.FortniteVersion >= 32.00 && LategameConfig::bPullGamemodeLootPool)
        {
            static UEAllocatedVector<FLateGameItem> Pool;
            if (Pool.empty())
                Pool = LootPoolWeapons({ "WID_SMG_", "WID_Sniper_", "WID_DMR_" });
            Snipers = Pool;
        }
        else if (VersionInfo.FortniteVersion >= 33.00)
        {
            Snipers =
            {
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/ChronoWeaponGameplay/Items/ExplosiveRepeater/WID_Sniper_ExplosiveRepeater_Athena_VR.WID_Sniper_ExplosiveRepeater_Athena_VR")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/MotherGameplay/Items/ReactorGrade/WID_Sniper_ReactorGrade_Athena_VR_Ore_T03.WID_Sniper_ReactorGrade_Athena_VR_Ore_T03")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/MotherGameplay/Items/ReactorGrade/WID_Sniper_ReactorGrade_Athena_SR_Ore_T03.WID_Sniper_ReactorGrade_Athena_SR_Ore_T03")),
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/FirePetalWeaponGameplay/Gameplay/FirePetal_SMG/WID_SMG_FirePetal_Athena_R.WID_SMG_FirePetal_Athena_R")),
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/FirePetalWeaponGameplay/Gameplay/FirePetal_SMG/WID_SMG_FirePetal_Athena_VR.WID_SMG_FirePetal_Athena_VR")),
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/FirePetalWeaponGameplay/Gameplay/FirePetal_SMG/WID_SMG_FirePetal_Athena_SR.WID_SMG_FirePetal_Athena_SR")),
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/FirePetalWeaponGameplay/Gameplay/FirePetal_SMG_Quirky/WID_SMG_FirePetal_Quirky_Athena_R.WID_SMG_FirePetal_Quirky_Athena_R")),
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/FirePetalWeaponGameplay/Gameplay/FirePetal_SMG_Quirky/WID_SMG_FirePetal_Quirky_Athena_VR.WID_SMG_FirePetal_Quirky_Athena_VR")),
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/FirePetalWeaponGameplay/Gameplay/FirePetal_SMG_Quirky/WID_SMG_FirePetal_Quirky_Athena_SR.WID_SMG_FirePetal_Quirky_Athena_SR")),
            };
        }
        else if (VersionInfo.FortniteVersion >= 32.00 && VersionInfo.FortniteVersion < 33.00)
        {
            Snipers =
            {
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Clyde/Weapons/WID_Clyde_Sniper_Standard_Scope_Athena_VR_Ore_T03.WID_Clyde_Sniper_Standard_Scope_Athena_VR_Ore_T03")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Clyde/Weapons/WID_Clyde_Sniper_Standard_Scope_Athena_SR_Ore_T03.WID_Clyde_Sniper_Standard_Scope_Athena_SR_Ore_T03")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Pistol_AutoHeavyPDW_Athena_R_Ore_T03.WID_Pistol_AutoHeavyPDW_Athena_R_Ore_T03")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Pistol_Auto_Athena_R.WID_Pistol_Auto_Athena_R")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Pistol_Auto_Athena_VR.WID_Pistol_Auto_Athena_VR")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Pistol_Auto_Athena_SR.WID_Pistol_Auto_Athena_SR")),
            };
        }
        else if (VersionInfo.FortniteVersion >= 31.00 && VersionInfo.FortniteVersion < 32.00)
        {
            Snipers =
            {
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/FlexLegendWeaponGameplay/Gameplay/DualSMGs/WID_FlexLegend_DualSMGs_Athena_R.WID_FlexLegend_DualSMGs_Athena_R")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/FlexLegendWeaponGameplay/Gameplay/DualSMGs/WID_FlexLegend_DualSMGs_Athena_VR.WID_FlexLegend_DualSMGs_Athena_VR")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/FlexLegendWeaponGameplay/Gameplay/DualSMGs/WID_FlexLegend_DualSMGs_Athena_SR.WID_FlexLegend_DualSMGs_Athena_SR")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaSMG_DPS/WID_SMG_Paprika_DPS_Athena_R.WID_SMG_Paprika_DPS_Athena_R")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaSMG_DPS/WID_SMG_Paprika_DPS_Athena_VR.WID_SMG_Paprika_DPS_Athena_VR")),
                FLateGameItem(1, FindObject<UFortItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaSMG_DPS/WID_SMG_Paprika_DPS_Athena_SR.WID_SMG_Paprika_DPS_Athena_SR")),
            };
        }
        else if (VersionInfo.FortniteVersion >= 30.00)
        {
            Snipers =
            {

                // Harbinger SMG
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/SunRoseWeaponsGameplay/Items/Weapons/HarbingerSMG/WID_SMG_SunRose_DPS_Athena_UR_Boss.WID_SMG_SunRose_DPS_Athena_UR_Boss")), // Boss / mythic
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/SunRoseWeaponsGameplay/Items/Weapons/HarbingerSMG/WID_SMG_SunRose_DPS_Athena_SR.WID_SMG_SunRose_DPS_Athena_SR")), //  gold
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/SunRoseWeaponsGameplay/Items/Weapons/HarbingerSMG/WID_SMG_SunRose_DPS_Athena_VR.WID_SMG_SunRose_DPS_Athena_VR")), // epic
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/SunRoseWeaponsGameplay/Items/Weapons/HarbingerSMG/WID_SMG_SunRose_DPS_Athena_R.WID_SMG_SunRose_DPS_Athena_R")), // blue
#ifdef HITSCAN_WEAPONS
                // thunder smg (hitscan)
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaSMG_Burst/HitscanWIDs/WID_SMG_Paprika_Burst_Athena_HS_SR.WID_SMG_Paprika_Burst_Athena_HS_SR")), // thunder busrt gold
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaSMG_Burst/HitscanWIDs/WID_SMG_Paprika_Burst_Athena_HS_VR.WID_SMG_Paprika_Burst_Athena_HS_VR")), // thunder busrt epic
                FLateGameItem(1, FindObject<UFortWeaponRangedItemDefinition>(L"/PaprikaCoreWeapons/Items/Weapons/PaprikaSMG_Burst/HitscanWIDs/WID_SMG_Paprika_Burst_Athena_HS_R.WID_SMG_Paprika_Burst_Athena_HS_R")) // thunder busrt blue
#endif
            };
        }
    };
    // LG V1
    if (!LategameConfig::bLateGameVersionized)
    {
        Snipers =
        {
            FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Sniper_BoltAction_Scope_Athena_SR_Ore_T03.WID_Sniper_BoltAction_Scope_Athena_SR_Ore_T03")), // bolt
            FLateGameItem(1, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Pistol_Scavenger_Athena_VR_Ore_T03.WID_Pistol_Scavenger_Athena_VR_Ore_T03")) // tac smg
        };

    };

    if (VersionInfo.FortniteVersion >= 32.00 && LategameConfig::bPullGamemodeLootPool)
    {
        static UEAllocatedVector<FLateGameItem> GamemodePool;
        if (GamemodePool.empty())
            GamemodePool = LootPoolWeapons({ "WID_SMG_", "WID_Sniper_", "WID_DMR_" });
        if (!GamemodePool.empty())
            Snipers = GamemodePool;
    }

    if (LategameConfig::bLateGameCustom)
    {
        /* soon */
        Snipers =
        {
           
            FLateGameItem(LategameConfig::CustomSlot3ItemCount, FindObject<UFortItemDefinition>(LategameConfig::CustomSlot3Item)),

        };
    }

    std::cout << "LATEGAME >> (Snipers/Utils)\n";
    return PickLateGameItem(Snipers, L"/Game/Athena/Items/Weapons/WID_Sniper_BoltAction_Scope_Athena_SR_Ore_T03.WID_Sniper_BoltAction_Scope_Athena_SR_Ore_T03");
}

// TOD: add ch5 heals
FLateGameItem LateGame::GetHeal(int Slot)
{
    if (LategameConfig::bLateGameCustom)
    {
        auto CustomItem = FindObject<UFortItemDefinition>(Slot == 0 ? LategameConfig::CustomSlot4Item : LategameConfig::CustomSlot5Item);

        if (CustomItem)
            return FLateGameItem((uint32)(Slot == 0 ? LategameConfig::CustomSlot4ItemCount : LategameConfig::CustomSlot5ItemCount), CustomItem);
    }

    static UEAllocatedVector<FLateGameItem> Heals
    {
        FLateGameItem(3, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Consumables/Shields/Athena_Shields.Athena_Shields")), // big pots
        FLateGameItem(6, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Consumables/ShieldSmall/Athena_ShieldSmall.Athena_ShieldSmall")) // minis
    };

    static bool bAdded = false;
    if (!bAdded)
    {
        bAdded = true;

        auto ChugSplash = FLateGameItem(6, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Consumables/ChillBronco/Athena_ChillBronco.Athena_ChillBronco"));
        auto SlurpJuice = FLateGameItem(2, FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Consumables/PurpleStuff/Athena_PurpleStuff.Athena_PurpleStuff"));

        if (ChugSplash.Item)
            Heals.push_back(ChugSplash);

        if (SlurpJuice.Item)
            Heals.push_back(SlurpJuice);
    }

    return Heals[rand() % Heals.size()];
}

const UFortItemDefinition* LateGame::GetAmmo(EAmmoType AmmoType)
{
    static UEAllocatedVector<const UFortItemDefinition*> Ammos
    {
        FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsLight.AthenaAmmoDataBulletsLight"),
        FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Ammo/AthenaAmmoDataShells.AthenaAmmoDataShells"),
        FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsMedium.AthenaAmmoDataBulletsMedium"),
        FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Ammo/AmmoDataRockets.AmmoDataRockets"),
        FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsHeavy.AthenaAmmoDataBulletsHeavy"),

         FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Ammo/AthenaAmmoDataEnergyCell.AthenaAmmoDataEnergyCell")
    };

    return Ammos[(uint8)AmmoType];
    std::cout << "LATEGAME >> (Ammos)\n";
}

const UFortItemDefinition* LateGame::GetResource(EFortResourceType ResourceType)
{
    static UEAllocatedVector<const UFortItemDefinition*> Resources
    {
        FindObject<UFortItemDefinition>(L"/Game/Items/ResourcePickups/WoodItemData.WoodItemData"),
        FindObject<UFortItemDefinition>(L"/Game/Items/ResourcePickups/StoneItemData.StoneItemData"),
        FindObject<UFortItemDefinition>(L"/Game/Items/ResourcePickups/MetalItemData.MetalItemData")
    };
    std::cout << "LATEGAME >> (Mats)\n";


    return Resources[(uint8)ResourceType];


}