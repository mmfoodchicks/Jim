#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QRTypes.h"
#include "QRRemnantStructure.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRemnantStructureWakeChanged,
	AQRRemnantStructure*, Structure, EQRRemnantWakeState, NewState);

// Base class for all Progenitor (Remnant) structures on Tharsis IV.
//
// The Progenitors (Colonial Authority classified designation: Source Culture Zero / SC-0)
// have no surviving representatives. What remains is automated infrastructure still running
// after thousands of years. These structures are not ruins — they are functional systems
// left in standby, and they respond to the player's Rift research progress.
//
// Wake state progression:
//   Dormant    → safe to explore; basic research yields; Vanguard patrols nearby
//   Stirring   → mild atmospheric effects; better research; Vanguard becomes alert
//   Active     → full data access; signal pulses visible; nearby wildlife disturbed
//   Hostile    → automated defensive systems engage; dangerous to approach without protection
//   Subsiding  → defenses stand down after a cooldown; returns toward Active
//
// Blueprint subclass handles all visual/audio state changes.
UCLASS(Abstract, BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRRemnantStructure : public AActor
{
	GENERATED_BODY()

public:
	AQRRemnantStructure();

	// ── Identity ─────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Remnant")
	FName StructureId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Remnant")
	EQRRemnantStructureType StructureType = EQRRemnantStructureType::DataArchive;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Remnant")
	FText DisplayName;

	// ── Wake State ───────────────────────────
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_WakeState, Category = "Remnant")
	EQRRemnantWakeState CurrentWakeState = EQRRemnantWakeState::Dormant;

	// Rift research progress [0..1] required to transition from Dormant → Stirring.
	// ResonanceChambers need higher thresholds; Signal Spires stir early.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Remnant", meta = (ClampMin = "0", ClampMax = "1"))
	float StirThreshold = 0.25f;

	// Progress required to go Stirring → Active
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Remnant", meta = (ClampMin = "0", ClampMax = "1"))
	float ActiveThreshold = 0.55f;

	// ── Research Yield ────────────────────────
	// Research points granted to the colony's Research component when a player studies this structure.
	// Multiplied by 1.5 when Active, 0.5 when Hostile (partial access through defenses).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Remnant", meta = (ClampMin = "0"))
	float BaseResearchPointsOnStudy = 30.0f;

	// How many seconds of uninterrupted study are required before points are granted.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Remnant", meta = (ClampMin = "1"))
	float StudyDurationSeconds = 8.0f;

	// ── Defensive Systems (Hostile state only) ──
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Remnant")
	bool bHasDefensiveSystems = false;

	// DPS applied to unshielded actors within DefensiveRadius when Hostile
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Remnant", meta = (ClampMin = "0"))
	float DefensiveDamagePerSecond = 8.0f;

	// Radius of the defensive field in world units (cm)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Remnant", meta = (ClampMin = "100"))
	float DefensiveRadiusCm = 1200.0f;

	// Real-seconds the structure stays Hostile before beginning to Subside
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Remnant", meta = (ClampMin = "1"))
	float HostileDurationSeconds = 120.0f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Remnant")
	float HostileTimeRemaining = 0.0f;

	// ── Codex ────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Remnant")
	FName CodexEntryId;

	// ── Events ───────────────────────────────
	UPROPERTY(BlueprintAssignable, Category = "Remnant|Events")
	FOnRemnantStructureWakeChanged OnWakeStateChanged;

	// ── Interface ────────────────────────────
	// Called by the GameMode each time Rift research progress changes.
	// Drives Dormant→Stirring→Active transitions automatically.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Remnant")
	void NotifyRiftResearchProgress(float Progress);

	// Force-advance to the next wake state (cheat/quest use)
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Remnant")
	void AdvanceWakeState();

	// Call with DeltaTime each frame while a player is within study range and interacting.
	// Returns accumulated research points (grants the full BaseResearchPointsOnStudy when done).
	// Returns -1 if currently Hostile and bHasDefensiveSystems is true.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Remnant")
	float TickStudy(float DeltaTime);

	UFUNCTION(BlueprintPure, Category = "Remnant")
	bool CanBeStudied() const { return CurrentWakeState != EQRRemnantWakeState::Hostile || !bHasDefensiveSystems; }

	UFUNCTION(BlueprintPure, Category = "Remnant")
	float GetStudyYield() const;

	virtual void Tick(float DeltaTime) override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Remnant")
	void OnWakeStateEntered(EQRRemnantWakeState NewState);
	virtual void OnWakeStateEntered_Implementation(EQRRemnantWakeState NewState) {}

private:
	float StudyAccumulator = 0.0f;
	bool bStudyComplete = false;

	void SetWakeState(EQRRemnantWakeState NewState);
	void TickDefensiveDamage(float DeltaTime);

	UFUNCTION()
	void OnRep_WakeState();
};
