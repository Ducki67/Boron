#pragma once

// Dont ask why i made the configration like this i like it this way and its more pleasing to my eyes :sob:

/*
struct FConfiguration
{
    static inline auto Playlist = L"/Game/Athena/Playlists/Playlist_DefaultSolo.Playlist_DefaultSolo";
    static inline auto MaxTickRate = 30;
    static inline auto bLateGame = false;
    static inline auto LateGameZone = 3;          // starting zone
    static inline auto bLateGameLongZone = false; // zone doesnt close for a long time
    static inline auto bEnableCheats = true;
    static inline auto SiphonAmount = 50; // set to 0 to disable
    static inline auto bInfiniteMats = false;
    static inline auto bInfiniteAmmo = false;
    static inline auto bForceRespawns = false; // build your client with this too!
    static inline auto bJoinInProgress = false;
    static inline auto bAutoRestart = false;
    static inline auto bKeepInventory = false;
    static inline auto Port = 7777;
    static inline auto bEnableIris = true;
    static inline constexpr auto bGUI = true;
    static inline constexpr auto bCustomCrashReporter = true;
    static inline constexpr auto bUseStdoutLog = false;
    static inline constexpr auto WebhookURL = ""; // fill in if you want status to send to a webhook
};
*/




#define MANUAL_SERVER_SETUP // if defined then the gameserver WONT set up the playlist automatically  You HAVE TO click on the "Setup server" Button on the GUI
#define HITSCAN_WEAPONS // some hitscan weapons shit for ch5 8will be removed later on)

struct LategameConfig
{
    // loot settings
    static inline auto bLateGameVersionized = true;
    static inline auto bLateGameCustom = false;
    static inline wchar_t CustomShotgunItem[500] = L"/Game/Athena/Items/Weapons/WID_Shotgun_Standard_Athena_SR_Ore_T03.WID_Shotgun_Standard_Athena_SR_Ore_T03";
    static inline wchar_t CustomAssaultRifleItem[500] = L"/Game/Athena/Items/Weapons/WID_Assault_Heavy_Athena_SR_Ore_T03.WID_Assault_Heavy_Athena_SR_Ore_T03";
    static inline wchar_t CustomSniperItem[500] = L"/Game/Athena/Items/Weapons/WID_Sniper_Heavy_Athena_SR_Ore_T03.WID_Sniper_Heavy_Athena_SR_Ore_T03";
    static inline wchar_t CustomUtilItem[500] = L"/Game/Athena/Items/Consumables/ShockwaveGrenade/Athena_ShockGrenade.Athena_ShockGrenade";
    static inline int CustomUtilItemCount = 1;

    
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
    static inline auto Route = "/api/reload/vbucks"; // TODO
    static inline auto ApiKey = "-";
};

struct FConfig
{

    static inline auto bGameSessions = false; // GSS  for GSSMMs and backends

    static inline auto Playlist = L"/Game/Athena/Playlists/Playlist_DefaultSolo.Playlist_DefaultSolo";
    // static inline auto Playlist = L"/Game/Athena/Playlists/Showdown/Playlist_ShowdownAlt_Solo.Playlist_ShowdownAlt_Solo";
    // static inline auto Playlist = L"/Game/Athena/Playlists/Creative/Playlist_PlaygroundV2.Playlist_PlaygroundV2";
    // static inline auto Playlist = L"/Game/Athena/Playlists/gg/Playlist_Gg_Reverse.Playlist_Gg_Reverse"; // gungame
    // static inline auto Playlist = L"/Game/Athena/Playlists/Playground/Playlist_Playground.Playlist_Playground";

    static inline auto MaxTickRate = 30;

    static inline auto Port = 7777; // can be 7777  or 7778
    static inline auto bEnableIris = true;
    static inline constexpr auto bGUI = true;
    static inline constexpr auto bCustomCrashReporter = true;
    static inline constexpr auto bUseStdoutLog = false;

    struct CreativeModeConfig
    {
        static inline auto bCustomMap = true;
        static inline auto CustomMapDefinition = L"/Game/Playgrounds/Items/Plots/TheBlock_Season7"; // FortCreativePortal.cpp line: 76
        // default map: L"/Game/Playgrounds/Items/Plots/Temperate_Medium.Temperate_Medium"
    };
};
