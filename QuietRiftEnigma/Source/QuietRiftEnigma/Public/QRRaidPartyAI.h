#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "QRWorldGenTypes.h"
#include "QRRaidPartyAI.generated.h"

class UQRSurvivalComponent;
class AActor;


/**
 * Raid-party AI state. Drives the tick-based FSM in UQRRaidPartyAI.
 *
 *   Marching — walking from spawn toward TargetLocation. Switches to
 *              Engage if a hostile pawn enters PerceptionRadius.
 *   Engage   — attacks the closest valid target. Returns to Marching
 *              when no targets remain.
 *   Retreat  — health below RetreatHealthFrac, head back toward
 *              CampOrigin. Survivors that reach origin "rejoin" their
 *              camp (currently destroyed; rejoin pathway is a follow-up).
 *   Dead     — killed in combat. Tick disabled.
 */
UENUM(BlueprintType)
enum class EQRRaidPartyState : uint8
{
	Marching UMETA(DisplayName = "Marching"),
	Engage   UMETA(DisplayName = "Engaging"),
	Retreat  UMETA(DisplayName = "Retreating"),
	Dead     UMETA(DisplayName = "Dead"),
};


/**
 * Lightweight AI for one raid-party NPC. Drop on any AActor with a
 * UQRSurvivalComponent and the actor will march toward TargetLocation
 * (set by AQRFactionCamp when the raid launches), engage anything it
 * sees, retreat when wounded.
 *
 * Tick-based — no behavior tree asset needed for v1. Real BTs are a
 * follow-on once NavMesh is in place. This component is the bridge
 * between the strategy layer (UQRCampSimComponent decides "send 5
 * soldiers to this location") and physical NPC actors taking that
 * action in the world.
 */
UCLASS(ClassGroup=(QuietRift), meta=(BlueprintSpawnableComponent))
class QUIETRIFTENIGMA_API UQRRaidPartyAI : public UActorComponent
{
	GENERATED_BODY()

public:
	UQRRaidPartyAI();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Raid")
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Raid")
	FVector CampOrigin = FVector::ZeroVector;

	// Source camp id — emitted when this NPC dies so the camp can
	// update its surviving-military count.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Raid")
	FName SourceCampId;

	// March / engage / retreat speeds (cm/s).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Raid|Movement",
		meta = (ClampMin = "50", ClampMax = "1000"))
	float MarchSpeed = 220.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Raid|Movement",
		meta = (ClampMin = "50", ClampMax = "1500"))
	float EngageSpeed = 320.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Raid|Combat",
		meta = (ClampMin = "100", ClampMax = "10000"))
	float PerceptionRadius = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Raid|Combat",
		meta = (ClampMin = "50", ClampMax = "1000"))
	float AttackRadius = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Raid|Combat",
		meta = (ClampMin = "0", ClampMax = "500"))
	float AttackDamage = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Raid|Combat",
		meta = (ClampMin = "0.2", ClampMax = "10"))
	float AttackInterval = 1.5f;

	// Below this fraction of MaxHealth, the NPC retreats.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Raid|Combat",
		meta = (ClampMin = "0", ClampMax = "1"))
	float RetreatHealthFrac = 0.30f;

	UPROPERTY(BlueprintReadOnly, Category = "QR|Raid|State")
	EQRRaidPartyState State = EQRRaidPartyState::Marching;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	TWeakObjectPtr<AActor> CurrentTarget;
	float AttackTimer = 0.0f;
	float StateTimer  = 0.0f;

	void SetState(EQRRaidPartyState NewState);

	bool MoveToward(const FVector& Destination, float Speed, float DeltaTime);
	AActor* ScanForHostile() const;
	bool ShouldRetreat() const;
};
