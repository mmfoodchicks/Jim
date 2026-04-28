#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "QRTypes.h"
#include "QRGameMode.generated.h"

class UQRColonyStateComponent;
class UQRResearchComponent;
class UQRSaveGameSystem;
class AQRRaidScheduler;

// Main game mode — controls session start, tutorial unlock flow, and ending resolution
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AQRGameMode();

	// ── World Time ────────────────────────────
	UPROPERTY(BlueprintReadOnly, Category = "World")
	float WorldTimeSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "World")
	int32 DayNumber = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
	float DayLengthRealSeconds = 1200.0f;  // 20 real-minutes per game-day

	UPROPERTY(BlueprintReadOnly, Category = "World")
	bool bIsNight = false;

	// ── Mission Tracking ─────────────────────
	UPROPERTY(BlueprintReadOnly, Category = "Missions")
	TArray<FName> CompletedMissionIds;

	UPROPERTY(BlueprintReadOnly, Category = "Missions")
	TArray<FName> ActiveMissionIds;

	// ── Components on GameState ───────────────
	UPROPERTY(BlueprintReadOnly, Category = "Colony")
	TObjectPtr<UQRColonyStateComponent> ColonyState;

	UPROPERTY(BlueprintReadOnly, Category = "Research")
	TObjectPtr<UQRResearchComponent> Research;

	// ── Save System ───────────────────────────
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	TObjectPtr<UQRSaveGameSystem> SaveSystem;

	// ── Interface ────────────────────────────
	UFUNCTION(BlueprintCallable, Category = "World")
	float GetDayProgress() const;

	UFUNCTION(BlueprintCallable, Category = "World")
	bool IsCurrentlyNight() const { return bIsNight; }

	UFUNCTION(BlueprintCallable, Category = "Missions")
	void CompleteMission(FName MissionId);

	UFUNCTION(BlueprintCallable, Category = "Missions")
	void ActivateMission(FName MissionId);

	UFUNCTION(BlueprintPure, Category = "Missions")
	bool IsMissionComplete(FName MissionId) const;

	UFUNCTION(BlueprintCallable, Category = "Save")
	void QuickSave();

	UFUNCTION(BlueprintCallable, Category = "Save")
	void QuickLoad();

	UFUNCTION(BlueprintNativeEvent, Category = "World")
	void OnDayStarted(int32 NewDay);
	virtual void OnDayStarted_Implementation(int32 NewDay) {}

	UFUNCTION(BlueprintNativeEvent, Category = "World")
	void OnNightStarted();
	virtual void OnNightStarted_Implementation() {}

	UFUNCTION(BlueprintNativeEvent, Category = "Missions")
	void OnMissionCompleted(FName MissionId);
	virtual void OnMissionCompleted_Implementation(FName MissionId) {}

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
};
