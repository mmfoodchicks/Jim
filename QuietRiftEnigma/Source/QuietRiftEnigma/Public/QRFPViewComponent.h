#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "QRFPViewComponent.generated.h"

class UCameraComponent;
class ACharacter;

/**
 * First-person camera polish component. Drop on AQRCharacter (or any
 * ACharacter subclass) and point CameraTarget at the FirstPersonCamera.
 * Adds the four bits the base camera doesn't:
 *
 *   1. Sprint FOV punch — FOV widens when sprinting, returns when stopping.
 *   2. ADS interpolation — narrow FOV + camera offset when aiming down sights.
 *   3. Crouch view-height blend — eye height interpolates instead of snapping.
 *   4. Head bob — gentle vertical + lateral camera offset while moving.
 *
 * State inputs the component reads:
 *   - Owner ACharacter velocity (for head bob amplitude)
 *   - Owner's crouch state (ACharacter::bIsCrouched)
 *   - bIsSprinting, polled via reflection so we don't need to hard-link
 *     to AQRCharacter
 *   - SetADS(bAiming) called by weapon code when the player aims
 *
 * The component does NOT modify input bindings or movement speed —
 * AQRCharacter already handles those. This component only paints the
 * camera; the rest of the character is unaffected.
 */
UCLASS(ClassGroup=(QuietRift), meta=(BlueprintSpawnableComponent))
class QUIETRIFTENIGMA_API UQRFPViewComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQRFPViewComponent();

	// ── Tuning ───────────────────────────────
	// Base FOV when standing/walking, not aiming.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FP View",
		meta = (ClampMin = "60", ClampMax = "120"))
	float BaseFOV = 90.0f;

	// FOV while sprinting — small widening sells speed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FP View",
		meta = (ClampMin = "60", ClampMax = "130"))
	float SprintFOV = 100.0f;

	// FOV while aiming down sights — narrower for zoom feel.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FP View",
		meta = (ClampMin = "20", ClampMax = "90"))
	float ADSFOV = 65.0f;

	// How fast FOV interpolates between states (units = lerp speed; ~6 is snappy).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FP View",
		meta = (ClampMin = "0.5", ClampMax = "30"))
	float FOVInterpSpeed = 6.0f;

	// ── Eye height (relative to capsule center, +Z up) ─
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FP View")
	float StandingEyeHeight = 64.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FP View")
	float CrouchedEyeHeight = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FP View",
		meta = (ClampMin = "0.5", ClampMax = "30"))
	float CrouchInterpSpeed = 8.0f;

	// ── Head bob ────────────────────────────
	// Vertical bob amplitude in cm at full sprint speed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FP View|HeadBob",
		meta = (ClampMin = "0", ClampMax = "5"))
	float BobAmplitudeZ = 1.4f;

	// Lateral bob amplitude in cm.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FP View|HeadBob",
		meta = (ClampMin = "0", ClampMax = "5"))
	float BobAmplitudeY = 0.8f;

	// Bob frequency at full sprint speed (Hz).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FP View|HeadBob",
		meta = (ClampMin = "0.5", ClampMax = "8"))
	float BobFrequency = 2.5f;

	// Disable bob entirely (e.g. cinematics).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FP View|HeadBob")
	bool bHeadBobEnabled = true;

	// ── ADS offset ────────────────────────────
	// Local-space camera offset added when aiming. Lets you nudge the
	// camera toward the held weapon's iron sight without moving the mesh.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FP View")
	FVector ADSCameraOffset = FVector(8.0f, 0.0f, -2.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FP View",
		meta = (ClampMin = "0.5", ClampMax = "30"))
	float ADSInterpSpeed = 12.0f;

	// ── Runtime API ──────────────────────────

	// Tell the component which camera to drive. Usually called from BeginPlay
	// after the parent character has constructed its FirstPersonCamera.
	UFUNCTION(BlueprintCallable, Category = "FP View")
	void SetCameraTarget(UCameraComponent* InCamera);

	// Toggle aiming-down-sights state (called by weapon code).
	UFUNCTION(BlueprintCallable, Category = "FP View")
	void SetADS(bool bAiming);

	UFUNCTION(BlueprintPure, Category = "FP View")
	bool IsADS() const { return bIsADS; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                            FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TObjectPtr<UCameraComponent> CameraTarget = nullptr;

	UPROPERTY()
	TObjectPtr<ACharacter> OwnerCharacter = nullptr;

	// Cached default location of the camera relative to its parent — we
	// add ADS offset to this, never overwrite the authored transform.
	FVector BaseCameraRelLocation = FVector::ZeroVector;

	bool bIsADS = false;

	// Internal state for time-driven head bob phase.
	float BobPhase = 0.0f;

	// Read bIsSprinting reflectively so we don't have a hard dep on AQRCharacter.
	bool QueryIsSprinting() const;
};
