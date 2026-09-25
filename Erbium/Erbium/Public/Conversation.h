#pragma once

#include "pch.h"
#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>
#include "../../FortniteGame/Public/FortPlayerPawnAthena.h"
#include "../../FortniteGame/Public/FortPlayerControllerAthena.h"
#include "../../FortniteGame/Public/FortInventory.h"
#include "../../FortniteGame/Public/FortLootPackage.h"
#include "../../Engine/Public/DataTable.h"
#include "../../Engine/Public/CurveTable.h"
#include "WeaponUpgrade.h"

namespace NPCConversation
{
    struct FNodeId
    {
        uint32 A = 0;
        uint32 B = 0;
        uint32 C = 0;
        uint32 D = 0;

        bool operator==(const FNodeId& Other) const
        {
            return A == Other.A && B == Other.B && C == Other.C && D == Other.D;
        }

        bool IsValid() const
        {
            return (A | B | C | D) != 0;
        }
    };

    struct FNodeIdHash
    {
        size_t operator()(const FNodeId& Id) const
        {
            return std::hash<uint64>()(((((uint64)Id.A << 32) | Id.B) * 0x9E3779B97F4A7C15ull) ^ (((uint64)Id.C << 32) | Id.D));
        }
    };

    struct FRawArray
    {
        uint8* Data;
        int32 Num;
        int32 Max;
    };

    struct alignas(8) FMessageConfigBlob
    {
        uint8 Data[0x28];
    };

    constexpr int32 ContextSize = 0x38;
    constexpr int32 TextSize = 0x18;
    constexpr int32 OptionSize = 0x70;
    constexpr int32 OptionTags = 0x18;
    constexpr int32 OptionType = 0x38;
    constexpr int32 OptionNode = 0x40;
    constexpr int32 MessageSize = 0x38;
    constexpr int32 MessageName = 0x8;
    constexpr int32 MessageText = 0x20;
    constexpr int32 PayloadSize = 0x68;
    constexpr int32 PayloadParticipants = 0x38;
    constexpr int32 PayloadNode = 0x48;
    constexpr int32 PayloadOptions = 0x58;
    constexpr int32 ResultSize = 0x70;
    constexpr int32 ResultChoice = 0x8;
    constexpr int32 ResultMessage = 0x38;
    constexpr int32 EntrySize = 0x10;
    constexpr int32 EntryListSize = 0x18;
    constexpr int32 CandidateSize = 0x80;
    constexpr int32 SaleRowSize = 0x48;
    constexpr int32 ServiceRowSize = 0x20;
    constexpr int32 InstanceNodeOffset = 0x110;

    enum EResult : uint8
    {
        ResultInvalid,
        ResultAbort,
        ResultAdvance,
        ResultAdvanceWithChoice,
        ResultPause,
        ResultReturnToLast,
        ResultReturnToCurrent,
        ResultReturnToStart
    };

    enum ERequirement : uint8
    {
        Passed,
        FailedButVisible,
        FailedAndHidden
    };

    enum EChoice : uint8
    {
        ServerOnly,
        UserChoiceAvailable,
        UserChoiceUnavailable
    };

    struct FTypes
    {
        const UClass* Database = nullptr;
        const UClass* Node = nullptr;
        const UClass* NodeWithLinks = nullptr;
        const UClass* TaskNode = nullptr;
        const UClass* LinkNode = nullptr;
        const UClass* ChoiceNode = nullptr;
        const UClass* RequirementNode = nullptr;
        const UClass* SideEffectNode = nullptr;
        const UClass* Speech = nullptr;
        const UClass* Back = nullptr;
        const UClass* Service = nullptr;
        const UClass* SellItem = nullptr;
        const UClass* UpgradeItem = nullptr;
        const UClass* HasService = nullptr;
        const UClass* HasNoActiveQuests = nullptr;
        const UClass* ParticipantComp = nullptr;
        const UClass* NonPlayerComp = nullptr;
        const UClass* NPCComp = nullptr;
        const UClass* PlayerComp = nullptr;
        const UClass* Instance = nullptr;
        const UClass* SpawnerConversation = nullptr;
        const UClass* CharacterData = nullptr;
        std::vector<const UClass*> Hidden;
        bool bInstanceNode = false;
        bool bInitialized = false;
        bool bEnabled = false;
    };

    struct FOffsets
    {
        uint32 DatabaseNodes, DatabaseEntries;
        uint32 NodeEval, NodeGuid, NodeOutputs, TaskSubNodes, LinkRemoteTag, ChoiceText, ChoiceTags;
        uint32 SpeechGeneral, SpeechPerSpeaker, SellSlot, ServiceCurrency, ServicePricing, HasServiceTag;
        uint32 InstanceParticipants, AuthConversations, AuthCurrent, ConversationsActive;
        uint32 EntryTag, InteractorTag, SelfTag;
        uint32 PawnOwner, ControllerOwner, CharacterData, CollisionProfile, BoxExtent, BoxOffset, SupportedServices, SupportedSales, MaxServices, ServicesTable, SalesTable;
        uint32 SpawnerCompClass, SpawnerEntryTag, SpawnerInteractorTag, SpawnerSelfTag, SpawnerCharacterData, SpawnerCollisionProfile, SpawnerBoxExtent, SpawnerBoxOffset;
        uint32 CharacterTag, CharacterName;
    };

    struct FBranch
    {
        FNodeId Node;
        std::vector<FNodeId> Scope;
        std::vector<uint8> Option;
    };

    struct FCheckpoint
    {
        FBranch Branch;
        std::vector<FNodeId> ScopeStack;
    };

    struct FParticipant
    {
        AActor* Actor = nullptr;
        uint64 Tag = 0;
        UObject* Component = nullptr;
    };

    struct FState
    {
        UObject* Instance = nullptr;
        AActor* NPC = nullptr;
        UObject* NPCComp = nullptr;
        AFortPlayerControllerAthena* PC = nullptr;
        UObject* PlayerComp = nullptr;
        uint64 NPCTag = 0;
        bool bStarted = false;
        bool bEnded = false;
        int32 Depth = 0;
        FBranch Current;
        FBranch Starting;
        std::vector<FBranch> Branches;
        std::vector<std::vector<uint8>> Choices;
        std::vector<FNodeId> ScopeStack;
        std::vector<FCheckpoint> Checkpoints;
        std::vector<FParticipant> Participants;
    };

    struct FSaleUnit
    {
        FFortItemEntry* Entry = nullptr;
        const UFortItemDefinition* AmmoDef = nullptr;
        int32 AmmoCount = 0;
        int32 Price = 0;
        FText Text{};
    };

    struct FSaleSlot
    {
        std::vector<FSaleUnit> Units;
        size_t Next = 0;
        bool bRolled = false;

        bool SoldOut() const
        {
            return Next >= Units.size();
        }

        FSaleUnit* Current()
        {
            if (Units.empty())
                return nullptr;
            return &Units[SoldOut() ? Units.size() - 1 : Next];
        }
    };

    struct FRegistry
    {
        std::unordered_map<FNodeId, UObject*, FNodeIdHash> Nodes;
        std::unordered_map<uint64, std::vector<FNodeId>> Entries;
        std::unordered_map<UObject*, bool> Databases;
    };

    inline FTypes& Types()
    {
        static FTypes Value;
        return Value;
    }

    inline FOffsets& Offs()
    {
        static FOffsets Value{};
        return Value;
    }

    inline FRegistry& Registry()
    {
        static FRegistry Value;
        return Value;
    }

    inline std::unordered_map<UObject*, FState>& States()
    {
        static std::unordered_map<UObject*, FState> Value;
        return Value;
    }

    inline std::unordered_map<UObject*, std::vector<FSaleSlot>>& Sales()
    {
        static std::unordered_map<UObject*, std::vector<FSaleSlot>> Value;
        return Value;
    }

    inline uint8* RawAdd(FRawArray& Arr, int32 ElemSize)
    {
        if (Arr.Num >= Arr.Max)
        {
            int32 NewMax = Arr.Max < 4 ? 4 : Arr.Max * 2;
            Arr.Data = FMemory::Realloc<uint8>(Arr.Data, (uint64)NewMax * ElemSize, 0);
            Arr.Max = NewMax;
        }
        auto Elem = Arr.Data + (uint64)Arr.Num * ElemSize;
        memset(Elem, 0, ElemSize);
        Arr.Num++;
        return Elem;
    }

    inline void RawRemoveAt(FRawArray& Arr, int32 Index, int32 ElemSize)
    {
        if (Index < 0 || Index >= Arr.Num)
            return;
        memmove(Arr.Data + (uint64)Index * ElemSize, Arr.Data + (uint64)(Index + 1) * ElemSize, (uint64)(Arr.Num - Index - 1) * ElemSize);
        Arr.Num--;
    }

    inline void RawFree(FRawArray& Arr)
    {
        if (Arr.Data)
            FMemory::Free(Arr.Data);
        Arr = {};
    }

    inline FRawArray RawCopy(const FRawArray& Src, int32 ElemSize)
    {
        FRawArray Out{};
        if (Src.Num <= 0 || !Src.Data)
            return Out;
        Out.Data = FMemory::Realloc<uint8>(nullptr, (uint64)Src.Num * ElemSize, 0);
        memcpy(Out.Data, Src.Data, (uint64)Src.Num * ElemSize);
        Out.Num = Src.Num;
        Out.Max = Src.Num;
        return Out;
    }

    template <typename T>
    inline T& At(const void* Obj, uint32 Offset)
    {
        return *(T*)((uint8*)Obj + Offset);
    }

    inline std::string TagString(uint64 Tag)
    {
        return std::string(((FName*)&Tag)->ToString().c_str());
    }

    inline bool TryProcessEvent(UObject* Obj, UFunction* Fn, void* Params)
    {
        __try
        {
            Obj->ProcessEvent(Fn, Params);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    inline bool TryIsA(const UObject* Obj, const UClass* Class)
    {
        __try
        {
            return Obj && Class && Obj->IsA(Class);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    inline bool IsA(const UObject* Obj, const UClass* Class)
    {
        return Obj && Class && Obj->IsA(Class);
    }

    inline bool TextEmpty(const void* Text)
    {
        if (!*(void* const*)Text)
            return true;
        return UKismetTextLibrary::Conv_TextToString(*(FText*)Text).ToString().empty();
    }

    inline const FText& ContinueText()
    {
        static FText Text = UKismetTextLibrary::Conv_StringToText(FString(L"Continue"));
        return Text;
    }

    struct FParams
    {
        UFunction* Fn = nullptr;
        std::vector<uint8> Data;

        uint8* Param(const char* Name)
        {
            auto Offset = ((const UStruct*)Fn)->GetOffset(Name);
            return Offset == (uint32)-1 ? nullptr : Data.data() + Offset;
        }
    };

    inline bool Prepare(FParams& Params, UObject* Obj, const char* Name)
    {
        Params.Fn = Obj ? Obj->GetFunction(Name) : nullptr;
        if (!Params.Fn)
            return false;
        auto Size = ((const UStruct*)Params.Fn)->GetPropertiesSize();
        Params.Data.assign((Size > 0 ? Size : 0) + 0x10, 0);
        return true;
    }

    inline void CallSingle(UObject* Obj, const char* Name, const void* Arg, int32 ArgSize)
    {
        FParams Params;
        if (!Obj || !Prepare(Params, Obj, Name))
            return;
        if (Arg && ArgSize > 0)
            memcpy(Params.Data.data(), Arg, ArgSize < (int32)Params.Data.size() ? ArgSize : Params.Data.size());
        TryProcessEvent(Obj, Params.Fn, Params.Data.data());
    }

    inline void SetBool(UObject* Obj, const char* Name, bool Value)
    {
        auto Prop = Obj ? Obj->GetProperty(Name) : nullptr;
        if (!Prop)
            return;
        auto Offset = GetFromOffset<uint32>(Prop, Offsets::Offset_Internal);
        auto Mask = Prop->GetFieldMask();
        auto& Byte = At<uint8>(Obj, Offset);
        if (Mask == 0xFF)
            Byte = Value ? 1 : 0;
        else
            Byte = Value ? (Byte | Mask) : (Byte & ~Mask);
    }

    inline UObject* AddComponent(AActor* Actor, const UClass* Class, bool bDeferred)
    {
        FParams Params;
        if (!Actor || !Class || FVector::Size() != 0xc || !Prepare(Params, Actor, "AddComponentByClass"))
            return nullptr;
        auto ClassParam = Params.Param("Class");
        auto Transform = Params.Param("RelativeTransform");
        auto Deferred = Params.Param("bDeferredFinish");
        auto Ret = Params.Param("ReturnValue");
        if (!ClassParam || !Transform || !Ret)
            return nullptr;
        *(const UClass**)ClassParam = Class;
        *(float*)(Transform + 0xC) = 1.f;
        *(float*)(Transform + 0x20) = 1.f;
        *(float*)(Transform + 0x24) = 1.f;
        *(float*)(Transform + 0x28) = 1.f;
        if (Deferred)
            *Deferred = bDeferred ? 1 : 0;
        if (!TryProcessEvent(Actor, Params.Fn, Params.Data.data()))
            return nullptr;
        return *(UObject**)Ret;
    }

    inline void FinishComponent(AActor* Actor, UObject* Component)
    {
        FParams Params;
        if (!Prepare(Params, Actor, "FinishAddComponent"))
            return;
        auto CompParam = Params.Param("Component");
        auto Transform = Params.Param("RelativeTransform");
        if (!CompParam || !Transform)
            return;
        *(UObject**)CompParam = Component;
        *(float*)(Transform + 0xC) = 1.f;
        *(float*)(Transform + 0x20) = 1.f;
        *(float*)(Transform + 0x24) = 1.f;
        *(float*)(Transform + 0x28) = 1.f;
        TryProcessEvent(Actor, Params.Fn, Params.Data.data());
    }

    inline void AddDatabase(UObject* Database)
    {
        auto& Reg = Registry();
        auto& O = Offs();
        if (!Database || Reg.Databases[Database])
            return;
        Reg.Databases[Database] = true;
        for (auto& [Key, Value] : At<TMap<FGuid, UObject*>>(Database, O.DatabaseNodes))
            if (Value && TryIsA(Value, Types().Node))
                Reg.Nodes[*(FNodeId*)&Key] = Value;
        auto& Entries = At<FRawArray>(Database, O.DatabaseEntries);
        for (int32 i = 0; i < Entries.Num; i++)
        {
            auto Entry = Entries.Data + (uint64)i * EntryListSize;
            auto& Destinations = *(FRawArray*)(Entry + 0x8);
            auto& List = Reg.Entries[*(uint64*)Entry];
            for (int32 j = 0; j < Destinations.Num; j++)
                List.push_back(*(FNodeId*)(Destinations.Data + (uint64)j * sizeof(FNodeId)));
        }
    }

    inline UObject* Resolve(const FNodeId& Id)
    {
        auto& Nodes = Registry().Nodes;
        auto It = Nodes.find(Id);
        return It != Nodes.end() ? It->second : nullptr;
    }

    inline FNodeId NodeGuid(UObject* Node)
    {
        return Node ? At<FNodeId>(Node, Offs().NodeGuid) : FNodeId{};
    }

    inline FRawArray& SubNodes(UObject* Task)
    {
        return At<FRawArray>(Task, Offs().TaskSubNodes);
    }

    inline std::vector<FNodeId> OutputsOf(UObject* Node)
    {
        std::vector<FNodeId> Out;
        if (!IsA(Node, Types().NodeWithLinks))
            return Out;
        auto& Arr = At<FRawArray>(Node, Offs().NodeOutputs);
        for (int32 i = 0; i < Arr.Num; i++)
            Out.push_back(*(FNodeId*)(Arr.Data + (uint64)i * sizeof(FNodeId)));
        return Out;
    }

    inline std::vector<FNodeId> EntryDestinations(uint64 Tag)
    {
        std::vector<FNodeId> Out;
        auto& Entries = Registry().Entries;
        auto It = Entries.find(Tag);
        if (It == Entries.end())
            return Out;
        for (auto& Id : It->second)
        {
            auto Node = Resolve(Id);
            if (IsA(Node, Types().TaskNode))
                Out.push_back(Id);
            else
                for (auto& Next : OutputsOf(Node))
                    Out.push_back(Next);
        }
        return Out;
    }

    inline FState* StateOf(UObject* Instance)
    {
        if (!Instance)
            return nullptr;
        auto& All = States();
        auto It = All.find(Instance);
        return It != All.end() && !It->second.bEnded ? &It->second : nullptr;
    }

    inline void FillContext(uint8* Context, FState& S, UObject* Task)
    {
        memset(Context, 0, ContextSize);
        *(UObject**)(Context + 0x8) = S.Instance;
        *(UObject**)(Context + 0x18) = Task;
        Context[0x30] = 1;
    }

    inline bool CallNode(UObject* Node, FParams& Params)
    {
        auto& Eval = At<UObject*>(Node, Offs().NodeEval);
        auto Old = Eval;
        Eval = (UObject*)UWorld::GetWorld();
        bool bOk = TryProcessEvent(Node, Params.Fn, Params.Data.data());
        Eval = Old;
        return bOk;
    }

    inline int32 CountOf(AFortPlayerControllerAthena* PC, const UFortItemDefinition* Def)
    {
        if (!PC || !PC->WorldInventory || !Def)
            return 0;
        int32 Total = 0;
        auto& Entries = PC->WorldInventory->Inventory.ReplicatedEntries;
        for (int32 i = 0; i < Entries.Num(); i++)
        {
            auto& Entry = Entries.Get(i, FFortItemEntry::Size());
            if (Entry.ItemDefinition == Def)
                Total += Entry.Count;
        }
        return Total;
    }

    inline bool TakeItems(AFortPlayerControllerAthena* PC, const UFortItemDefinition* Def, int32 Count)
    {
        if (Count <= 0)
            return true;
        if (CountOf(PC, Def) < Count)
            return false;
        auto Inventory = PC->WorldInventory;
        while (Count > 0)
        {
            FFortItemEntry* Entry = nullptr;
            for (int32 i = 0; i < Inventory->Inventory.ReplicatedEntries.Num(); i++)
            {
                auto& Candidate = Inventory->Inventory.ReplicatedEntries.Get(i, FFortItemEntry::Size());
                if (Candidate.ItemDefinition == Def && Candidate.Count > 0)
                {
                    Entry = &Candidate;
                    break;
                }
            }
            if (!Entry)
                break;
            int32 Take = Count < Entry->Count ? Count : Entry->Count;
            Count -= Take;
            if (Take >= Entry->Count)
            {
                Inventory->Remove(Entry->ItemGuid);
                continue;
            }
            Entry->Count -= Take;
            for (int32 i = 0; i < Inventory->Inventory.ItemInstances.Num(); i++)
            {
                auto Item = Inventory->Inventory.ItemInstances[i];
                if (Item && memcmp(&Item->ItemEntry.ItemGuid, &Entry->ItemGuid, sizeof(FGuid)) == 0)
                {
                    Item->ItemEntry.Count = Entry->Count;
                    Item->ItemEntry.bIsDirty = true;
                }
            }
            Inventory->UpdateEntry(*Entry);
        }
        return true;
    }

    inline std::string ItemDisplayName(const UFortItemDefinition* Def)
    {
        if (!Def)
            return "";
        auto& Text = Def->HasDisplayName() ? Def->DisplayName : Def->ItemName;
        return std::string(UKismetTextLibrary::Conv_TextToString((FText&)Text).ToString().c_str());
    }

    inline float EvaluatePricing(UCurveTable* Table, FName RowName, float Level)
    {
        float Value = 0.f;
        UDataTableFunctionLibrary::EvaluateCurveTableRow(Table, RowName, Level, nullptr, &Value, FString());
        return Value;
    }

    inline std::vector<std::string> ItemTags(const UFortItemDefinition* Def)
    {
        std::vector<std::string> Out;
        static auto Offset = Def ? Def->GetOffset("GameplayTags") : (uint32)-1;
        if (!Def || Offset == (uint32)-1)
            return Out;
        auto& Tags = *(FRawArray*)((uint8*)Def + Offset);
        for (int32 i = 0; i < Tags.Num; i++)
            Out.push_back(std::string(((FName*)(Tags.Data + (uint64)i * 8))->ToString().c_str()));
        return Out;
    }

    inline int32 PriceFor(UObject* Node, const UFortItemDefinition* Def, int32 Count)
    {
        static const int32 Fallback[] = { 25, 25, 50, 100, 245, 590 };
        if (!Def)
            return 0;
        int32 Rarity = Def->Rarity < 6 ? (int32)Def->Rarity : 1;
        auto ItemSuffix = "/" + std::string(Def->Name.ToString().c_str()) + "." + std::string(Def->Name.ToString().c_str());
        auto Tags = ItemTags(Def);
        auto& Tables = At<FRawArray>(Node, Offs().ServicePricing);
        float Best = 0.f;
        size_t BestLength = 0;
        for (int32 i = 0; i < Tables.Num; i++)
        {
            auto Table = *(UCurveTable**)(Tables.Data + (uint64)i * 8);
            if (!Table)
                continue;
            for (auto& [RowName, Curve] : Table->GetRowMap())
            {
                auto Name = std::string(RowName.ToString().c_str());
                if (Name.size() > ItemSuffix.size() && Name.compare(Name.size() - ItemSuffix.size(), ItemSuffix.size(), ItemSuffix) == 0)
                {
                    auto Value = EvaluatePricing(Table, RowName, (float)Rarity);
                    if (Value > 0.f)
                        return (int32)(Value * (float)(Count > 0 ? Count : 1) + 0.5f);
                }
                for (auto& Tag : Tags)
                {
                    bool bMatch = Tag == Name || (Tag.size() > Name.size() && Tag.compare(0, Name.size(), Name) == 0 && Tag[Name.size()] == '.');
                    if (bMatch && Name.size() > BestLength)
                    {
                        auto Value = EvaluatePricing(Table, RowName, (float)Rarity);
                        if (Value > 0.f)
                        {
                            Best = Value;
                            BestLength = Name.size();
                        }
                    }
                }
            }
        }
        return Best > 0.f ? (int32)(Best + 0.5f) : Fallback[Rarity];
    }

    inline int32 ServicePrice(UObject* Node, const char* Row, int32 Level, int32 Fallback)
    {
        auto& Tables = At<FRawArray>(Node, Offs().ServicePricing);
        for (int32 i = 0; i < Tables.Num; i++)
        {
            auto Table = *(UCurveTable**)(Tables.Data + (uint64)i * 8);
            if (!Table)
                continue;
            for (auto& [RowName, Curve] : Table->GetRowMap())
            {
                if (strcmp(RowName.ToString().c_str(), Row) != 0)
                    continue;
                auto Value = EvaluatePricing(Table, RowName, (float)Level);
                if (Value > 0.f)
                    return (int32)(Value + 0.5f);
            }
        }
        return Fallback;
    }

    inline FSaleSlot* SaleItem(FState& S, UObject* SellNode)
    {
        auto& O = Offs();
        if (!IsA(S.NPCComp, Types().NPCComp))
            return nullptr;
        int32 SellSlot = At<int32>(SellNode, O.SellSlot);
        int32 Slot = SellSlot > 0 ? SellSlot - 1 : 0;
        auto& Rows = At<FRawArray>(S.NPCComp, O.SupportedSales);
        if (Slot < 0 || Slot >= Rows.Num)
        {
            printf("[Boron][Conv] sale %s slot %d out of range rows=%d\n", TagString(S.NPCTag).c_str(), SellSlot, Rows.Num);
            return nullptr;
        }
        auto& Slots = Sales()[S.NPCComp];
        if ((int32)Slots.size() < Rows.Num)
            Slots.resize(Rows.Num);
        auto& Sale = Slots[Slot];
        if (!Sale.bRolled)
        {
            Sale.bRolled = true;
            auto Row = Rows.Data + (uint64)Slot * SaleRowSize;
            auto Tier = *(FName*)(Row + 0x10);
            auto Level = *(int32*)(Row + 0x18);
            const UFortItemDefinition* AmmoDef = nullptr;
            int32 AmmoCount = 0;
            std::vector<FFortItemEntry*> Loose;
            for (int32 Attempt = 0; Attempt < 2 && Sale.Units.empty() && Loose.empty(); Attempt++)
            {
                TArray<FFortItemEntry*> Drops{};
                UFortLootPackage::ChooseLootForContainer(Drops, Tier, Attempt == 0 && Level > 0 ? Level : -1);
                for (auto& Drop : Drops)
                {
                    if (!Drop)
                        continue;
                    if (!Drop->ItemDefinition)
                    {
                        free(Drop);
                        continue;
                    }
                    if (AFortInventory::IsPrimaryQuickbar(Drop->ItemDefinition))
                    {
                        FSaleUnit Unit;
                        Unit.Entry = Drop;
                        Sale.Units.push_back(Unit);
                    }
                    else
                        Loose.push_back(Drop);
                }
                Drops.Free();
            }
            for (auto Drop : Loose)
            {
                if (!Sale.Units.empty() && !AmmoDef && Drop->ItemDefinition->IsA(UFortAmmoItemDefinition::StaticClass()))
                {
                    AmmoDef = Drop->ItemDefinition;
                    AmmoCount = Drop->Count;
                    free(Drop);
                    continue;
                }
                FSaleUnit Unit;
                Unit.Entry = Drop;
                Sale.Units.push_back(Unit);
            }
            std::string Items;
            for (auto& Unit : Sale.Units)
            {
                if (AmmoDef && AFortInventory::IsPrimaryQuickbar(Unit.Entry->ItemDefinition))
                {
                    Unit.AmmoDef = AmmoDef;
                    Unit.AmmoCount = AmmoCount;
                }
                auto Def = Unit.Entry->ItemDefinition;
                Unit.Price = PriceFor(SellNode, Def, Unit.Entry->Count);
                auto Label = "Buy " + ItemDisplayName(Def) + (Unit.Entry->Count > 1 ? " x" + std::to_string(Unit.Entry->Count) : std::string()) + " - " + std::to_string(Unit.Price) + " Gold";
                Unit.Text = UKismetTextLibrary::Conv_StringToText(FString(std::wstring(Label.begin(), Label.end()).c_str()));
                Items += std::string(Def->Name.ToString().c_str()) + "x" + std::to_string(Unit.Entry->Count) + (Unit.AmmoDef ? "+ammo" : "") + "=" + std::to_string(Unit.Price) + " ";
            }
            printf("[Boron][Conv] sale %s slot %d tier=%s stock=%d -> %s\n", TagString(S.NPCTag).c_str(), SellSlot, Tier.ToString().c_str(), (int)Sale.Units.size(),
                   Items.empty() ? "nothing" : Items.c_str());
        }
        return Sale.Units.empty() ? nullptr : &Sale;
    }

    struct FUpgradeOffer
    {
        WeaponUpgrade::FHeld Held;
        const UFortItemDefinition* Target = nullptr;
        int32 Price = 0;
    };

    inline bool UpgradeOffer(FState& S, UObject* Node, FUpgradeOffer& Out)
    {
        if (!WeaponUpgrade::GetHeld(S.PC, Out.Held))
            return false;
        Out.Target = WeaponUpgrade::NextRarity(Out.Held.Def);
        if (!Out.Target)
            return false;
        Out.Price = ServicePrice(Node, "UpgradeItem", Out.Target->Rarity, 25);
        return true;
    }

    inline bool CanAffordUpgrade(FState& S, UObject* Node, const FUpgradeOffer& Offer)
    {
        auto Currency = At<const UFortItemDefinition*>(Node, Offs().ServiceCurrency);
        return !Currency || CountOf(S.PC, Currency) >= Offer.Price;
    }

    inline const FText& UpgradeText(const FUpgradeOffer& Offer)
    {
        static const char* RarityNames[] = { "Common", "Uncommon", "Rare", "Epic", "Legendary", "Mythic" };
        static std::unordered_map<std::string, FText> Texts;
        auto Rarity = Offer.Target->Rarity < 6 ? RarityNames[Offer.Target->Rarity] : "?";
        auto ItemName = ItemDisplayName(Offer.Held.Def);
        auto Label = "Upgrade " + ItemName + " to " + Rarity + " - " + std::to_string(Offer.Price) + " Gold";
        auto It = Texts.find(Label);
        if (It == Texts.end())
            It = Texts.emplace(Label, UKismetTextLibrary::Conv_StringToText(FString(std::wstring(Label.begin(), Label.end()).c_str()))).first;
        return It->second;
    }

    inline void Upgrade(UObject* Node, FState& S)
    {
        FUpgradeOffer Offer;
        if (!S.PC || !S.PC->WorldInventory || !UpgradeOffer(S, Node, Offer))
            return;
        auto Currency = At<const UFortItemDefinition*>(Node, Offs().ServiceCurrency);
        if (Currency && !TakeItems(S.PC, Currency, Offer.Price))
        {
            printf("[Boron][Conv] cannot afford upgrade %d (has %d)\n", Offer.Price, CountOf(S.PC, Currency));
            return;
        }
        FUpgradeOffer Current;
        if (!UpgradeOffer(S, Node, Current) || Current.Held.Def != Offer.Held.Def)
            return;
        if (WeaponUpgrade::ReplaceHeld(S.PC, Current.Held, Current.Target))
            printf("[Boron][Conv] upgrade %s -> %s price=%d\n", Offer.Held.Def->Name.ToString().c_str(), Offer.Target->Name.ToString().c_str(), Offer.Price);
    }

    inline bool HasSupportedService(FState& S, uint64 Tag)
    {
        if (!IsA(S.NPCComp, Types().NPCComp))
            return false;
        auto& Tags = At<FRawArray>(S.NPCComp, Offs().SupportedServices);
        for (int32 i = 0; i < Tags.Num; i++)
            if (*(uint64*)(Tags.Data + (uint64)i * 8) == Tag)
                return true;
        return false;
    }

    inline uint8 NodeRequirement(UObject* Node, UObject* Task, FState& S)
    {
        auto& T = Types();
        if (IsA(Node, T.HasService))
            return HasSupportedService(S, At<uint64>(Node, Offs().HasServiceTag)) ? Passed : FailedAndHidden;
        if (IsA(Node, T.HasNoActiveQuests))
            return Passed;
        for (auto Class : T.Hidden)
            if (IsA(Node, Class))
                return FailedAndHidden;
        if (IsA(Node, T.UpgradeItem))
        {
            FUpgradeOffer Offer;
            return UpgradeOffer(S, Node, Offer) && CanAffordUpgrade(S, Node, Offer) ? Passed : FailedButVisible;
        }
        if (IsA(Node, T.SellItem))
        {
            auto Sale = SaleItem(S, Node);
            return !Sale ? FailedAndHidden : (Sale->SoldOut() ? FailedButVisible : Passed);
        }
        FParams Params;
        if (!Prepare(Params, Node, "IsRequirementSatisfied"))
            return Passed;
        auto Context = Params.Param("Context");
        auto Ret = Params.Param("ReturnValue");
        if (!Context || !Ret)
            return Passed;
        FillContext(Context, S, Task);
        if (!CallNode(Node, Params))
            return Passed;
        return *Ret <= FailedAndHidden ? *Ret : Passed;
    }

    inline uint8 CheckRequirements(UObject* Task, FState& S)
    {
        uint8 Result = NodeRequirement(Task, Task, S);
        if (Result == FailedAndHidden)
            return Result;
        auto& Subs = SubNodes(Task);
        for (int32 i = 0; i < Subs.Num; i++)
        {
            auto Sub = *(UObject**)(Subs.Data + (uint64)i * 8);
            if (!IsA(Sub, Types().RequirementNode))
                continue;
            auto SubResult = NodeRequirement(Sub, Task, S);
            if (SubResult > Result)
                Result = SubResult;
            if (Result == FailedAndHidden)
                break;
        }
        return Result;
    }

    inline std::vector<FNodeId> DetermineBranches(FState& S, const std::vector<FNodeId>& Source, uint8 Maximum)
    {
        std::vector<FNodeId> Out;
        for (auto& Id : Source)
        {
            auto Node = Resolve(Id);
            if (IsA(Node, Types().TaskNode) && CheckRequirements(Node, S) <= Maximum)
                Out.push_back(Id);
        }
        return Out;
    }

    inline void GatherChoices(UObject* Task, FState& S, const std::vector<FNodeId>& Scope, std::vector<FBranch>& Out)
    {
        auto& T = Types();
        auto& O = Offs();
        UObject* ChoiceNode = nullptr;
        auto& Subs = SubNodes(Task);
        for (int32 i = 0; i < Subs.Num && !ChoiceNode; i++)
        {
            auto Sub = *(UObject**)(Subs.Data + (uint64)i * 8);
            if (IsA(Sub, T.ChoiceNode))
                ChoiceNode = Sub;
        }
        if (!ChoiceNode)
            return;
        auto Requirement = CheckRequirements(Task, S);
        if (Requirement == FailedAndHidden)
            return;

        FBranch Branch;
        Branch.Node = NodeGuid(Task);
        Branch.Scope = Scope;
        Branch.Option.assign(OptionSize, 0);
        auto Option = Branch.Option.data();

        FParams Params;
        if (Prepare(Params, ChoiceNode, "FillChoice"))
        {
            auto Context = Params.Param("Context");
            auto Entry = Params.Param("ChoiceEntry");
            if (Context && Entry)
            {
                FillContext(Context, S, Task);
                if (CallNode(ChoiceNode, Params))
                    memcpy(Option, Entry, OptionSize);
            }
        }
        if (TextEmpty(Option))
            memcpy(Option, (uint8*)ChoiceNode + O.ChoiceText, TextSize);
        if (((FRawArray*)(Option + OptionTags))->Num == 0)
            memcpy(Option + OptionTags, (uint8*)ChoiceNode + O.ChoiceTags, 0x20);
        if (IsA(Task, T.SellItem))
            if (auto Sale = SaleItem(S, Task))
                if (auto Unit = Sale->Current())
                    memcpy(Option, &Unit->Text, TextSize);
        if (IsA(Task, T.UpgradeItem))
        {
            FUpgradeOffer Offer;
            if (UpgradeOffer(S, Task, Offer))
                memcpy(Option, &UpgradeText(Offer), TextSize);
        }
        Option[OptionType] = Requirement == Passed ? UserChoiceAvailable : UserChoiceUnavailable;
        *(FNodeId*)(Option + OptionNode) = Branch.Node;
        Out.push_back(std::move(Branch));
    }

    inline void GenerateChoices(FState& S, const std::vector<FNodeId>& Destinations, const std::vector<FNodeId>& Scope, std::vector<FBranch>& Out, int32 Depth = 0)
    {
        auto& T = Types();
        if (Depth > 8)
            return;
        for (auto& Id : Destinations)
        {
            auto Node = Resolve(Id);
            if (!IsA(Node, T.TaskNode))
                continue;
            auto Start = Out.size();
            if (IsA(Node, T.LinkNode))
            {
                auto Legal = DetermineBranches(S, EntryDestinations(At<uint64>(Node, Offs().LinkRemoteTag)), FailedButVisible);
                auto Inner = Scope;
                Inner.push_back(Id);
                GenerateChoices(S, Legal, Inner, Out, Depth + 1);
            }
            else
                GatherChoices(Node, S, Scope, Out);
            if (Out.size() == Start && CheckRequirements(Node, S) == Passed)
            {
                FBranch Branch;
                Branch.Node = Id;
                Branch.Scope = Scope;
                Branch.Option.assign(OptionSize, 0);
                Branch.Option[OptionType] = ServerOnly;
                *(FNodeId*)(Branch.Option.data() + OptionNode) = Id;
                Out.push_back(std::move(Branch));
            }
        }
    }

    inline void UpdateNextChoices(FState& S)
    {
        std::vector<FBranch> All;
        auto Node = Resolve(S.Current.Node);
        if (IsA(Node, Types().TaskNode))
            GenerateChoices(S, OutputsOf(Node), {}, All);
        S.Branches = std::move(All);
        S.Choices.clear();
        for (auto& Branch : S.Branches)
            if (Branch.Option[OptionType] != ServerOnly)
                S.Choices.push_back(Branch.Option);
        if ((!S.Branches.empty() || !S.ScopeStack.empty()) && S.Choices.empty())
        {
            std::vector<uint8> Continue(OptionSize, 0);
            memcpy(Continue.data(), &ContinueText(), TextSize);
            Continue[OptionType] = UserChoiceAvailable;
            S.Choices.push_back(std::move(Continue));
        }
    }

    inline FRawArray BuildParticipants(FState& S)
    {
        FRawArray List{};
        for (auto& Part : S.Participants)
        {
            auto Entry = RawAdd(List, EntrySize);
            *(AActor**)Entry = Part.Actor;
            *(uint64*)(Entry + 0x8) = Part.Tag;
        }
        return List;
    }

    inline void SyncInstanceParticipants(FState& S)
    {
        auto& List = At<FRawArray>(S.Instance, Offs().InstanceParticipants);
        List.Num = 0;
        for (auto& Part : S.Participants)
        {
            auto Entry = RawAdd(List, EntrySize);
            *(AActor**)Entry = Part.Actor;
            *(uint64*)(Entry + 0x8) = Part.Tag;
        }
    }

    inline void NotifyStarted(FState& S, FParticipant& Part)
    {
        auto& O = Offs();
        auto Comp = Part.Component;
        if (!Comp)
            return;
        At<UObject*>(Comp, O.AuthCurrent) = S.Instance;
        *(UObject**)RawAdd(At<FRawArray>(Comp, O.AuthConversations), 8) = S.Instance;
        auto& Active = At<int32>(Comp, O.ConversationsActive);
        Active++;
        if (Comp == S.PlayerComp)
        {
            auto List = BuildParticipants(S);
            CallSingle(Comp, "ClientUpdateParticipants", &List, sizeof(FRawArray));
            RawFree(List);
            FParams Params;
            if (Prepare(Params, Comp, "ClientStartConversation"))
            {
                auto Conversation = Params.Param("Conversation");
                auto AsParticipant = Params.Param("AsParticipant");
                if (Conversation)
                    *(UObject**)Conversation = S.Instance;
                if (AsParticipant)
                    *(uint64*)AsParticipant = Part.Tag;
                if (AsParticipant)
                    TryProcessEvent(Comp, Params.Fn, Params.Data.data());
            }
            CallSingle(Comp, "ClientUpdateConversations", &Active, 4);
        }
        else if (IsA(Comp, Types().NPCComp))
        {
            SetBool(Comp, "bConversationModeActive", true);
            CallSingle(Comp, "OnRep_ConversationModeActive", nullptr, 0);
        }
    }

    inline void NotifyEnded(FState& S, FParticipant& Part)
    {
        auto& O = Offs();
        auto Comp = Part.Component;
        if (!Comp || At<UObject*>(Comp, O.AuthCurrent) != S.Instance)
            return;
        At<UObject*>(Comp, O.AuthCurrent) = nullptr;
        auto& List = At<FRawArray>(Comp, O.AuthConversations);
        for (int32 i = 0; i < List.Num; i++)
        {
            if (*(UObject**)(List.Data + (uint64)i * 8) == S.Instance)
            {
                RawRemoveAt(List, i, 8);
                break;
            }
        }
        auto& Active = At<int32>(Comp, O.ConversationsActive);
        if (Active > 0)
            Active--;
        if (Comp == S.PlayerComp)
            CallSingle(Comp, "ClientUpdateConversations", &Active, 4);
        else if (IsA(Comp, Types().NPCComp))
        {
            SetBool(Comp, "bConversationModeActive", false);
            CallSingle(Comp, "OnRep_ConversationModeActive", nullptr, 0);
        }
    }

    inline void AssignParticipant(FState& S, AActor* Actor, uint64 Tag)
    {
        if (!Actor || !(uint32)Tag)
            return;
        for (size_t i = 0; i < S.Participants.size(); i++)
        {
            if (S.Participants[i].Tag == Tag)
            {
                if (S.bStarted)
                    NotifyEnded(S, S.Participants[i]);
                S.Participants.erase(S.Participants.begin() + i);
                break;
            }
        }
        FParticipant Part;
        Part.Actor = Actor;
        Part.Tag = Tag;
        Part.Component = Actor->GetComponentByClass((UClass*)Types().ParticipantComp);
        S.Participants.push_back(Part);
        if (Actor->IsA<AFortPlayerControllerAthena>())
        {
            S.PC = (AFortPlayerControllerAthena*)Actor;
            S.PlayerComp = Part.Component;
        }
        else if (IsA(Part.Component, Types().NonPlayerComp))
        {
            S.NPC = Actor;
            S.NPCComp = Part.Component;
            S.NPCTag = Tag;
        }
        SyncInstanceParticipants(S);
        if (S.bStarted)
            NotifyStarted(S, S.Participants.back());
    }

    inline void Abort(FState& S)
    {
        if (S.bEnded)
            return;
        S.bEnded = true;
        if (S.bStarted)
            for (auto& Part : S.Participants)
                NotifyEnded(S, Part);
        S.bStarted = false;
    }

    inline void Cleanup()
    {
        auto& All = States();
        for (auto It = All.begin(); It != All.end();)
        {
            if (It->second.bEnded && It->second.Depth == 0)
                It = All.erase(It);
            else
                ++It;
        }
    }

    inline void BuildSpeech(UObject* Speech, FState& S, uint8* Result)
    {
        auto& O = Offs();
        auto Message = Result + ResultMessage;
        *(uint64*)Message = S.NPCTag;
        if (IsA(S.NPCComp, Types().NPCComp))
            if (auto Character = At<UObject*>(S.NPCComp, O.CharacterData))
                memcpy(Message + MessageName, (uint8*)Character + O.CharacterName, TextSize);

        const uint8* SpeakerConfig = nullptr;
        uint64 Keys[2] = {};
        if (IsA(S.NPCComp, Types().NonPlayerComp))
        {
            Keys[0] = At<uint64>(S.NPCComp, O.EntryTag);
            Keys[1] = S.NPCTag;
        }
        for (auto& [Key, Value] : At<TMap<FGameplayTag, FMessageConfigBlob>>(Speech, O.SpeechPerSpeaker))
        {
            auto KeyValue = *(uint64*)&Key;
            if (KeyValue && (KeyValue == Keys[0] || KeyValue == Keys[1]))
            {
                SpeakerConfig = Value.Data;
                break;
            }
        }
        auto General = (const uint8*)Speech + O.SpeechGeneral;

        const uint8* Picked = nullptr;
        for (auto Config : { SpeakerConfig, General })
        {
            if (!Config || Picked)
                continue;
            auto& Candidates = *(FRawArray*)Config;
            float BestPriority = -1e30f;
            std::vector<std::pair<const uint8*, float>> Pool;
            for (int32 i = 0; i < Candidates.Num; i++)
            {
                auto Candidate = Candidates.Data + (uint64)i * CandidateSize;
                if (((FRawArray*)(Candidate + 0x20))->Num > 0 || TextEmpty(Candidate))
                    continue;
                float Priority = ((FScalableFloat*)(Candidate + 0x30))->Evaluate();
                float Weight = ((FScalableFloat*)(Candidate + 0x58))->Evaluate();
                if (Priority > BestPriority)
                {
                    BestPriority = Priority;
                    Pool.clear();
                }
                if (Priority == BestPriority)
                    Pool.push_back({ Candidate, Weight > 0.f ? Weight : 1.f });
            }
            if (!Pool.empty())
            {
                float Total = 0.f;
                for (auto& Entry : Pool)
                    Total += Entry.second;
                float Roll = ((float)rand() / (float)RAND_MAX) * Total;
                Picked = Pool.back().first;
                for (auto& Entry : Pool)
                {
                    if (Roll <= Entry.second)
                    {
                        Picked = Entry.first;
                        break;
                    }
                    Roll -= Entry.second;
                }
            }
            else if (!TextEmpty(Config + 0x10))
                Picked = Config + 0x10;
        }
        if (Picked)
            memcpy(Message + MessageText, Picked, TextSize);
    }

    inline void Sell(UObject* SellNode, FState& S)
    {
        auto Sale = SaleItem(S, SellNode);
        if (!Sale || Sale->SoldOut() || !S.PC || !S.PC->WorldInventory)
            return;
        auto& Unit = Sale->Units[Sale->Next];
        auto Currency = At<const UFortItemDefinition*>(SellNode, Offs().ServiceCurrency);
        if (Currency && !TakeItems(S.PC, Currency, Unit.Price))
        {
            printf("[Boron][Conv] cannot afford %d (has %d)\n", Unit.Price, CountOf(S.PC, Currency));
            return;
        }
        S.PC->WorldInventory->GiveItem(*Unit.Entry);
        if (Unit.AmmoDef && Unit.AmmoCount > 0)
            S.PC->WorldInventory->GiveItem(Unit.AmmoDef, Unit.AmmoCount);
        Sale->Next++;
        printf("[Boron][Conv] sold %s price=%d left=%d\n", Unit.Entry->ItemDefinition->Name.ToString().c_str(), Unit.Price, (int)(Sale->Units.size() - Sale->Next));
    }

    inline std::vector<uint8> ExecuteTask(UObject* Task, FState& S)
    {
        auto& T = Types();
        std::vector<uint8> Result(ResultSize, 0);
        if (IsA(Task, T.SellItem) || IsA(Task, T.UpgradeItem))
        {
            if (IsA(Task, T.SellItem))
                Sell(Task, S);
            else
                Upgrade(Task, S);
            Result[0] = ResultReturnToCurrent;
            return Result;
        }
        FParams Params;
        if (Prepare(Params, Task, "ExecuteTaskNode"))
        {
            auto Context = Params.Param("Context");
            auto Ret = Params.Param("ReturnValue");
            if (Context && Ret)
            {
                FillContext(Context, S, Task);
                if (CallNode(Task, Params))
                    memcpy(Result.data(), Ret, ResultSize);
            }
        }
        if (IsA(Task, T.Speech) && (Result[0] == ResultInvalid || Result[0] == ResultPause))
        {
            if (Result[0] == ResultInvalid || TextEmpty(Result.data() + ResultMessage + MessageText))
                BuildSpeech(Task, S, Result.data());
            Result[0] = ResultPause;
        }
        else if (Result[0] == ResultInvalid)
        {
            if (IsA(Task, T.Back))
                Result[0] = ResultReturnToLast;
            else
                Result[0] = ResultAdvance;
        }
        printf("[Boron][Conv] node %s result=%d\n", Task->Class->Name.ToString().c_str(), Result[0]);
        return Result;
    }

    inline std::vector<uint8> ExecuteWithSideEffects(UObject* Task, FState& S)
    {
        auto Result = ExecuteTask(Task, S);
        if (Result[0] == ResultAbort || S.bEnded)
            return Result;
        auto& Subs = SubNodes(Task);
        for (int32 i = 0; i < Subs.Num; i++)
        {
            auto Sub = *(UObject**)(Subs.Data + (uint64)i * 8);
            if (!IsA(Sub, Types().SideEffectNode))
                continue;
            FParams Params;
            if (!Prepare(Params, Sub, "ServerCauseSideEffect"))
                continue;
            if (auto Context = Params.Param("Context"))
            {
                FillContext(Context, S, Task);
                CallNode(Sub, Params);
            }
        }
        CallSingle(S.PlayerComp, "ClientExecuteTaskAndSideEffects", &S.Current.Node, sizeof(FNodeId));
        return Result;
    }

    inline void OnNodeModified(FState& S);

    inline void ModifyCurrent(FState& S, const FBranch& Branch)
    {
        S.Current = Branch;
        for (auto& Id : Branch.Scope)
            S.ScopeStack.push_back(Id);
        if (Types().bInstanceNode && S.Instance)
            At<FNodeId>(S.Instance, InstanceNodeOffset) = Branch.Node;
        OnNodeModified(S);
    }

    inline void Pause(FState& S, const uint8* Message)
    {
        std::vector<uint8> Payload(PayloadSize, 0);
        memcpy(Payload.data(), Message, MessageSize);
        auto& Parts = *(FRawArray*)(Payload.data() + PayloadParticipants);
        Parts = BuildParticipants(S);
        *(FNodeId*)(Payload.data() + PayloadNode) = S.Current.Node;
        auto& Options = *(FRawArray*)(Payload.data() + PayloadOptions);
        for (auto& Choice : S.Choices)
            memcpy(RawAdd(Options, OptionSize), Choice.data(), OptionSize);
        S.Checkpoints.push_back({ S.Current, S.ScopeStack });
        CallSingle(S.PlayerComp, "ClientUpdateConversation", Payload.data(), PayloadSize);
        RawFree(Parts);
        RawFree(Options);
    }

    inline void ReturnToCheckpoint(FState& S, bool bPrevious)
    {
        if (bPrevious)
        {
            if (S.Checkpoints.size() < 2)
                return;
            S.Checkpoints.pop_back();
        }
        if (S.Checkpoints.empty())
            return;
        auto Checkpoint = S.Checkpoints.back();
        S.Checkpoints.pop_back();
        S.ScopeStack = Checkpoint.ScopeStack;
        ModifyCurrent(S, Checkpoint.Branch);
    }

    inline void ServerAdvance(FState& S, const uint8* Request)
    {
        if (S.bEnded || !S.bStarted || !S.Current.Node.IsValid())
            return;
        FNodeId Picked = Request ? *(const FNodeId*)Request : FNodeId{};
        std::vector<FBranch> Candidates;
        if (Picked.IsValid())
        {
            for (auto& Branch : S.Branches)
            {
                if (Branch.Option[OptionType] != ServerOnly && Branch.Node == Picked)
                {
                    Candidates.push_back(Branch);
                    break;
                }
            }
            if (Candidates.empty())
            {
                Abort(S);
                return;
            }
        }
        else
        {
            if (S.Branches.empty() && !S.ScopeStack.empty())
            {
                FBranch Back;
                Back.Node = S.ScopeStack.back();
                ModifyCurrent(S, Back);
                return;
            }
            for (auto& Branch : S.Branches)
                if (Branch.Option[OptionType] != UserChoiceUnavailable)
                    Candidates.push_back(Branch);
        }
        std::vector<FBranch> Valid;
        for (auto& Branch : Candidates)
        {
            auto Node = Resolve(Branch.Node);
            if (!IsA(Node, Types().TaskNode) || CheckRequirements(Node, S) == Passed)
                Valid.push_back(Branch);
        }
        if (Valid.empty())
        {
            if (Picked.IsValid() && !S.Checkpoints.empty())
                ReturnToCheckpoint(S, false);
            else
                Abort(S);
            return;
        }
        ModifyCurrent(S, Valid[rand() % Valid.size()]);
    }

    inline void OnNodeModified(FState& S)
    {
        if (S.bEnded)
            return;
        S.Depth++;
        auto Node = Resolve(S.Current.Node);
        if (S.Depth > 48 || !IsA(Node, Types().TaskNode))
        {
            Abort(S);
            S.Depth--;
            return;
        }
        auto Result = ExecuteWithSideEffects(Node, S);
        if (!S.bEnded)
        {
            if (!S.ScopeStack.empty() && S.ScopeStack.back() == NodeGuid(Node))
                S.ScopeStack.pop_back();
            UpdateNextChoices(S);
            switch (Result[0])
            {
            case ResultAdvance:
                ServerAdvance(S, nullptr);
                break;
            case ResultAdvanceWithChoice:
            {
                FBranch Branch;
                Branch.Node = *(FNodeId*)(Result.data() + ResultChoice);
                ModifyCurrent(S, Branch);
                break;
            }
            case ResultPause:
                Pause(S, Result.data() + ResultMessage);
                break;
            case ResultReturnToLast:
                ReturnToCheckpoint(S, true);
                break;
            case ResultReturnToCurrent:
                ReturnToCheckpoint(S, false);
                break;
            case ResultReturnToStart:
                S.Branches.clear();
                S.Choices.clear();
                S.ScopeStack.clear();
                S.Checkpoints.clear();
                ModifyCurrent(S, S.Starting);
                break;
            default:
                Abort(S);
                break;
            }
        }
        S.Depth--;
    }

    inline UObject* Start(uint64 EntryTag, AActor* Instigator, uint64 InstigatorTag, AActor* Target, uint64 TargetTag)
    {
        auto& T = Types();
        if (!T.bEnabled || !Instigator || !Target)
            return nullptr;
        auto Instance = (UObject*)UGameplayStatics::SpawnObject(T.Instance, (UObject*)UWorld::GetWorld());
        if (!Instance)
            return nullptr;
        auto& S = States()[Instance];
        S = FState{};
        S.Instance = Instance;
        AssignParticipant(S, Target, TargetTag);
        AssignParticipant(S, Instigator, InstigatorTag);
        auto Legal = DetermineBranches(S, EntryDestinations(EntryTag), FailedButVisible);
        printf("[Boron][Conv] start entry=%s starts=%d player=%p npc=%p\n", TagString(EntryTag).c_str(), (int)Legal.size(), (void*)S.PlayerComp, (void*)S.NPCComp);
        if (Legal.empty())
        {
            S.bEnded = true;
            return Instance;
        }
        FBranch Starting;
        Starting.Node = Legal[rand() % Legal.size()];
        S.Starting = Starting;
        S.Current = Starting;
        if (T.bInstanceNode)
            At<FNodeId>(Instance, InstanceNodeOffset) = Starting.Node;
        for (auto& Part : S.Participants)
            NotifyStarted(S, Part);
        S.bStarted = true;
        OnNodeModified(S);
        return Instance;
    }

    inline UObject* EnsurePlayerComponent(AFortPlayerControllerAthena* PC)
    {
        UObject* Comp = PC->GetComponentByClass((UClass*)Types().ParticipantComp);
        if (Comp)
            return Comp;
        static auto BPClass = FindObject<UClass>(L"/FortniteConversation/Conversation/FortPlayerConversationComponent.FortPlayerConversationComponent_C");
        Comp = AddComponent(PC, BPClass ? (const UClass*)BPClass : Types().PlayerComp, false);
        printf("[Boron][Conv] added player conversation component %p\n", (void*)Comp);
        return Comp;
    }

    inline bool TryStart(AFortPlayerControllerAthena* PC, AActor* Target)
    {
        auto& T = Types();
        if (!T.bEnabled || !PC || !Target || Target->IsA<AFortPlayerControllerAthena>())
            return false;
        auto NPCComp = Target->GetComponentByClass((UClass*)T.NonPlayerComp);
        if (!NPCComp)
            return false;
        auto& O = Offs();
        auto PlayerComp = EnsurePlayerComponent(PC);
        if (!PlayerComp)
            return false;
        if (auto Old = StateOf(At<UObject*>(PlayerComp, O.AuthCurrent)))
            Abort(*Old);
        Start(At<uint64>(NPCComp, O.EntryTag), PC, At<uint64>(NPCComp, O.InteractorTag), Target, At<uint64>(NPCComp, O.SelfTag));
        Cleanup();
        return true;
    }

    inline UDataTable* NPCTable(const wchar_t* Path)
    {
        auto Table = (UDataTable*)FindObject<UDataTable>(Path);
        if (Table)
            Table->AddToRoot();
        return Table;
    }

    inline UDataTable* SalesTable()
    {
        static auto Table = NPCTable(L"/BattlepassS15/Balance/DataTables/NPCSales.NPCSales");
        return Table;
    }

    inline UDataTable* ServicesTable()
    {
        static auto Table = NPCTable(L"/BattlepassS15/Balance/DataTables/NPCServices.NPCServices");
        return Table;
    }

    inline void FillSales(UObject* Comp)
    {
        auto& O = Offs();
        auto Character = At<UObject*>(Comp, O.CharacterData);
        auto& Table = At<UDataTable*>(Comp, O.SalesTable);
        if (!Table)
            Table = SalesTable();
        auto& Rows = At<FRawArray>(Comp, O.SupportedSales);
        if (!Character || !Table || Rows.Num > 0)
            return;
        auto NPCTag = At<uint64>(Character, O.CharacterTag);
        std::vector<uint8*> Matching;
        for (auto& [RowName, Row] : Table->GetRowMap())
            if (Row && *(uint64*)(Row + 0x8) == NPCTag)
                Matching.push_back(Row);
        std::stable_sort(Matching.begin(), Matching.end(), [](uint8* A, uint8* B) { return *(int32*)(A + 0x40) > *(int32*)(B + 0x40); });
        for (auto Row : Matching)
        {
            auto Elem = RawAdd(Rows, SaleRowSize);
            memcpy(Elem, Row, SaleRowSize);
            auto& Tags = *(FRawArray*)(Elem + 0x20);
            Tags = RawCopy(Tags, 8);
            auto& Parents = *(FRawArray*)(Elem + 0x30);
            Parents = RawCopy(Parents, 8);
        }
    }

    inline void FillServices(UObject* Comp)
    {
        auto& O = Offs();
        auto Character = At<UObject*>(Comp, O.CharacterData);
        auto& Table = At<UDataTable*>(Comp, O.ServicesTable);
        if (!Table)
            Table = ServicesTable();
        auto& Tags = At<FRawArray>(Comp, O.SupportedServices);
        if (!Character || !Table || Tags.Num > 0)
            return;
        auto NPCTag = At<uint64>(Character, O.CharacterTag);
        auto MaxServices = At<int32>(Comp, O.MaxServices);
        for (auto& [RowName, Row] : Table->GetRowMap())
        {
            if (!Row || *(uint64*)(Row + 0x8) != NPCTag)
                continue;
            if (MaxServices > 0 && Tags.Num >= MaxServices)
                break;
            float Chance = *(float*)(Row + 0x10);
            if (Chance > 0.f && Chance < 1.f && ((float)rand() / (float)RAND_MAX) > Chance)
                continue;
            auto Service = *(uint64*)(Row + 0x14);
            bool bDuplicate = false;
            for (int32 i = 0; i < Tags.Num && !bDuplicate; i++)
                bDuplicate = *(uint64*)(Tags.Data + (uint64)i * 8) == Service;
            if (!bDuplicate)
                *(uint64*)RawAdd(Tags, 8) = Service;
        }
    }

    inline void SetupNPC(AFortPlayerPawnAthena* Pawn, UObject* SpawnerConversation)
    {
        auto& T = Types();
        auto& O = Offs();
        if (!T.bEnabled || !Pawn || !IsA(SpawnerConversation, T.SpawnerConversation))
            return;
        auto CompClass = At<const UClass*>(SpawnerConversation, O.SpawnerCompClass);
        if (!CompClass)
            return;
        auto Comp = (UObject*)Pawn->GetComponentByClass((UClass*)CompClass);
        bool bAdded = false;
        if (!Comp)
        {
            Comp = AddComponent(Pawn, CompClass, true);
            bAdded = Comp != nullptr;
        }
        if (!IsA(Comp, T.NPCComp))
        {
            printf("[Boron][Conv] npc %s has no conversation component\n", CompClass->Name.ToString().c_str());
            return;
        }
        At<AActor*>(Comp, O.PawnOwner) = Pawn;
        At<AActor*>(Comp, O.ControllerOwner) = (AActor*)Pawn->Controller;
        At<uint64>(Comp, O.EntryTag) = At<uint64>(SpawnerConversation, O.SpawnerEntryTag);
        At<uint64>(Comp, O.InteractorTag) = At<uint64>(SpawnerConversation, O.SpawnerInteractorTag);
        At<uint64>(Comp, O.SelfTag) = At<uint64>(SpawnerConversation, O.SpawnerSelfTag);
        if (auto Character = At<UObject*>(SpawnerConversation, O.SpawnerCharacterData))
            At<UObject*>(Comp, O.CharacterData) = Character;
        At<uint64>(Comp, O.CollisionProfile) = At<uint64>(SpawnerConversation, O.SpawnerCollisionProfile);
        memcpy((uint8*)Comp + O.BoxExtent, (uint8*)SpawnerConversation + O.SpawnerBoxExtent, 12);
        memcpy((uint8*)Comp + O.BoxOffset, (uint8*)SpawnerConversation + O.SpawnerBoxOffset, 12);
        FillSales(Comp);
        FillServices(Comp);
        {
            auto Character = At<UObject*>(Comp, O.CharacterData);
            auto SalesTable = At<UDataTable*>(Comp, O.SalesTable);
            auto ServicesTable = At<UDataTable*>(Comp, O.ServicesTable);
            printf("[Boron][Conv] npc %s sales=%s rows=%d services=%s tags=%d\n", Character ? TagString(At<uint64>(Character, O.CharacterTag)).c_str() : "nochar",
                   SalesTable ? SalesTable->Name.ToString().c_str() : "null", At<FRawArray>(Comp, O.SupportedSales).Num,
                   ServicesTable ? ServicesTable->Name.ToString().c_str() : "null", At<FRawArray>(Comp, O.SupportedServices).Num);
        }
        if (bAdded)
            FinishComponent(Pawn, Comp);
        SetBool(Comp, "bCanStartConversation", true);
        CallSingle(Comp, "OnRep_CanStartConversation", nullptr, 0);
    }

    inline void MergeLootTables()
    {
        static const wchar_t* TierTables[] = { L"/BattlepassS15/Balance/DataTables/AthenaNPCBundleLootTierData_Client.AthenaNPCBundleLootTierData_Client" };
        static const wchar_t* PackageTables[] = { L"/BattlepassS15/Balance/DataTables/AthenaNPCBundleLootPackages_Client.AthenaNPCBundleLootPackages_Client" };
        int32 Tiers = 0;
        int32 Packages = 0;
        int32 Found = 0;
        for (auto Path : TierTables)
        {
            auto Table = (UDataTable*)FindObject<UDataTable>(Path);
            if (!Table)
                continue;
            Found++;
            Table->AddToRoot();
            for (auto& [Key, Val] : Table->GetRowMap())
            {
                auto Row = (FFortLootTierData*)Val;
                if (!Row)
                    continue;
                auto& Group = TierDataMap[Row->TierGroup.ComparisonIndex];
                bool bFound = false;
                for (auto& Existing : Group)
                    bFound |= Existing == Row;
                if (!bFound)
                {
                    Group.Add(Row);
                    Tiers++;
                }
            }
        }
        for (auto Path : PackageTables)
        {
            auto Table = (UDataTable*)FindObject<UDataTable>(Path);
            if (!Table)
                continue;
            Found++;
            Table->AddToRoot();
            for (auto& [Key, Val] : Table->GetRowMap())
            {
                auto Row = (FFortLootPackageData*)Val;
                if (!Row)
                    continue;
                auto& Group = LootPackageMap[Row->LootPackageID.ComparisonIndex];
                bool bFound = false;
                for (auto& Existing : Group)
                    bFound |= Existing == Row;
                if (!bFound)
                {
                    Group.Add(Row);
                    Packages++;
                }
            }
        }
        printf("[Boron][Conv] npc loot tables found=%d merged tiers=%d packages=%d\n", Found, Tiers, Packages);
    }

    inline std::string SoftPath(const uint8* SoftPtr)
    {
        return std::string(((FName*)(SoftPtr + 0x10))->ToString().c_str());
    }

    inline void DumpNPCData()
    {
        FILE* File = nullptr;
        if (fopen_s(&File, "Boron_NPCData.txt", "w") || !File)
            return;
        if (auto Table = SalesTable())
        {
            fprintf(File, "== NPCSales\n");
            for (auto& [Key, Row] : Table->GetRowMap())
                if (Row)
                    fprintf(File, "%s npc=%s tier=%s level=%d priority=%d\n", Key.ToString().c_str(), TagString(*(uint64*)(Row + 0x8)).c_str(),
                            ((FName*)(Row + 0x10))->ToString().c_str(), *(int32*)(Row + 0x18), *(int32*)(Row + 0x40));
        }
        static const wchar_t* ServiceTables[] = { L"NPCServices", L"NPCServices_Classic", L"NPCServices_Low", L"NPCServices_Heavy", L"NPCServices_Snipers",
                                                  L"NPCServices_Sword", L"NPCServices_Unvaulted", L"NPCServices_Bodyguard", L"NPCServices_HighExplosives" };
        for (auto Name : ServiceTables)
        {
            auto Path = std::wstring(L"/BattlepassS15/Balance/DataTables/") + Name + L"." + Name;
            auto Table = (UDataTable*)FindObject<UDataTable>(Path.c_str());
            if (!Table)
                continue;
            fprintf(File, "== %ls\n", Name);
            for (auto& [Key, Row] : Table->GetRowMap())
                if (Row)
                    fprintf(File, "%s npc=%s service=%s chance=%.2f priority=%d\n", Key.ToString().c_str(), TagString(*(uint64*)(Row + 0x8)).c_str(),
                            TagString(*(uint64*)(Row + 0x14)).c_str(), *(float*)(Row + 0x10), *(int32*)(Row + 0x1c));
        }
        if (auto Table = (UDataTable*)FindObject<UDataTable>(L"/BattlepassS15/Balance/DataTables/NPCQuests.NPCQuests"))
        {
            fprintf(File, "== NPCQuests\n");
            for (auto& [Key, Row] : Table->GetRowMap())
                if (Row)
                    fprintf(File, "%s npc=%s quest=%s weight=%.2f\n", Key.ToString().c_str(), TagString(*(uint64*)(Row + 0x8)).c_str(), SoftPath(Row + 0x10).c_str(),
                            *(float*)(Row + 0x38));
        }
        if (auto Table = (UDataTable*)FindObject<UDataTable>(L"/BattlepassS15/Balance/DataTables/AthenaNPCBundleLootTierData_Client.AthenaNPCBundleLootTierData_Client"))
        {
            fprintf(File, "== AthenaNPCBundleLootTierData_Client\n");
            for (auto& [Key, Val] : Table->GetRowMap())
            {
                auto Row = (FFortLootTierData*)Val;
                if (Row)
                    fprintf(File, "%s group=%s tier=%d weight=%.2f drops=%.2f package=%s\n", Key.ToString().c_str(), Row->TierGroup.ToString().c_str(), Row->LootTier,
                            Row->Weight, Row->NumLootPackageDrops, Row->LootPackage.ToString().c_str());
            }
        }
        if (auto Table = (UDataTable*)FindObject<UDataTable>(L"/BattlepassS15/Balance/DataTables/AthenaNPCBundleLootPackages_Client.AthenaNPCBundleLootPackages_Client"))
        {
            fprintf(File, "== AthenaNPCBundleLootPackages_Client\n");
            for (auto& [Key, Val] : Table->GetRowMap())
            {
                auto Row = (FFortLootPackageData*)Val;
                if (Row)
                    fprintf(File, "%s id=%s weight=%.2f call=%s item=%s count=%d\n", Key.ToString().c_str(), Row->LootPackageID.ToString().c_str(), Row->Weight,
                            Row->LootPackageCall.ToString().c_str(), SoftPath((const uint8*)&Row->ItemDefinition).c_str(), Row->Count);
            }
        }
        if (Types().Service)
        {
            std::vector<UCurveTable*> Pricing;
            for (int32 i = 0; i < TUObjectArray::Num(); i++)
            {
                auto Obj = (UObject*)TUObjectArray::GetObjectByIndex(i);
                if (!Obj || Obj->IsDefaultObject() || !Obj->IsA(Types().Service))
                    continue;
                auto& Tables = At<FRawArray>(Obj, Offs().ServicePricing);
                for (int32 j = 0; j < Tables.Num; j++)
                {
                    auto Table = *(UCurveTable**)(Tables.Data + (uint64)j * 8);
                    bool bSeen = !Table;
                    for (auto Seen : Pricing)
                        bSeen |= Seen == Table;
                    if (bSeen)
                        continue;
                    Pricing.push_back(Table);
                    fprintf(File, "== pricing %s (first used by %s)\n", Table->Name.ToString().c_str(), Obj->Class->Name.ToString().c_str());
                    for (auto& [RowName, Curve] : Table->GetRowMap())
                    {
                        fprintf(File, "%s", RowName.ToString().c_str());
                        for (int32 Level = 0; Level <= 5; Level++)
                        {
                            float Value = 0.f;
                            UDataTableFunctionLibrary::EvaluateCurveTableRow(Table, RowName, (float)Level, nullptr, &Value, FString());
                            fprintf(File, " x%d=%.1f", Level, Value);
                        }
                        fprintf(File, "\n");
                    }
                }
            }
        }
        fclose(File);
        printf("[Boron][Conv] npc data dumped to Boron_NPCData.txt\n");
    }

    inline int32 CountParams(UFunction* Fn)
    {
        int32 Count = 0;
        if (!Fn)
            return 0;
        for (auto& Param : Fn->GetParams().NameOffsetMap)
            if (!(Param.PropertyFlags & 0x400))
                Count++;
        return Count;
    }

    inline std::unordered_map<void*, int32>& ParamCounts()
    {
        static std::unordered_map<void*, int32> Value;
        return Value;
    }

    inline void StepRest(FFrame& Stack, void* Hook, int32 Stepped)
    {
        uint8 Scratch[0x200];
        for (int32 i = Stepped; i < ParamCounts()[Hook]; i++)
        {
            memset(Scratch, 0, sizeof(Scratch));
            Stack.StepCompiledIn(Scratch);
        }
    }

    template <uint8 Type, int32 Extra>
    inline void ExecHelperResult(UObject*, FFrame& Stack, void* Ret)
    {
        uint8 Context[0x80]{};
        uint8 Arg[0x80]{};
        Stack.StepCompiledIn(Context);
        if (Extra)
            Stack.StepCompiledIn(Arg);
        StepRest(Stack, (void*)&ExecHelperResult<Type, Extra>, Extra ? 2 : 1);
        Stack.IncrementCode();
        if (!Ret)
            return;
        auto Result = (uint8*)Ret;
        Result[0] = Type;
        if (Type == ResultAdvanceWithChoice)
            memcpy(Result + ResultChoice, Arg, 0x30);
        else if (Type == ResultPause)
            memcpy(Result + ResultMessage, Arg, MessageSize);
    }

    inline void ExecMakeParticipant(UObject*, FFrame& Stack, void*)
    {
        uint8 Context[0x80]{};
        AActor* Actor = nullptr;
        uint64 Tag = 0;
        Stack.StepCompiledIn(Context);
        Stack.StepCompiledIn(&Actor);
        Stack.StepCompiledIn(&Tag);
        StepRest(Stack, (void*)&ExecMakeParticipant, 3);
        Stack.IncrementCode();
        if (auto S = StateOf(*(UObject**)(Context + 0x8)))
            AssignParticipant(*S, Actor, Tag);
    }

    inline void ExecStartConversation(UObject*, FFrame& Stack, void* Ret)
    {
        uint64 EntryTag = 0;
        AActor* Instigator = nullptr;
        uint64 InstigatorTag = 0;
        AActor* Target = nullptr;
        uint64 TargetTag = 0;
        Stack.StepCompiledIn(&EntryTag);
        Stack.StepCompiledIn(&Instigator);
        Stack.StepCompiledIn(&InstigatorTag);
        Stack.StepCompiledIn(&Target);
        Stack.StepCompiledIn(&TargetTag);
        StepRest(Stack, (void*)&ExecStartConversation, 5);
        Stack.IncrementCode();
        auto Instance = Start(EntryTag, Instigator, InstigatorTag, Target, TargetTag);
        if (Ret)
            *(UObject**)Ret = Instance;
        Cleanup();
    }

    inline void ExecServerAdvance(UObject* Context, FFrame& Stack, void*)
    {
        uint8 Request[0x80]{};
        Stack.StepCompiledIn(Request);
        StepRest(Stack, (void*)&ExecServerAdvance, 1);
        Stack.IncrementCode();
        if (auto S = StateOf(At<UObject*>(Context, Offs().AuthCurrent)))
            ServerAdvance(*S, Request);
        Cleanup();
    }

    inline void ExecServerAbort(UObject* Context, FFrame& Stack, void*)
    {
        StepRest(Stack, (void*)&ExecServerAbort, 0);
        Stack.IncrementCode();
        if (auto S = StateOf(At<UObject*>(Context, Offs().AuthCurrent)))
            Abort(*S);
        Cleanup();
    }

    inline bool InstallHook(const UClass* Class, const char* Name, void* Detour)
    {
        auto Cdo = Class ? Class->GetDefaultObj() : nullptr;
        auto Fn = Cdo ? Cdo->GetFunction(Name) : nullptr;
        if (!Fn)
        {
            printf("[Boron][Conv] hook %s not found\n", Name);
            return false;
        }
        ParamCounts()[Detour] = CountParams(Fn);
        Hooking::ExecHook(Fn, Detour);
        return true;
    }

    inline int32 StructSize(const char* Name)
    {
        auto Struct = (const UStruct*)TUObjectArray::FindObject(Name, 0x10, nullptr);
        return Struct ? Struct->GetPropertiesSize() : -1;
    }

    inline uint32 OffsetOf(const UClass* Class, const char* Name, bool& bOk)
    {
        auto Offset = Class ? Class->GetOffset(Name) : (uint32)-1;
        if (Offset == (uint32)-1)
        {
            printf("[Boron][Conv] missing property %s\n", Name);
            bOk = false;
        }
        return Offset;
    }

    inline void Init()
    {
        auto& T = Types();
        if (T.bInitialized)
            return;
        T.bInitialized = true;
        if (VersionInfo.FortniteVersion < 15.0 || VersionInfo.FortniteVersion >= 16.0 || FVector::Size() != 0xc)
            return;

        T.Database = FindClass("ConversationDatabase");
        T.Node = FindClass("ConversationNode");
        T.NodeWithLinks = FindClass("ConversationNodeWithLinks");
        T.TaskNode = FindClass("ConversationTaskNode");
        T.LinkNode = FindClass("ConversationLinkNode");
        T.ChoiceNode = FindClass("ConversationChoiceNode");
        T.RequirementNode = FindClass("ConversationRequirementNode");
        T.SideEffectNode = FindClass("ConversationSideEffectNode");
        T.Speech = FindClass("FortConversationTaskNode_Speech");
        T.Back = FindClass("FortConversationTaskNode_Back");
        T.Service = FindClass("FortConversationTaskNode_Service");
        T.SellItem = FindClass("FortConversationTaskNode_SellItem");
        T.UpgradeItem = FindClass("FortConversationTaskNode_UpgradeItem");
        T.HasService = FindClass("FortConversationRequirement_HasService");
        T.HasNoActiveQuests = FindClass("FortConversationRequirement_HasNoActiveQuests");
        T.ParticipantComp = FindClass("ConversationParticipantComponent");
        T.NonPlayerComp = FindClass("FortNonPlayerConversationParticipantComponent");
        T.NPCComp = FindClass("FortNPCConversationParticipantComponent");
        T.PlayerComp = FindClass("FortPlayerConversationComponent");
        T.SpawnerConversation = FindClass("FortAthenaAISpawnerDataComponent_AIBotConversation");
        T.CharacterData = FindClass("FortTandemCharacterData");
        auto BaseInstance = FindClass("ConversationInstance");
        T.Instance = FindClass("FortConversationInstance");
        if (!T.Instance)
            T.Instance = BaseInstance;
        for (auto Name : { "FortConversationTaskNode_HireNPC", "FortConversationTaskNode_DuelNPC", "FortConversationTaskNode_GrantPlayerBounty", "FortConversationTaskNode_GrantQuest",
                           "FortConversationTaskNode_GrantSlottedQuest", "FortConversationTaskNode_GrantStaticQuest", "FortCompleteQuestConversationTaskNode",
                           "FortConversationRequirement_AllSlottedQuestPrerequisitesCompleted", "FortConversationRequirement_SlottedQuestNotCompletedThisMatch" })
            if (auto Class = FindClass(Name))
                T.Hidden.push_back(Class);

        if (!T.Database || !T.Node || !T.NodeWithLinks || !T.TaskNode || !T.LinkNode || !T.ChoiceNode || !T.RequirementNode || !T.SideEffectNode || !T.ParticipantComp || !T.NonPlayerComp || !T.NPCComp ||
            !T.Instance || !BaseInstance || !T.SpawnerConversation || !T.CharacterData)
        {
            printf("[Boron][Conv] conversation classes missing, NPC conversations disabled\n");
            return;
        }

        struct FExpected
        {
            const char* Name;
            int32 Size;
        };
        for (auto& Expected : { FExpected{ "ConversationContext", ContextSize }, FExpected{ "ClientConversationOptionEntry", OptionSize }, FExpected{ "ClientConversationMessage", MessageSize },
                                FExpected{ "ClientConversationMessagePayload", PayloadSize }, FExpected{ "ConversationTaskResult", ResultSize }, FExpected{ "ConversationParticipantEntry", EntrySize },
                                FExpected{ "ConversationEntryList", EntryListSize }, FExpected{ "ContextualMessageCandidate", CandidateSize }, FExpected{ "NPCSaleInventoryRow", SaleRowSize },
                                FExpected{ "NPCDynamicServiceRow", ServiceRowSize } })
        {
            auto Size = StructSize(Expected.Name);
            if (Size != Expected.Size)
            {
                printf("[Boron][Conv] struct %s size %d (expected %d), NPC conversations disabled\n", Expected.Name, Size, Expected.Size);
                return;
            }
        }
        T.bInstanceNode = BaseInstance->GetPropertiesSize() == 0x190;

        bool bOk = true;
        auto& O = Offs();
        O.DatabaseNodes = OffsetOf(T.Database, "ReachableNodeMap", bOk);
        O.DatabaseEntries = OffsetOf(T.Database, "EntryTags", bOk);
        O.NodeEval = OffsetOf(T.Node, "EvalWorldContextObj", bOk);
        O.NodeGuid = OffsetOf(T.Node, "Compiled_NodeGUID", bOk);
        O.NodeOutputs = OffsetOf(T.NodeWithLinks, "OutputConnections", bOk);
        O.TaskSubNodes = OffsetOf(T.TaskNode, "SubNodes", bOk);
        O.LinkRemoteTag = OffsetOf(T.LinkNode, "RemoteEntryTag", bOk);
        O.ChoiceText = OffsetOf(T.ChoiceNode, "DefaultChoiceDisplayText", bOk);
        O.ChoiceTags = OffsetOf(T.ChoiceNode, "ChoiceTags", bOk);
        O.InstanceParticipants = OffsetOf(BaseInstance, "Participants", bOk);
        O.AuthConversations = OffsetOf(T.ParticipantComp, "Auth_Conversations", bOk);
        O.AuthCurrent = OffsetOf(T.ParticipantComp, "Auth_CurrentConversation", bOk);
        O.ConversationsActive = OffsetOf(T.ParticipantComp, "ConversationsActive", bOk);
        O.EntryTag = OffsetOf(T.NonPlayerComp, "ConversationEntryTag", bOk);
        O.InteractorTag = OffsetOf(T.NonPlayerComp, "InteractorParticipantTag", bOk);
        O.SelfTag = OffsetOf(T.NonPlayerComp, "SelfParticipantTag", bOk);
        O.PawnOwner = OffsetOf(T.NPCComp, "PlayerPawnOwner", bOk);
        O.ControllerOwner = OffsetOf(T.NPCComp, "BotControllerOwner", bOk);
        O.CharacterData = OffsetOf(T.NPCComp, "CharacterData", bOk);
        O.CollisionProfile = OffsetOf(T.NPCComp, "ConversationInteractionCollisionProfile", bOk);
        O.BoxExtent = OffsetOf(T.NPCComp, "ConversationInteractionBoxExtent", bOk);
        O.BoxOffset = OffsetOf(T.NPCComp, "ConversationInteractionBoxOffset", bOk);
        O.SupportedServices = OffsetOf(T.NPCComp, "SupportedServices", bOk);
        O.SupportedSales = OffsetOf(T.NPCComp, "SupportedSales", bOk);
        O.MaxServices = OffsetOf(T.NPCComp, "MaxServices", bOk);
        O.ServicesTable = OffsetOf(T.NPCComp, "Services", bOk);
        O.SalesTable = OffsetOf(T.NPCComp, "SalesInventory", bOk);
        O.SpawnerCompClass = OffsetOf(T.SpawnerConversation, "ConversationComponentClass", bOk);
        O.SpawnerEntryTag = OffsetOf(T.SpawnerConversation, "ConversationEntryTag", bOk);
        O.SpawnerInteractorTag = OffsetOf(T.SpawnerConversation, "InteractorParticipantTag", bOk);
        O.SpawnerSelfTag = OffsetOf(T.SpawnerConversation, "SelfParticipantTag", bOk);
        O.SpawnerCharacterData = OffsetOf(T.SpawnerConversation, "CharacterData", bOk);
        O.SpawnerCollisionProfile = OffsetOf(T.SpawnerConversation, "ConversationInteractionCollisionProfile", bOk);
        O.SpawnerBoxExtent = OffsetOf(T.SpawnerConversation, "ConversationInteractionBoxExtent", bOk);
        O.SpawnerBoxOffset = OffsetOf(T.SpawnerConversation, "ConversationInteractionBoxOffset", bOk);
        O.CharacterTag = OffsetOf(T.CharacterData, "GameplayTag", bOk);
        O.CharacterName = OffsetOf(T.CharacterData, "DisplayName", bOk);
        if (T.Speech)
        {
            O.SpeechGeneral = OffsetOf(T.Speech, "GeneralConfig", bOk);
            O.SpeechPerSpeaker = OffsetOf(T.Speech, "SpeakerEntryTagToConfig", bOk);
        }
        if (T.Service)
        {
            O.ServiceCurrency = OffsetOf(T.Service, "ResourceCurrency", bOk);
            O.ServicePricing = OffsetOf(T.Service, "PricingTables", bOk);
        }
        if (T.SellItem)
            O.SellSlot = OffsetOf(T.SellItem, "SellSlot", bOk);
        if (T.HasService)
            O.HasServiceTag = OffsetOf(T.HasService, "ServiceTag", bOk);
        if (!bOk)
        {
            printf("[Boron][Conv] NPC conversations disabled\n");
            return;
        }

        static const char* Databases[] = { "NPC_Conversation_Bandolier", "NPC_Conversation_BeefBoss", "NPC_Conversation_BigChuggus", "NPC_Conversation_BigFoot", "NPC_Conversation_Blaze",
                                           "NPC_Conversation_Brutus", "NPC_Conversation_Bullseye", "NPC_Conversation_BunkerJonesy", "NPC_Conversation_Burnout", "NPC_Conversation_Bushranger",
                                           "NPC_Conversation_Cole", "NPC_Conversation_Deadfire", "NPC_Conversation_Doggo", "NPC_Conversation_Dummy", "NPC_Conversation_FarmerSteel",
                                           "NPC_Conversation_Fishstick", "NPC_Conversation_FutureSamurai", "NPC_Conversation_Gladiator", "NPC_Conversation_Grimbles", "NPC_Conversation_Guide",
                                           "NPC_Conversation_Kit", "NPC_Conversation_Kyle", "NPC_Conversation_Longshot", "NPC_Conversation_Outcast", "NPC_Conversation_Outlaw",
                                           "NPC_Conversation_Ragnarok", "NPC_Conversation_Rapscallion", "NPC_Conversation_Remedy", "NPC_Conversation_Ruckus", "NPC_Conversation_Shapeshifter",
                                           "NPC_Conversation_Sleuth", "NPC_Conversation_Snomando", "NPC_Conversation_Sparkplug", "NPC_Conversation_Splode", "NPC_Conversation_Sunflower",
                                           "NPC_Conversation_TheReaper", "NPC_Conversation_TomatoHead", "NPC_Conversation_Triggerfish", "NPC_Conversation_Turk", "NPC_Conversation_WeaponsExpert",
                                           "Share_Conversation_Quests", "Share_Conversation_Services" };
        std::string Missing;
        for (auto Name : Databases)
        {
            auto Path = std::string("/BattlepassS15/Conversations/") + Name + "." + Name;
            auto Database = (UObject*)FindObject<UObject>(std::wstring(Path.begin(), Path.end()).c_str());
            if (Database && Database->IsA(T.Database))
                AddDatabase(Database);
            else
                Missing += std::string(Name) + " ";
        }
        for (int32 i = 0; i < TUObjectArray::Num(); i++)
        {
            auto Obj = (UObject*)TUObjectArray::GetObjectByIndex(i);
            if (Obj && !Obj->IsDefaultObject() && Obj->IsA(T.Database))
                AddDatabase(Obj);
        }
        if (!Missing.empty())
            printf("[Boron][Conv] databases not found: %s\n", Missing.c_str());

        auto Helpers = FindClass("ConversationContextHelpers");
        InstallHook(Helpers, "AdvanceConversation", (void*)&ExecHelperResult<ResultAdvance, 0>);
        InstallHook(Helpers, "AdvanceConversationWithChoice", (void*)&ExecHelperResult<ResultAdvanceWithChoice, 1>);
        InstallHook(Helpers, "PauseConversationAndSendClientChoices", (void*)&ExecHelperResult<ResultPause, 1>);
        InstallHook(Helpers, "ReturnToLastClientChoice", (void*)&ExecHelperResult<ResultReturnToLast, 0>);
        InstallHook(Helpers, "ReturnToCurrentClientChoice", (void*)&ExecHelperResult<ResultReturnToCurrent, 0>);
        InstallHook(Helpers, "ReturnToConversationStart", (void*)&ExecHelperResult<ResultReturnToStart, 0>);
        InstallHook(Helpers, "MakeConversationParticipant", (void*)&ExecMakeParticipant);
        InstallHook(FindClass("ConversationLibrary"), "StartConversation", (void*)&ExecStartConversation);
        InstallHook(T.ParticipantComp, "ServerAdvanceConversation", (void*)&ExecServerAdvance);
        if (T.PlayerComp)
            InstallHook(T.PlayerComp, "RequestServerAbortConversation", (void*)&ExecServerAbort);

        MergeLootTables();
        DumpNPCData();
        T.bEnabled = true;
        auto& Reg = Registry();
        printf("[Boron][Conv] ready databases=%d nodes=%d entries=%d\n", (int)Reg.Databases.size(), (int)Reg.Nodes.size(), (int)Reg.Entries.size());
    }
}
