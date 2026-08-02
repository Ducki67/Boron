#pragma once
#include "../../pch.h"
#include "../../Engine/Public/AbilitySystemComponent.h"
#include "FortInventory.h"
#include "FortPhysicsPawn.h"

struct FFortAbilitySetHandle final
{
public:
    USCRIPTSTRUCT_COMMON_MEMBERS(FFortAbilitySetHandle);

    TWeakObjectPtr<UAbilitySystemComponent> TargetAbilitySystemComponent;
    TArray<FGameplayAbilitySpecHandle> GrantedAbilityHandles;
    TArray<FActiveGameplayEffectHandle> AppliedEffectHandles;
    TArray<FGuid> ItemGuidsForAdditionalItems;
};

class AFortWeapon : public AActor
{
public:
    UCLASS_COMMON_MEMBERS(AFortWeapon);

    DEFINE_PROP(ItemEntryGuid, FGuid);
    DEFINE_PROP(WeaponData, UFortWeaponItemDefinition*);
    DEFINE_PROP(WeaponModSlots, TArray<FFortWeaponModSlot>);
    DEFINE_PROP(ContextTrapItemDefinition, UFortItemDefinition*);
    DEFINE_PROP(PrimaryAbilitySpecHandle, FGameplayAbilitySpecHandle);
    DEFINE_PROP(SecondaryAbilitySpecHandle, FGameplayAbilitySpecHandle);
    DEFINE_PROP(ReloadAbilitySpecHandle, FGameplayAbilitySpecHandle);
    DEFINE_PROP(ImpactAbilitySpecHandle, FGameplayAbilitySpecHandle);
    DEFINE_PROP(ReticleTraceOverrideSpecHandle, FGameplayAbilitySpecHandle);
    DEFINE_PROP(EquippedAbilityHandles, TArray<FGameplayAbilitySpecHandle>);
    DEFINE_PROP(EquippedAbilitySetHandles, TArray<FFortAbilitySetHandle>);
    DEFINE_PROP(MountedWeaponInfoRepped, FMountedWeaponInfoRepped);
    DEFINE_BITFIELD_PROP(bCompletedWeaponLoad);

    DEFINE_FUNC(ServerReleaseWeaponAbility, void);
    DEFINE_FUNC(OnRep_MountedWeaponInfoRepped, void);
};

struct FHitResult
{
public:
    USCRIPTSTRUCT_COMMON_MEMBERS(FHitResult);

    DEFINE_STRUCT_PROP(BoneName, FName);
    DEFINE_STRUCT_PROP(ImpactPoint, FVector);
    DEFINE_STRUCT_PROP(ImpactNormal, FVector);
    DEFINE_STRUCT_PROP(HitObjectHandle, TWeakObjectPtr<AActor>);
    DEFINE_STRUCT_PROP(Component, TWeakObjectPtr<UActorComponent>);
};

class AFortWeaponRanged : public AFortWeapon
{
public:
    UCLASS_COMMON_MEMBERS(AFortWeaponRanged);

    DEFINE_PROP(AmmoCount, int32);

    static void ServerNotifyPawnHit_(UObject* Context, FFrame& Stack);
    static void ServerNotifyProjectilePawnHit(UObject* Context, FFrame& Stack);
    static void Hook();
};