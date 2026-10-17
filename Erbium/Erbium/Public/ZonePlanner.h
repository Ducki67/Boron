#pragma once
#include "../../pch.h"
#include <map>
#include <mutex>
#include <string>
#include <vector>

struct FZonePoint
{
    double X = 0;
    double Y = 0;
};

struct FZonePoi
{
    std::string Name;
    double X = 0;
    double Y = 0;
};

struct FZoneDot
{
    double X = 0;
    double Y = 0;
    int Team = 0;
    bool bBot = false;
};

struct FZonePins
{
    bool bStart = false;
    bool bEnd = false;
    FZonePoint Start;
    FZonePoint End;
    uint32_t Seed = 1;
    float Wander = 0.55f;
    std::map<int, FZonePoint> Phase;
    bool bFree = false;
};

struct FZoneAlign
{
    double OffU = 0;
    double OffV = 0;
    double Scale = 1;
    int Rot = 0;
    std::string Image = "auto";
};

struct FZonePreset
{
    std::string Map;
    std::string Name;
    FZonePins Pins;
};

struct FZoneSettings
{
    bool bEnabled = false;
    bool bNormalMatches = false;
    bool bRandomPoi = false;
    bool bNewSeedEachMatch = false;
};

struct FZoneView
{
    bool bWorld = false;
    std::string MapKey;
    std::string MapName;
    int ProjSource = 0;
    double U0 = 0, UX = 0, UY = 0, V0 = 0, VX = 0, VY = 0;
    FZonePoint MapCenter;
    std::vector<double> Radii;
    int StartPhase = 1;
    std::vector<FZonePoi> Pois;
    std::vector<FZoneDot> Dots;
    std::vector<FZonePoint> Actual;
    bool bHasStorm = false;
    FZonePoint StormCenter;
    double StormRadius = 0;
    FZonePoint NextCenter;
    double NextRadius = 0;
    int CurrentPhase = -1;
    bool bHasBus = false;
    FZonePoint Bus;
    bool bApplied = false;
    bool bEndPulled = false;
    std::string LastEvent;
};

class ZonePlanner
{
public:
    static void Tick();
    static void Apply(TArray<FVector>& Locations);
    static bool ApplyLegacy(const FVector& Seed);
    static bool PlannedCenter(int Index, FVector& Out);
    static void RequestRetarget();
    static bool IsSupported();
    static bool IsApplied();
    static void SetBlocked(bool);
    static bool IsArrayLayout();

    static FZoneView GetView();
    static FZoneSettings GetSettings();
    static void SetSettings(const FZoneSettings&);
    static FZonePins GetPins(const std::string& MapKey);
    static void SetPins(const std::string& MapKey, const FZonePins&);
    static FZoneAlign GetAlign(const std::string& MapKey);
    static void SetAlign(const std::string& MapKey, const FZoneAlign&);
    static std::vector<FZonePreset> GetPresets(const std::string& MapKey);
    static void SavePreset(const std::string& MapKey, const std::string& Name, const FZonePins&);
    static void DeletePreset(const std::string& MapKey, const std::string& Name);

    static int StartPhaseIndex();
    static FZonePins Rebase(const FZonePins& Pins, int OldStart, int NewStart);
    static std::vector<FZonePoint> BuildChain(const FZonePins& Pins, bool bRandomPoi, const std::vector<double>& Radii, int StartPhase,
                                              const std::vector<FZonePoint>& Base, const std::vector<FZonePoi>& Pois, FZonePoint MapCenter, bool* bEndPulled = nullptr);
    static std::string ModuleDir();
};
