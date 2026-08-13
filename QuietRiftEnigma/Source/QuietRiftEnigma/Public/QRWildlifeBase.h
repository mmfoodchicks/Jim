#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AIController.h"
#include "QRTypes.h"
#include "QRWildlifeBase.generated.h"

class UQRSurvivalComponent;
class UBehaviorTree;
class UAIPerceptionComponent;
class UStaticMeshComponent;
class USkeletalMesh;
class UAnimSequence;

// Drop entry when the animal is harvested/killed
USTRUCT(BlueprintType)
struct QUIETRIFTENIGMA_API FQRWildlifeDrop
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	int32 MinQuantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	int32 MaxQuantity = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0", ClampMax = "1"))
	float DropChance = 1.0f;
};

// Base class for all wildlife — prey, predators, and ambient creatures
UCLASS(Abstract, BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRWildlifeBase : public ACharacter
{
	GENERATED_BODY()

public:
	AQRWildlifeBase();

	// ── Identity ─────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	FName SpeciesId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	FText SpeciesDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	EQRWildlifeBehaviorRole BehaviorRole = EQRWildlifeBehaviorRole::Prey;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	EQRBiomeType PreferredBiome = EQRBiomeType::Forest;

	// ── Stats ─────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	float MaxHealth = 80.0f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Wildlife")
	float CurrentHealth;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	float MoveSpeedWalk = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	float MoveSpeedFlee = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	float MoveSpeedCharge = 500.0f;

	// Detection range for threat awareness
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	float ThreatDetectionRadius = 1500.0f;

	// How loud this animal is (0 = silent; affects player stealth)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	float NoiseFactor = 0.3f;

	// Mass in kg (affects player carry weight of carcass)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	float MassKg = 50.0f;

	// ── Critical-hit zone (headshots / weak spots) ──
	// Local-space (actor-relative, cm) centre of the weak point. If
	// bAutoCritFromBody is true this is recomputed in BeginPlay to the
	// head position (front + up) from the body dims. Armoured species
	// override it to a flank / underbelly in their constructor and set a
	// lower multiplier so the head is NOT the soft spot.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife|Combat")
	FVector CritZoneCenterLocal = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife|Combat", meta = (ClampMin = "5"))
	float CritZoneRadiusCm = 35.0f;

	// Damage multiplier when a shot lands in the crit zone. 2.0 = double.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife|Combat", meta = (ClampMin = "1.0"))
	float CritDamageMultiplier = 2.0f;

	// If true, BeginPlay auto-places CritZoneCenterLocal at the head
	// (front-upper) from the body dims. Turn off (or set the offset in a
	// ctor) for armoured species whose weak point isn't the head.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife|Combat")
	bool bAutoCritFromBody = true;

	// Armoured-head species: the skull/plating is too tough, so the auto
	// crit zone moves to the soft underbelly (lower, slightly rear)
	// instead of the head. Set true in the species constructor.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife|Combat")
	bool bArmoredHead = false;

	// ── Physical size (real-world) ────────────
	// Nose-to-tail length and ground-to-back height, in metres. These
	// drive the collision capsule and (when bAutoFitMeshToBody is true)
	// rescale the skeletal mesh in BeginPlay so the animal renders at its
	// canonical in-game size regardless of the source FBX scale.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife|Size", meta = (ClampMin = "0.1"))
	float BodyLengthMeters = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife|Size", meta = (ClampMin = "0.1"))
	float BodyHeightMeters = 1.0f;

	// If true, BeginPlay scales GetMesh() so its rendered height matches
	// BodyHeightMeters. Turn off for a BP whose mesh is already authored
	// at the correct scale.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife|Size")
	bool bAutoFitMeshToBody = true;

	// If true, BeginPlay drops the skinned body so its ACTUAL lowest
	// vertex rests on the capsule bottom (grounds meshes whose pivot
	// isn't at their feet -- e.g. a hip-pivoted Fab mesh, which is why
	// the German Shepherd looked like it floated). Set FALSE on the
	// floater-archetype species (gas bladders, wisps, swarms) whose
	// canon is to hover above the ground.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife|Size")
	bool bGroundBodyToFeet = true;

	// Optional explicit override for the placeholder mesh. Leave blank to
	// let BeginPlay auto-derive from the class name (e.g. AQRWildlife_
	// AshbackBoar -> /Game/Meshes/wildlife/SM_ANM_AshbackBoar). Only used
	// when no SkeletalMesh is assigned to GetMesh().
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife|Visual")
	FString FallbackMeshPath;

	// ── Skinned body + single-node animations (optional) ─────────
	// When DefaultBodyMesh resolves, BeginPlay assigns it to GetMesh()
	// (unless a designer already picked one) and -- if IdleAnim is also
	// set -- drives Idle/Walk/Run swaps through AnimationSingleNode on a
	// slow timer. Same no-AnimBP pattern the NPC brain uses; species
	// with nothing configured here pay nothing and keep the shape-coded
	// FallbackMesh placeholder.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife|Visual")
	TSoftObjectPtr<USkeletalMesh> DefaultBodyMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife|Visual|Animation")
	TSoftObjectPtr<UAnimSequence> IdleAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife|Visual|Animation")
	TSoftObjectPtr<UAnimSequence> WalkAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife|Visual|Animation")
	TSoftObjectPtr<UAnimSequence> RunAnim;

	// Held (non-looping) when the animal dies. Falls back to the
	// FallbackMesh topple when unset.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife|Visual|Animation")
	TSoftObjectPtr<UAnimSequence> DeathAnim;

	// Speed (cm/s) above which WalkAnim / RunAnim take over.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife|Visual|Animation",
		meta = (ClampMin = "1", ClampMax = "500"))
	float WalkAnimSpeedThreshold = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife|Visual|Animation",
		meta = (ClampMin = "50", ClampMax = "2000"))
	float RunAnimSpeedThreshold = 450.0f;

	// ── Attack (read by AQRWildlifeAIController) ──
	// Damage applied per swing. Predators set this high; prey leave it
	// low (only used if a prey species enters the Attacking state, e.g.
	// a cornered boar). The controller copies these on possession.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife|Combat", meta = (ClampMin = "0.0"))
	float AttackDamage = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife|Combat", meta = (ClampMin = "50.0"))
	float AttackRange = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife|Combat", meta = (ClampMin = "0.25"))
	float AttackIntervalSeconds = 1.5f;

	// ── Behavior Tree ─────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI")
	TObjectPtr<UBehaviorTree> BehaviorTree;

	// ── AI State ─────────────────────────────
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "AI")
	EQRWildlifeAIState AIState = EQRWildlifeAIState::Idle;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "AI")
	bool bIsDead = false;

	// ── Drops ────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drops")
	TArray<FQRWildlifeDrop> DeathDrops;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drops")
	TArray<FQRWildlifeDrop> HarvestDrops;

	// Time before carcass despawns (game-hours)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drops")
	float CarcassDespawnHours = 8.0f;

	// Herd/pack grouping ID (0 = solitary)
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "AI")
	int32 HerdGroupId = 0;

	// ── Interface ────────────────────────────
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Wildlife")
	void TakeDamage_Wildlife(float Amount, AActor* DamageCauser);

	// Bridges the engine damage pipeline (weapon fire, explosions, anything
	// calling AActor::TakeDamage / UGameplayStatics::ApplyDamage) into our
	// custom wildlife health model.
	virtual float TakeDamage(float DamageAmount, const struct FDamageEvent& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Wildlife")
	TArray<FQRWildlifeDrop> Harvest();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Wildlife")
	void SetAIState(EQRWildlifeAIState NewState);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Wildlife")
	void AlertHerd(AActor* Threat);

	UFUNCTION(BlueprintPure, Category = "Wildlife")
	bool IsDead() const { return bIsDead; }

	UFUNCTION(BlueprintPure, Category = "Wildlife")
	float GetHealthPercent() const { return MaxHealth > 0 ? CurrentHealth / MaxHealth : 0.0f; }

	UFUNCTION(BlueprintNativeEvent, Category = "Wildlife")
	void OnDied(AActor* Killer);
	virtual void OnDied_Implementation(AActor* Killer);

	UFUNCTION(BlueprintNativeEvent, Category = "Wildlife")
	void OnThreatDetected(AActor* Threat);
	virtual void OnThreatDetected_Implementation(AActor* Threat) {}

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	// Sizes the capsule from BodyLength/HeightMeters, syncs nav-agent +
	// step height, and (optionally) rescales the mesh to match. Called in
	// BeginPlay on both server and clients so visuals match everywhere.
	void ApplyBodySizing();

	// Placeholder body shown when GetMesh() has no SkeletalMesh assigned
	// (the v15 species classes don't ship a skeletal mesh, so without this
	// a spawned animal is an invisible capsule). An engine basic shape,
	// shape-coded by role: Cube = predator, Cylinder = prey, Sphere =
	// other. Hidden automatically if a real skeletal mesh is present.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wildlife|Visual")
	TObjectPtr<UStaticMeshComponent> FallbackMesh;

	// Generates runtime navmesh tiles around this animal so NavMesh
	// pathing works in the runtime-populated world.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wildlife|AI")
	TObjectPtr<class UNavigationInvokerComponent> NavInvoker;

	// Loads an engine basic-shape mesh into FallbackMesh sized to the body,
	// or hides it if a skeletal mesh is set. Called from BeginPlay.
	void SetupFallbackVisual();

	// Makes the capsule + visible body block ECC_Visibility so weapon
	// traces register hits. Called from BeginPlay (must run after profile
	// registration, otherwise the "Pawn" profile wipes the response).
	void SetupHitCollision();

	// Assigns DefaultBodyMesh into GetMesh() and starts the anim-swap
	// timer when IdleAnim is configured. Runs FIRST in BeginPlay so
	// ApplyBodySizing / SetupFallbackVisual see the real mesh.
	void SetupSkinnedBody();

	// 0.15s timer: velocity-driven Idle/Walk/Run single-node swap.
	void TickAnimSwap();

	FTimerHandle AnimSwapTimer;
	TWeakObjectPtr<UAnimSequence> LastPlayedAnim;
};
