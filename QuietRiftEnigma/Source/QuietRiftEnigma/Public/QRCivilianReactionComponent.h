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

	bool IsArmed() const;
	float GetMorale() const;
	float GetHealthFraction() const;
};
