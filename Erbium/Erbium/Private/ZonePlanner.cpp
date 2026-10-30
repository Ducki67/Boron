#include "pch.h"
#include "../Public/ZonePlanner.h"
#include "../Public/Configuration.h"
#include "../Public/Utils.h"
#include "../../FortniteGame/Public/BattleRoyaleGamePhaseLogic.h"
#include "../../FortniteGame/Public/FortGameMode.h"
#include "../../FortniteGame/Public/FortGameStateAthena.h"
#include "../../FortniteGame/Public/FortSafeZoneIndicator.h"
#include "../../FortniteGame/Public/FortPlayerStateAthena.h"
#include "../../FortniteGame/Public/FortPlayerPawnAthena.h"
#include "../../FortniteGame/Public/GameplayTagContainer.h"

#include <algorithm>
#include <atomic>
#include <random>
#include <cmath>
#include <fstream>
#include <sstream>

namespace
{
    struct FSrwMutex
    {
        SRWLOCK Lock = SRWLOCK_INIT;
        void lock() { AcquireSRWLockExclusive(&Lock); }
        void unlock() { ReleaseSRWLockExclusive(&Lock); }
    };
    FSrwMutex Mtx;
    FZoneView View;
    FZoneSettings Settings;
    std::map<std::string, FZonePins> PinsByMap;
    std::map<std::string, FZoneAlign> AlignByMap;
    std::vector<FZonePreset> Presets;
    bool bLoaded = false;
    std::atomic<bool> bRetarget{ false };
    std::atomic<bool> bBlocked{ false };
    std::vector<FZonePoint> LegacyChain;
    double LegacyStartRadius = 0;
    std::atomic<bool> bArrayRadii{ false };
    constexpr double Ch1MapW = 258000.0, Ch1MapX0 = 160883.0, Ch1MapY0 = -154975.0;
    void* SeededWorld = nullptr;

    struct FZoneRng
    {
        uint64_t State;

        double Next()
        {
            State += 0x9E3779B97F4A7C15ull;
            uint64_t Z = State;
            Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
            Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
            Z ^= Z >> 31;
            return (double)(Z >> 11) * (1.0 / 9007199254740992.0);
        }
    };

    std::string FilePath()
    {
        return ZonePlanner::ModuleDir() + "BoronZones.txt";
    }

    void WritePins(std::ostream& Out, const FZonePins& P)
    {
        Out << (int)P.bStart << " " << P.Start.X << " " << P.Start.Y << " " << (int)P.bEnd << " " << P.End.X << " " << P.End.Y << " " << P.Seed << " " << P.Wander << " " << P.Phase.size();
        for (auto& [K, Pt] : P.Phase)
            Out << " " << K << " " << Pt.X << " " << Pt.Y;
        Out << " " << (int)P.bFree;
    }

    bool ReadPins(std::istream& In, FZonePins& P)
    {
        int bS = 0, bE = 0;
        if (!(In >> bS >> P.Start.X >> P.Start.Y >> bE >> P.End.X >> P.End.Y >> P.Seed >> P.Wander))
            return false;
        P.bStart = bS != 0;
        P.bEnd = bE != 0;
        P.Phase.clear();
        int Count = 0;
        if (In >> Count)
            for (int i = 0; i < Count && i < 64; i++)
            {
                int K = 0;
                FZonePoint Pt;
                if (!(In >> K >> Pt.X >> Pt.Y))
                    break;
                P.Phase[K] = Pt;
            }
        int Free = 0;
        P.bFree = (In >> Free) && Free != 0;
        In.clear();
        return true;
    }

    void SaveLocked()
    {
        std::ofstream Out(FilePath(), std::ios::trunc);
        if (!Out)
            return;
        Out.precision(10);
        Out << "settings " << (int)Settings.bEnabled << " " << (int)Settings.bNormalMatches << " " << (int)Settings.bRandomPoi << " " << (int)Settings.bNewSeedEachMatch << "\n";
        for (auto& [Key, P] : PinsByMap)
        {
            Out << "pins " << Key << " ";
            WritePins(Out, P);
            Out << "\n";
        }
        for (auto& [Key, A] : AlignByMap)
            Out << "align " << Key << " " << A.OffU << " " << A.OffV << " " << A.Scale << " " << A.Rot << " " << A.Image << "\n";
        for (auto& Pr : Presets)
        {
            Out << "preset " << Pr.Map << " ";
            WritePins(Out, Pr.Pins);
            Out << " " << Pr.Name << "\n";
        }
    }

    void LoadLocked()
    {
        if (bLoaded)
            return;
        bLoaded = true;
        std::ifstream In(FilePath());
        std::string Line;
        while (std::getline(In, Line))
        {
            std::istringstream Ss(Line);
            std::string Kind, Key;
            Ss >> Kind;
            if (Kind == "settings")
            {
                int a = 0, b = 0, c = 0, d = 0;
                if (Ss >> a >> b >> c >> d)
                    Settings = { a != 0, b != 0, c != 0, d != 0 };
            }
            else if (Kind == "pins" && Ss >> Key)
            {
                FZonePins P;
                if (ReadPins(Ss, P))
                    PinsByMap[Key] = P;
            }
            else if (Kind == "align" && Ss >> Key)
            {
                FZoneAlign A;
                if (Ss >> A.OffU >> A.OffV >> A.Scale >> A.Rot >> A.Image)
                {
                    if (A.Image.empty() || isdigit((unsigned char)A.Image[0]))
                        A.Image = "auto";
                    AlignByMap[Key] = A;
                }
            }
            else if (Kind == "preset" && Ss >> Key)
            {
                FZonePreset Pr;
                Pr.Map = Key;
                if (!ReadPins(Ss, Pr.Pins))
                    continue;
                std::getline(Ss, Pr.Name);
                Pr.Name.erase(0, Pr.Name.find_first_not_of(' '));
                if (!Pr.Name.empty())
                    Presets.push_back(Pr);
            }
        }
    }

    std::string Lower(std::string S)
    {
        std::transform(S.begin(), S.end(), S.begin(), [](unsigned char c) { return (char)tolower(c); });
        return S;
    }

    std::string PrettyTag(const std::string& Tag)
    {
        auto Dot = Tag.find_last_of('.');
        std::string Seg = Dot == std::string::npos ? Tag : Tag.substr(Dot + 1);
        std::string Out;
        for (size_t i = 0; i < Seg.size(); i++)
        {
            char c = Seg[i];
            if (c == '_')
            {
                if (!Out.empty() && Out.back() != ' ')
                    Out += ' ';
                continue;
            }
            if (i > 0 && isupper((unsigned char)c) && (islower((unsigned char)Seg[i - 1]) || (i + 1 < Seg.size() && islower((unsigned char)Seg[i + 1]) && isupper((unsigned char)Seg[i - 1]))) && !Out.empty() && Out.back() != ' ')
                Out += ' ';
            Out += c;
        }
        return Out;
    }

    std::string Narrow(const UEAllocatedString& S)
    {
        return std::string(S.c_str());
    }

    UObject* GetMapManager(AFortGameStateAthena* GameState)
    {
        static auto ManagerClass = FindClass("FortInGameMapManager");
        if (!ManagerClass)
            return nullptr;
        static auto Offset = GameState->GetOffset("UIMapManager");
        if (Offset == (uint32_t)-1)
            return nullptr;
        auto Manager = *(UObject**)(__int64(GameState) + Offset);
        return Manager && Manager->IsA(ManagerClass) ? Manager : nullptr;
    }

    std::string RemixStage()
    {
        static std::string Stage;
        static uint64_t Next = 0;
        if (GetTickCount64() >= Next)
        {
            Next = GetTickCount64() + 10000;
            static auto TexClass = FindClass("Texture2D");
            std::string Found;
            for (int i = 0; TexClass && i < TUObjectArray::Num(); i++)
            {
                auto Object = (UObject*)TUObjectArray::GetObjectByIndex(i);
                if (!Object || !Object->IsA(TexClass))
                    continue;
                auto Name = Narrow(Object->Name.ToString());
                if (Name.starts_with("Week") && Name.ends_with("_Adjusted") && Name > Found)
                    Found = Name;
            }
            if (!Found.empty())
            {
                Found = Found.substr(0, Found.size() - 9);
                if (Found != Stage)
                    BORON_LOG("[Boron][Zones] remix map stage %s\n", Found.c_str());
                Stage = Found;
            }
        }
        if (!Stage.empty())
            return Stage;
        return VersionInfo.FortniteVersion >= 32.10 ? "Week4_Day5" : "Week1";
    }

    std::string DetectMapKey(AFortGameStateAthena* GameState, std::string& Pretty)
    {
        std::string Text;
        if (auto Manager = GetMapManager(GameState))
        {
            Text += Narrow(Manager->Class->Name.ToString());
            auto MatOffset = VersionInfo.FortniteVersion >= 28 ? Manager->GetOffset("MapMaterial") : (uint32_t)-1;
            if (MatOffset != (uint32_t)-1)
                if (auto Mat = *(UObject**)(__int64(Manager) + MatOffset))
                    Text += "|" + Narrow(Mat->Name.ToString());
        }
        char Pl[1024];
        sprintf_s(Pl, "|%ls", FConfig::Playlist);
        Text = Lower(Text + Pl);

        double V = VersionInfo.FortniteVersion;
        if (Text.find("dashberry") != std::string::npos)
        {
            Pretty = "Reload (DashBerry island)";
            return "DashBerry_Terrain";
        }
        if (Text.find("punchberry") != std::string::npos)
        {
            Pretty = "Reload (PunchBerry island)";
            return "PunchBerry_Terrain";
        }
        if (Text.find("blastberry") != std::string::npos)
        {
            Pretty = "Reload (BlastBerry island)";
            return "BlastBerry_Terrain";
        }
        if (Text.find("figment") != std::string::npos)
        {
            if (Text.find("s03") != std::string::npos)
            {
                Pretty = "Fortnite OG (Season 3 island)";
                return "Athena_Terrain_S03";
            }
            if (Text.find("s02") != std::string::npos)
            {
                Pretty = "Fortnite OG (Season 2 island)";
                return "Athena_Terrain_S02";
            }
            Pretty = "Fortnite OG (Season 1 island)";
            return "Athena_Terrain_S01";
        }
        if (V >= 32.0 && V < 33.0)
        {
            auto Stage = RemixStage();
            Pretty = "Chapter 2 Remix (" + Stage + ")";
            std::replace(Pretty.begin(), Pretty.end(), '_', ' ');
            return "Apollo_Terrain_Retro_" + Stage;
        }
        struct FChapter
        {
            double Below;
            const char* Key;
            const char* Name;
        };
        static const FChapter Chapters[] = {
            { 10.5, "Athena_Terrain", "Chapter 1 island" },
            { 19.0, "Apollo_Terrain", "Chapter 2 island" },
            { 23.0, "Artemis_Terrain", "Chapter 3 island" },
            { 27.0, "Asteria_Terrain", "Chapter 4 island" },
            { 28.0, "Athena_Terrain", "Fortnite OG (Chapter 4 OG season)" },
            { 32.0, "Helios_Terrain", "Chapter 5 island" },
            { 33.0, "Helios_Terrain", "Chapter 2 Remix (Chapter 5 Season 5)" },
            { 38.0, "Hermes_Terrain", "Chapter 6 island" },
            { 1e9, "Hermes_Terrain", "Chapter 6 Simpsons season" },
        };
        for (auto& C : Chapters)
            if (V < C.Below)
            {
                Pretty = C.Name;
                return C.Key;
            }
        Pretty = "Battle Royale";
        return "Hermes_Terrain";
    }

    bool CallGuarded(UObject* Object, UFunction* Function, void* Params)
    {
        __try
        {
            Object->ProcessEvent(Function, Params);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    bool SampleMap(UObject* Manager, UFunction* Function, uint32_t LocOff, uint32_t SizeOff, uint32_t RetOff, uint32_t ParamsSize, double X, double Y, double& U, double& V)
    {
        alignas(16) uint8_t Buf[0x400]{};
        if (ParamsSize > sizeof(Buf) || RetOff + 16 > sizeof(Buf))
            return false;
        bool bLwc = VersionInfo.FortniteVersion >= 20;
        if (bLwc)
        {
            *(double*)(Buf + LocOff) = X;
            *(double*)(Buf + LocOff + 8) = Y;
        }
        else
        {
            *(float*)(Buf + LocOff) = (float)X;
            *(float*)(Buf + LocOff + 4) = (float)Y;
        }
        *(float*)(Buf + SizeOff) = 1.f;
        if (!CallGuarded(Manager, Function, Buf))
            return false;
        U = bLwc ? *(double*)(Buf + RetOff) : *(float*)(Buf + RetOff);
        V = bLwc ? *(double*)(Buf + RetOff + 8) : *(float*)(Buf + RetOff + 4);
        return std::isfinite(U) && std::isfinite(V);
    }

    bool Calibrate(AFortGameStateAthena* GameState, FZonePoint Center, FZoneView& Out)
    {
        auto Manager = GetMapManager(GameState);
        auto Function = Manager ? Manager->GetFunction("BPWorldLocationToMapLocation") : nullptr;
        if (Function)
        {
            auto Params = Function->GetParams();
            if (Params.NameOffsetMap.size() >= 3)
            {
                uint32_t LocOff = Params.NameOffsetMap[0].Offset;
                uint32_t SizeOff = Params.NameOffsetMap[1].Offset;
                uint32_t RetOff = Params.NameOffsetMap[2].Offset;
                for (auto& P : Params.NameOffsetMap)
                    if (P.PropertyFlags & 0x400)
                        RetOff = P.Offset;
                const double D = 10000.0;
                double u0, v0, u1, v1, u2, v2;
                if (SampleMap(Manager, Function, LocOff, SizeOff, RetOff, Params.Size, Center.X, Center.Y, u0, v0) &&
                    SampleMap(Manager, Function, LocOff, SizeOff, RetOff, Params.Size, Center.X + D, Center.Y, u1, v1) &&
                    SampleMap(Manager, Function, LocOff, SizeOff, RetOff, Params.Size, Center.X, Center.Y + D, u2, v2))
                {
                    double UX = (u1 - u0) / D, UY = (u2 - u0) / D, VX = (v1 - v0) / D, VY = (v2 - v0) / D;
                    double Det = fabs(UX * VY - UY * VX);
                    double Scale = sqrt(Det);
                    if (Scale > 2e-7 && Scale < 5e-5 && u0 > -0.25 && u0 < 1.25 && v0 > -0.25 && v0 < 1.25)
                    {
                        Out.UX = UX;
                        Out.UY = UY;
                        Out.VX = VX;
                        Out.VY = VY;
                        Out.U0 = u0 - UX * Center.X - UY * Center.Y;
                        Out.V0 = v0 - VX * Center.X - VY * Center.Y;
                        Out.ProjSource = 1;
                        return true;
                    }
                    static int Rejects = 0;
                    if (Rejects++ < 3)
                        BORON_LOG("[Boron][Zones] map projection rejected: c=(%.4f,%.4f) dx=(%.4f,%.4f) dy=(%.4f,%.4f)\n", u0, v0, u1, v1, u2, v2);
                }
            }
        }

        if (VersionInfo.FortniteVersion >= 20 && GameState->MapInfo && GameState->MapInfo->HasCachedPlayableBoundsForClients())
        {
            auto& Bounds = GameState->MapInfo->CachedPlayableBoundsForClients;
            double Ox = Bounds.Origin.X, Oy = Bounds.Origin.Y;
            double E = (std::max)((double)Bounds.BoxExtent.X, (double)Bounds.BoxExtent.Y);
            if (E > 1000.0 && std::isfinite(Ox) && std::isfinite(Oy))
            {
                Out.UX = 0;
                Out.UY = 1.0 / (2 * E);
                Out.U0 = -(Oy - E) / (2 * E);
                Out.VX = -1.0 / (2 * E);
                Out.VY = 0;
                Out.V0 = (Ox + E) / (2 * E);
                Out.ProjSource = 2;
                return true;
            }
        }

        if (VersionInfo.FortniteVersion < 10.20)
        {
            Out.UX = 0;
            Out.UY = 1.0 / Ch1MapW;
            Out.U0 = -Ch1MapY0 / Ch1MapW;
            Out.VX = -1.0 / Ch1MapW;
            Out.VY = 0;
            Out.V0 = Ch1MapX0 / Ch1MapW;
            Out.ProjSource = 4;
            return true;
        }

        static auto MapManagerClass = FindClass("FortInGameMapManager");
        if (VersionInfo.FortniteVersion < 10.20 && MapManagerClass)
        {
            TArray<AActor*> Managers;
            Utils::GetAll(MapManagerClass, Managers);
            auto Manager = Managers.Num() > 0 ? Managers[0] : nullptr;
            Managers.Free();
            auto CaptureOff = Manager ? Manager->GetOffset("SceneCapture") : (uint32_t)-1;
            auto Capture = CaptureOff != (uint32_t)-1 ? *(AActor**)(__int64(Manager) + CaptureOff) : nullptr;
            auto CompOff = Capture ? Capture->GetOffset("CaptureComponent2D") : (uint32_t)-1;
            auto Comp = CompOff != (uint32_t)-1 ? *(UObject**)(__int64(Capture) + CompOff) : nullptr;
            auto OrthoOff = Comp ? Comp->GetOffset("OrthoWidth") : (uint32_t)-1;
            double Ortho = OrthoOff != (uint32_t)-1 ? *(float*)(__int64(Comp) + OrthoOff) : 0.0;
            auto Loc = Capture ? Capture->K2_GetActorLocation() : FVector();
            static int Logged = 0;
            if (Logged++ < 3)
                BORON_LOG("[Boron][Zones] map capture: manager=%p capture=%p ortho=%.0f at=(%.0f,%.0f) center=(%.0f,%.0f)\n", (void*)Manager, (void*)Capture, Ortho, (double)Loc.X, (double)Loc.Y, Center.X,
                          Center.Y);
            if (Ortho > 10000.0 && std::isfinite(Ortho))
            {
                double E = Ortho / 2;
                Out.UX = 0;
                Out.UY = 1.0 / (2 * E);
                Out.U0 = -(Loc.Y - E) / (2 * E);
                Out.VX = -1.0 / (2 * E);
                Out.VY = 0;
                Out.V0 = (Loc.X + E) / (2 * E);
                Out.ProjSource = 3;
                return true;
            }
        }
        return false;
    }

    std::vector<FZonePoi> CollectPois()
    {
        std::vector<FZonePoi> Out;
        std::vector<int> Counts;
        auto AddPoi = [&](const std::string& Name, double X, double Y)
        {
            if (Name.find_first_not_of('?') == std::string::npos)
                return;
            auto It = std::find_if(Out.begin(), Out.end(), [&](const FZonePoi& P) { return P.Name == Name; });
            if (It == Out.end())
            {
                Out.push_back({ Name, X, Y });
                Counts.push_back(1);
                return;
            }
            auto Idx = It - Out.begin();
            int n = ++Counts[Idx];
            It->X += (X - It->X) / n;
            It->Y += (Y - It->Y) / n;
        };

        auto TitleCase = [](std::string S)
        {
            bool bStart = true;
            for (auto& C : S)
            {
                C = bStart ? (char)toupper((unsigned char)C) : (char)tolower((unsigned char)C);
                bStart = C == ' ' || C == '-';
            }
            return S;
        };

        if (VersionInfo.FortniteVersion < 10.20)
        {
            static auto WsClass = FindClass("FortWorldSettings");
            static auto LocStruct = SDK::FindStruct("MapLocation");
            UObject* Ws = nullptr;
            for (int i = 0; WsClass && LocStruct && !Ws && i < TUObjectArray::Num(); i++)
            {
                auto Object = (UObject*)TUObjectArray::GetObjectByIndex(i);
                if (Object && !Object->IsDefaultObject() && Object->IsA(WsClass))
                    Ws = Object;
            }
            auto LocsOff = Ws ? Ws->GetOffset("MapLocations") : (uint32_t)-1;
            if (LocsOff != (uint32_t)-1)
            {
                auto& Locs = *(TArray<uint8_t>*)(__int64(Ws) + LocsOff);
                int Stride = LocStruct->GetPropertiesSize();
                for (int i = 0; Locs.GetData() && Stride > 0x20 && i < Locs.Num() && i < 64; i++)
                {
                    auto Elem = (uint8_t*)Locs.GetData() + (size_t)i * Stride;
                    auto Name = Narrow(UKismetTextLibrary::Conv_TextToString(*(FText*)Elem).ToString());
                    auto Pos = (float*)(Elem + 0x18);
                    if (Name.size() < 2 || !std::isfinite(Pos[0]) || !std::isfinite(Pos[1]))
                        continue;
                    AddPoi(TitleCase(Name), Ch1MapX0 - Pos[1] * Ch1MapW, Ch1MapY0 + Pos[0] * Ch1MapW);
                }
            }
            struct FCh1Poi
            {
                const char* Name;
                double U, V;
            };
            static const FCh1Poi Season1[] = {
                    { "Anarchy Acres", 0.530, 0.213 }, { "Pleasant Park", 0.283, 0.289 }, { "Wailing Woods", 0.823, 0.268 }, { "Tomato Town", 0.660, 0.333 }, { "Loot Lake", 0.435, 0.381 },
                    { "Lonely Lodge", 0.894, 0.422 },  { "Dusty Depot", 0.580, 0.471 },   { "Retail Row", 0.740, 0.531 },    { "Greasy Grove", 0.230, 0.617 }, { "Salty Springs", 0.563, 0.631 },
                    { "Fatal Fields", 0.598, 0.754 },  { "Moisty Mire", 0.830, 0.780 },   { "Flush Factory", 0.355, 0.876 },
            };
            static const FCh1Poi Season2[] = {
                { "Junk Junction", 0.195, 0.115 }, { "Haunted Hills", 0.255, 0.165 }, { "Snobby Shores", 0.090, 0.445 }, { "Shifty Shafts", 0.240, 0.485 }, { "Tilted Towers", 0.367, 0.483 },
            };
            static const FCh1Poi Season4[] = {
                { "Junk Junction", 0.195, 0.115 }, { "Haunted Hills", 0.255, 0.165 }, { "Snobby Shores", 0.090, 0.445 }, { "Shifty Shafts", 0.240, 0.485 }, { "Tilted Towers", 0.367, 0.483 },
                { "Risky Reels", 0.715, 0.180 },   { "Lucky Landing", 0.505, 0.885 },
            };
            if (Out.empty())
            {
                double FN = VersionInfo.FortniteVersion;
                for (auto& P : Season1)
                    if (FN < 4.0 || strcmp(P.Name, "Dusty Depot"))
                        AddPoi(P.Name, Ch1MapX0 - P.V * Ch1MapW, Ch1MapY0 + P.U * Ch1MapW);
                if (FN >= 4.0)
                    AddPoi("Dusty Divot", Ch1MapX0 - 0.471 * Ch1MapW, Ch1MapY0 + 0.580 * Ch1MapW);
                if (FN >= 2.0)
                {
                    auto Extra = FN >= 4.0 ? Season4 : Season2;
                    size_t Count = FN >= 4.0 ? std::size(Season4) : std::size(Season2);
                    for (size_t k = 0; k < Count; k++)
                        AddPoi(Extra[k].Name, Ch1MapX0 - Extra[k].V * Ch1MapW, Ch1MapY0 + Extra[k].U * Ch1MapW);
                }
            }
            static int MapLocLogs = 0;
            if (MapLocLogs++ < 3)
                BORON_LOG("[Boron][Zones] map labels: settings=%p labels=%zu\n", (void*)Ws, Out.size());
            if (Out.size() >= 3)
                return Out;
        }

        static auto DiscClass = FindClass("FortPoi_DiscoverableComponent");
        static auto ActorClass = FindClass("Actor");
        if (DiscClass && ActorClass)
        {
            static uint32_t NameOff = (uint32_t)-2, DisableOff = (uint32_t)-2;
            for (int i = 0; i < TUObjectArray::Num(); i++)
            {
                auto Object = (UObject*)TUObjectArray::GetObjectByIndex(i);
                if (!Object || Object->IsDefaultObject() || !Object->IsA(DiscClass))
                    continue;
                auto Owner = Object->Outer;
                if (!Owner || Owner->IsDefaultObject() || !Owner->IsA(ActorClass))
                    continue;
                if (NameOff == (uint32_t)-2)
                {
                    NameOff = Object->GetOffset("PlayerFacingName");
                    DisableOff = Object->GetOffset("bDisableMapLocationText");
                }
                if (NameOff == (uint32_t)-1)
                    break;
                if (DisableOff != (uint32_t)-1 && *(bool*)(__int64(Object) + DisableOff))
                    continue;
                auto Name = Narrow(UKismetTextLibrary::Conv_TextToString(*(FText*)(__int64(Object) + NameOff)).ToString());
                while (!Name.empty() && isspace((unsigned char)Name.back()))
                    Name.pop_back();
                while (!Name.empty() && isspace((unsigned char)Name.front()))
                    Name.erase(Name.begin());
                if (Name.size() < 2)
                    continue;
                auto Loc = ((AActor*)Owner)->K2_GetActorLocation();
                if (Loc.X == 0 && Loc.Y == 0)
                    continue;
                AddPoi(Name, (double)Loc.X, (double)Loc.Y);
            }
            if (Out.size() >= 3)
            {
                static int DiscLogs = 0;
                if (DiscLogs++ < 3)
                {
                    BORON_LOG("[Boron][Zones] places from map labels: %zu\n", Out.size());
                    for (size_t k = 0; DiscLogs == 1 && k < Out.size(); k++)
                        BORON_LOG("[Boron][Zones]   place %s (%.0f,%.0f) n=%d\n", Out[k].Name.c_str(), Out[k].X, Out[k].Y, Counts[k]);
                }
                std::sort(Out.begin(), Out.end(), [](const FZonePoi& A, const FZonePoi& B) { return A.Name < B.Name; });
                return Out;
            }
            Out.clear();
            Counts.clear();
        }

        static auto PoiClass = FindClass("FortPoiVolume");
        if (!PoiClass)
            return Out;

        std::vector<AActor*> Volumes;
        auto World = UWorld::GetWorld();
        auto GameState = World ? (UObject*)World->GameState : nullptr;
        auto ManagerOffset = GameState ? GameState->GetOffset("PoiManager") : (uint32_t)-1;
        auto Manager = ManagerOffset != (uint32_t)-1 ? *(UObject**)(__int64(GameState) + ManagerOffset) : nullptr;
        if (Manager)
            for (auto Field : { "NamedOnMapPoiVolumes", "AllPoiVolumes" })
            {
                auto Offset = Manager->GetOffset(Field);
                if (Offset == (uint32_t)-1)
                    continue;
                auto& Arr = *(TArray<AActor*>*)(__int64(Manager) + Offset);
                for (int i = 0; i < Arr.Num() && i < 4096; i++)
                    if (Arr[i])
                        Volumes.push_back(Arr[i]);
                if (!Volumes.empty())
                    break;
            }
        TArray<AActor*> Found;
        if (Volumes.empty())
        {
            Utils::GetAll(PoiClass, Found);
            for (int i = 0; i < Found.Num(); i++)
                if (Found[i])
                    Volumes.push_back(Found[i]);
        }

        for (auto Volume : Volumes)
        {
            auto TagsOffset = Volume->GetOffset("LocationTags");
            if (TagsOffset == (uint32_t)-1)
                continue;
            auto& Tags = *(TArray<uint8_t>*)(__int64(Volume) + TagsOffset);
            auto Data = (uint8_t*)Tags.GetData();
            std::string Best;
            for (int t = 0; Data && t < Tags.Num() && t < 16; t++)
            {
                auto Tag = Narrow(((FName*)(Data + t * FGameplayTag::Size()))->ToString());
                auto L = Lower(Tag);
                if (L.find("location") == std::string::npos || L.find("unnamed") != std::string::npos || L.find("landmark") != std::string::npos)
                    continue;
                if (Best.empty() || L.find(".poi.") != std::string::npos)
                    Best = Tag;
            }
            if (Best.empty())
                continue;
            auto Name = PrettyTag(Best);
            if (Name.size() < 3 || (Lower(Name).rfind("location", 0) == 0 && Name.size() <= 11))
                continue;
            auto Loc = Volume->K2_GetActorLocation();
            FVector Origin{}, Extent{};
            Volume->GetActorBounds(false, &Origin, &Extent, false);
            if (Extent.X > 100.f && Extent.Y > 100.f && std::isfinite(Origin.X) && std::isfinite(Origin.Y))
                Loc = Origin;
            static int VolLogs = 0;
            if (VolLogs++ < 40)
                BORON_LOG("[Boron][Zones]   volume %s loc=(%.0f,%.0f) ext=(%.0f,%.0f)\n", Name.c_str(), (double)Loc.X, (double)Loc.Y, (double)Extent.X, (double)Extent.Y);
            if (Loc.X == 0 && Loc.Y == 0)
                continue;
            auto It = std::find_if(Out.begin(), Out.end(), [&](const FZonePoi& P) { return P.Name == Name; });
            if (It == Out.end())
            {
                Out.push_back({ Name, (double)Loc.X, (double)Loc.Y });
                Counts.push_back(1);
            }
            else
            {
                auto Idx = It - Out.begin();
                int n = ++Counts[Idx];
                It->X += ((double)Loc.X - It->X) / n;
                It->Y += ((double)Loc.Y - It->Y) / n;
            }
        }
        Found.Free();
        static int PoiLogs = 0;
        if (PoiLogs++ < 3)
            BORON_LOG("[Boron][Zones] places: manager=%p volumes=%zu named=%zu\n", (void*)Manager, Volumes.size(), Out.size());
        std::sort(Out.begin(), Out.end(), [](const FZonePoi& A, const FZonePoi& B) { return A.Name < B.Name; });
        return Out;
    }

    AFortSafeZoneIndicator* GetIndicator(AFortGameStateAthena* GameState)
    {
        if (VersionInfo.FortniteVersion >= 25.20)
        {
            auto Logic = UFortGameStateComponent_BattleRoyaleGamePhaseLogic::Get(UWorld::GetWorld());
            return Logic ? Logic->SafeZoneIndicator : nullptr;
        }
        return GameState->HasSafeZoneIndicator() ? (AFortSafeZoneIndicator*)GameState->SafeZoneIndicator : nullptr;
    }

    std::vector<FZonePoint> ReadActual(AFortGameMode* GameMode)
    {
        std::vector<FZonePoint> Out;
        if (bArrayRadii && !LegacyChain.empty())
            return LegacyChain;
        TArray<FVector>* Arr = nullptr;
        if (VersionInfo.FortniteVersion >= 25.20)
            Arr = &UFortGameStateComponent_BattleRoyaleGamePhaseLogic::SafeZoneLocations;
        else if (GameMode && GameMode->HasSafeZoneLocations())
            Arr = &GameMode->SafeZoneLocations;
        if (!Arr || !Arr->GetData())
            return Out;
        for (int i = 0; i < Arr->Num() && i < 64; i++)
        {
            auto& V = Arr->Get(i, FVector::Size());
            Out.push_back({ (double)V.X, (double)V.Y });
        }
        return Out;
    }

    std::vector<double> ReadRadii(AFortGameStateAthena* GameState, int Count)
    {
        std::vector<double> Out;
        auto MapInfo = GameState->MapInfo;
        if (!MapInfo || Count <= 0)
            return Out;
        if (MapInfo->HasSafeZoneDefinition())
        {
            auto& Def = MapInfo->SafeZoneDefinition;
            for (int i = 0; i < Count && i < 64; i++)
                Out.push_back((double)Def.Radius.Evaluate((float)i));
            return Out;
        }
        if (MapInfo->HasSafeZoneDefinitions() && FFortSafeZoneDefinition::HasRadius())
        {
            bArrayRadii = true;
            if (MapInfo->HasSafeZoneStartingRadius())
                Out.push_back((double)MapInfo->SafeZoneStartingRadius.Evaluate());
            auto& Defs = MapInfo->SafeZoneDefinitions;
            for (int i = 0; i < Defs.Num() && (int)Out.size() < Count && i < 64; i++)
                Out.push_back((double)Defs.Get(i, FFortSafeZoneDefinition::Size()).Radius.Evaluate());
        }
        while (!Out.empty() && (int)Out.size() < Count && Out.size() < 64)
            Out.push_back(Out.back());
        return Out;
    }

    FZonePoint MapCenterOf(AFortGameStateAthena* GameState)
    {
        static auto Fn = GameState->MapInfo->GetFunction("GetMapCenter");
        if (Fn)
        {
            auto C = GameState->MapInfo->GetMapCenter();
            return { (double)C.X, (double)C.Y };
        }
        static void* CachedWorld = nullptr;
        static FZonePoint Cached;
        auto World = UWorld::GetWorld();
        static auto FoundationClass = FindClass("BuildingFoundation");
        if (CachedWorld != (void*)World && FoundationClass)
        {
            TArray<AActor*> Foundations;
            Utils::GetAll(FoundationClass, Foundations);
            if (Foundations.Num() > 20)
            {
                double MinX = 1e18, MinY = 1e18, MaxX = -1e18, MaxY = -1e18;
                for (int i = 0; i < Foundations.Num(); i++)
                {
                    if (!Foundations[i])
                        continue;
                    auto L = Foundations[i]->K2_GetActorLocation();
                    MinX = (std::min)(MinX, (double)L.X);
                    MinY = (std::min)(MinY, (double)L.Y);
                    MaxX = (std::max)(MaxX, (double)L.X);
                    MaxY = (std::max)(MaxY, (double)L.Y);
                }
                if (MaxX > MinX && MaxY > MinY)
                {
                    Cached = { (MinX + MaxX) / 2, (MinY + MaxY) / 2 };
                    CachedWorld = (void*)World;
                }
            }
            Foundations.Free();
        }
        return Cached;
    }

    int PhaseCount(AFortGameStateAthena* GameState)
    {
        auto MapInfo = GameState->MapInfo;
        if (!MapInfo)
            return 0;
        int Count = 0;
        if (MapInfo->HasSafeZoneDefinition())
            Count = (int)MapInfo->SafeZoneDefinition.Count.Evaluate();
        else if (MapInfo->HasSafeZoneDefinitions())
            Count = MapInfo->SafeZoneDefinitions.Num() + (MapInfo->HasSafeZoneStartingRadius() ? 1 : 0);
        auto Playlist = GameState->HasCurrentPlaylistInfo() ? (UFortPlaylistAthena*)GameState->CurrentPlaylistInfo.BasePlaylist : nullptr;
        if (VersionInfo.FortniteVersion >= 25.20 && Playlist && Playlist->LastSafeZoneIndex != -1)
            Count = (std::min)(Count, Playlist->LastSafeZoneIndex + 1);
        return std::clamp(Count, 0, 64);
    }

    void RetargetLegacy(AFortGameStateAthena* GameState, AFortGameMode* GameMode, const std::string& MapKey, const std::vector<FZonePoi>& Pois, FZonePoint MapCenter)
    {
        if (!GameMode || !GameMode->HasSafeZoneLocations() || !GameMode->HasSafeZonePhase())
            return;
        auto& Locs = GameMode->SafeZoneLocations;
        int N = Locs.GetData() ? Locs.Num() : 0;
        bool bOwn = N < 2;
        if (bOwn)
        {
            std::lock_guard Lock(Mtx);
            N = (int)LegacyChain.size();
        }
        int Anchor = (int)GameMode->SafeZonePhase + ZonePlanner::StartPhaseIndex() - LategameConfig::LateGameZone;
        if (Anchor < 0 || Anchor >= N - 1)
            return;

        FZonePins Pins;
        {
            std::lock_guard Lock(Mtx);
            Pins = PinsByMap[MapKey];
        }
        if (!Pins.bEnd && Pins.Phase.empty())
            return;

        auto Radii = ReadRadii(GameState, N);
        if ((int)Radii.size() < N)
            return;
        std::vector<FZonePoint> Base;
        if (bOwn)
        {
            std::lock_guard Lock(Mtx);
            Base = LegacyChain;
        }
        else
            for (int i = 0; i < N; i++)
            {
                auto& V = Locs.Get(i, FVector::Size());
                Base.push_back({ (double)V.X, (double)V.Y });
            }
        if ((int)Base.size() < N)
            return;
        Pins = ZonePlanner::Rebase(Pins, ZonePlanner::StartPhaseIndex(), Anchor);
        Pins.bStart = true;
        Pins.Start = Base[Anchor];
        bool bPulled = false;
        auto Chain = ZonePlanner::BuildChain(Pins, false, Radii, Anchor, Base, Pois, MapCenter, &bPulled);
        if (!bOwn)
            for (int i = Anchor + 1; i < N; i++)
            {
                auto& V = Locs.Get(i, FVector::Size());
                V.X = Chain[i].X;
                V.Y = Chain[i].Y;
            }

        std::lock_guard Lock(Mtx);
        if (bOwn && (int)LegacyChain.size() == N)
            for (int i = Anchor + 1; i < N; i++)
                LegacyChain[i] = Chain[i];
        View.bEndPulled = bPulled;
        View.LastEvent = "Endgame moved, used from phase " + std::to_string(Anchor + 2);
        BORON_LOG("[Boron][Zones] legacy retarget from phase %d (%d locations)%s\n", Anchor + 1, N, bPulled ? ", end pulled in" : "");
    }

    void Retarget(AFortGameStateAthena* GameState, AFortGameMode* GameMode, const std::string& MapKey, const std::vector<FZonePoi>& Pois, FZonePoint MapCenter)
    {
        auto Indicator = GetIndicator(GameState);
        if (!Indicator)
            return;
        if (!Indicator->HasSafeZonePhases())
        {
            RetargetLegacy(GameState, GameMode, MapKey, Pois, MapCenter);
            return;
        }
        auto& Phases = Indicator->SafeZonePhases;
        int N = Phases.Num();
        int Anchor = Indicator->CurrentPhase;
        if (Anchor < 0 || Anchor >= N - 1)
            return;

        FZonePins Pins;
        bool bRandomPoi;
        {
            std::lock_guard Lock(Mtx);
            Pins = PinsByMap[MapKey];
            bRandomPoi = false;
        }
        if (!Pins.bEnd && Pins.Phase.empty())
            return;

        std::vector<double> Radii;
        std::vector<FZonePoint> Base;
        for (int i = 0; i < N; i++)
        {
            auto& Ph = Phases.Get(i, FFortSafeZonePhaseInfo::Size());
            Radii.push_back(Ph.Radius);
            Base.push_back({ (double)Ph.Center.X, (double)Ph.Center.Y });
        }
        Pins = ZonePlanner::Rebase(Pins, ZonePlanner::StartPhaseIndex(), Anchor);
        Pins.bStart = true;
        Pins.Start = Base[Anchor];
        bool bPulled = false;
        auto Chain = ZonePlanner::BuildChain(Pins, bRandomPoi, Radii, Anchor, Base, Pois, MapCenter, &bPulled);

        TArray<FVector>* Locs = VersionInfo.FortniteVersion >= 25.20 ? &UFortGameStateComponent_BattleRoyaleGamePhaseLogic::SafeZoneLocations
                                : (GameMode && GameMode->HasSafeZoneLocations() ? &GameMode->SafeZoneLocations : nullptr);
        for (int i = Anchor + 1; i < N; i++)
        {
            auto& Ph = Phases.Get(i, FFortSafeZonePhaseInfo::Size());
            Ph.Center.X = Chain[i].X;
            Ph.Center.Y = Chain[i].Y;
            if (Locs && Locs->GetData() && i < Locs->Num())
            {
                auto& L = Locs->Get(i, FVector::Size());
                L.X = Chain[i].X;
                L.Y = Chain[i].Y;
            }
        }
        auto& NextNext = Phases.Get(Anchor + 1, FFortSafeZonePhaseInfo::Size());
        if (Indicator->HasNextNextCenter())
            Indicator->NextNextCenter = NextNext.Center;
        if (Indicator->HasFutureReplicator() && Indicator->FutureReplicator)
            Indicator->FutureReplicator->NextNextCenter = NextNext.Center;

        std::lock_guard Lock(Mtx);
        View.bEndPulled = bPulled;
        View.LastEvent = "Endgame moved live from phase " + std::to_string(Anchor + 1);
        BORON_LOG("[Boron][Zones] live retarget from phase %d (%d phases)%s\n", Anchor + 1, N, bPulled ? ", end pulled in" : "");
    }
}

std::string ZonePlanner::ModuleDir()
{
    HMODULE Module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&ZonePlanner::ModuleDir, &Module);
    char Path[MAX_PATH]{};
    GetModuleFileNameA(Module, Path, MAX_PATH);
    std::string Dir = Path;
    return Dir.substr(0, Dir.find_last_of("\\/") + 1);
}

int ZonePlanner::StartPhaseIndex()
{
    if (!LategameConfig::bLateGame)
        return 1;
    return LategameConfig::LateGameZone + (VersionInfo.FortniteVersion >= 24 ? 3 : 0) - (VersionInfo.FortniteVersion < 25.20 ? (bArrayRadii ? -1 : 1) : 0);
}

FZonePins ZonePlanner::Rebase(const FZonePins& Pins, int OldStart, int NewStart)
{
    FZonePins Out = Pins;
    Out.Phase.clear();
    for (auto& [K, Pt] : Pins.Phase)
    {
        int Rel = K + OldStart - NewStart;
        if (Rel > 0)
            Out.Phase[Rel] = Pt;
    }
    return Out;
}

std::vector<FZonePoint> ZonePlanner::BuildChain(const FZonePins& Pins, bool bRandomPoi, const std::vector<double>& Radii, int StartPhase, const std::vector<FZonePoint>& Base,
                                                const std::vector<FZonePoi>& Pois, FZonePoint MapCenter, bool* bEndPulled)
{
    int N = (int)Radii.size();
    std::vector<FZonePoint> C(N);
    for (int i = 0; i < N; i++)
        C[i] = i < (int)Base.size() ? Base[i] : MapCenter;
    if (bEndPulled)
        *bEndPulled = false;
    if (N < 2)
        return C;

    int S0 = (std::clamp)(StartPhase, 0, N - 1);
    FZoneRng Rng{ (uint64_t)Pins.Seed * 0x2545F4914F6CDD1Dull + 0x1234567ull };

    std::map<int, FZonePoint> Anchors;
    if (Pins.bStart)
        Anchors[S0] = Pins.Start;
    for (auto& [K, Pt] : Pins.Phase)
        if (S0 + K > S0 && S0 + K < N - 1)
            Anchors[S0 + K] = Pt;
    if (Pins.bEnd)
        Anchors[N - 1] = Pins.End;
    else if (bRandomPoi && !Pois.empty())
    {
        auto& Poi = Pois[(std::min)(Pois.size() - 1, (size_t)(Rng.Next() * (double)Pois.size()))];
        Anchors[N - 1] = { Poi.X, Poi.Y };
    }
    if (Anchors.empty())
        return C;

    double W = (std::clamp)((double)Pins.Wander, 0.0, 1.0);
    std::vector<double> Step(N, 0.0);
    for (int i = S0; i < N - 1; i++)
        Step[i] = (std::max)(0.0, Radii[i] - Radii[i + 1]) * 0.92;
    std::vector<double> Cap(N + 1, 0.0);
    for (int i = N - 2; i >= 0; i--)
        Cap[i] = Cap[i + 1] + Step[i];

    std::vector<std::pair<int, FZonePoint>> A(Anchors.begin(), Anchors.end());
    for (size_t k = 1; k < A.size() && !Pins.bFree; k++)
    {
        auto& Pa = A[k - 1].second;
        auto& Pb = A[k].second;
        double Seg = Cap[A[k - 1].first] - Cap[A[k].first];
        double Dx = Pb.X - Pa.X, Dy = Pb.Y - Pa.Y, D = sqrt(Dx * Dx + Dy * Dy);
        if (D > Seg && D > 0)
        {
            Pb = { Pa.X + Dx * Seg / D, Pa.Y + Dy * Seg / D };
            if (bEndPulled)
                *bEndPulled = true;
        }
    }

    auto RandomDir = [&](double& Dx, double& Dy)
    {
        double Ang = Rng.Next() * 6.283185307179586;
        Dx = cos(Ang);
        Dy = sin(Ang);
    };

    int First = A.front().first;
    C[First] = A.front().second;
    for (int i = First - 1; i >= S0; i--)
    {
        double Dx, Dy;
        RandomDir(Dx, Dy);
        double Cx = MapCenter.X - C[i + 1].X, Cy = MapCenter.Y - C[i + 1].Y;
        double Cl = sqrt(Cx * Cx + Cy * Cy);
        if (Cl > 1.0)
        {
            Dx = Dx * 0.5 + Cx / Cl * 0.5;
            Dy = Dy * 0.5 + Cy / Cl * 0.5;
            double Dl = sqrt(Dx * Dx + Dy * Dy);
            if (Dl > 1e-6)
            {
                Dx /= Dl;
                Dy /= Dl;
            }
        }
        double L = (std::min)(Step[i] * Rng.Next() * (0.25 + 0.75 * W), Cl);
        C[i] = { C[i + 1].X + Dx * L, C[i + 1].Y + Dy * L };
    }

    for (size_t k = 1; k < A.size(); k++)
    {
        int Ia = A[k - 1].first, Ib = A[k].first;
        FZonePoint End = A[k].second;
        C[Ia] = A[k - 1].second;
        if (Pins.bFree)
        {
            double Total = Cap[Ia] - Cap[Ib];
            for (int i = Ia + 1; i < Ib; i++)
            {
                double T = Total > 0 ? (Cap[Ia] - Cap[i]) / Total : (double)(i - Ia) / (double)(Ib - Ia);
                C[i] = { C[Ia].X + (End.X - C[Ia].X) * T, C[Ia].Y + (End.Y - C[Ia].Y) * T };
            }
            C[Ib] = End;
            continue;
        }
        for (int i = Ia; i < Ib; i++)
        {
            double Rx = End.X - C[i].X, Ry = End.Y - C[i].Y;
            double Rem = sqrt(Rx * Rx + Ry * Ry);
            double Left = Cap[i] - Cap[Ib];
            double T = Left > 0 ? Step[i] / Left : 1.0;
            double Slack = (std::min)((Cap[i + 1] - Cap[Ib]) - Rem * (1.0 - T), Step[i] - Rem * T);
            double J = (std::max)(0.0, Slack) * Rng.Next() * W;
            double Jx, Jy;
            RandomDir(Jx, Jy);
            C[i + 1] = { C[i].X + Rx * T + Jx * J, C[i].Y + Ry * T + Jy * J };
        }
        C[Ib] = End;
    }

    for (int i = A.back().first; i < N - 1; i++)
    {
        double Dx, Dy;
        RandomDir(Dx, Dy);
        double L = Step[i] * Rng.Next() * (0.25 + 0.75 * W);
        C[i + 1] = { C[i].X + Dx * L, C[i].Y + Dy * L };
    }

    for (int i = 0; i < S0; i++)
        C[i] = C[S0];
    return C;
}

void ZonePlanner::Apply(TArray<FVector>& Locations)
{
    auto World = UWorld::GetWorld();
    auto GameState = World ? (AFortGameStateAthena*)World->GameState : nullptr;
    {
        std::lock_guard Lock(Mtx);
        View.bApplied = false;
        LegacyChain.clear();
    }
    if (!IsSupported() || bBlocked || !GameState || !GameState->HasMapInfo() || !GameState->MapInfo || !Locations.GetData() || Locations.Num() < 2)
        return;

    std::string Pretty;
    auto MapKey = DetectMapKey(GameState, Pretty);
    FZoneSettings S;
    FZonePins Pins;
    std::vector<FZonePoi> Pois;
    {
        std::lock_guard Lock(Mtx);
        LoadLocked();
        S = Settings;
        if (!S.bEnabled || !LategameConfig::bLateGame)
            return;
        if (false && SeededWorld != (void*)World)
        {
            SeededWorld = (void*)World;
            PinsByMap[MapKey].Seed = (uint32_t)(GetTickCount64() * 2654435761ull) | 1u;
            SaveLocked();
        }
        Pins = PinsByMap[MapKey];
        Pois = View.MapKey == MapKey ? View.Pois : std::vector<FZonePoi>{};
    }

    int N = Locations.Num();
    auto Radii = ReadRadii(GameState, N);
    if ((int)Radii.size() < N)
    {
        BORON_LOG("[Boron][Zones] not applied: %d locations but %d radii\n", N, (int)Radii.size());
        return;
    }
    std::vector<FZonePoint> Base;
    for (int i = 0; i < N; i++)
    {
        auto& V = Locations.Get(i, FVector::Size());
        Base.push_back({ (double)V.X, (double)V.Y });
    }
    bool bPulled = false;
    auto Chain = BuildChain(Pins, false, Radii, StartPhaseIndex(), Base, Pois, MapCenterOf(GameState), &bPulled);
    for (int i = 0; i < N; i++)
    {
        auto& V = Locations.Get(i, FVector::Size());
        V.X = Chain[i].X;
        V.Y = Chain[i].Y;
    }

    std::lock_guard Lock(Mtx);
    View.bApplied = true;
    View.bEndPulled = bPulled;
    View.LastEvent = "Custom zones applied (" + std::to_string(N) + " phases)";
    BORON_LOG("[Boron][Zones] applied on %s: start=%d phases=%d r0=%.0f rStart=%.0f seed=%u%s\n", MapKey.c_str(), StartPhaseIndex(), N, Radii[0], Radii[(std::clamp)(StartPhaseIndex(), 0, N - 1)], Pins.Seed, bPulled ? " (end pulled in)" : "");
}

bool ZonePlanner::ApplyLegacy(const FVector& Seed)
{
    auto World = UWorld::GetWorld();
    auto GameState = World ? (AFortGameStateAthena*)World->GameState : nullptr;
    {
        std::lock_guard Lock(Mtx);
        View.bApplied = false;
        LegacyChain.clear();
    }
    if (!IsSupported() || bBlocked || !GameState || !GameState->HasMapInfo() || !GameState->MapInfo)
        return false;

    std::string Pretty;
    auto MapKey = DetectMapKey(GameState, Pretty);
    FZonePins Pins;
    std::vector<FZonePoi> Pois;
    bool bCustom = false;
    {
        std::lock_guard Lock(Mtx);
        LoadLocked();
        if (!LategameConfig::bLateGame)
            return false;
        bCustom = Settings.bEnabled;
        if (bCustom)
            Pins = PinsByMap[MapKey];
        Pois = View.MapKey == MapKey ? View.Pois : std::vector<FZonePoi>{};
    }
    if (!bCustom)
    {
        std::random_device Rd;
        Pins.bStart = true;
        Pins.Start = { (double)Seed.X, (double)Seed.Y };
        if (!Pois.empty())
        {
            auto& Poi = Pois[Rd() % Pois.size()];
            Pins.Start = { Poi.X, Poi.Y };
        }
        Pins.Seed = Rd() | 1;
        Pins.Wander = 1.f;
    }

    int N = PhaseCount(GameState);
    auto Radii = ReadRadii(GameState, N);
    if (N < 2 || (int)Radii.size() < N)
    {
        BORON_LOG("[Boron][Zones] legacy not applied: %d phases but %d radii\n", N, (int)Radii.size());
        return false;
    }
    std::vector<FZonePoint> Base(N, FZonePoint{ (double)Seed.X, (double)Seed.Y });
    bool bPulled = false;
    auto Chain = BuildChain(Pins, false, Radii, StartPhaseIndex(), Base, Pois, MapCenterOf(GameState), &bPulled);

    std::lock_guard Lock(Mtx);
    LegacyChain = Chain;
    LegacyStartRadius = Radii[(std::clamp)(StartPhaseIndex(), 0, N - 1)];
    if (bArrayRadii && GameState->MapInfo->HasSafeZoneDefinitions())
    {
        auto& Defs = GameState->MapInfo->SafeZoneDefinitions;
        int Clamped = 0;
        for (int j = 0; j + 1 < StartPhaseIndex() && j < Defs.Num(); j++)
        {
            auto& R = Defs.Get(j, FFortSafeZoneDefinition::Size()).Radius;
            if (R.Evaluate() > LegacyStartRadius)
            {
                R.Value = (float)LegacyStartRadius;
                R.Curve.CurveTable = nullptr;
                Clamped++;
            }
        }
        BORON_LOG("[Boron][Zones] pre-start radii clamped to %.0f (%d phases)\n", LegacyStartRadius, Clamped);
    }
    View.bApplied = true;
    View.bEndPulled = bPulled;
    View.LastEvent = (bCustom ? "Custom zones applied (" : "Random zones applied (") + std::to_string(N) + " phases)";
    int S0 = (std::clamp)(StartPhaseIndex(), 0, N - 1);
    BORON_LOG("[Boron][Zones] legacy %s on %s: start=%d phases=%d r0=%.0f rStart=%.0f startC=(%.0f,%.0f) endC=(%.0f,%.0f)%s\n", bCustom ? "applied" : "random", MapKey.c_str(), S0, N, Radii[0], Radii[S0],
              Chain[S0].X, Chain[S0].Y, Chain[N - 1].X, Chain[N - 1].Y, bPulled ? " (end pulled in)" : "");
    return true;
}

bool ZonePlanner::PlannedCenter(int Index, FVector& Out)
{
    std::lock_guard Lock(Mtx);
    if (!View.bApplied || Index < 0 || Index >= (int)LegacyChain.size())
        return false;
    Out.X = (float)LegacyChain[Index].X;
    Out.Y = (float)LegacyChain[Index].Y;
    return true;
}

bool ZonePlanner::IsArrayLayout()
{
    return bArrayRadii;
}

bool ZonePlanner::IsSupported()
{
    return true;
}

bool ZonePlanner::IsApplied()
{
    std::lock_guard Lock(Mtx);
    return View.bApplied;
}

void ZonePlanner::SetBlocked(bool b)
{
    bBlocked = b;
}

void ZonePlanner::RequestRetarget()
{
    bRetarget = true;
}

void ZonePlanner::Tick()
{
    auto World = UWorld::GetWorld();
    auto GameState = World ? (AFortGameStateAthena*)World->GameState : nullptr;
    auto GameMode = World ? (AFortGameMode*)World->AuthorityGameMode : nullptr;
    if (!IsSupported() || !LategameConfig::bLateGame || !GameState || !GameState->HasMapInfo() || !GameState->MapInfo)
        return;

    static void* LastWorld = nullptr;
    static double NextFast = 0, NextSlow = 0;
    static FZoneView Cache;
    double Now = (double)GetTickCount64() / 1000.0;

    if (LastWorld != (void*)World)
    {
        LastWorld = (void*)World;
        Cache = FZoneView{};
        NextFast = NextSlow = 0;
    }

    if (Now >= NextSlow)
    {
        NextSlow = Now + (Cache.ProjSource == 1 && Cache.Pois.size() >= 5 && !Cache.Radii.empty() ? 15.0 : 2.0);
        Cache.MapKey = DetectMapKey(GameState, Cache.MapName);
        Cache.MapCenter = MapCenterOf(GameState);
        static int CalibrationTries = 0;
        if (Cache.ProjSource == 0)
            CalibrationTries = 0;
        if (Cache.ProjSource != 1 && CalibrationTries++ < 40)
            Calibrate(GameState, Cache.MapCenter, Cache);
        if (Cache.Pois.size() < 5)
            Cache.Pois = CollectPois();
        if (Cache.Pois.size() < 5 && Cache.ProjSource != 0)
        {
            struct FFixedPoi
            {
                const char* Name;
                double U, V;
            };
            static const FFixedPoi Season14[] = {
                { "Craggy Cliffs", 0.610, 0.131 },  { "Steamy Stacks", 0.863, 0.191 }, { "Coral Castle", 0.218, 0.209 },  { "Doom's Domain", 0.446, 0.254 },
                { "Sweaty Sands", 0.227, 0.368 },   { "Stark Industries", 0.675, 0.387 }, { "Salty Springs", 0.406, 0.445 }, { "Dirty Docks", 0.928, 0.440 },
                { "The Ruins", 0.528, 0.472 },      { "Holly Hedges", 0.204, 0.534 },  { "Weeping Woods", 0.374, 0.607 }, { "Retail Row", 0.866, 0.643 },
                { "Lazy Lake", 0.706, 0.678 },      { "Slurpy Swamp", 0.329, 0.740 },  { "Catty Corner", 0.807, 0.756 },  { "The Fortilla", 0.186, 0.804 },
                { "Misty Meadows", 0.603, 0.825 },
            };
            static const FFixedPoi Season15[] = {
                { "Stealthy Stronghold", 0.408, 0.138 }, { "Craggy Cliffs", 0.610, 0.131 },  { "Steamy Stacks", 0.863, 0.191 }, { "Coral Castle", 0.218, 0.209 },
                { "Pleasant Park", 0.446, 0.255 },       { "Sweaty Sands", 0.227, 0.368 },   { "Colossal Coliseum", 0.644, 0.382 }, { "Salty Towers", 0.406, 0.445 },
                { "Dirty Docks", 0.928, 0.440 },         { "Holly Hedges", 0.204, 0.534 },   { "Weeping Woods", 0.374, 0.607 }, { "Hunter's Haven", 0.563, 0.639 },
                { "Retail Row", 0.866, 0.643 },          { "Lazy Lake", 0.706, 0.678 },      { "Slurpy Swamp", 0.329, 0.740 },  { "Catty Corner", 0.807, 0.756 },
                { "Misty Meadows", 0.603, 0.825 },
            };
            static const FFixedPoi Season20[] = {
                { "Logjam Lotus", 0.333, 0.203 },   { "Sleepy Sound", 0.577, 0.207 },     { "Shifty Shafts", 0.452, 0.277 },      { "Rave Cave", 0.208, 0.347 },
                { "Lazy Lagoon", 0.718, 0.336 },    { "Coney Crossroads", 0.570, 0.401 }, { "Reality Falls", 0.187, 0.534 },      { "Tilted Towers", 0.388, 0.526 },
                { "Sanctuary", 0.689, 0.540 },      { "Greasy Grove", 0.256, 0.693 },     { "Rocky Reels", 0.503, 0.686 },        { "Shuffled Shrines", 0.653, 0.672 },
                { "The Joneses", 0.796, 0.674 },    { "Condo Canyon", 0.720, 0.809 },     { "Chonker's Speedway", 0.588, 0.863 }, { "Synapse Station", 0.400, 0.885 },
            };
            double FN = VersionInfo.FortniteVersion;
            const FFixedPoi* Fixed = nullptr;
            size_t FixedCount = 0;
            if (Cache.MapKey == "Apollo_Terrain" && fabs(FN - 14.60) < 0.001)
                Fixed = Season14, FixedCount = std::size(Season14);
            else if (Cache.MapKey == "Apollo_Terrain" && fabs(FN - 15.30) < 0.001)
                Fixed = Season15, FixedCount = std::size(Season15);
            else if (Cache.MapKey == "Artemis_Terrain" && fabs(FN - 21.40) < 0.001)
                Fixed = Season20, FixedCount = std::size(Season20);
            double Det = Cache.UX * Cache.VY - Cache.UY * Cache.VX;
            if (Fixed && fabs(Det) > 1e-12)
            {
                Cache.Pois.clear();
                for (size_t k = 0; k < FixedCount; k++)
                {
                    auto& P = Fixed[k];
                    double Du = P.U - Cache.U0, Dv = P.V - Cache.V0;
                    Cache.Pois.push_back({ P.Name, (Du * Cache.VY - Dv * Cache.UY) / Det, (Dv * Cache.UX - Du * Cache.VX) / Det });
                }
            }
        }
        int N = PhaseCount(GameState);
        if ((int)Cache.Radii.size() != N)
            Cache.Radii = ReadRadii(GameState, N);
        Cache.StartPhase = StartPhaseIndex();
        Cache.bWorld = true;
    }

    if (Now >= NextFast)
    {
        NextFast = Now + 0.2;
        Cache.Actual = ReadActual(GameMode);

        Cache.Dots.clear();
        AFortPlayerPawnAthena* FirstHuman = nullptr;
        static auto PawnClass = FindClass("FortPlayerPawnAthena");
        if (PawnClass)
        {
            TArray<AActor*> Pawns;
            Utils::GetAll(PawnClass, Pawns);
            for (int i = 0; i < Pawns.Num() && i < 200; i++)
            {
                auto Pawn = (AFortPlayerPawnAthena*)Pawns[i];
                if (!Pawn)
                    continue;
                auto PS = (AFortPlayerStateAthena*)Pawn->PlayerState;
                if (!PS)
                    continue;
                auto Loc = Pawn->K2_GetActorLocation();
                Cache.Dots.push_back({ (double)Loc.X, (double)Loc.Y, (int)PS->TeamIndex, (bool)PS->bIsABot });
                if (!FirstHuman && !PS->bIsABot)
                    FirstHuman = Pawn;
            }
            Pawns.Free();
        }

        Cache.bHasBus = false;
        static auto AircraftClass = FindClass("FortAthenaAircraft");
        if (AircraftClass)
        {
            TArray<AActor*> Buses;
            Utils::GetAll(AircraftClass, Buses);
            if (Buses.Num() > 0 && Buses[0])
            {
                auto Loc = Buses[0]->K2_GetActorLocation();
                Cache.Bus = { (double)Loc.X, (double)Loc.Y };
                Cache.bHasBus = true;
            }
            Buses.Free();
        }

        Cache.bHasStorm = false;
        auto Indicator = GetIndicator(GameState);
        if (Indicator && Indicator->HasSafeZonePhases())
        {
            auto& Phases = Indicator->SafeZonePhases;
            int Cur = Indicator->CurrentPhase;
            Cache.CurrentPhase = Cur;
            if (Cur >= 0 && Cur < Phases.Num())
            {
                auto& Next = Phases.Get(Cur, FFortSafeZonePhaseInfo::Size());
                auto& Prev = Phases.Get(Cur > 0 ? Cur - 1 : Cur, FFortSafeZonePhaseInfo::Size());
                double T = (double)UGameplayStatics::GetTimeSeconds(World);
                double A = Indicator->SafeZoneStartShrinkTime, B = Indicator->SafeZoneFinishShrinkTime;
                double F = B > A ? std::clamp((T - A) / (B - A), 0.0, 1.0) : (T >= B ? 1.0 : 0.0);
                Cache.NextCenter = { (double)Next.Center.X, (double)Next.Center.Y };
                Cache.NextRadius = Next.Radius;
                Cache.StormCenter = { Prev.Center.X + (Next.Center.X - Prev.Center.X) * F, Prev.Center.Y + (Next.Center.Y - Prev.Center.Y) * F };
                Cache.StormRadius = Prev.Radius + (Next.Radius - Prev.Radius) * F;
                Cache.bHasStorm = true;
            }
        }

        else if (Indicator && Indicator->HasNextCenter() && Indicator->HasNextRadius() && GameMode && GameMode->HasSafeZonePhase())
        {
            Cache.CurrentPhase = (int)GameMode->SafeZonePhase + StartPhaseIndex() - LategameConfig::LateGameZone;
            if (bArrayRadii && LategameConfig::bLateGame && Cache.CurrentPhase < StartPhaseIndex() && StartPhaseIndex() < (int)LegacyChain.size())
            {
                auto& P = LegacyChain[StartPhaseIndex()];
                if (LegacyStartRadius > 0 && Indicator->NextRadius > LegacyStartRadius)
                    Indicator->NextRadius = (float)LegacyStartRadius;
                if (Indicator->NextCenter.X != (decltype(Indicator->NextCenter.X))P.X || Indicator->NextCenter.Y != (decltype(Indicator->NextCenter.Y))P.Y)
                {
                    Indicator->NextCenter.X = (decltype(Indicator->NextCenter.X))P.X;
                    Indicator->NextCenter.Y = (decltype(Indicator->NextCenter.Y))P.Y;
                    static int PreLogs = 0;
                    if (PreLogs++ < 3)
                        BORON_LOG("[Boron][Zones] pre-start next center -> (%.0f,%.0f) r=%.0f phase=%d\n", P.X, P.Y, (double)Indicator->NextRadius, Cache.CurrentPhase);
                }
            }
            auto Next = Indicator->NextCenter;
            auto Last = Indicator->HasLastCenter() ? Indicator->LastCenter : Next;
            double NextR = Indicator->NextRadius;
            double LastR = Indicator->HasLastRadius() ? Indicator->LastRadius : NextR;
            double T = (double)UGameplayStatics::GetTimeSeconds(World);
            double A = Indicator->SafeZoneStartShrinkTime, B = Indicator->SafeZoneFinishShrinkTime;
            double F = B > A ? std::clamp((T - A) / (B - A), 0.0, 1.0) : (T >= B ? 1.0 : 0.0);
            if (NextR > 0)
            {
                Cache.NextCenter = { (double)Next.X, (double)Next.Y };
                Cache.NextRadius = NextR;
                Cache.StormCenter = { Last.X + (Next.X - Last.X) * F, Last.Y + (Next.Y - Last.Y) * F };
                Cache.StormRadius = LastR + (NextR - LastR) * F;
                Cache.bHasStorm = true;
            }
            static double NextInside = 0;
            if (VersionInfo.FortniteVersion < 21.10 && GameMode && GameMode->HasbSafeZoneActive() && GameMode->bSafeZoneActive && Now >= NextInside)
            {
                NextInside = Now + 1.0;
                static decltype(GameMode->GetFunction("")) InsideFn = nullptr;
                if (!InsideFn)
                    InsideFn = GameMode->GetFunction("SafeZoneInsideChecks");
                if (InsideFn)
                {
                    char Params[16]{};
                    GameMode->ProcessEvent(InsideFn, Params);
                }
                static bool bLoggedInside = false;
                if (!bLoggedInside)
                {
                    bLoggedInside = true;
                    BORON_LOG("[Boron][Storm] SafeZoneInsideChecks fn=%p\n", (void*)InsideFn);
                }
            }
            static double NextStormLog = 0;
            if (Cache.bHasStorm && FirstHuman && Now >= NextStormLog)
            {
                NextStormLog = Now + 5.0;
                auto L = FirstHuman->K2_GetActorLocation();
                double Dist = hypot(L.X - Cache.StormCenter.X, L.Y - Cache.StormCenter.Y);
                float Hp = FirstHuman->HasHealthSet() && FirstHuman->HealthSet && FirstHuman->HealthSet->HasHealth() ? *(float*)((uint8_t*)&FirstHuman->HealthSet->Health + 12) : -1.f;
                BORON_LOG("[Boron][Storm] phase=%d dist=%.0f stormR=%.0f hp=%.0f outside=%d flagOut=%d flagIn=%d active=%d paused=%d ge=%p\n", Cache.CurrentPhase + 1, Dist, Cache.StormRadius, (double)Hp,
                          Dist > Cache.StormRadius, FirstHuman->HasbIsOutsideSafeZone() ? (int)FirstHuman->bIsOutsideSafeZone : -1,
                          FirstHuman->HasbIsInsideSafeZone() ? (int)FirstHuman->bIsInsideSafeZone : -1, GameMode->HasbSafeZoneActive() ? (int)GameMode->bSafeZoneActive : -1,
                          GameMode->HasbSafeZonePaused() ? (int)GameMode->bSafeZonePaused : -1, GameMode->HasGE_OutsideSafeZone() ? (void*)GameMode->GE_OutsideSafeZone : nullptr);
            }
        }

        std::lock_guard Lock(Mtx);
        LoadLocked();
        auto Applied = View.bApplied;
        auto Pulled = View.bEndPulled;
        auto Event = View.LastEvent;
        View = Cache;
        View.bApplied = Applied;
        View.bEndPulled = Pulled;
        View.LastEvent = Event;
    }

    if (bRetarget.exchange(false))
        Retarget(GameState, GameMode, Cache.MapKey, Cache.Pois, Cache.MapCenter);
}

FZoneView ZonePlanner::GetView()
{
    std::lock_guard Lock(Mtx);
    return View;
}

FZoneSettings ZonePlanner::GetSettings()
{
    std::lock_guard Lock(Mtx);
    LoadLocked();
    return Settings;
}

void ZonePlanner::SetSettings(const FZoneSettings& New)
{
    std::lock_guard Lock(Mtx);
    LoadLocked();
    Settings = New;
    SaveLocked();
}

FZonePins ZonePlanner::GetPins(const std::string& MapKey)
{
    std::lock_guard Lock(Mtx);
    LoadLocked();
    auto It = PinsByMap.find(MapKey);
    return It == PinsByMap.end() ? FZonePins{} : It->second;
}

void ZonePlanner::SetPins(const std::string& MapKey, const FZonePins& New)
{
    std::lock_guard Lock(Mtx);
    LoadLocked();
    PinsByMap[MapKey] = New;
    SaveLocked();
}

FZoneAlign ZonePlanner::GetAlign(const std::string& MapKey)
{
    std::lock_guard Lock(Mtx);
    LoadLocked();
    auto It = AlignByMap.find(MapKey);
    return It == AlignByMap.end() ? FZoneAlign{} : It->second;
}

void ZonePlanner::SetAlign(const std::string& MapKey, const FZoneAlign& New)
{
    std::lock_guard Lock(Mtx);
    LoadLocked();
    AlignByMap[MapKey] = New;
    SaveLocked();
}

std::vector<FZonePreset> ZonePlanner::GetPresets(const std::string& MapKey)
{
    std::lock_guard Lock(Mtx);
    LoadLocked();
    std::vector<FZonePreset> Out;
    for (auto& P : Presets)
        if (P.Map == MapKey)
            Out.push_back(P);
    return Out;
}

void ZonePlanner::SavePreset(const std::string& MapKey, const std::string& Name, const FZonePins& Pins)
{
    std::lock_guard Lock(Mtx);
    LoadLocked();
    for (auto& P : Presets)
        if (P.Map == MapKey && P.Name == Name)
        {
            P.Pins = Pins;
            SaveLocked();
            return;
        }
    Presets.push_back({ MapKey, Name, Pins });
    SaveLocked();
}

void ZonePlanner::DeletePreset(const std::string& MapKey, const std::string& Name)
{
    std::lock_guard Lock(Mtx);
    LoadLocked();
    Presets.erase(std::remove_if(Presets.begin(), Presets.end(), [&](const FZonePreset& P) { return P.Map == MapKey && P.Name == Name; }), Presets.end());
    SaveLocked();
}
