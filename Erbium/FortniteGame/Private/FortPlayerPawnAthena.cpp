#include "pch.h"
#include "../../Erbium/Public/Configuration.h"
#include "../Public/FortPlayerPawnAthena.h"
#include "../Public/FortInventory.h"
#include "../Public/FortPhysicsPawn.h"
#include "../Public/FortPlayerControllerAthena.h"
#include "../Public/FortWeapon.h"
#include "../Public/BuildingSMActor.h"
#include "../Public/FortGameStateAthena.h"
#include "../Public/BattleRoyaleGamePhaseLogic.h"

struct _Pad_0xC
{
    uint8_t Padding[0xC];
};

struct _Pad_0x18
{
    uint8_t Padding[0x18];
};

struct FFortPickupRequestInfo final
{
public:
    struct FGuid SwapWithItem;    // 0x0000(0x0010)(ZeroConstructor, IsPlainOldData, NoDestructor, HasGetValueTypeHash, NativeAccessSpecifierPublic)
    float FlyTime;                // 0x0010(0x0004)(ZeroConstructor, IsPlainOldData, NoDestructor, HasGetValueTypeHash, NativeAccessSpecifierPublic)
    struct _Pad_0xC Direction;    // 0x0014(0x000C)(ZeroConstructor, IsPlainOldData, NoDestructor, HasGetValueTypeHash, NativeAccessSpecifierPublic)
    uint8 bPlayPickupSound : 1;   // 0x0020(0x0001)(BitIndex: 0x00, PropSize: 0x0001 (NoDestructor, HasGetValueTypeHash, NativeAccessSpecifierPublic))
    uint8 bIsAutoPickup : 1;      // 0x0020(0x0001)(BitIndex: 0x01, PropSize: 0x0001 (NoDestructor, HasGetValueTypeHash, NativeAccessSpecifierPublic))
    uint8 bUseRequestedSwap : 1;  // 0x0020(0x0001)(BitIndex: 0x02, PropSize: 0x0001 (NoDestructor, HasGetValueTypeHash, NativeAccessSpecifierPublic))
    uint8 bTrySwapWithWeapon : 1; // 0x0020(0x0001)(BitIndex: 0x03, PropSize: 0x0001 (NoDestructor, HasGetValueTypeHash, NativeAccessSpecifierPublic))
    uint8 Pad_21[0x3];            // 0x0021(0x0003)(Fixing Struct Size After Last Property [ Dumper-7 ])
};

struct alignas(0x8) FFortPickupRequestInfoNew final
{
public:
    struct FGuid SwapWithItem; // 0x0000(0x0010)(ZeroConstructor, IsPlainOldData, NoDestructor, HasGetValueTypeHash, NativeAccessSpecifierPublic)
    float FlyTime;             // 0x0010(0x0004)(ZeroConstructor, IsPlainOldData, NoDestructor, HasGetValueTypeHash, NativeAccessSpecifierPublic)
    uint8 Pad_1[0x4];
    struct _Pad_0x18 Direction;   // 0x0014(0x000C)(ZeroConstructor, IsPlainOldData, NoDestructor, HasGetValueTypeHash, NativeAccessSpecifierPublic)
    uint8 bPlayPickupSound : 1;   // 0x0020(0x0001)(BitIndex: 0x00, PropSize: 0x0001 (NoDestructor, HasGetValueTypeHash, NativeAccessSpecifierPublic))
    uint8 bIsAutoPickup : 1;      // 0x0020(0x0001)(BitIndex: 0x01, PropSize: 0x0001 (NoDestructor, HasGetValueTypeHash, NativeAccessSpecifierPublic))
    uint8 bUseRequestedSwap : 1;  // 0x0020(0x0001)(BitIndex: 0x02, PropSize: 0x0001 (NoDestructor, HasGetValueTypeHash, NativeAccessSpecifierPublic))
    uint8 bTrySwapWithWeapon : 1; // 0x0020(0x0001)(BitIndex: 0x03, PropSize: 0x0001 (NoDestructor, HasGetValueTypeHash, NativeAccessSpecifierPublic))
    uint8 Pad_2[0x7];             // 0x0021(0x0003)(Fixing Struct Size After Last Property [ Dumper-7 ])
};

uint64_t SetPickupTarget_ = 0;

static void (*ServerHandlePickupProbeOG)(UObject*, FFrame&) = nullptr;
static void ServerHandlePickupProbe(UObject* Context, FFrame& Stack)
{
    AFortPickupAthena* Pickup = nullptr;
    float InFlyTime;
    FVector InStartDirection;
    bool bPlayPickupSound;

    Stack.StepCompiledIn(&Pickup);
    Stack.StepCompiledIn(&InFlyTime);
    Stack.StepCompiledIn(&InStartDirection);
    Stack.StepCompiledIn(&bPlayPickupSound);
    Stack.IncrementCode();

    auto Pawn = (AFortPlayerPawnAthena*)Context;
    auto PC = (Pawn && Pawn->Controller) ? (AFortPlayerControllerAthena*)Pawn->Controller : nullptr;

    int before = -1, after = -1;

    if (PC && PC->WorldInventory)
        before = PC->WorldInventory->Inventory.ReplicatedEntries.Num();

    if (PC && PC->WorldInventory && Pickup && Pickup->PrimaryPickupItemEntry.ItemDefinition)
    {
        auto Inv = PC->WorldInventory;
        auto Def = Pickup->PrimaryPickupItemEntry.ItemDefinition;
        auto MaxStack = Def->GetMaxStackSize();

        auto Existing = Inv->Inventory.ReplicatedEntries.Search([&](FFortItemEntry& e) {
            return e.ItemDefinition == Def && e.Count < MaxStack;
        }, FFortItemEntry::Size());

        bool bGiven = false;

        if (Existing && MaxStack > 1)
        {
            auto Total = Existing->Count + Pickup->PrimaryPickupItemEntry.Count;
            Existing->Count = Total > MaxStack ? MaxStack : Total;
            Inv->Update(Existing);
            bGiven = true;
        }
        else
        {
            Inv->GiveItem(Pickup->PrimaryPickupItemEntry);
            bGiven = Inv->Inventory.ReplicatedEntries.Num() > before;
        }

        Inv->SetRequiresUpdate();

        after = PC->WorldInventory->Inventory.ReplicatedEntries.Num();

        if (bGiven)
        {
            if (!Pickup->bPickedUp)
            {
                Pickup->bPickedUp = true;
                Pickup->OnRep_bPickedUp();
            }

            Pickup->K2_DestroyActor();
        }
        else
        {
            static int kept = 0;

            if (kept++ < 25)
                printf("[Boron][Pickup] ServerHandlePickup KEPT #%d pickup=%p def=%p inventory full, not destroying\n",
                       kept, (void*)Pickup, (void*)Def);
        }
    }

    static int n = 0;

    if (n++ < 25)
        printf("[Boron][Pickup] ServerHandlePickup #%d pickup=%p PC=%p def=%p count=%d entries %d -> %d\n",
               n, (void*)Pickup, (void*)PC,
               (void*)(Pickup ? Pickup->PrimaryPickupItemEntry.ItemDefinition : nullptr),
               Pickup ? Pickup->PrimaryPickupItemEntry.Count : -1, before, after);
}

void AFortPlayerPawnAthena::ServerHandlePickup_(UObject* Context, FFrame& Stack)
{
    AFortPickupAthena* Pickup;
    float InFlyTime;
    FVector InStartDirection;
    bool bPlayPickupSound;
    Stack.StepCompiledIn(&Pickup);
    Stack.StepCompiledIn(&InFlyTime);
    Stack.StepCompiledIn(&InStartDirection);
    Stack.StepCompiledIn(&bPlayPickupSound);
    Stack.IncrementCode();
    auto Pawn = (AFortPlayerPawnAthena*)Context;

    if (VersionInfo.EngineVersion >= 5.4)
    {
        static bool once = false;
        if (!once) { once = true; printf("[Boron][RpcProbe] ServerHandlePickup exec FIRED\n"); }
    }
    if (!Pawn || !Pickup || Pickup->bPickedUp)
        return;

    /*Pickup->SetLifeSpan(5.f);
    if (FFortPickupLocationData::HasbPlayPickupSound())
            Pickup->PickupLocationData.bPlayPickupSound = bPlayPickupSound;
    Pickup->PickupLocationData.PickupTarget = Pawn;
    Pickup->PickupLocationData.StartDirection = InStartDirection;
    Pickup->PickupLocationData.FlyTime /= Pawn->PickupSpeedMultiplier;
    Pickup->OnRep_PickupLocationData();

    Pickup->bPickedUp = true;
    Pickup->OnRep_bPickedUp();


    Pawn->IncomingPickups.Add(Pickup);*/
    auto SetPickupTarget = (void (*&)(AFortPickupAthena*, AFortPlayerPawnAthena*, float, FVector, bool))SetPickupTarget_;

    SetPickupTarget(Pickup, Pawn, InFlyTime / (Pawn->HasPickupSpeedMultiplier() ? Pawn->PickupSpeedMultiplier : 1), InStartDirection, bPlayPickupSound);
}

void AFortPlayerPawnAthena::ServerHandlePickupInfo(UObject* Context, FFrame& Stack)
{
    bool bTrySwapWithWeapon;
    bool bUseRequestedSwap;
    bool bPlayPickupSound;
    FGuid SwapWithItem;
    float FlyTime;
    FVector Direction;

    AFortPickupAthena* Pickup;
    Stack.StepCompiledIn(&Pickup);
    if (VersionInfo.FortniteVersion >= 20.00)
    {
        FFortPickupRequestInfoNew Params;
        Stack.StepCompiledIn(&Params);
        bTrySwapWithWeapon = Params.bTrySwapWithWeapon;
        bUseRequestedSwap = Params.bUseRequestedSwap;
        bPlayPickupSound = Params.bPlayPickupSound;
        SwapWithItem = Params.SwapWithItem;
        FlyTime = Params.FlyTime;
        Direction = *(FVector*)&Params.Direction;
    }
    else
    {
        FFortPickupRequestInfo Params;
        Stack.StepCompiledIn(&Params);
        bTrySwapWithWeapon = Params.bTrySwapWithWeapon;
        bUseRequestedSwap = Params.bUseRequestedSwap;
        bPlayPickupSound = Params.bPlayPickupSound;
        SwapWithItem = Params.SwapWithItem;
        FlyTime = Params.FlyTime;
        Direction = *(FVector*)&Params.Direction;
    }
    Stack.IncrementCode();
    auto Pawn = (AFortPlayerPawnAthena*)Context;

    if (!Pawn || !Pickup || Pickup->bPickedUp)
        return;

    // SetPickupTarget   stuff uhh  maybe
    if (VersionInfo.EngineVersion >= 5.4)
    {
        auto PC = Pawn->Controller ? (AFortPlayerControllerAthena*)Pawn->Controller : nullptr;

        // pickup shit
        static int pn = 0;
        if (pn++ < 25)
            printf("[Boron][Pickup] RPC #%d pickup=%p bPickedUp=%d def=%p count=%d PC=%p inv=%p\n",
                   pn, (void*)Pickup, (int)Pickup->bPickedUp,
                   (void*)Pickup->PrimaryPickupItemEntry.ItemDefinition, Pickup->PrimaryPickupItemEntry.Count,
                   (void*)PC, (void*)(PC ? PC->WorldInventory : nullptr));

        if (PC && PC->WorldInventory && Pickup->PrimaryPickupItemEntry.ItemDefinition)
        {
            auto Inv = PC->WorldInventory;
            auto& Entry = Pickup->PrimaryPickupItemEntry;
            auto Def = Entry.ItemDefinition;
            auto MaxStack = Def->GetMaxStackSize();
            int32 Remaining = Entry.Count > 0 ? Entry.Count : 1;

            if (MaxStack > 1)
                for (int i = 0; i < Inv->Inventory.ReplicatedEntries.Num() && Remaining > 0; i++)
                {
                    auto& Existing = Inv->Inventory.ReplicatedEntries.Get(i, FFortItemEntry::Size());

                    if (Existing.ItemDefinition != Def || Existing.Count >= MaxStack)
                        continue;

                    auto Space = MaxStack - Existing.Count;
                    auto Added = Remaining < Space ? Remaining : Space;

                    Existing.Count += Added;
                    Remaining -= Added;
                    Inv->Update(&Existing);
                }

            static int dumpN = 0;

            if (dumpN++ < 4)
            {
                printf("[Boron][Inv] ---- dump #%d entries=%d ----\n", dumpN, Inv->Inventory.ReplicatedEntries.Num());

                for (int i = 0; i < Inv->Inventory.ReplicatedEntries.Num(); i++)
                {
                    auto& E = Inv->Inventory.ReplicatedEntries.Get(i, FFortItemEntry::Size());

                    if (!E.ItemDefinition)
                    {
                        printf("[Boron][Inv]  [%02d] <NULL DEF> count=%d\n", i, E.Count);
                        continue;
                    }

                    printf("[Boron][Inv]  [%02d] %s type=%d prim=%d count=%d max=%d\n",
                           i, E.ItemDefinition->Name.ToString().c_str(), (int)E.ItemDefinition->ItemType,
                           (int)AFortInventory::IsPrimaryQuickbar(E.ItemDefinition), E.Count,
                           E.ItemDefinition->GetMaxStackSize());
                }

                printf("[Boron][Inv] enum harvest=%d resource=%d ammo=%d trap=%d build=%d edit=%d ingr=%d\n",
                       (int)EFortItemType::GetWeaponHarvest(), (int)EFortItemType::GetWorldResource(),
                       (int)EFortItemType::GetAmmo(), (int)EFortItemType::GetTrap(),
                       (int)EFortItemType::GetBuildingPiece(), (int)EFortItemType::GetEditTool(),
                       (int)EFortItemType::GetIngredient());
            }

            bool bBlocked = false;
            bool bSwapped = false;
            bool bPrimary = AFortInventory::IsPrimaryQuickbar(Def);
            int PrimaryCount = -1;

            if (Remaining > 0 && bPrimary)
            {
                PrimaryCount = 0;

                for (int i = 0; i < Inv->Inventory.ReplicatedEntries.Num(); i++)
                {
                    auto& Existing = Inv->Inventory.ReplicatedEntries.Get(i, FFortItemEntry::Size());

                    if (Existing.ItemDefinition && AFortInventory::IsPrimaryQuickbar(Existing.ItemDefinition))
                        PrimaryCount++;
                }

                if (PrimaryCount >= 5)
                {
                    FGuid DropGuid = SwapWithItem;

                    if (!bUseRequestedSwap || (!DropGuid.A && !DropGuid.B && !DropGuid.C && !DropGuid.D))
                    {
                        auto Cur = (AFortWeapon*)Pawn->CurrentWeapon;

                        if (Cur && Cur->HasItemEntryGuid())
                            DropGuid = Cur->ItemEntryGuid;
                    }

                    auto DropEntry = Inv->Inventory.ReplicatedEntries.Search([&](FFortItemEntry& e) { return e.ItemGuid == DropGuid; }, FFortItemEntry::Size());

                    if (!DropEntry || !DropEntry->ItemDefinition || !AFortInventory::IsPrimaryQuickbar(DropEntry->ItemDefinition))
                        for (int i = Inv->Inventory.ReplicatedEntries.Num() - 1; i >= 0; i--)
                        {
                            auto& Candidate = Inv->Inventory.ReplicatedEntries.Get(i, FFortItemEntry::Size());

                            if (Candidate.ItemDefinition && AFortInventory::IsPrimaryQuickbar(Candidate.ItemDefinition))
                            {
                                DropEntry = &Candidate;
                                DropGuid = Candidate.ItemGuid;
                                break;
                            }
                        }

                    if (DropEntry && DropEntry->ItemDefinition && AFortInventory::IsPrimaryQuickbar(DropEntry->ItemDefinition))
                    {
                        AFortInventory::SpawnPickup(Pawn->K2_GetActorLocation() + Pawn->GetActorForwardVector() * 70.f + FVector(0, 0, 50), *DropEntry,
                                                    EFortPickupSourceTypeFlag::GetPlayer(), EFortPickupSpawnSource::GetUnset(), Pawn);
                        Inv->Remove(DropGuid);
                        bSwapped = true;
                    }
                    else
                        bBlocked = true;
                }
            }

            static int gn = 0;
            if (gn++ < 25)
                printf("[Boron][Pickup] give #%d def=%s max=%d want=%d left=%d prim=%d primCount=%d cap=%d swapped=%d blocked=%d entries=%d\n",
                       gn, Def->Name.ToString().c_str(), MaxStack, Entry.Count, Remaining, (int)bPrimary, PrimaryCount,
                       5, (int)bSwapped, (int)bBlocked,
                       Inv->Inventory.ReplicatedEntries.Num());

            if (!bBlocked)
            {
                if (Remaining > 0)
                    Inv->GiveItem(Entry, Remaining);

                Pickup->bPickedUp = true;
                Pickup->OnRep_bPickedUp();
                Pickup->K2_DestroyActor();
            }

            Inv->SetRequiresUpdate();
        }
        return;
    }

    if (bUseRequestedSwap && Pawn->CurrentWeapon && AFortInventory::IsPrimaryQuickbar(((AFortWeapon*)Pawn->CurrentWeapon)->WeaponData) &&
        AFortInventory::IsPrimaryQuickbar(Pickup->PrimaryPickupItemEntry.ItemDefinition))
    {
        auto PlayerController = (AFortPlayerControllerAthena*)Pawn->Controller;
        /*auto SwapEntry = PlayerController->WorldInventory->Inventory.ReplicatedEntries.Search([&](FFortItemEntry& entry)
                { return entry.ItemGuid == SwapWithItem; }, FFortItemEntry::Size());
        PlayerController->SwappingItemDefinition = SwapEntry; // proper af*/
        PlayerController->bTryPickupSwap = true;
    }

    if (VersionInfo.FortniteVersion >= 16 && VersionInfo.FortniteVersion < 17)
    {
        Pickup->SetLifeSpan(5.f);
        if (Pickup->PickupLocationData.HasbPlayPickupSound())
            Pickup->PickupLocationData.bPlayPickupSound = bPlayPickupSound;
        Pickup->PickupLocationData.PickupTarget = Pawn;
        if (Pickup->PickupLocationData.HasItemOwner())
            Pickup->PickupLocationData.ItemOwner = Pawn;
        if (Pickup->PickupLocationData.HasPickupGuid())
            Pickup->PickupLocationData.PickupGuid = bUseRequestedSwap ? SwapWithItem : Pickup->PrimaryPickupItemEntry.ItemGuid;
        Pickup->PickupLocationData.FlyTime = 0.4f;
        if (Pickup->PickupLocationData.HasStartDirection())
            Pickup->PickupLocationData.StartDirection = Direction;
        Pickup->OnRep_PickupLocationData();

        Pickup->bPickedUp = true;
        Pickup->OnRep_bPickedUp();

        if (Pawn->HasIncomingPickups())
            Pawn->IncomingPickups.Add(Pickup);

        return;
    }

    auto SetPickupTarget = (void (*&)(AFortPickupAthena*, AFortPlayerPawnAthena*, float, FVector&, bool))SetPickupTarget_;

    SetPickupTarget(Pickup, Pawn, FlyTime / (Pawn->HasPickupSpeedMultiplier() ? Pawn->PickupSpeedMultiplier : 1), Direction, bPlayPickupSound);
}

void AFortPlayerPawnAthena::ServerHandlePickupWithRequestedSwap(UObject* Context, FFrame& Stack)
{
    AFortPickupAthena* Pickup;
    FGuid Swap;
    float InFlyTime;
    FVector InStartDirection;
    bool bPlayPickupSound;
    Stack.StepCompiledIn(&Pickup);
    Stack.StepCompiledIn(&Swap);
    Stack.StepCompiledIn(&InFlyTime);
    Stack.StepCompiledIn(&InStartDirection);
    Stack.StepCompiledIn(&bPlayPickupSound);
    Stack.IncrementCode();

    auto Pawn = (AFortPlayerPawnAthena*)Context;

    if (!Pawn || !Pickup || Pickup->bPickedUp)
        return;
    auto PlayerController = (AFortPlayerControllerAthena*)Pawn->Controller;

    PlayerController->bTryPickupSwap = true;

    auto SetPickupTarget = (void (*&)(AFortPickupAthena*, AFortPlayerPawnAthena*, float, FVector&, bool))SetPickupTarget_;

    SetPickupTarget(Pickup, Pawn, InFlyTime / (Pawn->HasPickupSpeedMultiplier() ? Pawn->PickupSpeedMultiplier : 1), InStartDirection, bPlayPickupSound);
    /*Pickup->SetLifeSpan(5.f);
    Pickup->PickupLocationData.bPlayPickupSound = bPlayPickupSound;
    Pickup->PickupLocationData.PickupGuid = Pickup->PrimaryPickupItemEntry.ItemGuid;
    Pickup->PickupLocationData.PickupTarget = Pawn;
    Pickup->PickupLocationData.FlyTime /= Pawn->PickupSpeedMultiplier;
    //Pickup->PickupLocationData.StartDirection = Params.Direction.QuantizeNormal();
    Pickup->OnRep_PickupLocationData();

    Pickup->bPickedUp = true;
    Pickup->OnRep_bPickedUp();


    Pawn->IncomingPickups.Add(Pickup);*/
}

bool AFortPlayerPawnAthena::FinishedTargetSpline(void* _Pickup)
{
    auto Pickup = (AFortPickupAthena*)_Pickup;

    auto Pawn = (AFortPlayerPawnAthena*)Pickup->PickupLocationData.PickupTarget;
    if (!Pawn)
        return FinishedTargetSplineOG(Pickup);

    auto PlayerController = (AFortPlayerControllerAthena*)Pawn->Controller;
    if (!PlayerController)
        return FinishedTargetSplineOG(Pickup);

    // if (auto entry = PlayerController->HasSwappingItemDefinition() ? (FFortItemEntry*)PlayerController->SwappingItemDefinition : nullptr)
    if (PlayerController->HasbTryPickupSwap() ? PlayerController->bTryPickupSwap : false)
    {
        FVector FinalLoc = Pawn->K2_GetActorLocation();

        FVector ForwardVector = Pawn->GetActorForwardVector();
        ForwardVector.Z = 0.0f;
        ForwardVector.Normalize();

        FinalLoc = FinalLoc + ForwardVector * 450.f;
        FinalLoc.Z += 50.f;

        const float RandomAngleVariation = ((float)rand() * 0.00109866634f) - 18.f;
        const float FinalAngle = RandomAngleVariation * 0.017453292519943295f;

        FinalLoc.X += cos(FinalAngle) * 100.f;
        FinalLoc.Y += sin(FinalAngle) * 100.f;

        if (AFortInventory::IsPrimaryQuickbar(((AFortWeapon*)Pawn->CurrentWeapon)->WeaponData) && AFortInventory::IsPrimaryQuickbar(Pickup->PrimaryPickupItemEntry.ItemDefinition))
        {
            PlayerController->bTryPickupSwap = false;

            auto entry = PlayerController->WorldInventory->Inventory.ReplicatedEntries.Search([&](FFortItemEntry& entry)
            { return entry.ItemGuid == ((AFortWeapon*)PlayerController->Pawn->CurrentWeapon)->ItemEntryGuid; }, FFortItemEntry::Size());

            AFortInventory::SpawnPickup(PlayerController->GetViewTarget()->K2_GetActorLocation(), *entry, EFortPickupSourceTypeFlag::GetPlayer(), EFortPickupSpawnSource::GetUnset(), PlayerController->MyFortPawn, -1,
                                        true, true, true, nullptr, FinalLoc);
            // SwapEntry(PC, *entry, Pickup->PrimaryPickupItemEntry);
            PlayerController->WorldInventory->Remove(entry->ItemGuid);
            auto Item = PlayerController->WorldInventory->GiveItem(Pickup->PrimaryPickupItemEntry);
            PlayerController->ServerExecuteInventoryItem(Item->ItemEntry.ItemGuid);
            /*if (VersionInfo.FortniteVersion < 3)
            {
                    auto& QuickBar = (AFortInventory::IsPrimaryQuickbar(Item->ItemEntry.ItemDefinition) || Item->ItemEntry.ItemDefinition->ItemType ==
            EFortItemType::GetWeaponHarvest()) ? PlayerController->QuickBars->PrimaryQuickBar : PlayerController->QuickBars->SecondaryQuickBar; int i = 0; for (i = 0; i <
            QuickBar.Slots.Num(); i++)
                    {
                            auto& Slot = QuickBar.Slots.Get(i, FQuickBarSlot::Size());

                            for (auto& SlotItem : Slot.Items)
                                    if (SlotItem == Item->ItemEntry.ItemGuid)
                                    {
                                            PlayerController->QuickBars->ServerActivateSlotInternal(!(AFortInventory::IsPrimaryQuickbar(Item->ItemEntry.ItemDefinition) ||
            Item->ItemEntry.ItemDefinition->ItemType == EFortItemType::GetWeaponHarvest()), i, 0.f, true); break;
                                    }
                    }
            }
            else
                    PlayerController->ClientEquipItem(Item->ItemEntry.ItemGuid, true);*/
        }
        else
            AFortInventory::SpawnPickup(PlayerController->GetViewTarget()->K2_GetActorLocation(), Pickup->PrimaryPickupItemEntry, EFortPickupSourceTypeFlag::GetPlayer(), EFortPickupSpawnSource::GetUnset(),
                                        PlayerController->MyFortPawn, -1, true, true, true, nullptr, FinalLoc);
    }
    else
        PlayerController->InternalPickup(&Pickup->PrimaryPickupItemEntry);

    return FinishedTargetSplineOG(Pickup);
}

uint64_t OnRep_ZiplineState = 0;
void AFortPlayerPawnAthena::ServerSendZiplineState(UObject* Context, FFrame& Stack)
{
    FZiplinePawnState State;

    Stack.StepCompiledIn(&State);
    Stack.IncrementCode();

    auto Pawn = (AFortPlayerPawnAthena*)Context;

    if (!Pawn)
        return;

    auto Zipline = Pawn->GetActiveZipline();

    auto PreviousState = Pawn->ZiplineState;

    memcpy((PBYTE)&Pawn->ZiplineState, (const PBYTE)&State, FZiplinePawnState::Size());

    if (OnRep_ZiplineState)
        ((void (*)(AFortPlayerPawnAthena*))OnRep_ZiplineState)(Pawn);

    if (State.bJumped)
    {
        auto Velocity = Pawn->CharacterMovement->Velocity;
        auto VelocityX = Velocity.X * -0.5f;
        auto VelocityY = Velocity.Y * -0.5f;
        Pawn->LaunchCharacterJump(FVector{ VelocityX >= -750 ? min(VelocityX, 750) : -750, VelocityY >= -750 ? min(VelocityY, 750) : -750, 1200 }, false, false, true, true);
    }

    auto NewZipline = Pawn->GetActiveZipline();

    static auto ZipLineClass = FindObject<UClass>(L"/Ascender/Gameplay/Ascender/B_Athena_Zipline_Ascender.B_Athena_Zipline_Ascender_C");
    if (auto Ascender = Zipline->Cast<AFortAscenderZipline>(ZipLineClass))
    {
        Ascender->PawnUsingHandle = nullptr;
        Ascender->PreviousPawnUsingHandle = Pawn;
        Ascender->OnRep_PawnUsingHandle();
    }
    else if (auto Ascender = NewZipline->Cast<AFortAscenderZipline>(ZipLineClass))
    {
        Ascender->PawnUsingHandle = Pawn;
        Ascender->PreviousPawnUsingHandle = nullptr;
        Ascender->OnRep_PawnUsingHandle();
    }
}

void AFortPlayerPawnAthena::OnCapsuleBeginOverlap_(UObject* Context, FFrame& Stack)
{
    UObject* OverlappedComp;
    AActor* OtherActor;
    UObject* OtherComp;
    int32 OtherBodyIndex;
    bool bFromSweep;
    struct
    {
        uint8_t Padding[0x100];
    } SweepResult;
    Stack.StepCompiledIn(&OverlappedComp);
    Stack.StepCompiledIn(&OtherActor);
    Stack.StepCompiledIn(&OtherComp);
    Stack.StepCompiledIn(&OtherBodyIndex);
    Stack.StepCompiledIn(&bFromSweep);
    Stack.StepCompiledIn(&SweepResult);
    Stack.IncrementCode();

    auto Pawn = (AFortPlayerPawnAthena*)Context;

    if (VersionInfo.EngineVersion >= 5.4)
    {
        static auto ProbePickupCls = FindClass("FortPickupAthena");
        static int ovl = 0;

        if (ProbePickupCls && OtherActor && OtherActor->IsA(ProbePickupCls) && ovl++ < 20)
        {
            auto P = (AFortPickupAthena*)OtherActor;
            bool hasPickedUp = P->HasbPickedUp();
            uint8_t flagsByte = hasPickedUp ? *(uint8_t*)(__int64(P) + AFortPickupAthena::bPickedUp__Offset) : 0xFF;

            struct
            {
                const AFortPlayerPawnAthena* FortPawn;
                bool bOverride;
                bool Ret;
                char pad[6];
            } ci{ Pawn, false, false, {} };
            static auto CanInteractFn = P->GetFunction("BlueprintCanInteract");
            if (CanInteractFn)
                P->ProcessEvent(CanInteractFn, &ci);

            struct
            {
                char Text[0x10];
                bool Ret;
                char pad[7];
            } it{};
            static auto GetTextFn = P->GetFunction("GetInteractText");
            if (GetTextFn)
                P->ProcessEvent(GetTextFn, &it);

            struct
            {
                char Text[0x10];
                uint32_t Tag;
                bool Ret;
                char pad[3];
            } et{};
            static auto ErrTextFn = P->GetFunction("GetInteractErrorText");
            if (ErrTextFn)
                P->ProcessEvent(ErrTextFn, &et);

            FName etn{};
            etn.ComparisonIndex = et.Tag;
            printf("[Boron][Pickup] canInteract=%d ovr=%d hasText=%d hasErr=%d errTag=%s\n",
                   (int)ci.Ret, (int)ci.bOverride, (int)it.Ret, (int)et.Ret,
                   et.Tag ? etn.ToString().c_str() : "none");
            printf("[Boron][Pickup] overlap #%d pickup=%p pawn=%p def=%p flagsByte=0x%02X stoppedSim=%d(has=%d) suppressWidget=%d useWidget=%d blockedAuto=%d dummyItem=%p moveComp=%p capsule=%p aimRadius=%.1f\n",
                   ovl, (void*)OtherActor, (void*)Pawn,
                   (void*)P->PrimaryPickupItemEntry.ItemDefinition,
                   flagsByte,
                   (int)P->bServerStoppedSimulation, (int)P->HasbServerStoppedSimulation(),
                   (int)P->bSuppressInteractionWidget, (int)P->bUsePickupWidget, (int)P->bBlockedFromAutoPickup,
                   P->HasPrimaryPickupDummyItem() ? (void*)P->PrimaryPickupDummyItem : (void*)(__int64)-1,
                   P->HasMovementComponent() ? (void*)P->MovementComponent : (void*)(__int64)-1,
                   P->HasTouchCapsule() ? (void*)P->TouchCapsule : (void*)(__int64)-1,
                   P->HasOverrideInteractAimRadius() ? P->OverrideInteractAimRadius : -1.f);
        }
    }

    static auto FortPCClass = FindClass("FortPlayerController");

    if (!Pawn || !Pawn->Controller || !Pawn->Controller->IsA(FortPCClass))
        return callOG(Pawn, Stack.GetCurrentNativeFunction(), OnCapsuleBeginOverlap, OverlappedComp, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);

    auto Pickup = OtherActor->Cast<AFortPickupAthena>();
    if (!Pickup || !Pickup->PrimaryPickupItemEntry.ItemDefinition || !((AFortPlayerControllerAthena*)Pawn->Controller)->WorldInventory)
        return callOG(Pawn, Stack.GetCurrentNativeFunction(), OnCapsuleBeginOverlap, OverlappedComp, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);

    auto MaxStack = Pickup->PrimaryPickupItemEntry.ItemDefinition->GetMaxStackSize();
    auto itemEntry = ((AFortPlayerControllerAthena*)Pawn->Controller)->WorldInventory->Inventory.ReplicatedEntries.Search([&](FFortItemEntry& entry) {
        return entry.ItemDefinition == Pickup->PrimaryPickupItemEntry.ItemDefinition && entry.Count <= MaxStack;
    }, FFortItemEntry::Size());

    if (GameRuleConfig::bCH5AutoPickupWeapons && VersionInfo.EngineVersion >= 5.4 && Pickup && Pickup->PawnWhoDroppedPickup != Pawn &&
        AFortInventory::IsPrimaryQuickbar(Pickup->PrimaryPickupItemEntry.ItemDefinition))
    {
        auto Inv = ((AFortPlayerControllerAthena*)Pawn->Controller)->WorldInventory;
        int primaryCount = 0;

        for (int i = 0; i < Inv->Inventory.ReplicatedEntries.Num(); i++)
        {
            auto& Item = Inv->Inventory.ReplicatedEntries.Get(i, FFortItemEntry::Size());

            if (Item.ItemDefinition && AFortInventory::IsPrimaryQuickbar(Item.ItemDefinition))
                primaryCount++;
        }

        static int wp = 0;
        if (wp++ < 15)
            printf("[Boron][Pickup] CH5 weapon overlap primaryCount=%d pickup=%p\n", primaryCount, (void*)Pickup);

        if (primaryCount < 5)
        {
            Pawn->ServerHandlePickup(Pickup, Pickup->PickupLocationData.FlyTime, FVector(), true);
            return callOG(Pawn, Stack.GetCurrentNativeFunction(), OnCapsuleBeginOverlap, OverlappedComp, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
        }
    }

    if (Pickup && Pickup->PawnWhoDroppedPickup != Pawn)
    {
        if ((!itemEntry && ((Pickup->PrimaryPickupItemEntry.ItemDefinition->HasbForceAutoPickup() &&
                             (Pickup->PrimaryPickupItemEntry.ItemDefinition->HasbForceAutoPickup()
                                  ? Pickup->PrimaryPickupItemEntry.ItemDefinition->bForceAutoPickup
                                  : (Pickup->PrimaryPickupItemEntry.ItemDefinition->GetPickupComponent() ? Pickup->PrimaryPickupItemEntry.ItemDefinition->GetPickupComponent()->bForceAutoPickup : false))) ||
                            !AFortInventory::IsPrimaryQuickbar(Pickup->PrimaryPickupItemEntry.ItemDefinition))) ||
            (itemEntry && itemEntry->Count < MaxStack))
            Pawn->ServerHandlePickup(Pickup, Pickup->PickupLocationData.FlyTime, FVector(), true);
    }

    return callOG(Pawn, Stack.GetCurrentNativeFunction(), OnCapsuleBeginOverlap, OverlappedComp, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
}

void AFortPlayerPawnAthena::MovingEmoteStopped(UObject* Context, FFrame& Stack)
{
    Stack.IncrementCode();
    auto Pawn = (AFortPlayerPawnAthena*)Context;

    if (Pawn->HasbIsPlayingEmote() && Pawn->bIsPlayingEmote)
        return;

    static auto HasbMovingEmote = Pawn->HasbMovingEmote();
    if (HasbMovingEmote)
        Pawn->bMovingEmote = false;

    static auto HasbMovingEmoteForwardOnly = Pawn->HasbMovingEmoteForwardOnly();
    if (HasbMovingEmoteForwardOnly)
        Pawn->bMovingEmoteForwardOnly = false;

    static auto HasbMovingEmoteFollowingOnly = Pawn->HasbMovingEmoteFollowingOnly();
    if (HasbMovingEmoteFollowingOnly)
        Pawn->bMovingEmoteFollowingOnly = false;

    if (Pawn->HasLastReplicatedEmoteExecuted())
    {
        auto OldEmote = Pawn->LastReplicatedEmoteExecuted;
        Pawn->LastReplicatedEmoteExecuted = nullptr;
        Pawn->OnRep_LastReplicatedEmoteExecuted(OldEmote);
    }
}

class UGA_Athena_MedConsumable_Parent_C : public UObject
{
public:
    UCLASS_COMMON_MEMBERS(UGA_Athena_MedConsumable_Parent_C);

    DEFINE_PROP(PlayerPawn, AFortPlayerPawnAthena*);
    DEFINE_PROP(HealsShields, bool);
    DEFINE_PROP(HealsHealth, bool);
    DEFINE_PROP(HealthHealAmount, float);
};

void AFortPlayerPawnAthena::Athena_MedConsumable_Triggered(UObject* Context, FFrame& Stack)
{
    UGA_Athena_MedConsumable_Parent_C* Consumable = (UGA_Athena_MedConsumable_Parent_C*)Context;

    printf("Called yo\n");
    if (!Consumable || (!Consumable->HealsShields && !Consumable->HealsHealth) || !Consumable->PlayerPawn)
        return Athena_MedConsumable_TriggeredOG(Context, Stack);

    auto PlayerState = (AFortPlayerStateAthena*)Consumable->PlayerPawn->PlayerState;
    static auto ShieldCue = FName(L"GameplayCue.Shield.PotionConsumed");
    static auto HealthCue = FName(L"GameplayCue.Athena.Health.HealUsed");

    auto Handle = PlayerState->AbilitySystemComponent->MakeEffectContext();
    FGameplayTag Tag{};
    FName CueName = Consumable->HealsShields ? ShieldCue : HealthCue;

    if (Consumable->HealsHealth && Consumable->HealsShields)
    {
        static auto HealthHealAmountOffset = Consumable->GetOffset("HealthHealAmount");
        auto HealthHealAmount = Consumable->HasHealthHealAmount() ? *(float*)(__int64(Consumable) + HealthHealAmountOffset) : *(double*)(__int64(Consumable) + HealthHealAmountOffset);
        if (Consumable->PlayerPawn->GetHealth() + HealthHealAmount <= 100)
            CueName = HealthCue;
    }
    Tag.TagName = CueName;

    auto PredictionKey = (FPredictionKey*)malloc(FPredictionKey::Size());
    memset((PBYTE)PredictionKey, 0, FPredictionKey::Size());

    PlayerState->AbilitySystemComponent->NetMulticast_InvokeGameplayCueAdded(Tag, *PredictionKey, Handle);
    PlayerState->AbilitySystemComponent->NetMulticast_InvokeGameplayCueExecuted(Tag, *PredictionKey, Handle);

    free(PredictionKey);

    return Athena_MedConsumable_TriggeredOG(Context, Stack);
}

void AFortPlayerPawnAthena::ServerOnExitVehicle_(UObject* Context, FFrame& Stack, AActor** Ret)
{
    struct FVehicleExitData
    {
        uint8_t Pad[0x30];
    };

    FVehicleExitData VehicleExitData;
    uint8_t ExitForceBehavior;
    bool bDestroyVehicleWhenForced;
    if (VersionInfo.FortniteVersion >= 29)
        Stack.StepCompiledIn(&VehicleExitData);
    else
    {
        Stack.StepCompiledIn(&ExitForceBehavior);
        Stack.StepCompiledIn(&bDestroyVehicleWhenForced);
    }

    Stack.IncrementCode();

    auto Pawn = (AFortPlayerPawnAthena*)Context;

    static auto GetVehicleFunc = Pawn->GetFunction("GetVehicleActor");
    if (!GetVehicleFunc)
        GetVehicleFunc = Pawn->GetFunction("GetVehicle");
    auto Vehicle = Pawn->Call<AActor*>(GetVehicleFunc);

    if (!Vehicle && Pawn->IsA<AFortCharacterVehicle>())
        Vehicle = Pawn;

    if (!Vehicle)
    {
        if (VersionInfo.FortniteVersion >= 29)
            return callOG(Pawn, Stack.GetCurrentNativeFunction(), ServerOnExitVehicle, VehicleExitData);
        else
            return callOG(Pawn, Stack.GetCurrentNativeFunction(), ServerOnExitVehicle, ExitForceBehavior, bDestroyVehicleWhenForced);
    }

    UFortVehicleSeatWeaponComponent* SeatWeaponComponent = (UFortVehicleSeatWeaponComponent*)Vehicle->GetComponentByClass(UFortVehicleSeatWeaponComponent::StaticClass());

    if (!SeatWeaponComponent)
    {
        printf("nop %s\n", Pawn ? Pawn->Class->Name.ToString().c_str() : nullptr);
        if (VersionInfo.FortniteVersion >= 29)
            return callOG(Pawn, Stack.GetCurrentNativeFunction(), ServerOnExitVehicle, VehicleExitData);
        else
            return callOG(Pawn, Stack.GetCurrentNativeFunction(), ServerOnExitVehicle, ExitForceBehavior, bDestroyVehicleWhenForced);
    }

    UFortVehicleSeatComponent* SeatComponent = (UFortVehicleSeatComponent*)Vehicle->GetComponentByClass(UFortVehicleSeatComponent::StaticClass());

    auto PlayerController = (AFortPlayerControllerAthena*)Pawn->Controller;

    auto SeatIdx = SeatComponent->FindSeatIndex(Pawn);

    UFortWeaponItemDefinition* Weapon = nullptr;
    if (SeatWeaponComponent)
    {
        for (int i = 0; i < SeatWeaponComponent->WeaponSeatDefinitions.Num(); i++)
        {
            auto& WeaponDefinition = SeatWeaponComponent->WeaponSeatDefinitions.Get(i, FWeaponSeatDefinition::Size());

            if (WeaponDefinition.SeatIndex != SeatIdx)
                continue;

            Weapon = WeaponDefinition.VehicleWeapon;
            break;
        }

        // printf("Weapon: %s\n", Weapon ? Weapon->Name.ToString().c_str() : "<null>");
        if (Weapon)
        {
            auto ItemEntry = PlayerController->WorldInventory->Inventory.ReplicatedEntries.Search([Weapon](FFortItemEntry& entry) { return entry.ItemDefinition == Weapon; }, FFortItemEntry::Size());

            if (ItemEntry)
                PlayerController->WorldInventory->Remove(ItemEntry->ItemGuid);
        }
    }
    if (VersionInfo.FortniteVersion >= 29)
        *Ret = callOGWithRet(Pawn, Stack.GetCurrentNativeFunction(), ServerOnExitVehicle, VehicleExitData);
    else
        *Ret = callOGWithRet(Pawn, Stack.GetCurrentNativeFunction(), ServerOnExitVehicle, ExitForceBehavior, bDestroyVehicleWhenForced);

    if (Weapon)
    {
        auto LastItem = Pawn->HasPreviousWeapon() ? (AFortWeapon*)Pawn->PreviousWeapon : nullptr;

        if (LastItem)
        {
            PlayerController->ServerExecuteInventoryItem(LastItem->ItemEntryGuid);
            PlayerController->ClientEquipItem(LastItem->ItemEntryGuid, true);
        }
        else
        {
            printf("yo\n");
            auto pickaxeEntry =
                PlayerController->WorldInventory->Inventory.ReplicatedEntries.Search([](FFortItemEntry& entry) { return entry.ItemDefinition->IsA<UFortWeaponMeleeItemDefinition>(); }, FFortItemEntry::Size());

            if (pickaxeEntry)
            {
                printf("yo2\n");
                PlayerController->ServerExecuteInventoryItem(pickaxeEntry->ItemGuid);
                PlayerController->ClientEquipItem(pickaxeEntry->ItemGuid, true);
            }
        }
    }
}

void AFortPlayerPawnAthena::EmoteStopped_(UObject* Context, FFrame& Stack)
{
    UObject* MontageItemDef;

    Stack.StepCompiledIn(&MontageItemDef);
    Stack.IncrementCode();
    auto Pawn = (AFortPlayerPawnAthena*)Context;

    if (Pawn->HasLastReplicatedEmoteExecuted() && Pawn->LastReplicatedEmoteExecuted == MontageItemDef)
    {
        auto OldEmote = Pawn->LastReplicatedEmoteExecuted;
        Pawn->LastReplicatedEmoteExecuted = nullptr;
        Pawn->OnRep_LastReplicatedEmoteExecuted(OldEmote);
    }

    return callOG(Pawn, Stack.GetCurrentNativeFunction(), EmoteStopped, MontageItemDef);
}

void AFortPlayerPawnAthena::EndSkydiving(AFortPlayerPawnAthena* Pawn)
{
    EndSkydivingOG(Pawn);

    auto PlayerController = (AFortPlayerControllerAthena*)Pawn->Controller;

    if (PlayerController && Pawn->bIsSkydiving)
        PlayerController->GetQuestManager(1)->SendStatEvent(PlayerController, EFortQuestObjectiveStatEvent::GetLand(), 1, Pawn);

    if (Pawn->bIsSkydivingFromBus)
    {
        PlayerController->GetQuestManager(1)->SendStatEvent(PlayerController, EFortQuestObjectiveStatEvent::GetVisit(), 1, Pawn);
    }
}

void AFortPlayerPawnAthena::ServerReviveFromDBNO(UObject* Context, FFrame& Stack)
{
    AFortPlayerControllerAthena* EventInstigator;

    Stack.StepCompiledIn(&EventInstigator);
    Stack.IncrementCode();
    auto Pawn = (AFortPlayerPawnAthena*)Context;
}

void AFortPlayerPawnAthena::ServerThrowCarriedPlayer_(UObject* Context, FFrame& Stack)
{
    Stack.IncrementCode();
    auto Pawn = (AFortPlayerPawnAthena*)Context;

    callOG(Pawn, Stack.GetCurrentNativeFunction(), ServerThrowCarriedPlayer);
    Pawn->LocalThrowCarriedPlayer();
}

void AFortPlayerPawnAthena::SetIsInsideSafeZone(AFortPlayerPawnAthena* _this, bool bNewValue)
{
    _this->bIsInsideSafeZone = bNewValue;
    _this->OnRep_IsInsideSafeZone();
}

void AFortPlayerPawnAthena::UpdateOutsideSafeZone(AFortPlayerPawnAthena* _this)
{
    _this->bIsInsideSafeZone = !_this->bIsInAnyStorm;
    _this->OnRep_IsInsideSafeZone();
}

void UClamberingComponent::ServerStartClambering(UObject* Context, FFrame& Stack)
{
    static int n = 0;
    auto Comp = (UClamberingComponent*)Context;

    if (n++ < 12)
        printf("[Boron][Clamber] ServerStartClambering comp=%p localState=%d repState=%d\n", (void*)Comp,
               Comp->HasLocalClamberingState() ? (int)Comp->LocalClamberingState : -1,
               Comp->HasReplicatedClamberingState() ? (int)Comp->ReplicatedClamberingState : -1);

    return ServerStartClamberingOG(Context, Stack);
}

void UClamberingComponent::NetMulticast_ClamberingLedgeFailed(UObject* Context, FFrame& Stack)
{
    static const char* Reasons[] = { "None",
                                     "Unknown",
                                     "DebugForced",
                                     "OwnerDied",
                                     "OwnerDBNO",
                                     "OwnerLaunched",
                                     "SynchedActionNotStarted",
                                     "OwnerTeleported",
                                     "Ledge_PlayerTooFar",
                                     "Ledge_TargetLocationInvalid",
                                     "Ledge_TargetActorInvalid",
                                     "Ledge_TargetActorDestroyed",
                                     "Ledge_BlockerEncountered" };

    uint8 Reason = Stack.Locals ? *(uint8*)(__int64(Stack.Locals) + 0x0) : 0xFF;
    uint8 State = Stack.Locals ? *(uint8*)(__int64(Stack.Locals) + 0x1) : 0xFF;

    static int n = 0;

    if (n++ < 24)
        printf("[Boron][Clamber] LEDGE FAILED reason=%d (%s) state=%d\n", (int)Reason, Reason < 13 ? Reasons[Reason] : "?", (int)State);

    return NetMulticast_ClamberingLedgeFailedOG(Context, Stack);
}

void UClamberingComponent::Configure(AActor* Pawn)
{
    if (VersionInfo.EngineVersion < 5.4 || !Pawn)
        return;

    static int n = 0;
    bool bLog = n++ < 4;

    auto Cls = StaticClass();
    auto Comp = Cls ? (UClamberingComponent*)Pawn->GetComponentByClass((UClass*)Cls) : nullptr;

    if (!Comp)
    {
        if (bLog)
            printf("[Boron][Clamber] component missing on pawn=%p (cls=%p)\n", (void*)Pawn, (void*)Cls);
        return;
    }

    float Enabled = Comp->HasClamberingEnabled() ? Comp->ClamberingEnabled.Evaluate() : -1.f;
    float MaxDist = Comp->HasServerValidatePlayerMaxDistance() ? Comp->ServerValidatePlayerMaxDistance.Evaluate() : -1.f;
    float FailDelay = Comp->HasServerFailDelay() ? Comp->ServerFailDelay.Evaluate() : -1.f;
    float SyncDelay = Comp->HasSynchedActionFailDelay() ? Comp->SynchedActionFailDelay.Evaluate() : -1.f;

    if (Comp->HasServerValidatePlayerMaxDistance() && MaxDist < 500.f)
    {
        Comp->ServerValidatePlayerMaxDistance.Curve.CurveTable = nullptr;
        Comp->ServerValidatePlayerMaxDistance.Value = 5000.f;
    }

    if (bLog)
        printf("[Boron][Clamber] pawn=%p comp=%p enabled=%.2f indicator=%.2f maxDist=%.0f failDelay=%.2f syncDelay=%.2f mme=%p isEnabled=%d autoClamber=%d\n",
               (void*)Pawn, (void*)Comp, Enabled, Comp->HasClamberIndicatorEnabled() ? Comp->ClamberIndicatorEnabled.Evaluate() : -1.f, MaxDist, FailDelay,
               SyncDelay, Comp->HasMovementModeExtension() ? (void*)Comp->MovementModeExtension : nullptr,
               Comp->GetFunction("IsClamberingEnabled") ? (int)Comp->IsClamberingEnabled() : -1,
               Comp->GetFunction("IsAutoClamberingEnabled") ? (int)Comp->IsAutoClamberingEnabled() : -1);
}

void UClamberingComponent::PostLoadHook()
{
    if (VersionInfo.EngineVersion < 5.4)
        return;

    auto Default = GetDefaultObj();

    if (!Default)
    {
        printf("[Boron][Clamber] ClamberingComponent class not present on this build\n");
        return;
    }

    auto StartFn = Default->GetFunction("ServerStartClambering");
    auto FailFn = Default->GetFunction("NetMulticast_ClamberingLedgeFailed");

    printf("[Boron][Clamber] hooks: start=%p fail=%p\n", (void*)StartFn, (void*)FailFn);

    if (StartFn)
        Hooking::ExecHook(StartFn, ServerStartClambering, ServerStartClamberingOG);

    if (FailFn)
        Hooking::ExecHook(FailFn, NetMulticast_ClamberingLedgeFailed, NetMulticast_ClamberingLedgeFailedOG);
}

void AFortPlayerPawnAthena::PostLoadHook()
{
    OnRep_ZiplineState = FindOnRep_ZiplineState();
    SetPickupTarget_ = FindSetPickupTarget();

    auto ServerHandlePickupInfoFn = GetDefaultObj()->GetFunction("ServerHandlePickupInfo");

    if (VersionInfo.EngineVersion >= 5.4)
        printf("[Boron][Pickup] hook install: PickupInfo=%p Pickup=%p WithSwap=%p Overlap=%p\n",
               (void*)ServerHandlePickupInfoFn,
               (void*)GetDefaultObj()->GetFunction("ServerHandlePickup"),
               (void*)GetDefaultObj()->GetFunction("ServerHandlePickupWithRequestedSwap"),
               (void*)GetDefaultObj()->GetFunction("OnCapsuleBeginOverlap"));

    if (ServerHandlePickupInfoFn)
        Hooking::ExecHook(ServerHandlePickupInfoFn, ServerHandlePickupInfo);
    else
    {
        Hooking::ExecHook(GetDefaultObj()->GetFunction("ServerHandlePickup"), ServerHandlePickup_);
        Hooking::ExecHook(GetDefaultObj()->GetFunction("ServerHandlePickupWithRequestedSwap"), ServerHandlePickupWithRequestedSwap);
    }

    if (VersionInfo.EngineVersion >= 5.4 && ServerHandlePickupInfoFn)
        Hooking::ExecHook(GetDefaultObj()->GetFunction("ServerHandlePickup"), ServerHandlePickupProbe, ServerHandlePickupProbeOG);

    AFortWeaponRanged::Hook();

    Hooking::Hook(FindFinishedTargetSpline(), FinishedTargetSpline, FinishedTargetSplineOG);
    Hooking::ExecHook(GetDefaultObj()->GetFunction("OnCapsuleBeginOverlap"), OnCapsuleBeginOverlap_, OnCapsuleBeginOverlap_OG);

    Hooking::ExecHook(GetDefaultObj()->GetFunction("ServerSendZiplineState"), ServerSendZiplineState);
    Hooking::ExecHook(GetDefaultObj()->GetFunction("MovingEmoteStopped"), MovingEmoteStopped);

    Hooking::ExecHook(GetDefaultObj()->GetFunction("ServerOnExitVehicle"), ServerOnExitVehicle_, ServerOnExitVehicle_OG);

    Hooking::ExecHook(GetDefaultObj()->GetFunction("EmoteStopped"), EmoteStopped_, EmoteStopped_OG);

    auto EndSkydivingFn = GetDefaultObj()->GetFunction("EndSkydiving");

    if (EndSkydivingFn)
        Hooking::Hook<AFortPlayerPawnAthena>(EndSkydivingFn->GetVTableIndex(), EndSkydiving, EndSkydivingOG);

    Hooking::ExecHook(GetDefaultObj()->GetFunction("ServerReviveFromDBNO"), ServerReviveFromDBNO);
    Hooking::ExecHook(GetDefaultObj()->GetFunction("ServerThrowCarriedPlayer"), ServerThrowCarriedPlayer_, ServerThrowCarriedPlayer_OG);

    // zone fix for s18+
    if (VersionInfo.FortniteVersion >= 18)
    {
        auto SetIsInsideSafeZoneBase = Memcury::Scanner::FindPattern("74 ? 33 D2 48 8B ? E8 ? ? ? ? 48 8B ? B2 01 48 8B ? FF 90"); // mov dl, 1 variant

        if (!SetIsInsideSafeZoneBase.IsValid())
            SetIsInsideSafeZoneBase = Memcury::Scanner::FindPattern("74 ? 33 D2 48 8B ? E8 ? ? ? ? 48 8B ? 41 8A ? 48 8B ? FF 90"); // mov dl, rXXb variant

        if (!SetIsInsideSafeZoneBase.IsValid())
            SetIsInsideSafeZoneBase = Memcury::Scanner::FindPattern("0F 84 ? ? ? ? 33 D2 48 8B ? E8 ? ? ? ? 48 8B ? B2 01 48 8B ? FF 90"); // imm32 jz variant: first two should find it, but just incase.

        if (SetIsInsideSafeZoneBase.IsValid())
        {
            auto SetIsInsideSafeZoneVftPtr = SetIsInsideSafeZoneBase.ScanFor({ 0xFF, 0x90 }).AbsoluteOffset(2).Get();

            Hooking::Hook<AFortPlayerPawnAthena>(*(uint32_t*)SetIsInsideSafeZoneVftPtr / 8, SetIsInsideSafeZone);
            Hooking::Hook<AFortPlayerPawnAthena>(*(uint32_t*)SetIsInsideSafeZoneVftPtr / 8 + 1, UpdateOutsideSafeZone);
        }
    }
}

static AFortPlayerPawnAthena* ResolveShooter(AFortWeaponRanged* Weapon)
{
    if (!Weapon)
        return nullptr;

    auto Shooter = (AFortPlayerPawnAthena*)(Weapon->HasOwner() ? Weapon->Owner : nullptr);

    if (!Shooter && Weapon->HasInstigator())
        Shooter = (AFortPlayerPawnAthena*)Weapon->Instigator;

    return Shooter;
}

static void SendDamageCue(AFortWeaponRanged* Weapon, AActor* HitActor, FHitResult& Hit, float Magnitude, bool bFatal, bool bCritical,
                          bool bShield, bool bShieldDestroyed, bool bNonPlayer)
{
    static bool bChecked = false;
    static bool bAvailable = false;

    auto Shooter = ResolveShooter(Weapon);

    if (!bChecked)
    {
        bChecked = true;
        bAvailable = FAthenaBatchedDamageGameplayCues_Shared::StaticStruct() && FAthenaBatchedDamageGameplayCues_NonShared::StaticStruct()
                     && Shooter && Shooter->GetFunction("NetMulticast_Athena_BatchedDamageCues");

        printf("[Boron][Cue] shared=%p nonShared=%p fn=%p sharedSize=%d nonSharedSize=%d available=%d\n",
               (void*)FAthenaBatchedDamageGameplayCues_Shared::StaticStruct(), (void*)FAthenaBatchedDamageGameplayCues_NonShared::StaticStruct(),
               (void*)(Shooter ? Shooter->GetFunction("NetMulticast_Athena_BatchedDamageCues") : nullptr),
               FAthenaBatchedDamageGameplayCues_Shared::StaticStruct() ? FAthenaBatchedDamageGameplayCues_Shared::Size() : -1,
               FAthenaBatchedDamageGameplayCues_NonShared::StaticStruct() ? FAthenaBatchedDamageGameplayCues_NonShared::Size() : -1, (int)bAvailable);
    }

    if (!bAvailable || !Shooter || !HitActor)
        return;

    auto Shared = (FAthenaBatchedDamageGameplayCues_Shared*)malloc(FAthenaBatchedDamageGameplayCues_Shared::Size());
    auto NonShared = (FAthenaBatchedDamageGameplayCues_NonShared*)malloc(FAthenaBatchedDamageGameplayCues_NonShared::Size());

    memset((PBYTE)Shared, 0, FAthenaBatchedDamageGameplayCues_Shared::Size());
    memset((PBYTE)NonShared, 0, FAthenaBatchedDamageGameplayCues_NonShared::Size());

    Shared->Location = Hit.ImpactPoint;
    Shared->Normal = Hit.ImpactNormal;
    Shared->Magnitude = Magnitude;
    Shared->bWeaponActivate = true;
    Shared->bIsBallistic = true;
    Shared->bIsBeam = false;
    Shared->bIsFatal = bFatal;
    Shared->bIsCritical = bCritical;
    Shared->bIsShield = bShield;
    Shared->bIsShieldDestroyed = bShieldDestroyed;
    Shared->bIsValid = true;

    NonShared->HitActor = HitActor;

    if (bNonPlayer)
    {
        NonShared->NonPlayerHitActor = HitActor;
        Shared->NonPlayerLocation = Hit.ImpactPoint;
        Shared->NonPlayerNormal = Hit.ImpactNormal;
        Shared->NonPlayerMagnitude = Magnitude;
        Shared->NonPlayerbIsFatal = bFatal;
        Shared->NonPlayerbIsCritical = bCritical;
    }

    Shooter->NetMulticast_Athena_BatchedDamageCues(*Shared, *NonShared);

    free(Shared);
    free(NonShared);
}

static void ApplyRangedHit(AFortWeaponRanged* Weapon, FHitResult& Hit, const char* Path)
{
    if (!Weapon || !Weapon->HasWeaponData() || !Weapon->WeaponData)
        return;

    auto Stats = AFortInventory::GetStats(Weapon->WeaponData);

    if (!Stats)
        return;

    const char* Source = "handle";
    auto HitActor = Hit.HitObjectHandle.Get();

    if (!HitActor && FHitResult::HasComponent())
        if (auto HitComponent = Hit.Component.Get())
        {
            HitActor = (AActor*)HitComponent->GetOwner();
            Source = "component";
        }

    std::string WeaponName = Weapon->WeaponData->Name.ToString().c_str();

    if (!HitActor)
    {
        static int nn = 0;

        if (nn++ < 15)
            printf("[Boron][Damage] path=%s weapon=%s NO ACTOR (handle+component both null, hasComp=%d)\n",
                   Path, WeaponName.c_str(), (int)FHitResult::HasComponent());

        return;
    }

    std::string ActorName = "null";

    if (HitActor->Class)
        ActorName = HitActor->Class->Name.ToString().c_str();

    {
        static std::vector<std::string> Seen;
        auto Key = std::string(Path) + "|" + WeaponName;

        if (std::find(Seen.begin(), Seen.end(), Key) == Seen.end())
        {
            Seen.push_back(Key);
            printf("[Boron][Damage] FIRST path=%s weapon=%s actor=%s src=%s dmg=%.1f env=%.1f\n",
                   Path, WeaponName.c_str(), ActorName.c_str(), Source, Stats->DmgPB, Stats->EnvDmgPB);
        }
    }

    static auto PawnClass = FindClass("FortPawn");
    static auto BuildingClass = FindClass("BuildingSMActor");

    static int pn = 0, bn = 0, on = 0;

    if (PawnClass && HitActor->IsA(PawnClass))
    {
        static const int AircraftPhase = 3;
        static const int BusFlyingStep = 5;

        bool bPregame = false;

        if (auto GamePhaseLogic = UFortGameStateComponent_BattleRoyaleGamePhaseLogic::Get(UWorld::GetWorld()))
        {
            static int PhaseOff = -2;
            static int StepOff = -2;

            if (PhaseOff == -2)
                PhaseOff = GamePhaseLogic->GetOffset("GamePhase");

            if (StepOff == -2)
                StepOff = GamePhaseLogic->GetOffset("GamePhaseStep");

            int Phase = PhaseOff > 0 ? (int)GetFromOffset<uint8>(GamePhaseLogic, PhaseOff) : -1;
            int Step = StepOff > 0 ? (int)GetFromOffset<uint8>(GamePhaseLogic, StepOff) : -1;

            bPregame = (Phase >= 0 && Phase < AircraftPhase) || (Phase == AircraftPhase && Step >= 0 && Step < BusFlyingStep);

            static int wn = 0;

            if (wn++ < 8)
                printf("[Boron][Damage] phase=%d step=%d (off %d/%d) pregame=%d\n", Phase, Step, PhaseOff, StepOff, (int)bPregame);
        }

        auto Target = (AFortPlayerPawnAthena*)HitActor;
        float Shield = Target->GetShield();
        float Health = Target->GetHealth();

        if (Health <= 0.f || Target->IsDBNO())
            return;

        bool bLog = pn++ < 20;
        auto Bone = Hit.BoneName.ToString();
        bool bCrit = strstr(Bone.c_str(), "head") != nullptr;
        float Crit = Stats->DamageZone_Critical > 0.f ? Stats->DamageZone_Critical : 1.f;
        float Damage = Stats->DmgPB * (bCrit ? Crit : 1.f);

        if (Damage <= 0.f)
            return;

        if (bPregame)
        {
            SendDamageCue(Weapon, HitActor, Hit, Damage, false, bCrit, Shield > 0.f, false, false);
            return;
        }

        float Remaining = Damage;

        if (Shield > 0.f)
        {
            if (Shield <= Remaining)
            {
                Remaining -= Shield;
                Target->SetShield(0.f);
            }
            else
            {
                Target->SetShield(Shield - Remaining);
                Remaining = 0.f;
            }
        }

        bool bFatal = false;

        if (Remaining > 0.f)
        {
            if (Health <= Remaining)
                bFatal = true;
            else
                Target->SetHealth(Health - Remaining);
        }

        Target->ForceNetUpdate();

        SendDamageCue(Weapon, HitActor, Hit, Damage, bFatal, bCrit, Shield > 0.f, Shield > 0.f && Target->GetShield() <= 0.f, false);

        if (bLog)
            printf("[Boron][Damage] pawn path=%s cls=%s bone=%s crit=%d dmg=%.1f hp %.0f->%.0f sh %.0f->%.0f fatal=%d\n",
                   Path, ActorName.c_str(), Bone.c_str(), (int)bCrit, Damage,
                   Health, Target->GetHealth(), Shield, Target->GetShield(), (int)bFatal);

        if (!bFatal)
            return;

        AActor* KillerController = nullptr;
        auto Shooter = (AFortPlayerPawnAthena*)(Weapon->HasOwner() ? Weapon->Owner : nullptr);

        if (!Shooter && Weapon->HasInstigator())
            Shooter = (AFortPlayerPawnAthena*)Weapon->Instigator;

        if (Shooter && Shooter->HasController())
            KillerController = Shooter->Controller;

        UObject* KillerASC = nullptr;

        if (Shooter && Shooter->HasPlayerState())
            if (auto ShooterState = (AFortPlayerStateAthena*)Shooter->PlayerState)
                if (ShooterState->HasAbilitySystemComponent())
                    KillerASC = ShooterState->AbilitySystemComponent;

        UObject* TargetASC = nullptr;

        if (Target->HasPlayerState())
            if (auto TargetState = (AFortPlayerStateAthena*)Target->PlayerState)
                if (TargetState->HasAbilitySystemComponent())
                    TargetASC = TargetState->AbilitySystemComponent;

        FGameplayTag DeathReason;
        memset(&DeathReason, 0, sizeof(DeathReason));

        const char* Path2 = "none";

        if (Target->GetFunction("ForceKill"))
        {
            Path2 = "ForceKill";
            Target->ForceKill(DeathReason, KillerController, Weapon);
        }

        if (Target->GetHealth() > 0.f && !Target->IsDBNO() && KillerASC && Target->GetFunction("DoFatalDamage"))
        {
            Path2 = "DoFatalDamage";
            Target->DoFatalDamage(KillerASC);
        }

        if (Target->GetHealth() > 0.f && !Target->IsDBNO())
            Target->SetHealth(0.f);

        if (Target->GetHealth() <= 0.f && !Target->IsDBNO() && Target->GetFunction("KillDisconnectedPawn"))
        {
            Path2 = "KillDisconnectedPawn";
            Target->KillDisconnectedPawn();
        }

        Target->ForceNetUpdate();

        static int kn = 0;

        if (kn++ < 12)
            printf("[Boron][Damage] fatal via=%s target=%p killerPC=%p killerASC=%p targetASC=%p hpAfter=%.0f dbno=%d\n",
                   Path2, (void*)Target, (void*)KillerController, (void*)KillerASC, (void*)TargetASC,
                   Target->GetHealth(), (int)Target->IsDBNO());

        return;
    }

    if (BuildingClass && HitActor->IsA(BuildingClass))
    {
        bool bLog = bn++ < 12;
        auto Building = (ABuildingSMActor*)HitActor;
        float Damage = Stats->EnvDmgPB;

        if (Damage <= 0.f)
            return;

        float Left = Building->GetHealth() - Damage;

        Building->SetHealth(Left);
        Building->ForceNetUpdate();

        SendDamageCue(Weapon, HitActor, Hit, Damage, Left <= 0.f, false, false, false, true);

        if (bLog)
            printf("[Boron][Damage] building path=%s dmg=%.1f left=%.0f dorm=%d\n",
                   Path, Damage, Left, (int)Building->GetNetDormancy());

        if (Left <= 0.f)
            Building->K2_DestroyActor();

        return;
    }

    if (on++ < 20)
        printf("[Boron][Damage] UNHANDLED path=%s cls=%s\n", Path, ActorName.c_str());
}

void AFortWeaponRanged::ServerNotifyPawnHit_(UObject* Context, FFrame& Stack)
{
    static int hitOff = -2;

    if (hitOff == -2)
    {
        auto Fn = Stack.GetCurrentNativeFunction();
        hitOff = Fn ? (int)Fn->GetOffset("Hit") : -1;
        printf("[Boron][Damage] weapon ServerNotifyPawnHit first call, Hit offset=0x%X\n", hitOff);
    }

    Stack.IncrementCode();

    if (hitOff < 0 || !Stack.Locals)
        return;

    ApplyRangedHit((AFortWeaponRanged*)Context, *(FHitResult*)(__int64(Stack.Locals) + hitOff), "weapon");
}

void AFortWeaponRanged::ServerNotifyProjectilePawnHit(UObject* Context, FFrame& Stack)
{
    static int hitOff = -2;

    if (hitOff == -2)
    {
        auto Fn = Stack.GetCurrentNativeFunction();
        hitOff = Fn ? (int)Fn->GetOffset("Hit") : -1;
        printf("[Boron][Damage] projectile ServerNotifyPawnHit first call, Hit offset=0x%X\n", hitOff);
    }

    Stack.IncrementCode();

    auto Projectile = (AActor*)Context;

    if (hitOff < 0 || !Stack.Locals || !Projectile)
        return;

    static auto WeaponClass = FindClass("FortWeaponRanged");
    AActor* Owner = Projectile->HasOwner() ? Projectile->Owner : nullptr;

    if (!Owner || !WeaponClass || !Owner->IsA(WeaponClass))
    {
        static int pw = 0;

        if (pw++ < 10)
            printf("[Boron][Damage] projectile owner not a weapon: proj=%s owner=%s\n",
                   Projectile->Class ? Projectile->Class->Name.ToString().c_str() : "null",
                   Owner && Owner->Class ? Owner->Class->Name.ToString().c_str() : "null");

        return;
    }

    ApplyRangedHit((AFortWeaponRanged*)Owner, *(FHitResult*)(__int64(Stack.Locals) + hitOff), "projectile");
}

void AFortWeaponRanged::Hook()
{
    if (VersionInfo.EngineVersion < 5.4)
        return;

    auto Fn = GetDefaultObj()->GetFunction("ServerNotifyPawnHit");

    auto ProjectileClass = FindClass("FortProjectileAthena");
    auto ProjectileDefault = ProjectileClass ? ProjectileClass->GetDefaultObj() : nullptr;
    auto ProjectileFn = ProjectileDefault ? ProjectileDefault->GetFunction("ServerNotifyPawnHit") : nullptr;

    printf("[Boron][Damage] hooks: weapon=%p projectileCls=%p projectileFn=%p\n",
           (void*)Fn, (void*)ProjectileClass, (void*)ProjectileFn);

    if (Fn)
        Hooking::ExecHook(Fn, ServerNotifyPawnHit_);

    if (ProjectileFn)
        Hooking::ExecHook(ProjectileFn, ServerNotifyProjectilePawnHit);
}
