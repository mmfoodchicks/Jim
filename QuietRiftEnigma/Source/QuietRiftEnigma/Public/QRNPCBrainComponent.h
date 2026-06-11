#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "QRNPCBrainComponent.generated.h"

class AActor;
class UQRCivilianReactionComponent;


/**
 * One civilian's living brain. Drop on any AActor (typically
 * AQRNPCActor) and the actor stops being a statue:
 *
 *   Idle       — stand at HomePosition for a moment, glance around.
 *   Wander     — pick a random nav-rough point within WanderRadius and
 *                walk there at WalkSpeed.
 *   Work       — head to AssignedWorkPost during work hours; stand
 *                there as long as it's day.
 *   Sleep      — head to AssignedBed at night and stay put.
 *   Socialize  — walk toward the nearest other brain-carrying NPC.
 *   Reacting   — UQRCivilianReactionComponent owns the body (Flee /
 *                Fight / Hide). Brain idles in that case.
 *
 * Two-Hz think, direct-interp movement (no NavMesh dep), idempotent
 * across save/load. Same code-only FSM treatment wildlife uses, so
 * the dev map's NPCs walk around and look populated immediately
 * without per-NPC behavior tree authoring.
 */
UENUM(BlueprintType)
enum class EQRNPCBrainState : uint8
{
	Idle,
	Wander,
	Work,
	Sleep,
	Socialize,
	Reacting,
};


UCLASS(ClassGroup=(QuietRift), meta=(BlueprintSpawnableComponent))
class QUIETRIFTENIGMA_API UQRNPCBrainComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQRNPCBrainComponent();

	// ── Movement ──────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|NPC|Brain",
		meta = (ClampMin = "20", ClampMax = "600"))
	float WalkSpeed = 130.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|NPC|Brain",
		meta = (ClampMin = "100", ClampMax = "20000"))
	float WanderRadius = 1500.0f;

	// Threshold (cm) considered "arrived" at a target point.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|NPC|Brain",
		meta = (ClampMin = "20", ClampMax = "500"))
	float ArriveRadius = 80.0f;

	// ── Schedule ──────────────────────────────────────────────────
	// Day fraction [0..1] where work starts / ends. Sleep occupies the
	// rest of the night past WorkEnd. Defaults: work 0.30 -> 0.70.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|NPC|Brain|Schedule",
		meta = (ClampMin = "0", ClampMax = "1"))
	float WorkStartFrac = 0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|NPC|Brain|Schedule",
		meta = (ClampMin = "0", ClampMax = "1"))
	float WorkEndFrac = 0.70f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|NPC|Brain|Schedule")
	FVector AssignedWorkPost = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|NPC|Brain|Schedule")
	FVector AssignedBed = FVector::ZeroVector;

	// Drop both pegged at the actor's spawn location -- a brand new NPC
	// with no designer wiring still wanders around its spawn point
	// instead of standing in the void.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|NPC|Brain|Schedule")
	bool bUseSpawnLocationAsHome = true;

	// ── Socialization ─────────────────────────────────────────────
	// Probability per think tick (2 Hz) of choosing Socialize over
	// Wander while idle in daytime. Higher = chattier village vibe.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|NPC|Brain|Social",
		meta = (ClampMin = "0", ClampMax = "1"))
	float SocializeChance = 0.08f;

	// Max range (cm) the NPC will look for a chat partner.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|NPC|Brain|Social",
		meta = (ClampMin = "200", ClampMax = "5000"))
	float SocialRangeCm = 1500.0f;

	// ── State (read-only at runtime) ──────────────────────────────
	UPROPERTY(BlueprintReadOnly, Category = "QR|NPC|Brain|State")
	EQRNPCBrainState State = EQRNPCBrainState::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "QR|NPC|Brain|State")
	FVector HomePosition = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "QR|NPC|Brain|State")
	FVector CurrentTarget = FVector::ZeroVector;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	// 2 Hz "what should I be doing now" planner. Cheap.
	void Think();

	// Per-frame interpolation toward CurrentTarget at WalkSpeed.
	bool StepTowardTarget(float DeltaSeconds);

	bool IsWorkHour() const;
	bool IsNightHour() const;

	FVector PickWanderPoint() const;
	AActor* FindSocialPartner() const;

	// Cache the reaction component once; it OWNS the body while in
	// Flee/Fight/Hide so the brain just yields to it.
	UPROPERTY()
	TWeakObjectPtr<UQRCivilianReactionComponent> Reaction;

	float ThinkAccumulator = 0.0f;
	float StateTimer = 0.0f;
};
