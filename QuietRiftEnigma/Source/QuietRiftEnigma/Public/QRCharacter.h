#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "QRTypes.h"
#include "QRCharacter.generated.h"

class UQRInventoryComponent;
class UQRSurvivalComponent;
class UQRWeaponComponent;
class UQRFactionComponent;
class UCameraComponent;
class USpringArmComponent;
class UInputMappingContext;
class UInputAction;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteract, AActor*, Target);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCrouchToggled);

// The player character — first-person, with all survival/inventory components wired
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AQRCharacter();

	// ── Components ───────────────────────────
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UQRInventoryComponent> Inventory;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UQRSurvivalComponent> Survival;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UQRWeaponComponent> Weapon;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UQRFactionComponent> Faction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	// First-person arm mesh
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visuals")
	TObjectPtr<USkeletalMeshComponent> ArmsMesh;

	// ── Input ─────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> CrouchAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> FireAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ReloadAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> InventoryAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> SprintAction;

	// ── Config ───────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float WalkSpeed = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float SprintSpeed = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float CrouchSpeed = 150.0f;

	// Interaction ray distance (cm)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	float InteractDistance = 250.0f;

	// ── State ────────────────────────────────
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Character")
	bool bIsSprinting = false;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Character")
	bool bIsOverEncumbered = false;

	// Unique survivor ID for save/load tracking
	UPROPERTY(BlueprintReadOnly, Category = "Character")
	FName SurvivorId;

	// Player identity — name, pronouns, voice profile. Drives dialogue
	// substitution via UQRPronounLibrary so NPCs greet the player by the
	// correct name + pronouns. Set at character creation; replicated so
	// clients see the same identity the server uses for dialogue subs.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Character|Identity")
	FQRPlayerIdentity PlayerIdentity;

	// ── Events ───────────────────────────────
	UPROPERTY(BlueprintAssignable, Category = "Character|Events")
	FOnInteract OnInteract;

	// ── Interface ────────────────────────────
	UFUNCTION(BlueprintCallable, Category = "Character")
	void TryInteract();

	// Fire the equipped weapon along the camera forward vector. Local
	// caller (input event); routes through Server_Fire on a client.
	UFUNCTION(BlueprintCallable, Category = "Character")
	void TryFireWeapon();

	UFUNCTION(Server, Reliable)
	void Server_Fire(FVector TraceStart, FVector TraceForward, bool bIsAimed, bool bIsMoving);

	// Begin reloading the equipped weapon.
	UFUNCTION(BlueprintCallable, Category = "Character")
	void TryReload();

	UFUNCTION(Server, Reliable)
	void Server_Reload();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Character")
	void SetSprinting(bool bSprint);

	UFUNCTION(BlueprintPure, Category = "Character")
	bool CanSprint() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Character")
	void OnInteractableFound(AActor* Interactable);
	virtual void OnInteractableFound_Implementation(AActor* Interactable) {}

	UFUNCTION(BlueprintNativeEvent, Category = "Character")
	void OnDied();
	virtual void OnDied_Implementation();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION(Server, Reliable)
	void Server_Interact(AActor* Target);
	void Move(const struct FInputActionValue& Value);
	void Look(const struct FInputActionValue& Value);
	void StartSprint();
	void StopSprint();

	void ScanForInteractable();
	TWeakObjectPtr<AActor> CurrentInteractable;
};
