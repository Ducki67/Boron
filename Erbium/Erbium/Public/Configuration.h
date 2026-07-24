#pragma once

// Dont ask why i made the configration like this i like it this way and its more pleasing to my eyes :sob:


//#define MANUAL_SERVER_SETUP // if defined then the gameserver WONT set up the playlist automatically  You HAVE TO click on the "Setup server" Button on the GUI
#define HITSCAN_WEAPONS // some hitscan weapons shit for ch5 (will be removed later on)
///#define AUTOHOSTER_CONFIGURATOR // this will be for autohoster exes so the exe it self can change all configs even if the dll is compiled  (Coming later)


/*
S30+ exepermenttal support notes:
set the Playlist id's path to start with "/BRPlaylists/..."
set bSaveConsoleLog to true
set bUseStdoutLog to true
set bGUI to false (for now this is very needed)

*/



struct LategameConfig
{
    // loot settings
    static inline auto bLateGameVersionized = true;
    static inline auto bLateGameCustom = false;
    static inline wchar_t CustomSlot1Item[500] = L"/Game/Athena/Items/Weapons/WID_Shotgun_Standard_Athena_SR_Ore_T03.WID_Shotgun_Standard_Athena_SR_Ore_T03";
    static inline int CustomSlot1ItemCount = 1;
    static inline wchar_t CustomSlot2Item[500] = L"/Game/Athena/Items/Weapons/WID_Assault_AutoHigh_Athena_SR_Ore_T03.WID_Assault_AutoHigh_Athena_SR_Ore_T03"; // scar i think / added so the godl AK on  som seasons If not exist wont crash ur game
    static inline int CustomSlot2ItemCount = 1;
    static inline wchar_t CustomSlot3Item[500] = L"/Game/Athena/Items/Consumables/ShockwaveGrenade/Athena_ShockGrenade.Athena_ShockGrenade";
    static inline int CustomSlot3ItemCount = 1;
    static inline wchar_t CustomSlot4Item[500] = L"/Game/Athena/Items/Consumables/Shields/Athena_Shields.Athena_Shields";
    static inline int CustomSlot4ItemCount = 3;
    static inline wchar_t CustomSlot5Item[500] = L"/Game/Athena/Items/Consumables/ShieldSmall/Athena_ShieldSmall.Athena_ShieldSmall";
    static inline int CustomSlot5ItemCount = 6;

    // zones
    static inline auto bLateGame = false;
    static inline auto LateGameZone = 3;          // starting zone
    static inline auto bLateGameLongZone = false; // zone does not close for a long time
    static inline auto bLateGameMovingBus = false;


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
    static inline auto bCreativeExtraAmmo = false; // this is buggy btw! fuckes inventory up a bit (dont use it unless u wanna see the bug)
    static inline auto bBossAI = false; // VERY VERY experimental (might not be fixed later) - patrol/chase/shoot + mythic/keycard drops
   
    static inline auto bCH5AutoPickupWeapons = false; //temp shit dont use it its ass
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

    // NOTE:  /BRPlaylists/  is the correct path AND "durian"  doesnt exist on s30+
    //  also if u looking for playlist ids look at: https://github.com/Ducki67/OGFN-Build-Dumps  i have some ids there and might upload some more
    
    
    static inline wchar_t Playlist[9999] = L"/Game/Athena/Playlists/Playlist_DefaultSolo.Playlist_DefaultSolo";
    
    // static inline wchar_t Playlist[9999] = L"/Game/Athena/Playlists/Showdown/Playlist_ShowdownAlt_Solo.Playlist_ShowdownAlt_Solo";
    // static inline wchar_t Playlist[9999] = L"/Game/Athena/Playlists/Creative/Playlist_PlaygroundV2.Playlist_PlaygroundV2";  // creative
   
    // static inline wchar_t Playlist[9999] = L"/Game/Athena/Playlists/Playground/Playlist_Playground.Playlist_Playground"; // playground
    
    
    /// LTMs (still working on them kinda) 
    
    // static inline wchar_t Playlist[9999] = L"/Game/Athena/Playlists/Sneaky/Playlist_Sneaky_Solo.Playlist_Sneaky_Solo"; // Fog of War  (works but no marking when dealing dmg to players)
    // static inline wchar_t Playlist[9999] = L"/Game/Athena/Playlists/Titanium/Playlist_Titanium_Solo.Playlist_Titanium_Solo"; // Rags to Riches (its s13 only and TODO: weapon uprgadas)
    // static inline wchar_t Playlist[9999] = L"/Game/Athena/Playlists/Tutorial/Playlist_Tutorial_1.Playlist_Tutorial_1"; // tutorial  (works lmao)
    // static inline wchar_t Playlist[9999] = L"/Game/Athena/Playlists/gg/Playlist_Gg_Reverse.Playlist_Gg_Reverse"; // gungame (maybe ill add this but dont ask bruh :/ )


    /*  31.41 Testing playlists*/
    // static inline wchar_t Playlist[9999] = L"/BRPlaylists/Athena/Playlists/Playlist_DefaultSolo.Playlist_DefaultSolo"; // BR
    //static inline wchar_t Playlist[9999] = L"/BlastBerry/Playlists/Playlist_SunflowerSolo.Playlist_SunflowerSolo"; // venture Reload (testing)



    /* static inline wchar_t Playlist[9999] = L"";
    static inline wchar_t Playlist[9999] = L"";
    static inline wchar_t Playlist[9999] = L"";*/




    static inline auto MaxTickRate = 30;

    static inline auto Port = 7777; // can be 7777  or 7778
    static inline auto bEnableIris = true;
    static inline constexpr auto bGUI = true;
    static inline constexpr auto bCustomCrashReporter = true;
    static inline constexpr auto bUseStdoutLog = false;
    static inline constexpr auto bSaveConsoleLog = false; // for 31.41 use this to check for some logs Boron_Console.txt  btw) 

    struct CreativeModeConfig
    {   // TODO: fix island not laoding on s7 creative  i swear its the same code but on s8+ works but not on s7 what thge helly :sob:
        // Note: dont ask me to add Island slecting for the creative protal menu NO i wont give that to you skids hell no!
        static inline auto bCustomMap = false;
        static inline wchar_t CustomMapDefinition[9999] = L"/Game/Playgrounds/Items/Plots/TheBlock_Season7"; //  The Block map | see at FortCreativePortal.cpp line: 76 what it does
        // default map: L"/Game/Playgrounds/Items/Plots/Temperate_Medium.Temperate_Medium"
    };

    // TODO: fix teh creative inv shit



    struct GuiShit
    {
        static inline auto bPlayBuildsResetAnimation = true; // on some builds might crash  (later ill add it)
    };

};
