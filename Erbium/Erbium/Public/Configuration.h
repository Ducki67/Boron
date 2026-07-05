#pragma once

// Dont ask why i made the configration like this i like it this way and its more pleasing to my eyes :sob:


#define MANUAL_SERVER_SETUP // if defined then the gameserver WONT set up the playlist automatically  You HAVE TO click on the "Setup server" Button on the GUI
#define HITSCAN_WEAPONS // some hitscan weapons shit for ch5 (will be removed later on)

struct LategameConfig
{
    // loot settings
    static inline auto bLateGameVersionized = true;
    static inline auto bLateGameCustom = false;
    static inline wchar_t CustomShotgunItem[500] = L"/Game/Athena/Items/Weapons/WID_Shotgun_Standard_Athena_SR_Ore_T03.WID_Shotgun_Standard_Athena_SR_Ore_T03";
    static inline wchar_t CustomAssaultRifleItem[500] = L"/Game/Items/Weapons/Ranged/Assault/Auto/WID_OB_Assault_Auto_C_Ore_T01.WID_OB_Assault_Auto_C_Ore_T01"; // scar i think / added so the godl AK on  som seasons If not exist wont crash ur game
    static inline wchar_t CustomSniperItem[500] = L"/Game/Athena/Items/Weapons/WID_Sniper_Heavy_Athena_SR_Ore_T03.WID_Sniper_Heavy_Athena_SR_Ore_T03";
    static inline wchar_t CustomUtilItem[500] = L"/Game/Athena/Items/Consumables/ShockwaveGrenade/Athena_ShockGrenade.Athena_ShockGrenade";
    static inline int CustomUtilItemCount = 1;

    // zones
    static inline auto bLateGame = false;
    static inline auto LateGameZone = 3;          // starting zone
    static inline auto bLateGameLongZone = false; // zone does not close for a long time


};

struct GameRuleConfig
{
    static inline auto bEnableCheats = true;
    static inline auto SiphonAmount = 50; // set to 0 to disable / TODO: make this work for  <s3
    static inline auto bInfiniteMats = true;
    static inline auto bInfiniteAmmo = true;

    // Respawn settings
    static inline auto bForceRespawns = false;      // build your client with this too!
    static inline int RespawnHightClient = 10000;   // respawn hight value (Client.cpp Line: 173)
    static inline int RespawnTimeClient = 5;        // respawnt time for clients as value (Client.cpp Line: 179)
    static inline int RespawnHightGamemode = 10000; // (FortGamemode.cpp Line: 74)
    static inline int RespawnTimeGamemode = 5;      // (FortGamemode.cpp Line: 80)

    static inline auto bJoinInProgress = false;
    static inline auto bAutoRestart = false;
    static inline auto bKeepInventory = false;
};

struct DiscordWebhookConfig
{
    static inline constexpr auto WebhookURL = ""; // fill in if you want status to send to a webhook
    // static inline auto DeveloperWebhookURL = ""; // fill in if you want developer status/crash logs etc to send to a webhook
};

struct RewardConfig // V-Bucks on Kill/Win   // TODO
{
    static inline auto bEnableVBucksReward = false;
    static inline auto RewardOnKill = true;
    static inline auto RewardOnWin = true;
    static inline auto BackendBaseURL = "http://127.0.0.1:3551";
    static inline auto Route = "/api/fnn_backend/vbucks"; // TODO
    static inline auto ApiKey = "-";
};

struct FConfig
{

    static inline auto bGameSessions = false; // GSS  for GSSMMs and backends (TDOD: wayy more later tho but ill add it)

    static inline auto Playlist = L"/Game/Athena/Playlists/Playlist_DefaultSolo.Playlist_DefaultSolo";
    // static inline auto Playlist = L"/Game/Athena/Playlists/Showdown/Playlist_ShowdownAlt_Solo.Playlist_ShowdownAlt_Solo";
    // static inline auto Playlist = L"/Game/Athena/Playlists/Creative/Playlist_PlaygroundV2.Playlist_PlaygroundV2";  // creative
   
    // static inline auto Playlist = L"/Game/Athena/Playlists/Playground/Playlist_Playground.Playlist_Playground"; // playfround
    
    
    /// LTMs (still working on them kinda) 
    
    // static inline auto Playlist = L"/Game/Athena/Playlists/Sneaky/Playlist_Sneaky_Solo.Playlist_Sneaky_Solo"; // Fog of War  (works but no marking when dealing dmg to players)
    // static inline auto Playlist = L"/Game/Athena/Playlists/Titanium/Playlist_Titanium_Solo.Playlist_Titanium_Solo"; // Rags to Riches (its s13 only and TODO: weapon uprgadas)
    // static inline auto Playlist = L"/Game/Athena/Playlists/Tutorial/Playlist_Tutorial_1.Playlist_Tutorial_1"; // tutorial  (works lmao)
    // static inline auto Playlist = L"/Game/Athena/Playlists/gg/Playlist_Gg_Reverse.Playlist_Gg_Reverse"; // gungame (maybe ill add this but dont ask bruh :/ )


    /* static inline auto Playlist = L"";
    static inline auto Playlist = L"";
    static inline auto Playlist = L"";*/




    static inline auto MaxTickRate = 30;

    static inline auto Port = 7777; // can be 7777  or 7778
    static inline auto bEnableIris = true;
    static inline constexpr auto bGUI = true;
    static inline constexpr auto bCustomCrashReporter = true;
    static inline constexpr auto bUseStdoutLog = false;

    struct CreativeModeConfig
    {
        // Note: dont ask me to add Island slecting for the creative protal menu NO i wont give that to you skids hell no!
        static inline auto bCustomMap = true;
        static inline auto CustomMapDefinition = L"/Game/Playgrounds/Items/Plots/TheBlock_Season7"; //  The Block map | see at FortCreativePortal.cpp line: 76 what it does
        // default map: L"/Game/Playgrounds/Items/Plots/Temperate_Medium.Temperate_Medium"
    };
};
