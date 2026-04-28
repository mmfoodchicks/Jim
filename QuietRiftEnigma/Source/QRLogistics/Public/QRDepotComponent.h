#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "QRTypes.h"
#include "QRDepotComponent.generated.h"

class UQRItemInstance;
class UQRItemDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDepotChanged);

// A physical depot that stores items of specific categories
// NPCs haul items to/from depots; stations pull from nearby depots
UCLASS(ClassGroup=(QuietRift), meta=(BlueprintSpawnableComponent))
class QRLOGISTICS_API UQRDepotComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQRDepotComponent();

	// ── Config ───────────────────────────────
	// Which depot category this accepts (e.g. Depot.Category.Wood)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Depot")
	FGameplayTag AcceptedCategory;

	// Pull radius in world units (meters * 100 for UE)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Depot", meta = (ClampMin = "100"))
	float PullRadiusCm = 2500.0f;  // 25m default

	// Priority when multiple depots of same category exist (higher = preferred)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Depot")
	int32 PullPriority = 0;

	// Max number of items this depot can hold
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Depot", meta = (ClampMin = "1"))
	int32 MaxCapacity = 200;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Depot")
	FText DepotDisplayName = FText::FromString("Depot");

	// ── State ────────────────────────────────
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Depot")
	TArray<TObjectPtr<UQRItemInstance>> StoredItems;

	UPROPERTY(BlueprintAssignable, Category = "Depot|Events")
	FOnDepotChanged OnDepotChanged;

	// ── Transactions ─────────────────────────
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Depot")
	bool DepositItem(UQRItemInstance* Item);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Depot")
	UQRItemInstance* WithdrawItem(FName ItemId, int32 Quantity);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Depot")
	bool CanAccept(const UQRItemDefinition* ItemDef) const;

	// ── Queries ──────────────────────────────
	UFUNCTION(BlueprintPure, Category = "Depot")
	int32 CountItem(FName ItemId) const;

	UFUNCTION(BlueprintPure, Category = "Depot")
	int32 GetTotalStoredCount() const;

	UFUNCTION(BlueprintPure, Category = "Depot")
	bool IsFull() const;

	UFUNCTION(BlueprintPure, Category = "Depot")
	bool IsWithinPullRadius(const AActor* StationActor) const;

	UFUNCTION(BlueprintPure, Category = "Depot")
	TArray<UQRItemInstance*> GetAllItems() const { return StoredItems; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
