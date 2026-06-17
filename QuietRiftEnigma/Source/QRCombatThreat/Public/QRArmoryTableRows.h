#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "QRArmoryTableRows.generated.h"

/**
 * Row structs for the armory DataTables. Column names mirror the CSV
 * headers in /Content/QuietRift/Data/ exactly so UE's CSV importer can
 * map without remapping.
 *
 *   DT_ArmoryAmmo.csv        -> FQRArmoryAmmoRow
 *   DT_ArmoryWeapons.csv     -> FQRArmoryWeaponRow
 *   DT_ArmoryAttachments.csv -> FQRArmoryAttachmentRow
 *   DT_TechNodes.csv         -> FQRTechNodeRow
 *
 * Runtime systems (UQRWeaponComponent::ConfigureForWeaponId etc.) are
 * name-driven for now, but having these rows means designers can author
 * stats in the CSV and the gameplay code can later load them via
 * FindRow<T>() instead of token parsing.
 */

USTRUCT(BlueprintType)
struct QRCOMBATTHREAT_API FQRArmoryAmmoRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName  AmmoId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32  Tier = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Rarity;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float  WeightKgPerRound = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32  StackLimit = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float  AmmoDamageMult = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float  AmmoArmorPierceAdd = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float  NoiseMult = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float  KickMult = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32  Batch = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString CraftedAt;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName  RecipeId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString UnlockReq;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString GuidebookDescription;
};

USTRUCT(BlueprintType)
struct QRCOMBATTHREAT_API FQRArmoryWeaponRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName  WeaponId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName  AmmoId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32  Tier = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Rarity;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float  WeightKg = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float  DurabilityMax = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float  BaseDamage = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float  BaseArmorPierce = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float  RPM = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32  Mag = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float  ReloadS = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float  KickV = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float  KickH = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float  NoiseRadiusM = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float  SpreadMOA = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float  DamageDecayM = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float  MinDmgMult = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString CraftedAt;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString RecipeId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString UnlockReq;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Role;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString GuidebookDescription;
};

USTRUCT(BlueprintType)
struct QRCOMBATTHREAT_API FQRArmoryAttachmentRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName  AttachmentId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Slot;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32  Tier = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Rarity;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float  WeightKg = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float  DurabilityMax = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float  TradeValue = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float  NoiseMult = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float  KickVMult = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float  KickHMult = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float  SpreadMult = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float  MagSizeMult = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float  ReloadSMult = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float  AimTimeAdd = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float  Zoom = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString CraftedAt;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString RecipeId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString UnlockReq;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString GuidebookDescription;
};

USTRUCT(BlueprintType)
struct QRCOMBATTHREAT_API FQRTechNodeRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName  TechNodeId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Tier;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Family;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Prerequisites;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName  RequiredReferenceComponentId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32  ResearchPointsRequired = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString UnlockedRecipeIds;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Notes;
};
