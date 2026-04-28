#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "QRTypes.h"
#include "QRTechNode.generated.h"

// State machine for a single tech unlock
UENUM(BlueprintType)
enum class EQRTechNodeState : uint8
{
	Locked          UMETA(DisplayName = "Locked"),
	ResearchPending UMETA(DisplayName = "Research Pending"),
	AwaitingRefComp UMETA(DisplayName = "Awaiting Ref Component"),
	Unlocked        UMETA(DisplayName = "Unlocked"),
};

// Defines a technology unlock node in the tech tree
UCLASS(BlueprintType, Const)
class QRCRAFTINGRESEARCH_API UQRTechNode : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TechNode")
	FName TechNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TechNode")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TechNode")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TechNode")
	EQRTechTier Tier = EQRTechTier::T0_Primitive;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TechNode")
	EQRResearchFamily Family = EQRResearchFamily::Materials;

	// TechNodes that must be unlocked first
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TechNode")
	TArray<FName> Prerequisites;

	// Physical Reference Component required for final unlock
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TechNode")
	FName RequiredReferenceComponentId;

	// Research points needed (0 = no research phase)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TechNode", meta = (ClampMin = "0"))
	float ResearchPointsRequired = 100.0f;

	// Recipes this node unlocks
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TechNode")
	TArray<FName> UnlockedRecipeIds;

	// Stations this node unlocks for construction
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TechNode")
	TArray<FName> UnlockedStationIds;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("QRTechNode"), TechNodeId);
	}
};

// Runtime state tracking for a tech node (stored in the colony save)
USTRUCT(BlueprintType)
struct QRCRAFTINGRESEARCH_API FQRTechNodeRuntime
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FName TechNodeId;

	UPROPERTY(BlueprintReadOnly)
	EQRTechNodeState State = EQRTechNodeState::Locked;

	UPROPERTY(BlueprintReadOnly)
	float AccumulatedResearchPoints = 0.0f;

	// True if the Reference Component has been physically delivered
	UPROPERTY(BlueprintReadOnly)
	bool bRefComponentDelivered = false;
};
