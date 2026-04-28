#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "QRTypes.h"
#include "QRStationBase.generated.h"

class UQRDepotComponent;
class UQRItemInstance;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStationQueueChanged, int32, QueueSize);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStationOutputReady, FName, ItemId, int32, Quantity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStationBlockerChanged, FText, BlockerReason);

// A station processes queued tasks using inputs from nearby depots
// Blueprint subclasses wire up specific recipes (workbench, forge, kiln, etc.)
UCLASS(Abstract, BlueprintType, Blueprintable)
class QRLOGISTICS_API AQRStationBase : public AActor
{
	GENERATED_BODY()

public:
	AQRStationBase();

	// ── Config ───────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station")
	FGameplayTag StationTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station")
	FText StationDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station")
	EQRPowerQuality RequiredPowerQuality = EQRPowerQuality::None;

	// True if an NPC must be staffing this station
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station")
	bool bRequiresWorker = false;

	// World units radius to search for input depots
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station")
	float PullRadiusCm = 2500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station")
	int32 PullPriority = 0;

	// ── Runtime State ─────────────────────────
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Station")
	bool bIsPowered = false;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Station")
	bool bIsStaffed = false;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Station")
	bool bIsBlocked = false;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Station")
	FText CurrentBlockerReason;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Station")
	float CurrentProgress = 0.0f;   // 0..1 for current task

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Station")
	int32 QueueSize = 0;

	// ── Events ───────────────────────────────
	UPROPERTY(BlueprintAssignable, Category = "Station|Events")
	FOnStationQueueChanged OnQueueChanged;

	UPROPERTY(BlueprintAssignable, Category = "Station|Events")
	FOnStationOutputReady OnOutputReady;

	UPROPERTY(BlueprintAssignable, Category = "Station|Events")
	FOnStationBlockerChanged OnBlockerChanged;

	// ── Blueprint Interface ───────────────────
	UFUNCTION(BlueprintCallable, Category = "Station")
	void SetPowered(bool bNewPowered);

	UFUNCTION(BlueprintCallable, Category = "Station")
	void SetStaffed(bool bNewStaffed);

	UFUNCTION(BlueprintCallable, Category = "Station")
	void SetBlocker(FText Reason);

	UFUNCTION(BlueprintCallable, Category = "Station")
	void ClearBlocker();

	UFUNCTION(BlueprintPure, Category = "Station")
	bool CanOperate() const;

	// Find all depots within pull radius that accept a given category
	UFUNCTION(BlueprintCallable, Category = "Station")
	TArray<UQRDepotComponent*> FindNearbyDepots(FGameplayTag DepotCategory) const;

	// Try to pull N items of ItemId from nearby depots
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Station")
	bool PullFromDepots(FName ItemId, FGameplayTag Category, int32 Quantity, TArray<UQRItemInstance*>& OutItems);

	// ── Overrideable work loop ────────────────
	UFUNCTION(BlueprintNativeEvent, Category = "Station")
	void OnWorkTick(float DeltaTime);
	virtual void OnWorkTick_Implementation(float DeltaTime) {}

	UFUNCTION(BlueprintNativeEvent, Category = "Station")
	void OnTaskCompleted(FName OutputItemId, int32 OutputQuantity);
	virtual void OnTaskCompleted_Implementation(FName OutputItemId, int32 OutputQuantity);

	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
};
