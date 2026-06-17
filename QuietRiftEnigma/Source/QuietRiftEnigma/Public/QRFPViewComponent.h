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
	float ADSFOV = 45.0f;

	// FOV when ADSing with a long-range scope equipped (~4× magnification).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FP View",
		meta = (ClampMin = "5", ClampMax = "60"))
	float ScopeFOV = 20.0f;

	// True while the held weapon carries a scope attachment. Set by
	// AQRCharacter when the active hand-slot item changes. ADS while
	// this is true uses ScopeFOV / ScopeZoomMultiplier + shows the scope
	// overlay widget.
	UPROPERTY(BlueprintReadOnly, Category = "FP View")
	bool bScopeAvailable = false;

	// Magnification multiplier for the currently-equipped scope. 1.0 is
	// the baseline (uses ScopeFOV as-is); a 16× optic narrows further by
	// dividing ScopeFOV by the multiplier above 4× (the v8 patch's 8X
	// and 16X scopes). Set by AQRCharacter when the held weapon or its
	// optic changes.
	UPROPERTY(BlueprintReadOnly, Category = "FP View")
	float ScopeZoomMultiplier = 1.0f;

	UFUNCTION(BlueprintCallable, Category = "FP View")
	void SetScopeAvailable(bool bHasScope);

	UFUNCTION(BlueprintCallable, Category = "FP View")
	void SetScopeZoomMultiplier(float Mult);

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
	float BobAmplitudeZ = 0.9f;

	// Lateral bob amplitude in cm.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FP View|HeadBob",
		meta = (ClampMin = "0", ClampMax = "5"))
	float BobAmplitudeY = 0.5f;

	// Bob frequency at full sprint speed (Hz). Slower than a real footstep
	// rhythm so the camera doesn't read as "twitchy."
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FP View|HeadBob",
		meta = (ClampMin = "0.5", ClampMax = "8"))
	float BobFrequency = 1.4f;

	// Disable bob entirely (e.g. cinematics).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FP View|HeadBob")
	bool bHeadBobEnabled = true;

	// ── ADS offset ────────────────────────────
	// Local-space camera offset added when aiming. The Y/Z move the camera
	// laterally and vertically onto the gun's sight line; X eases it
	// slightly forward to read as "leaning into the scope." Tuned to
	// match AQRCharacter::HeldItemBaseLocation = (38, 9, -14).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FP View")
	FVector ADSCameraOffset = FVector(8.0f, 9.0f, -2.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FP View",
		meta = (ClampMin = "0.5", ClampMax = "30"))
	float ADSInterpSpeed = 12.0f;

	// Mouse-look multiplier while ADS'd. <1 = slower look (the usual FPS
	// feel), 1 = no change. Composes with FOV-zoom turn slowdown.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FP View",
		meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float ADSLookSensitivityMult = 0.5f;

	// ── Lean ─────────────────────────────────
	// Camera roll when fully leaned. Positive value = camera rolls toward
	// the lean side (right lean rolls camera clockwise from player POV).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FP View|Lean",
		meta = (ClampMin = "0", ClampMax = "45"))
	float MaxLeanRollDeg = 16.0f;

	// Lateral camera offset (cm) when fully leaned — lets the player
	// physically peek past cover instead of just rotating the camera.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FP View|Lean",
		meta = (ClampMin = "0", ClampMax = "60"))
	float MaxLeanOffsetY = 22.0f;

	// Forward dip — small forward shift to mimic the player tilting their
	// head out from cover. Mostly cosmetic.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FP View|Lean",
		meta = (ClampMin = "0", ClampMax = "10"))
	float MaxLeanOffsetX = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FP View|Lean",
		meta = (ClampMin = "0.5", ClampMax = "30"))
	float LeanInterpSpeed = 10.0f;

	// If true, a sideways trace from the camera detects walls and caps the
	// effective lean so the camera doesn't clip through geometry.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FP View|Lean")
	bool bClampLeanAgainstWalls = true;

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

	// Set the desired lean direction: -1 = left, 0 = center, +1 = right.
	// Held-key handlers should call this with +/-1 on press and 0 on release.
	UFUNCTION(BlueprintCallable, Category = "FP View|Lean")
	void SetLeanInput(float Direction);

	UFUNCTION(BlueprintPure, Category = "FP View|Lean")
	float GetCurrentLean() const { return CurrentLean; }

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

	// Target lean (-1..+1) set by input; CurrentLean is interpolated toward it.
	float LeanInput = 0.0f;
	float CurrentLean = 0.0f;

	// Wall-clamp smoothing state (anti-shake) -- see TickComponent.
	float SmoothedLeanClamp = 0.0f;

	// Read bIsSprinting reflectively so we don't have a hard dep on AQRCharacter.
	bool QueryIsSprinting() const;

	// Cast a short trace from the camera in the lean direction and return
	// the clamped lean magnitude (1.0 = full lean allowed, 0.0 = blocked).
	float ComputeLeanWallClamp(float DesiredLean) const;
};
