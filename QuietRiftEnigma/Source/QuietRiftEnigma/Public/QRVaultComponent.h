#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "QRVaultComponent.generated.h"

class ACharacter;
class UCapsuleComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnVaultStarted);

/**
 * Drop on AQRCharacter. The character's jump handler calls TryVault();
 * if it returns true a vault has been launched and the normal Jump is
 * skipped, otherwise the character falls through to ACharacter::Jump().
 *
 * Detection (all distances in cm):
 *   1. Trace forward from chest height for VaultForwardReach. If nothing
 *      is hit close in front, no vault.
 *   2. From the forward hit point, trace straight down starting from
 *      VaultMaxObstacleHeight above the player's feet. The hit's Z gives
 *      the obstacle top.
 *   3. Reject if the obstacle is shorter than VaultMinObstacleHeight
 *      (just step over it) or taller than VaultMaxObstacleHeight
 *      (can't reach the top from here).
 *   4. Capsule sweep at the landing spot (a short distance past the
 *      forward face, on top of the obstacle) to confirm the player fits.
 *   5. If everything passes, LaunchCharacter with an up + forward kick.
 *
 * Networking: LaunchCharacter goes through CharacterMovement's standard
 * replicated move buffer, so calling it on the autonomous proxy is safe.
 * No custom RPC needed at this scope.
 */
UCLASS(ClassGroup=(QuietRift), meta=(BlueprintSpawnableComponent))
class QUIETRIFTENIGMA_API UQRVaultComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQRVaultComponent();

	// ── Tuning ───────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vault",
		meta = (ClampMin = "30", ClampMax = "200"))
	float VaultForwardReach = 90.0f;

	// Obstacles shorter than this are walked over, not vaulted.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vault",
		meta = (ClampMin = "10", ClampMax = "100"))
	float VaultMinObstacleHeight = 35.0f;

	// Tallest obstacle the player can reach the top of (cm above feet).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vault",
		meta = (ClampMin = "60", ClampMax = "220"))
	float VaultMaxObstacleHeight = 140.0f;

	// Required vertical clearance above the obstacle top so the player's
	// capsule fits when landing. Roughly capsule full height.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vault",
		meta = (ClampMin = "50", ClampMax = "250"))
	float VaultRequiredClearance = 100.0f;

	// Horizontal landing offset past the obstacle face.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vault",
		meta = (ClampMin = "20", ClampMax = "200"))
	float VaultLandOffsetForward = 60.0f;

	// Velocity applied to the character on a successful vault.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vault",
		meta = (ClampMin = "100", ClampMax = "1200"))
	float VaultLaunchUp = 480.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vault",
		meta = (ClampMin = "100", ClampMax = "1200"))
	float VaultLaunchForward = 420.0f;

	// Lockout after a vault so the player can't trigger again mid-air.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vault",
		meta = (ClampMin = "0", ClampMax = "3"))
	float VaultCooldown = 0.45f;

	// Block vaulting while sprinting? Most games allow sprint-vaulting; default off.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vault")
	bool bBlockWhileSprinting = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vault")
	bool bDrawDebug = false;

	// ── Events ───────────────────────────────
	UPROPERTY(BlueprintAssignable, Category = "Vault|Events")
	FOnVaultStarted OnVaultStarted;

	// ── API ──────────────────────────────────
	// Returns true if a vault was started this call (caller should then
	// skip the normal Jump path). Returns false otherwise.
	UFUNCTION(BlueprintCallable, Category = "Vault")
	bool TryVault();

	UFUNCTION(BlueprintPure, Category = "Vault")
	bool IsOnCooldown() const;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TObjectPtr<ACharacter> OwnerCharacter = nullptr;

	float LastVaultTimeSeconds = -1000.0f;

	// Run all detection traces; if everything passes, fill OutLaunchOrigin
	// with the launch location for debug and return true.
	bool DetectVault(FVector& OutLandSpot) const;
};
