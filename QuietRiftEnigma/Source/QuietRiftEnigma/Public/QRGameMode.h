#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "QRTypes.h"
#include "QRSaveTypes.h"
#include "QRGameMode.generated.h"

class UQRColonyStateComponent;
class UQRResearchComponent;
class UQRSaveGameSystem;
class UQRWeatherComponent;
class AQRRaidScheduler;
class AQRVanguardColony;
class AQRCharacter;
class UQRDeathScreenWidget;
class UQRMissionDirector;

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

	// Weather component — lives on the GameState. GameMode drives its time advance each tick.
	UPROPERTY(BlueprintReadOnly, Category = "World")
	TObjectPtr<UQRWeatherComponent> Weather;

	// The hardcoded Vanguard Concordat actor, located in the level by class on BeginPlay.
	// GameMode drives its game-hours clock each tick so its raid cooldowns and hostility
	// decay advance regardless of which Blueprint subclass the level designer used.
	UPROPERTY(BlueprintReadOnly, Category = "Concordat")
	TObjectPtr<AQRVanguardColony> VanguardConcordat;

	// ── Save System ───────────────────────────
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	TObjectPtr<UQRSaveGameSystem> SaveSystem;

	// Mission director — owns the active mission list, rolls templates,
	// listens to inventory / wildlife / codex events for auto-progress.
	UPROPERTY(BlueprintReadOnly, Category = "Missions")
	TObjectPtr<UQRMissionDirector> MissionDirector;

	// Optional class overrides for the world's atmosphere managers.
	// Auto-spawned on BeginPlay if no instance already exists in the
	// level. Set bAutoSpawnAtmosphere = false to skip.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "World|Atmosphere")
	TSubclassOf<class AQRSkyManager> SkyManagerClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "World|Atmosphere")
	TSubclassOf<class AQRWeatherFXManager> WeatherFXManagerClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "World|Atmosphere")
	bool bAutoSpawnAtmosphere = true;

	UPROPERTY(BlueprintReadOnly, Category = "World|Atmosphere")
	TObjectPtr<class AQRSkyManager> SkyManager;

	UPROPERTY(BlueprintReadOnly, Category = "World|Atmosphere")
	TObjectPtr<class AQRWeatherFXManager> WeatherFXManager;

	// ── Session Setup ─────────────────────────
	// Maximum players this session was created for (set by lobby before travel).
	// Drives starting NPC count: solo=3, 2p=2, 3p=1, 4p+=0.
	// Blueprint subclass reads this to spawn the correct number of starting survivors.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Session")
	int32 MaxPlayers = 1;

	// Returns how many NPC survivors should be pre-spawned at session start.
	// Scales inversely with player count so the total effective workforce stays balanced.
	UFUNCTION(BlueprintPure, Category = "Session")
	int32 GetStartingNPCCount() const;

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

	// Autosave interval in real seconds. 0 disables the periodic timer
	// (manual + lifecycle saves still work).
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Save",
		meta = (ClampMin = "0", ClampMax = "3600"))
	float AutosaveIntervalSeconds = 300.0f;  // 5 minutes default

	// Slot the autosave / quicksave / quickload all use.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Save")
	FString AutosaveSlotName = TEXT("QuickSave");

	// Console-callable wrappers so the in-editor "QR.Save" / "QR.Load"
	// commands fire on whichever AQRGameMode is currently authoritative.
	UFUNCTION(Exec, Category = "Save")
	void QR_Save();

	UFUNCTION(Exec, Category = "Save")
	void QR_Load();

	// Apply the most recently loaded save data to a newly-spawned
	// player pawn. Called by AQRCharacter::BeginPlay so vitals and
	// inventory line up after the async load completes.
	UFUNCTION(BlueprintCallable, Category = "Save")
	void ApplyLoadedDataToPlayer(AQRCharacter* Player);

	// Default slot name used by QuickSave / QuickLoad and the autosave
	// on EndPlay. Designer can override per-session.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Save")
	FString AutosaveSlotName = TEXT("QuickSave");

	// ── Death / Respawn ─────────────────────────
	// Called by AQRCharacter::OnDied. Mounts a death screen on the local
	// PC, sets a timer for RespawnDelaySeconds, then RestartPlayer-s.
	UFUNCTION(BlueprintCallable, Category = "Player")
	void HandlePlayerDied(AQRCharacter* DeadPawn);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Player",
		meta = (ClampMin = "0.5", ClampMax = "30"))
	float RespawnDelaySeconds = 3.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player")
	TSubclassOf<UQRDeathScreenWidget> DeathScreenClass;

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
	virtual void Logout(AController* Exiting) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	// Cached save snapshot from the most recent successful load.
	// AQRCharacter peeks at this on its own BeginPlay via
	// ApplyLoadedDataToPlayer so vitals + inventory restore correctly
	// after the async LoadFromSlot completes.
	FQRGameSaveData PendingLoadedData;
	bool            bHasPendingLoadedData = false;

	void HandleLoadComplete(bool bSuccess, const FQRGameSaveData& Data);

	// Driven by AutosaveIntervalSeconds. Set in BeginPlay, cleared in
	// EndPlay; fires QuickSave on each tick.
	FTimerHandle AutosaveTimerHandle;
	void HandleAutosaveTick();
};
