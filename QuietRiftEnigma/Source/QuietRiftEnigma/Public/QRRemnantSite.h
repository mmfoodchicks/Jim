#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QRWorldGenTypes.h"
#include "QRRemnantSite.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class UPointLightComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRemnantWakeStateChanged,
	EQRRemnantWakeState, NewState);

/**
 * Source Culture Zero (SC-0) infrastructure — a Remnant site placed
 * by the worldgen pipeline. Drives the five-state wake FSM from
 * Master GDD §13:
 *
 *   Dormant   — quiet, safe to approach, basic research yield
 *   Stirring  — mild atmospheric FX, slightly better yield, alerts patrols
 *   Active    — full data access, stronger pulses
 *   Hostile   — automated defenses engage, area takes damage
 *   Subsiding — returning toward Active
 *
 * Wake state escalates by external trigger — usually the player's
 * Rift research progress crosses a threshold, or a player physically
 * enters the proximity radius while research is at certain tiers.
 * For v1 we expose explicit SetWakeState calls; UQRResearchComponent
 * will broadcast events that bump the state in a later pass.
 *
 * The Kind field decides which of the four canonical structure types
 * this is (Signal Spire / Power Core / Data Archive / Resonance
 * Chamber). Designer picks a mesh per kind in BP; default constructor
 * uses a placeholder.
 */
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRRemnantSite : public AActor
{
	GENERATED_BODY()

public:
	AQRRemnantSite();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QR|Remnant")
	TObjectPtr<USphereComponent> ProximitySphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QR|Remnant")
	TObjectPtr<UStaticMeshComponent> StructureMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QR|Remnant")
	TObjectPtr<UPointLightComponent> PulseLight;

	// What kind of remnant this is. Drives mesh + yield + escalation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Remnant")
	EQRRemnantKind Kind = EQRRemnantKind::SignalSpire;

	// Current wake state (replicated so all clients see pulses).
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_WakeState, Category = "QR|Remnant")
	EQRRemnantWakeState WakeState = EQRRemnantWakeState::Dormant;

	// Hostile-state damage dealt to any actor inside the sphere per
	// second. Multiplied by overlap dwell time so a quick pass deals
	// less than camping near the structure.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Remnant",
		meta = (ClampMin = "0", ClampMax = "500"))
	float HostileDamagePerSecond = 25.0f;

	// Auto-subside timer: once Hostile state has been active for this
	// many seconds, the FSM falls back to Subsiding. 0 = no auto.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Remnant",
		meta = (ClampMin = "0", ClampMax = "600"))
	float HostileAutoSubsideSeconds = 30.0f;

	// Research yield multiplier per wake state. Higher state → higher
	// yield until Hostile, then it drops and risk spikes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Remnant")
	TMap<EQRRemnantWakeState, float> ResearchYieldByState;

	// Event for UI / VFX / faction alert subscribers.
	UPROPERTY(BlueprintAssignable, Category = "QR|Remnant|Events")
	FOnRemnantWakeStateChanged OnWakeStateChanged;

	// Push the state. Server-only — clients see it via OnRep.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "QR|Remnant")
	void SetWakeState(EQRRemnantWakeState NewState);

	UFUNCTION(BlueprintPure, Category = "QR|Remnant")
	float CurrentResearchYieldScalar() const;

	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_WakeState();

	UFUNCTION()
	void HandleSphereBeginOverlap(UPrimitiveComponent* Overlapped,
		AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);

	UFUNCTION()
	void HandleSphereEndOverlap(UPrimitiveComponent* Overlapped,
		AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	// Apply the visual treatment matching the current wake state
	// (light pulse rate / color / intensity).
	void ApplyVisualForState();

private:
	float HostileTimer = 0.0f;
	TSet<TWeakObjectPtr<AActor>> OverlapsInside;
};
