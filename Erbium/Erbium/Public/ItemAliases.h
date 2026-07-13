#pragma once

#include <string>
#include <unordered_map>
#include <algorithm>
#include <cctype>

namespace BoronItems
{
    inline std::string ToLower(const std::string& In)
    {
        std::string Out = In;
        std::transform(Out.begin(), Out.end(), Out.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        return Out;
    }

    inline const std::unordered_map<std::string, std::string>& RaritySuffixes()
    {
        static const std::unordered_map<std::string, std::string> Map = {
            { "c", "Common" },
            { "common", "Common" },
            { "grey", "Common" },
            { "gray", "Common" },
            { "uc", "Uncommon" },
            { "uncommon", "Uncommon" },
            { "green", "Uncommon" },
            { "r", "Rare" },
            { "rare", "Rare" },
            { "blue", "Rare" },
            { "vr", "Epic" },
            { "epic", "Epic" },
            { "purple", "Epic" },
            { "sr", "Legendary" },
            { "legendary", "Legendary" },
            { "gold", "Legendary" },
            { "orange", "Legendary" },
            { "m", "Mythic" },
            { "mythic", "Mythic" },
            { "e", "Exotic" },
            { "exotic", "Exotic" }
        };
        return Map;
    }

    inline const std::unordered_map<std::string, std::string>& Aliases()
    {
        static const std::unordered_map<std::string, std::string> Map = {
            { "pump", "Pump Shotgun" },
            { "pumpvr", "Pump Shotgun" },
            { "tac", "Tactical Shotgun" },
            { "tacshotgun", "Tactical Shotgun" },
            { "combatshotgun", "Combat Shotgun" },
            { "combatsg", "Combat Shotgun" },
            { "charge", "Charge Shotgun" },
            { "chargesg", "Charge Shotgun" },
            { "heavyshotgun", "Heavy Shotgun" },
            { "leversg", "Lever Action Shotgun" },
            { "drumsg", "Drum Shotgun" },
            { "doublebarrel", "Double Barrel Shotgun" },
            { "db", "Double Barrel Shotgun" },
            { "scar", "Assault Rifle" },
            { "ar", "Assault Rifle" },
            { "assault", "Assault Rifle" },
            { "burst", "Burst Assault Rifle" },
            { "burstar", "Burst Assault Rifle" },
            { "heavyar", "Heavy Assault Rifle" },
            { "scoped", "Scoped Assault Rifle" },
            { "scopedar", "Scoped Assault Rifle" },
            { "tacar", "Tactical Assault Rifle" },
            { "famas", "Burst Assault Rifle" },
            { "infantry", "Infantry Rifle" },
            { "hunting", "Hunting Rifle" },
            { "smg", "Submachine Gun" },
            { "submachine", "Submachine Gun" },
            { "tacsmg", "Tactical Submachine Gun" },
            { "compact", "Compact SMG" },
            { "compactsmg", "Compact SMG" },
            { "suppressedsmg", "Suppressed Submachine Gun" },
            { "p90", "Compact SMG" },
            { "pistol", "Pistol" },
            { "revolver", "Revolver" },
            { "deagle", "Hand Cannon" },
            { "handcannon", "Hand Cannon" },
            { "suppressedpistol", "Suppressed Pistol" },
            { "dualpistols", "Dual Pistols" },
            { "sixshooter", "Six Shooter" },
            { "tacpistol", "Tactical Pistol" },
            { "sniper", "Bolt-Action Sniper Rifle" },
            { "bolt", "Bolt-Action Sniper Rifle" },
            { "boltsniper", "Bolt-Action Sniper Rifle" },
            { "heavysniper", "Heavy Sniper Rifle" },
            { "semisniper", "Semi-Auto Sniper Rifle" },
            { "autosniper", "Automatic Sniper Rifle" },
            { "huntingrifle", "Hunting Rifle" },
            { "rpg", "Rocket Launcher" },
            { "rocket", "Rocket Launcher" },
            { "rocketlauncher", "Rocket Launcher" },
            { "grenadelauncher", "Grenade Launcher" },
            { "gl", "Grenade Launcher" },
            { "quadlauncher", "Quad Launcher" },
            { "guidedmissile", "Guided Missile" },
            { "minigun", "Minigun" },
            { "lmg", "Light Machine Gun" },
            { "grenade", "Grenade" },
            { "nade", "Grenade" },
            { "clinger", "Clinger" },
            { "stink", "Stink Bomb" },
            { "shockwave", "Shockwave Grenade" },
            { "impulse", "Impulse Grenade" },
            { "boogie", "Boogie Bomb" },
            { "remoteexplosives", "Remote Explosives" },
            { "c4", "Remote Explosives" },
            { "dynamite", "Dynamite" },
            { "bandages", "Bandages" },
            { "bandage", "Bandages" },
            { "medkit", "Med-Kit" },
            { "shield", "Shield Potion" },
            { "shieldpotion", "Shield Potion" },
            { "minishield", "Small Shield Potion" },
            { "minis", "Small Shield Potion" },
            { "slurp", "Slurp Juice" },
            { "chugjug", "Chug Jug" },
            { "chug", "Chug Jug" },
            { "chugsplash", "Chug Splash" },
            { "splash", "Chug Splash" },
            { "medmist", "Med Mist" },
            { "wood", "Wood" },
            { "brick", "Stone" },
            { "stone", "Stone" },
            { "metal", "Metal" },
            { "lightammo", "Light Bullets" },
            { "mediumammo", "Medium Bullets" },
            { "heavyammo", "Heavy Bullets" },
            { "shellammo", "Shells" },
            { "rocketammo", "Rockets" },
            { "launchpad", "Launch Pad" },
            { "cozycampfire", "Cozy Campfire" },
            { "campfire", "Cozy Campfire" },
            { "porta", "Port-A-Fort" },
            { "portafort", "Port-A-Fort" },
            { "bush", "Bush" },
            { "grappler", "Grappler" },
            { "rifttogo", "Rift-To-Go" },
            { "balloons", "Balloons" }
        };
        return Map;
    }

    inline bool Resolve(const std::string& Input, std::string& NameToken, std::string& RarityName)
    {
        std::string Cleaned;
        for (char c : Input)
            if (c != ' ')
                Cleaned += c;

        std::string Lowered = ToLower(Cleaned);
        std::string Base = Lowered;
        RarityName.clear();

        auto Underscore = Lowered.find_last_of('_');
        if (Underscore != std::string::npos)
        {
            std::string Suffix = Lowered.substr(Underscore + 1);
            auto RarityIt = RaritySuffixes().find(Suffix);
            if (RarityIt != RaritySuffixes().end())
            {
                RarityName = RarityIt->second;
                Base = Lowered.substr(0, Underscore);
            }
        }

        auto AliasIt = Aliases().find(Base);
        if (AliasIt != Aliases().end())
        {
            NameToken = AliasIt->second;
            return true;
        }

        NameToken.clear();
        return false;
    }
}
