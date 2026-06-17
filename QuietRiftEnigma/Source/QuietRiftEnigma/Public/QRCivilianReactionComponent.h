#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "QRWorldGenTypes.h"
#include "QRCivilianReactionComponent.generated.h"

class AQRFactionCamp;
class AActor;


/**
 * Reaction state of a civilian when a raid is incoming or active.
 */
UENUM(BlueprintType)
enum class EQRCivilianMode : uint8
{
	Normal     UMETA(DisplayName = "Normal"),
	AlertCalm  UMETA(DisplayName = "Alerted (calm)"),
	Fight      UMETA(DisplayName = "Fight"),
	Flee       UMETA(DisplayName = "Flee"),
	Hide       UMETA(DisplayName = "Hide"),
};


/**
 * Drop on any friendly AQRNPCActor (player-side villagers, traders,
 * researchers) and they'll react to incoming raid plans:
 *
 *   • High morale + armed + close-to-attack-radius   → Fight
 *   • High morale + unarmed                           → Hide (run to
 *                                                       nearest cover)
 *   • Low morale OR injured                           → Flee
 *
 * Subscribes to every AQRFactionCamp's Sim->OnRaidLaunched event on
 * BeginPlay so any camp's raid plan is evaluated against this
 * civilian's position. If the plan's TargetLocation is within
 * TriggerRadiusCm of me, I react.
 *
 * v1 ships the reaction-mode switch + a simple Flee implementation.
 * Real Fight / Hide AI behavior (combat shooting / running to
 * marked safe rooms) is the next pass.
 */
UCLASS(ClassGroup=(QuietRift), meta=(BlueprintSpawnableComponent))
class QUIETRIFTENIGMA_API UQRCivilianReactionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQRCivilianReactionComponent();

	// How close to me a raid plan must come before I react.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Civilian",
		meta = (ClampMin = "500", ClampMax = "50000"))
	float TriggerRadiusCm = 5000.0f;

	// Morale below this → Flee regardless of armed status.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Civilian",
		meta = (ClampMin = "0", ClampMax = "100"))
	float FleeMoraleThreshold = 35.0f;

	// Health fraction below this → Flee regardless of morale.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Civilian",
		meta = (ClampMin = "0", ClampMax = "1"))
	float FleeHealthThreshold = 0.40f;

	// Movement speed while fleeing (cm/s).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Civilian",
		meta = (ClampMin = "50", ClampMax = "1000"))
	float FleeSpeed = 400.0f;

	// ── Fight mode ───────────────────────────────────────────────
	// Seconds between shots while in Fight. Civilians are not soldiers —
	// slow, deliberate fire.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Civilian|Fight",
		meta = (ClampMin = "0.3", ClampMax = "10"))
	float FireIntervalSeconds = 1.8f;

	// Max engagement range (cm). Beyond this they hold fire and keep
	// facing the threat.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Civilian|Fight",
		meta = (ClampMin = "500", ClampMax = "10000"))
	float EngageRangeCm = 3000.0f;

	// Damage per landed shot. Militia with a real weapon in the hand slot
	// hit ~50% harder (checked at fire time).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Civilian|Fight",
		meta = (ClampMin = "1", ClampMax = "100"))
	float FightDamage = 10.0f;

	// Hit probability per shot — civilians under stress miss a lot.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Civilian|Fight",
		meta = (ClampMin = "0.05", ClampMax = "1"))
	float FightHitChance = 0.55f;

	UPROPERTY(BlueprintReadOnly, Category = "QR|Civilian|State")
	EQRCivilianMode Mode = EQRCivilianMode::Normal;

	UFUNCTION(BlueprintCallable, Category = "QR|Civilian")
	void EnterMode(EQRCivilianMode NewMode, FVector ThreatLocation);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UFUNCTION()
	void HandleRaidLaunched(FQRRaidPlan Plan);

	FVector LastThreatLocation = FVector::ZeroVector;
	float StateTimer = 0.0f;
	float FireCooldown = 0.0f;

	// Nearest live raider (actor carrying UQRRaidPartyAI) within
	// EngageRangeCm, or null. Refreshes LastThreatLocation when found.
	AActor* AcquireFightTarget();
	// One aimed shot at the target: hit roll, then engine damage so the
	// target's TakeDamage/Survival pipeline resolves it.
	void FireAtTarget(AActor* Target);

	bool IsArmed() const;
	float GetMorale() const;
	float GetHealthFraction() const;
};
