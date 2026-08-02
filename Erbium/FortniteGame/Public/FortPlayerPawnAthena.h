#pragma once
#include "../../pch.h"
#include "../../Engine/Public/CurveTable.h"
#include "GameplayTagContainer.h"

class UCharacterMovementComponent : public UObject
{
public:
    UCLASS_COMMON_MEMBERS(UCharacterMovementComponent);

    DEFINE_PROP(Velocity, FVector);
    DEFINE_BITFIELD_PROP(bCheatFlying);
    DEFINE_BITFIELD_PROP(bIgnoreClientMovementErrorChecksAndCorrection);
    DEFINE_BITFIELD_PROP(bServerAcceptClientAuthoritativePosition);

    DEFINE_FUNC(SetMovementMode, void);
};

struct FAthenaBatchedDamageGameplayCues_Shared
{
public:
    USCRIPTSTRUCT_COMMON_MEMBERS(FAthenaBatchedDamageGameplayCues_Shared);

    DEFINE_STRUCT_PROP(Location, FVector);
    DEFINE_STRUCT_PROP(Normal, FVector);
    DEFINE_STRUCT_PROP(Magnitude, float);
    DEFINE_STRUCT_PROP(bWeaponActivate, bool);
    DEFINE_STRUCT_PROP(bIsFatal, bool);
    DEFINE_STRUCT_PROP(bIsCritical, bool);
    DEFINE_STRUCT_PROP(bIsShield, bool);
    DEFINE_STRUCT_PROP(bIsShieldDestroyed, bool);
    DEFINE_STRUCT_PROP(bIsBallistic, bool);
    DEFINE_STRUCT_PROP(bIsBeam, bool);
    DEFINE_STRUCT_PROP(NonPlayerLocation, FVector);
    DEFINE_STRUCT_PROP(NonPlayerNormal, FVector);
    DEFINE_STRUCT_PROP(NonPlayerMagnitude, float);
    DEFINE_STRUCT_PROP(NonPlayerbIsFatal, bool);
    DEFINE_STRUCT_PROP(NonPlayerbIsCritical, bool);
    DEFINE_STRUCT_PROP(bIsValid, bool);
};

struct FAthenaBatchedDamageGameplayCues_NonShared
{
public:
    USCRIPTSTRUCT_COMMON_MEMBERS(FAthenaBatchedDamageGameplayCues_NonShared);

    DEFINE_STRUCT_PROP(HitActor, AActor*);
    DEFINE_STRUCT_PROP(NonPlayerHitActor, AActor*);
};

class USkeletalMeshComponent : public UObject
{
public:
    UCLASS_COMMON_MEMBERS(USkeletalMeshComponent);

    DEFINE_PROP(VisibilityBasedAnimTickOption, uint8);
    DEFINE_BITFIELD_PROP(bEnableUpdateRateOptimizations);
};

class UClamberingComponent : public UObject
{
public:
    UCLASS_COMMON_MEMBERS(UClamberingComponent);

    DEFINE_PROP(LocalClamberingState, uint8);
    DEFINE_PROP(ReplicatedClamberingState, uint8);
    DEFINE_PROP(ClamberingEnabled, FScalableFloat);
    DEFINE_PROP(ClamberIndicatorEnabled, FScalableFloat);
    DEFINE_PROP(ServerValidatePlayerMaxDistance, FScalableFloat);
    DEFINE_PROP(ServerFailDelay, FScalableFloat);
    DEFINE_PROP(SynchedActionFailDelay, FScalableFloat);
    DEFINE_PROP(bPerformTargetingWhileWalking, bool);
    DEFINE_PROP(bPerformTargetingWhileSwimming, bool);
    DEFINE_PROP(MovementModeExtension, UClass*);

    DEFINE_FUNC(IsClamberingEnabled, bool);
    DEFINE_FUNC(IsAutoClamberingEnabled, bool);
    DEFINE_FUNC(ShouldShowClamberIndicator, bool);

    static void Configure(AActor* Pawn);

    DefUHookOg(ServerStartClambering);
    DefUHookOg(NetMulticast_ClamberingLedgeFailed);

    InitPostLoadHooks;
};

struct FZiplinePawnState
{
public:
    USCRIPTSTRUCT_COMMON_MEMBERS(FZiplinePawnState);

    DEFINE_STRUCT_PROP(bJumped, bool);

    uint8_t Padding[0x100];
};

class AFortAscenderZipline : public AActor
{
public:
    UCLASS_COMMON_MEMBERS(AFortAscenderZipline);

    DEFINE_NEWOBJ_PROP(PawnUsingHandle, AActor);
    DEFINE_PROP(PreviousPawnUsingHandle, TWeakObjectPtr<AActor>);

    DEFINE_FUNC(OnRep_PawnUsingHandle, void);
    DEFINE_FUNC(OnZipliningStarted, void);
    DEFINE_FUNC(OnZipliningStopped, void);
};

struct FFortGameplayAttributeData
{
public:
    USCRIPTSTRUCT_COMMON_MEMBERS(FFortGameplayAttributeData);

    DEFINE_STRUCT_PROP(CurrentValue, float);
    DEFINE_STRUCT_PROP(BaseValue, float);
    DEFINE_STRUCT_PROP(Minimum, float);
    DEFINE_STRUCT_PROP(Maximum, float);
    DEFINE_STRUCT_PROP(UnclampedBaseValue, float);
    DEFINE_STRUCT_PROP(UnclampedCurrentValue, float);
};

class UFortHealthSet : public UObject
{
public:
    UCLASS_COMMON_MEMBERS(UFortHealthSet);

    DEFINE_PROP(Health, FFortGameplayAttributeData);

    DEFINE_FUNC(OnRep_Health, void);
};

struct FDamagerInfo
{
public:
    AActor* DamageCauser;
    int32 DamageAmount;
    FGameplayTagContainer SourceTags;
};

class AFortPlayerPawnAthena : public AActor
{
public:
    UCLASS_COMMON_MEMBERS(AFortPlayerPawnAthena);

    DEFINE_PROP(CurrentWeapon, AActor*); // everything breaks if we include FortWeapon.h so
    DEFINE_PROP(PreviousWeapon, AActor*);
    DEFINE_PROP(Controller, AActor*);
    DEFINE_PROP(IncomingPickups, TArray<AActor*>);
    DEFINE_PROP(CharacterMovement, UCharacterMovementComponent*);
    DEFINE_PROP(ZiplineState, FZiplinePawnState);
    DEFINE_BITFIELD_PROP(bMovingEmote);
    DEFINE_PROP(EmoteWalkSpeed, float);
    DEFINE_BITFIELD_PROP(bMovingEmoteForwardOnly);
    DEFINE_BITFIELD_PROP(bMovingEmoteFollowingOnly);
    DEFINE_PROP(LastFallDistance, float);
    DEFINE_PROP(GameplayTags, FGameplayTagContainer);
    DEFINE_BITFIELD_PROP(bIsInAnyStorm);
    DEFINE_BITFIELD_PROP(bIsInsideSafeZone);
    DEFINE_PROP(AIControllerClass, TSubclassOf<AActor>);
    DEFINE_PROP(PlayerState, AActor*);
    DEFINE_PROP(BaseEyeHeight, float);
    DEFINE_PROP(OnHeldObjectPickedUp, TMulticastInlineDelegate<void(AActor*)>);
    DEFINE_PROP(OnHeldObjectDropped, TMulticastInlineDelegate<void(AActor*)>);
    DEFINE_PROP(OnEnteredAircraft, TMulticastInlineDelegate<void()>);
    DEFINE_PROP(PickupSpeedMultiplier, float);
    DEFINE_PROP(HeldObject, TWeakObjectPtr<AActor>);
    DEFINE_PROP(RepActiveMovementModeExtension, void*);
    DEFINE_BITFIELD_PROP(bIsPlayingEmote);
    DEFINE_PROP(HealthSet, UFortHealthSet*);
    DEFINE_PROP(CurrentWeaponList, TArray<AActor*>);
    DEFINE_PROP(bShouldDropItemsOnDeath, bool);
    DEFINE_PROP(MoveSoundStimulusBroadcastInterval, uint16_t);
    DEFINE_PROP(Damagers, TArray<FDamagerInfo>);
    DEFINE_PROP(LastReplicatedEmoteExecuted, UObject*);
    DEFINE_PROP(Mesh, UActorComponent*);
    DEFINE_BITFIELD_PROP(bIsSkydiving);
    DEFINE_BITFIELD_PROP(bIsSkydivingFromBus);
    DEFINE_PROP(RegisteredMovementModeExtentionLogic, TMap<uint32, UObject*>);
    DEFINE_PROP(VehicleInputComponent, UObject*);

    DEFINE_FUNC(BeginSkydiving, void);
    DEFINE_FUNC(GetHealth, float);
    DEFINE_FUNC(GetShield, float);
    DEFINE_FUNC(SetHealth, void);
    DEFINE_FUNC(SetShield, void);
    DEFINE_FUNC(SetMaxHealth, void);
    DEFINE_FUNC(DoFatalDamage, void);
    DEFINE_FUNC(ForceKill, void);
    DEFINE_FUNC(KillDisconnectedPawn, void);
    DEFINE_FUNC(NetMulticast_Athena_BatchedDamageCues, void);
    DEFINE_FUNC(EquipWeaponDefinition, AActor*);
    DEFINE_FUNC(PawnStartFire, void);
    DEFINE_FUNC(PawnStopFire, void);
    DEFINE_FUNC(AddMovementInput, void);
    DEFINE_FUNC(OnRep_IsTargeting, void);
    DEFINE_BITFIELD_PROP(bIsTargeting);
    DEFINE_FUNC(LaunchCharacterJump, void);
    DEFINE_FUNC(OnCapsuleBeginOverlap, void);
    DEFINE_FUNC(ServerHandlePickup, void);
    DEFINE_FUNC(IsDBNO, bool);
    DEFINE_FUNC(PickUpActor, void);
    DEFINE_FUNC(OnRep_IsInAnyStorm, void);
    DEFINE_FUNC(OnRep_IsInsideSafeZone, void);
    DEFINE_FUNC(OnRep_PlayerState, void);
    DEFINE_FUNC(ServerSetAttachment, void);
    DEFINE_FUNC(GetActiveZipline, AFortAscenderZipline*);
    DEFINE_FUNC(ServerOnExitVehicle, AActor*);
    DEFINE_FUNC(SetInVortex, void);
    DEFINE_FUNC(ClientInternalEquipWeapon, void);
    DEFINE_FUNC(ServerInternalEquipWeapon, void);
    DEFINE_FUNC(SetGravityMultiplier, void);
    DEFINE_FUNC(OnRep_LastReplicatedEmoteExecuted, void);
    DEFINE_FUNC(EmoteStopped, void);
    DEFINE_FUNC(ServerChoosePart, void);
    DEFINE_FUNC(ServerThrowCarriedPlayer, void);
    DEFINE_FUNC(LocalThrowCarriedPlayer, void);
    DEFINE_FUNC(GetVehicleActor, AActor*);

    DefUHookOg(ServerHandlePickup_);
    DefUHookOg(ServerHandlePickupInfo); //TODO: needs a fix for crash on 16.xx builds (confirmed a crash on 16.40)
    DefHookOg(bool, FinishedTargetSpline, void*);
    DefUHookOg(ServerSendZiplineState);
    DefUHookOg(OnCapsuleBeginOverlap_);
    static void MovingEmoteStopped(UObject*, FFrame&);
    DefUHookOg(Athena_MedConsumable_Triggered);
    DefUHookOgRet(AActor*, ServerOnExitVehicle_);
    DefUHookOg(EmoteStopped_);
    static void ServerHandlePickupWithRequestedSwap(UObject*, FFrame&);
    DefHookOg(void, EndSkydiving, AFortPlayerPawnAthena*);
    DefUHookOg(ServerReviveFromDBNO);
    DefUHookOg(ServerThrowCarriedPlayer_);
    static void SetIsInsideSafeZone(AFortPlayerPawnAthena* _this, bool bNewValue);
    static void UpdateOutsideSafeZone(AFortPlayerPawnAthena* _this);

    InitPostLoadHooks;
};