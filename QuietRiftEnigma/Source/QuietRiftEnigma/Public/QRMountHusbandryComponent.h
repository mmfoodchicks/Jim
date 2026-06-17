#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "QRMountHusbandryComponent.generated.h"

class AActor;
class UQRWeatherComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMountTamed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMountPanicked);


/**
 * Mount husbandry loop (Master GDD §22). Drop on any AQRWildlifeBase
 * subclass intended to be tameable. The loop is three numbers:
 *
 *   BaseTameDays           — game-days of feeding/grooming before the
 *                            animal can be mounted at all.
 *   P_tameFailPerDay       — 0..1 chance per day that an in-progress
 *                            taming session breaks (animal flees, bites,
 *                            resets DaysTamed).
 *   CurrentStressPool      — 0..100. Climbs on rough rides, weather,
 *                            wounds; drops over time with rest. At 85+,
 *                            the next FeedOrPet/Mount attempt triggers
 *                            Panic (buck rider, refuse mount, briefly
 *                            untamed).
 *
 * Designer interaction surface (player or NPC handler):
 *   FeedOrPet()       — daily care; advances DaysTamed, reduces stress.
 *   TickGameHours()   — called from the game-mode tick by the camp /
 *                       wildlife actor that owns the host.
 *   ApplyStress()     — push stress up (combat hit, harsh weather,
 *                       overload, predator nearby).
 *   TryMount(Rider)   — gated by IsTamed AND stress < PanicThreshold.
 *   Panic()           — bucks the rider, sets bIsTamed=false (one-shot
 *                       broken trust), zeros DaysTamed.
 */
UCLASS(ClassGroup=(QuietRift), meta=(BlueprintSpawnableComponent))
class QUIETRIFTENIGMA_API UQRMountHusbandryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQRMountHusbandryComponent();

	// ── Tuning ──────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Mount",
		meta = (ClampMin = "1", ClampMax = "30"))
	float BaseTameDays = 7.0f;

	// Per-day chance an unattended taming session breaks (animal not
	// fed within FeedIntervalHours).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Mount",
		meta = (ClampMin = "0", ClampMax = "1"))
	float P_tameFailPerDay = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Mount",
		meta = (ClampMin = "1", ClampMax = "48"))
	float FeedIntervalHours = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Mount",
		meta = (ClampMin = "50", ClampMax = "100"))
	float PanicThreshold = 85.0f;

	// Stress decay (points per game-hour while not under stressors).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Mount",
		meta = (ClampMin = "0", ClampMax = "20"))
	float StressDecayPerHour = 1.5f;

	// Extra stress per game-hour while a rider is mounted.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Mount",
		meta = (ClampMin = "0", ClampMax = "20"))
	float StressFromRidingPerHour = 3.0f;

	// Stress from active weather event each tick.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Mount",
		meta = (ClampMin = "0", ClampMax = "10"))
	float StressFromWeatherPerHour = 0.8f;

	// ── State ───────────────────────────────────────────────────
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "QR|Mount|State")
	float DaysTamed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "QR|Mount|State")
	bool bIsTamed = false;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "QR|Mount|State",
		meta = (ClampMin = "0", ClampMax = "100"))
	float CurrentStressPool = 0.0f;

	// Game-hours since the last successful Feed/Pet interaction.
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "QR|Mount|State")
	float HoursSinceLastCare = 0.0f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "QR|Mount|State")
	TObjectPtr<AActor> CurrentRider = nullptr;

	UPROPERTY(BlueprintAssignable, Category = "QR|Mount|Events")
	FOnMountTamed OnTamed;

	UPROPERTY(BlueprintAssignable, Category = "QR|Mount|Events")
	FOnMountPanicked OnPanicked;

	// ── API ─────────────────────────────────────────────────────
	UFUNCTION(BlueprintCallable, Category = "QR|Mount")
	void TickGameHours(float DeltaGameHours);

	// Daily care interaction. Resets HoursSinceLastCare, advances
	// DaysTamed (capped at BaseTameDays * 2), and clears 20 stress.
	// Returns true on success — false if a panicking animal refused.
	UFUNCTION(BlueprintCallable, Category = "QR|Mount")
	bool FeedOrPet();

	UFUNCTION(BlueprintCallable, Category = "QR|Mount")
	void ApplyStress(float Amount);

	// Try to mount. Fails when not tamed, when a rider is already up,
	// or when stress is at/above PanicThreshold (calls Panic first).
	UFUNCTION(BlueprintCallable, Category = "QR|Mount")
	bool TryMount(AActor* Rider);

	UFUNCTION(BlueprintCallable, Category = "QR|Mount")
	void Dismount();

	UFUNCTION(BlueprintCallable, Category = "QR|Mount")
	void Panic();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
