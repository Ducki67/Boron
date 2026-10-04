#include "pch.h"
#include "../Public/FortLootPackage.h"
#include "../../Erbium/Public/Configuration.h"
#include "../Public/BuildingContainer.h"
#include "../Public/FortGameMode.h"
#include "../Public/FortKismetLibrary.h"

struct FFortLootLevelData
{
public:
    USCRIPTSTRUCT_COMMON_MEMBERS(FFortLootLevelData);

    DEFINE_STRUCT_PROP(Category, FName);
    DEFINE_STRUCT_PROP(category, FName);
    DEFINE_STRUCT_PROP(LootLevel, int32);
    DEFINE_STRUCT_PROP(MinItemLevel, int32);
    DEFINE_STRUCT_PROP(MaxItemLevel, int32);
};

int GetLevel(const FDataTableCategoryHandle& CategoryHandle)
{
    auto GameMode = (AFortGameMode*)UWorld::GetWorld()->AuthorityGameMode;
    auto GameState = (AFortGameStateAthena*)GameMode->GameState;

    if (!CategoryHandle.DataTable)
        return 0;

    if (!CategoryHandle.ColumnName)
        return 0;

    if (!CategoryHandle.RowContents.ComparisonIndex)
        return 0;

    int Level = 0;
    FFortLootLevelData* LootLevelData = nullptr;

    for (auto& LootLevelDataPair : (TMap<FName, FFortLootLevelData*>)CategoryHandle.DataTable->RowMap)
    {
        if (!LootLevelDataPair.Value())
            continue;

        if ((FFortLootLevelData::HasCategory() ? LootLevelDataPair.Value()->Category : LootLevelDataPair.Value()->category) != CategoryHandle.RowContents ||
            LootLevelDataPair.Value()->LootLevel > GameState->WorldLevel || LootLevelDataPair.Value()->LootLevel <= Level)
            continue;

        Level = LootLevelDataPair.Value()->LootLevel;
        LootLevelData = LootLevelDataPair.Value();
    }

    if (LootLevelData)
    {
        auto subbed = LootLevelData->MaxItemLevel - LootLevelData->MinItemLevel;

        if (subbed <= -1)
            subbed = 0;
        else
        {
            auto calc = (int)(((float)rand() / 32767) * (float)(subbed + 1));
            if (calc <= subbed)
                subbed = calc;
        }

        return subbed + LootLevelData->MinItemLevel;
    }

    return 0;
}

struct FWeaponPickupAmmoCountData
{
public:
    USCRIPTSTRUCT_COMMON_MEMBERS(FWeaponPickupAmmoCountData);

    DEFINE_STRUCT_PROP(AmmoItemDefinitionTag, FGameplayTag);
    DEFINE_STRUCT_PROP(SpawnCount, FScalableFloat);
};

struct FHelperGameplayTagToAmmoCountMultiplier
{
public:
    USCRIPTSTRUCT_COMMON_MEMBERS(FHelperGameplayTagToAmmoCountMultiplier);

    DEFINE_STRUCT_PROP(Tag, FGameplayTag);
    DEFINE_STRUCT_PROP(CountMultiplier, FScalableFloat);
};

class UFortWeaponPickupSpawnAmmoData : public UObject
{
public:
    UCLASS_COMMON_MEMBERS(UFortWeaponPickupSpawnAmmoData);

    DEFINE_PROP(WeaponPickupAmmoCountArray, TArray<FWeaponPickupAmmoCountData>);
    DEFINE_PROP(DefaultWeaponAmmoMultiplier, FScalableFloat);
    DEFINE_PROP(WeaponPickupAmmoMultiplierOverrideArray, TArray<FHelperGameplayTagToAmmoCountMultiplier>);
    DEFINE_PROP(SourceToAmmoMultiplierOverrideArray, TArray<FHelperGameplayTagToAmmoCountMultiplier>);
};

void UFortLootPackage::SetupLDSForPackage(TArray<FFortItemEntry*>& LootDrops, SDK::FName Package, int i, FName TierGroup, int WorldLevel, ABuildingContainer* Container)
{
    TArray<FFortLootPackageData*> LPGroups{};

    for (auto const& Val : LootPackageMap[Package.ComparisonIndex])
    {
        if (!Val)
            continue;

        if (i != -1 && Val->LootPackageCategory != i)
            continue;

        if (WorldLevel >= 0)
        {
            if (Val->MaxWorldLevel >= 0 && WorldLevel > Val->MaxWorldLevel)
                continue;

            if (Val->MinWorldLevel >= 0 && WorldLevel < Val->MinWorldLevel)
                continue;
        }

        LPGroups.Add(Val);
    }

    static int LDSDiag = 0;
    bool bLDSDiag = VersionInfo.FortniteVersion >= 32 && LDSDiag++ < 24;
    if (bLDSDiag)
        printf("[Boron][LDS] pkg=%s cat=%d all=%d groups=%d\n", Package.ToString().c_str(), i, LootPackageMap[Package.ComparisonIndex].Num(), LPGroups.Num());

    if (LPGroups.Num() == 0)
        return;

    auto LootPackage = PickWeighted(LPGroups, [](float Total) { return ((float)rand() / 32767.f) * Total; });
    if (bLDSDiag)
        printf("[Boron][LDS]   picked=%p call=%d count=%d item=%p\n", LootPackage, LootPackage ? LootPackage->LootPackageCall.Num() : -1, LootPackage ? LootPackage->Count : -1, LootPackage ? (void*)LootPackage->ItemDefinition.Get() : nullptr);
    if (!LootPackage)
        return;

    if (LootPackage->LootPackageCall.Num() > 1)
    {
        for (int i = 0; i < LootPackage->Count; i++)
            SetupLDSForPackage(LootDrops, UKismetStringLibrary::Conv_StringToName(LootPackage->LootPackageCall), 0, TierGroup, WorldLevel);

        return;
    }

    auto ItemDefinition = LootPackage->ItemDefinition.Get();

    if (!ItemDefinition)
        return;

    FFortItemEntry* AmmoEntry = nullptr;

    if (VersionInfo.FortniteVersion >= 11 && VersionInfo.FortniteVersion != 19.40)
    {
        if (auto WorldItemDefinition = ItemDefinition->Cast<UFortWorldItemDefinition>())
        {
            auto AmmoDef = WorldItemDefinition->GetAmmoWorldItemDefinition_BP();

            if (auto AmmoDefinition = AmmoDef->Cast<UFortAmmoItemDefinition>())
            {
                auto SpawnAmmoData = FindObject<UFortWeaponPickupSpawnAmmoData>(L"/Game/Athena/Balance/Pickups/FortWeaponPickupSpawnAmmoData.FortWeaponPickupSpawnAmmoData");

                FGameplayTagContainer AmmoTags{};

                auto Interface = (IGameplayTagAssetInterface*)AmmoDefinition->GetInterface(IGameplayTagAssetInterface::StaticClass());
                if (Interface)
                {
                    auto GetOwnedGameplayTags = (void (*)(IGameplayTagAssetInterface*, FGameplayTagContainer*))Interface->Vft[0x2];
                    GetOwnedGameplayTags(Interface, &AmmoTags);
                    // Interface->GetOwnedGameplayTags(&TargetTags);
                }

                float AmmoCount = 0.f;
                for (int i = 0; i < SpawnAmmoData->WeaponPickupAmmoCountArray.Num(); i++)
                {
                    auto& AmmoCountData = SpawnAmmoData->WeaponPickupAmmoCountArray.Get(i, FWeaponPickupAmmoCountData::Size());

                    if (AmmoTags.HasTag(AmmoCountData.AmmoItemDefinitionTag))
                    {
                        AmmoCount = AmmoCountData.SpawnCount.Evaluate((float)WorldLevel);
                        break;
                    }
                }

                auto Multiplier = SpawnAmmoData->DefaultWeaponAmmoMultiplier.Evaluate((float)WorldLevel);

                FGameplayTagContainer WeaponTags{};

                auto Interface2 = (IGameplayTagAssetInterface*)ItemDefinition->GetInterface(IGameplayTagAssetInterface::StaticClass());
                if (Interface2)
                {
                    auto GetOwnedGameplayTags = (void (*)(IGameplayTagAssetInterface*, FGameplayTagContainer*))Interface2->Vft[0x2];
                    GetOwnedGameplayTags(Interface2, &WeaponTags);
                    // Interface->GetOwnedGameplayTags(&TargetTags);
                }

                for (int i = 0; i < SpawnAmmoData->WeaponPickupAmmoMultiplierOverrideArray.Num(); i++)
                {
                    auto& MultiplierData = SpawnAmmoData->WeaponPickupAmmoMultiplierOverrideArray.Get(i, FHelperGameplayTagToAmmoCountMultiplier::Size());

                    if (WeaponTags.HasTag(MultiplierData.Tag))
                    {
                        Multiplier = MultiplierData.CountMultiplier.Evaluate((float)WorldLevel);
                        break;
                    }
                }

                float SourceMultiplier = 1.f;

                if (Container)
                {
                    FGameplayTagContainer SourceTags{};

                    auto Interface3 = (IGameplayTagAssetInterface*)Container->GetInterface(IGameplayTagAssetInterface::StaticClass());
                    if (Interface3)
                    {
                        auto GetOwnedGameplayTags = (void (*)(IGameplayTagAssetInterface*, FGameplayTagContainer*))Interface3->Vft[0x2];
                        GetOwnedGameplayTags(Interface3, &SourceTags);
                        // Interface->GetOwnedGameplayTags(&TargetTags);
                    }

                    for (int i = 0; i < SpawnAmmoData->SourceToAmmoMultiplierOverrideArray.Num(); i++)
                    {
                        auto& MultiplierData = SpawnAmmoData->SourceToAmmoMultiplierOverrideArray.Get(i, FHelperGameplayTagToAmmoCountMultiplier::Size());

                        if (SourceTags.HasTag(MultiplierData.Tag))
                        {
                            SourceMultiplier = MultiplierData.CountMultiplier.Evaluate((float)WorldLevel);
                            break;
                        }
                    }
                    SourceTags.GameplayTags.Free();
                    SourceTags.ParentTags.Free();
                }

                auto FinalCount = AmmoCount * Multiplier * SourceMultiplier;

                if (FinalCount > 0.f)
                    AmmoEntry = AFortInventory::MakeItemEntry(
                        AmmoDefinition, (int)FinalCount,
                        AmmoDefinition->IsA(UFortWorldItemDefinition::StaticClass())
                            ? (AmmoDefinition->HasLootLevelData() ? std::clamp(GetLevel(AmmoDefinition->LootLevelData), AmmoDefinition->MinLevel, AmmoDefinition->MaxLevel > 0 ? AmmoDefinition->MaxLevel : 99999) : 0)
                            : 0);

                AmmoTags.GameplayTags.Free();
                AmmoTags.ParentTags.Free();
                WeaponTags.GameplayTags.Free();
                WeaponTags.ParentTags.Free();
            }
        }
    }

    bool found = false;
    bool foundAmmo = false;
    for (auto& LootDrop : LootDrops)
    {
        if (!FFortItemEntry::HasCount())
            break;

        if (/*(!AmmoDef || AmmoDef->DropCount) && */ LootDrop->ItemDefinition == ItemDefinition && LootDrop->Count < ItemDefinition->GetMaxStackSize())
        {
            LootDrop->Count += LootPackage->Count;

            if (LootDrop->Count > ItemDefinition->GetMaxStackSize())
            {
                auto OGCount = LootDrop->Count;
                LootDrop->Count = ItemDefinition->GetMaxStackSize();

                // if (Inventory::GetQuickbar(LootDrop.ItemDefinition) == EFortQuickBars::Secondary)
                LootDrops.Add(AFortInventory::MakeItemEntry(
                    ItemDefinition, OGCount - (int32)ItemDefinition->GetMaxStackSize(),
                    ItemDefinition->IsA(UFortWorldItemDefinition::StaticClass())
                        ? (ItemDefinition->HasLootLevelData() ? std::clamp(GetLevel(ItemDefinition->LootLevelData), ItemDefinition->MinLevel, ItemDefinition->MaxLevel > 0 ? ItemDefinition->MaxLevel : 99999) : 0)
                        : 0));
            }

            // if (Inventory::GetQuickbar(LootDrop.ItemDefinition) == EFortQuickBars::Secondary)
            found = true;
            break;
        }

        if (AmmoEntry && AmmoEntry->ItemDefinition && LootDrop->ItemDefinition == AmmoEntry->ItemDefinition)
        {
            LootDrop->Count += AmmoEntry->Count;

            if (LootDrop->Count > AmmoEntry->ItemDefinition->GetMaxStackSize())
            {
                auto OGCount = LootDrop->Count;
                LootDrop->Count = AmmoEntry->ItemDefinition->GetMaxStackSize();

                // if (!AFortInventory::IsPrimaryQuickbar(LootDrop->ItemDefinition))
                LootDrops.Add(AFortInventory::MakeItemEntry(AmmoEntry->ItemDefinition, OGCount - AmmoEntry->ItemDefinition->GetMaxStackSize(),
                                                            AmmoEntry->ItemDefinition->IsA(UFortWorldItemDefinition::StaticClass())
                                                                ? (AmmoEntry->ItemDefinition->HasLootLevelData() ? std::clamp(GetLevel(AmmoEntry->ItemDefinition->LootLevelData), AmmoEntry->ItemDefinition->MinLevel,
                                                                                                                              AmmoEntry->ItemDefinition->MaxLevel > 0 ? AmmoEntry->ItemDefinition->MaxLevel : 99999)
                                                                                                                 : 0)
                                                                : 0));
            }

            // if (Inventory::GetQuickbar(LootDrop.ItemDefinition) == EFortQuickBars::Secondary)
            free(AmmoEntry);
            foundAmmo = true;
        }
    }

    if (!found && LootPackage->Count > 0)
        LootDrops.Add(AFortInventory::MakeItemEntry(
            ItemDefinition, LootPackage->Count,
            ItemDefinition->IsA(UFortWorldItemDefinition::StaticClass())
                ? (ItemDefinition->HasLootLevelData() ? std::clamp(GetLevel(ItemDefinition->LootLevelData), ItemDefinition->MinLevel, ItemDefinition->MaxLevel > 0 ? ItemDefinition->MaxLevel : 99999) : 0)
                : 0));

    if (!foundAmmo && AmmoEntry && LootPackage->Count > 0)
        LootDrops.Add(AmmoEntry);
    LPGroups.Free();
}

void UFortLootPackage::ChooseLootForContainer(TArray<FFortItemEntry*>& LootDrops, FName TierGroup, int LootTier, int WorldLevel, ABuildingContainer* Container)
{
    TArray<FFortLootTierData*> TierDataGroups{};

    for (auto const& Val : TierDataMap[TierGroup.ComparisonIndex])
        if (LootTier == -1 ? true : LootTier == Val->LootTier)
            TierDataGroups.Add(Val);

    static int TierDiag = 0;
    if (VersionInfo.FortniteVersion >= 32 && TierDiag++ < 4)
    {
        printf("[Boron][LDS] tierRows=%d offW=0x%x offNum=0x%x offTier=0x%x offPkg=0x%x\n", TierDataGroups.Num(), FFortLootTierData::Weight__Offset, FFortLootTierData::NumLootPackageDrops__Offset, FFortLootTierData::LootTier__Offset, FFortLootTierData::LootPackage__Offset);
        for (int i = 0; i < TierDataGroups.Num() && i < 4; i++)
            printf("[Boron][LDS]   row%d w=%.2f num=%.2f tier=%d pkg=%s\n", i, TierDataGroups[i]->Weight, TierDataGroups[i]->NumLootPackageDrops, TierDataGroups[i]->LootTier, TierDataGroups[i]->LootPackage.ToString().c_str());
    }

    auto LootTierData = PickWeighted(TierDataGroups, [](float Total) { return ((float)rand() / 32767.f) * Total; });

    if (!LootTierData)
        return;

    // printf("Picked LootTierData %s\n", LootTierData->LootPackage.ToString().c_str());

    if (FFortLootTierData::HasNumLootPackageDropsRange())
    {
        auto& Range = LootTierData->NumLootPackageDropsRange;
        if (Range.Y > 0)
            LootTierData->NumLootPackageDrops = (float)(Range.X + ((double)rand() / 32767.0) * (Range.Y - Range.X));
    }

    if (LootTierData->NumLootPackageDrops <= 0)
        return;

    // printf("Selecting %f loot drops from <unk>\n", LootTierData->NumLootPackageDrops);
    if (VersionInfo.FortniteVersion == 19.40)
    {
        auto& MinArr = LootTierData->LootPackageCategoryMinArray;

        if (MinArr.Num() > 1 && MinArr[1] == 0 && LootTierData->LootPackage.ToString().starts_with("WorldPKG.AthenaLoot.Weapon."))
        {
            MinArr[1] = 1;
            LootTierData->NumLootPackageDrops++;
        }
    }

    int DropCount;
    if (LootTierData->NumLootPackageDrops < 1.f)
        DropCount = 1;
    else
    {
        DropCount = (int)((LootTierData->NumLootPackageDrops * 2.f) - .5f) >> 1;

        float RemainderSomething = LootTierData->NumLootPackageDrops - (float)DropCount;

        if (RemainderSomething > 0.0000099999997f)
            DropCount += RemainderSomething >= ((float)rand() / 32767.f);
    }

    float AmountOfLootDrops = 0;
    float MinLootDrops = 0;

    std::unordered_map<int, int> NumMap;

    for (int i = 0; i < LootTierData->LootPackageCategoryMinArray.Num(); i++)
    {
        NumMap[i] = LootTierData->LootPackageCategoryMinArray[i];
        AmountOfLootDrops += LootTierData->LootPackageCategoryMinArray[i];
    }

    int SumWeights = 0;
    std::unordered_map<int, int> WeightMap;

    for (int i = 0; i < LootTierData->LootPackageCategoryWeightArray.Num(); i++)
    {
        if (LootTierData->LootPackageCategoryWeightArray[i] > 0 && (LootTierData->LootPackageCategoryMaxArray[i] < 0 || NumMap[i] < LootTierData->LootPackageCategoryMaxArray[i]))
        {
            WeightMap[i] = LootTierData->LootPackageCategoryWeightArray[i];
            SumWeights += LootTierData->LootPackageCategoryWeightArray[i];
        }
    }

    if (AmountOfLootDrops < DropCount)
        while (SumWeights > 0)
        {
            auto RandomValue = (float)rand() / 32767.f;
            auto RandomWeight = (int)std::floor(RandomValue * SumWeights);

            int Category = -1;
            for (auto& [DropCategory, Weight] : WeightMap)
            {
                if (RandomWeight <= Weight && RandomWeight <= LootTierData->NumLootPackageDrops)
                {
                    Category = DropCategory;
                    break;
                }

                RandomWeight -= Weight;
            }

            if (Category != -1)
            {
                AmountOfLootDrops++;
                NumMap[Category]++;

                if (LootTierData->LootPackageCategoryMaxArray[Category] >= 0 && NumMap[Category] >= LootTierData->LootPackageCategoryMaxArray[Category])
                {
                    SumWeights -= LootTierData->LootPackageCategoryWeightArray[Category];
                    WeightMap.erase(Category);
                }

                if (AmountOfLootDrops >= DropCount)
                    break;
            }
        }

    /*if (AmountOfLootDrops < DropCount)
            while (SumWeights > 0)
            {
                    AmountOfLootDrops++;

                    if (AmountOfLootDrops >= DropCount)
                    {
                            //AmountOfLootDrops = AmountOfLootDrops;
                            break;
                    }

                    SumWeights--;
            }

    if (!AmountOfLootDrops)
            return {};*/

    static int CLDiag = 0;
    if (VersionInfo.FortniteVersion >= 32 && CLDiag++ < 8)
        printf("[Boron][LDS] tierPkg=%s num=%.2f drops=%d cats=%d min0=%d\n", LootTierData->LootPackage.ToString().c_str(), LootTierData->NumLootPackageDrops, DropCount, (int)NumMap.size(), NumMap.size() ? NumMap[0] : -1);

    LootDrops.Reserve((int)DropCount);

    int SpawnedItems = 0;
    int CurrentCategory = 0;
    while (SpawnedItems < DropCount && CurrentCategory < NumMap.size())
    {
        for (int j = 0; j < NumMap[CurrentCategory]; j++)
            SetupLDSForPackage(LootDrops, LootTierData->LootPackage, CurrentCategory, TierGroup, WorldLevel);

        SpawnedItems += NumMap[CurrentCategory];
        CurrentCategory++;
    }
    TierDataGroups.Free();
}

bool UFortLootPackage::SpawnLootHook(ABuildingContainer* Container)
{
    static int SpawnLootDiag = 0;
    if (VersionInfo.FortniteVersion >= 32 && SpawnLootDiag++ < 4)
        printf("[Boron][Chest] SpawnLoot %s searched=%d off=0x%x mask=0x%x\n", Container->Name.ToString().c_str(), (int)Container->bAlreadySearched, ABuildingContainer::bAlreadySearched__Offset, ABuildingContainer::bAlreadySearched__FieldMask);

    if (Container->bAlreadySearched)
        return false;

    auto RealTierGroup = Container->SearchLootTierGroup;
    auto GameMode = ((AFortGameMode*)UWorld::GetWorld()->AuthorityGameMode);
    if (GameMode->HasRedirectAthenaLootTierGroups())
    {
        static auto RedirectAthenaLootTierGroupsOff = GameMode->GetOffset("RedirectAthenaLootTierGroups");

        if (VersionInfo.FortniteVersion >= 20)
        {
            auto& RedirectAthenaLootTierGroups = *(TMap<int32, int32>*)(__int64(GameMode) + RedirectAthenaLootTierGroupsOff);

            for (const auto& [OldTierGroup, RedirectedTierGroup] : RedirectAthenaLootTierGroups)
            {
                if (OldTierGroup == Container->SearchLootTierGroup.ComparisonIndex)
                {
                    RealTierGroup.ComparisonIndex = RedirectedTierGroup;
                    break;
                }
            }
        }
        else
        {
            auto& RedirectAthenaLootTierGroups = *(TMap<FName, FName>*)(__int64(GameMode) + RedirectAthenaLootTierGroupsOff);

            for (const auto& [OldTierGroup, RedirectedTierGroup] : RedirectAthenaLootTierGroups)
            {
                if (OldTierGroup == Container->SearchLootTierGroup)
                {
                    RealTierGroup = RedirectedTierGroup;
                    break;
                }
            }
        }
    }
    else
    {
        static auto Loot_Treasure = FName(L"Loot_Treasure");
        static auto Loot_Ammo = FName(L"Loot_Ammo");
        static auto Loot_AthenaTreasure = FName(L"Loot_AthenaTreasure");
        static auto Loot_AthenaAmmoLarge = FName(L"Loot_AthenaAmmoLarge");

        if (Container->SearchLootTierGroup == Loot_Treasure)
            RealTierGroup = Loot_AthenaTreasure;
        else if (Container->SearchLootTierGroup == Loot_Ammo)
            RealTierGroup = Loot_AthenaAmmoLarge;
    }

    TArray<FFortItemEntry*> LootDrops{};

    UFortLootPackage::ChooseLootForContainer(LootDrops, RealTierGroup, -1, GameMode->GameState->WorldLevel, Container);

    if (VersionInfo.FortniteVersion >= 32 && SpawnLootDiag < 12)
        printf("[Boron][Chest] tier=%s idx=%d groups=%d drops=%d\n", RealTierGroup.ToString().c_str(), RealTierGroup.ComparisonIndex, TierDataMap[RealTierGroup.ComparisonIndex].Num(), LootDrops.Num());

    for (auto& LootDrop : LootDrops)
    {
        AFortInventory::SpawnPickup(Container, *LootDrop);
        free(LootDrop);
    }

    Container->bAlreadySearched = true;
    Container->OnRep_bAlreadySearched();
    Container->SearchBounceData.SearchAnimationCount++;
    Container->BounceContainer();
    // if (Container->bDestroyContainerOnSearch)
    //	Container->K2_DestroyActor();

    return true;
}

void UFortLootPackage::SpawnLoot(FName& TierGroup, FVector Loc)
{
    auto& RealTierGroup = TierGroup;

    auto GameMode = ((AFortGameMode*)UWorld::GetWorld()->AuthorityGameMode);
    if (GameMode->HasRedirectAthenaLootTierGroups())
    {
        for (const auto& [OldTierGroup, RedirectedTierGroup] : GameMode->RedirectAthenaLootTierGroups)
        {
            if (OldTierGroup == TierGroup)
            {
                RealTierGroup = RedirectedTierGroup;
                break;
            }
        }
    }
    else
    {
        static auto Loot_Treasure = FName(L"Loot_Treasure");
        static auto Loot_Ammo = FName(L"Loot_Ammo");
        static auto Loot_AthenaTreasure = FName(L"Loot_AthenaTreasure");
        static auto Loot_AthenaAmmoLarge = FName(L"Loot_AthenaAmmoLarge");

        if (TierGroup == Loot_Treasure)
            RealTierGroup = Loot_AthenaTreasure;
        else if (TierGroup == Loot_Ammo)
            RealTierGroup = Loot_AthenaAmmoLarge;
    }

    TArray<FFortItemEntry*> LootDrops{};

    UFortLootPackage::ChooseLootForContainer(LootDrops, RealTierGroup);

    for (auto& LootDrop : LootDrops)
    {
        AFortInventory::SpawnPickup(Loc, *LootDrop);
        free(LootDrop);
    }
}

bool ServerOnAttemptInteract(ABuildingContainer* BuildingContainer, AFortPlayerPawnAthena*)
{

    if (!BuildingContainer)
        return false;

    if (BuildingContainer->bAlreadySearched)
        return true;

    // SpawnLoot(BuildingContainer->SearchLootTierGroup, BuildingContainer->K2_GetActorLocation() + BuildingContainer->GetActorRightVector() * 70.f + FVector{ 0, 0, 50 });
    UFortLootPackage::SpawnLootHook(BuildingContainer);

    /*BuildingContainer->bAlreadySearched = true;
    BuildingContainer->OnRep_bAlreadySearched();
    BuildingContainer->SearchBounceData.SearchAnimationCount++;
    BuildingContainer->BounceContainer();*/

    return true;
}

void UFortLootPackage::SpawnFloorLootForContainer(const UClass* ContainerType)
{
    TArray<ABuildingContainer*> Containers;
    Utils::GetAll<ABuildingContainer>(ContainerType, Containers);

    for (auto& BuildingContainer : Containers)
    {
        if (VersionInfo.FortniteVersion >= 11.00)
        {
            BuildingContainer->K2_DestroyActor();
        }
        // SpawnLootHook(BuildingContainer);
        else
        {
            SpawnLootHook(BuildingContainer);
            // SpawnLoot(BuildingContainer->SearchLootTierGroup, BuildingContainer->K2_GetActorLocation() + BuildingContainer->GetActorForwardVector() *
            // BuildingContainer->LootSpawnLocation_Athena.X + BuildingContainer->GetActorRightVector() * BuildingContainer->LootSpawnLocation_Athena.Y +
            // BuildingContainer->GetActorUpVector() * BuildingContainer->LootSpawnLocation_Athena.Z);

            if (VersionInfo.FortniteVersion > 3.3)
                BuildingContainer->K2_DestroyActor();
            else
            {
                BuildingContainer->bAlreadySearched = true;
                BuildingContainer->OnRep_bAlreadySearched();
            }
        }
    }

    Containers.Free();
}

void UFortLootPackage::SpawnConsumableActor(ABGAConsumableSpawner* Spawner)
{
    return;
    TArray<FFortItemEntry*> LootDrops{};

    UFortLootPackage::ChooseLootForContainer(LootDrops, Spawner->SpawnLootTierGroup);
    if (LootDrops.Num() == 0)
        return;

    auto ItemDefinition = (UBGAConsumableWrapperItemDefinition*)LootDrops[0]->ItemDefinition;

    auto GroundLoc = UFortKismetLibrary::FindGroundLocationAt(UWorld::GetWorld(), nullptr, Spawner->K2_GetActorLocation(), -1000.f, 2500.f, FName(L"FortDynamicMeshPhysics"));
    auto SpawnTransform = FTransform(GroundLoc, Spawner->K2_GetActorRotation());

    auto Class = ItemDefinition->ConsumableClass.Get();
    if (Class)
        UWorld::SpawnActor(Class, SpawnTransform);

    for (auto& LootDrop : LootDrops)
        free(LootDrop);
}

void (*OnAuthorityRandomUpgradeAppliedOG)(ABuildingContainer*, FName&);
void OnAuthorityRandomUpgradeApplied(ABuildingContainer* Container, FName& UpgradeTierGroup)
{
    if (VersionInfo.FortniteVersion >= 32 || !Container->HasChosenRandomUpgrade()) // 15.10 what
        return OnAuthorityRandomUpgradeAppliedOG(Container, UpgradeTierGroup);

    auto ChosenRandomUpgrade = Container->ChosenRandomUpgrade;

    if (Container->HasAlternateMeshes())
    {
        if (ChosenRandomUpgrade < 0 || ChosenRandomUpgrade >= Container->AlternateMeshes.Num())
            return OnAuthorityRandomUpgradeAppliedOG(Container, UpgradeTierGroup);

        auto& AlternateMeshSet = Container->AlternateMeshes[ChosenRandomUpgrade];

        Container->ReplicatedLootTier = AlternateMeshSet.Tier;
        Container->OnRep_LootTier();
    }
    else
    {
        auto ClassData = Container->GetClassData();
        if (ChosenRandomUpgrade < 0 || ChosenRandomUpgrade >= ClassData->AlternateMeshes.Num())
            return OnAuthorityRandomUpgradeAppliedOG(Container, UpgradeTierGroup);

        auto& AlternateMeshSet = ClassData->AlternateMeshes[ChosenRandomUpgrade];

        Container->ReplicatedLootTier = AlternateMeshSet.Tier;
        Container->OnRep_LootTier();
    }

    return OnAuthorityRandomUpgradeAppliedOG(Container, UpgradeTierGroup);
}

void PostUpdate(ABuildingSMActor* BuildingSMActor)
{
    static auto Chest = FindObject<UClass>(L"/Game/Building/ActorBlueprints/Containers/Tiered_Chest_6_Parent.Tiered_Chest_6_Parent_C");
    static auto AmmoCrate = FindObject<UClass>(L"/Game/Building/ActorBlueprints/Containers/Tiered_Short_Ammo_3_Parent.Tiered_Short_Ammo_3_Parent_C");

    if (auto Container = BuildingSMActor->Cast<ABuildingContainer>())
    {
        if (Container->IsA(Chest) || Container->IsA(AmmoCrate))
            return;

        if (!Container->bStartAlreadySearched_Athena)
        {
            Container->bAlreadySearched = true;
            Container->OnRep_bAlreadySearched();
        }
    }
}

bool bDidntFind = false;
void UFortLootPackage::Hook()
{
    /*if (VersionInfo.FortniteVersion < 3)
    {
        auto PostUpdate_ = Memcury::Scanner::FindStringRef(L"ABuildingSMActor::PostUpdate() Building: %s, AltMeshIdx: %d", false, 0, VersionInfo.FortniteVersion >= 19)
                               .ScanFor({ 0x40, 0x53 }, false)
                               .Get();

        Hooking::Hook(PostUpdate_, PostUpdate);
    }*/

    if (VersionInfo.FortniteVersion >= 11.00)
    {
        if (!FindSpawnLoot())
            bDidntFind = true;
        Hooking::Hook(FindSpawnLoot(), SpawnLootHook);

        auto OnAuthorityRandomUpgradeAppliedAddr = FindFunctionCall(L"OnAuthorityRandomUpgradeApplied", std::vector<uint8_t>{ 0x48, 0x89, 0x5C });
        if (VersionInfo.FortniteVersion >= 32 && OnAuthorityRandomUpgradeAppliedAddr)
            if (auto Owner = FindOwningFunction(OnAuthorityRandomUpgradeAppliedAddr))
                OnAuthorityRandomUpgradeAppliedAddr = Owner;
        Hooking::Hook(OnAuthorityRandomUpgradeAppliedAddr, OnAuthorityRandomUpgradeApplied, OnAuthorityRandomUpgradeAppliedOG);
        return;
    }
    else
    {
        auto ServerOnAttemptInteractRef = Memcury::Scanner::FindStringRef(L"ABuildingContainer::ServerOnAttemptInteract %s failed for %s");

        if (ServerOnAttemptInteractRef.Get())
        {
            auto UnderlyingCall = ServerOnAttemptInteractRef;
            UnderlyingCall.ScanFor({ 0x41, 0xFF }, false);

            if (UnderlyingCall.Get() != ServerOnAttemptInteractRef.Get())
            {
                auto VFTIndex = *(uint32*)(UnderlyingCall.Get() + 3);

                if (VFTIndex != 0)
                {
                    Hooking::Hook<ABuildingContainer>(VFTIndex / 8, ServerOnAttemptInteract);

                    return;
                }
            }
        }
    }

    // if we cant find serveronattemptinteract
    bDidntFind = true;
    return;
}
