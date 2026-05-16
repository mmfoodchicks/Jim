#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "QRTypes.h"
#include "QRCharacter.generated.h"

class UQRInventoryComponent;
class UQRSurvivalComponent;
class UQRWeaponComponent;
class UQRFactionComponent;
class UQRFPViewComponent;
class UQRVaultComponent;
class UQRHotbarComponent;
class UQRBuildModeComponent;
class UCameraComponent;
class UStaticMeshComponent;
class USpringArmComponent;
class UInputMappingContext;
class UInputAction;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteract, AActor*, Target);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCrouchToggled);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCreativeBrowserToggled);

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UQRVaultComponent> Vault;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UQRHotbarComponent> Hotbar;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UQRBuildModeComponent> Build;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	// First-person arm mesh
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visuals")
	TObjectPtr<USkeletalMeshComponent> ArmsMesh;

	// Mesh shown when the player is holding an item. Attaches to ArmsMesh
	// at SOCKET_GripPoint if that socket exists, otherwise to the arms root.
	// Driven by Hotbar's active slot → inventory HandSlot → this mesh's
	// static mesh is set from the item definition's WorldMesh.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visuals")
	TObjectPtr<UStaticMeshComponent> HeldItemMesh;

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

	// I — toggles full inventory grid overlay.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> InventoryAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LeanLeftAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LeanRightAction;

	// Drop the currently-held hotbar item. Bind to G (or whatever — must NOT
	// be Q/E, those are lean).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> DropAction;

	// Toggle the creative item-browser widget. Bind to Tab. The character
	// fires OnCreativeBrowserToggled; the WBP_CreativeBrowser widget listens
	// and shows/hides itself.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> CreativeBrowserAction;

	// One action per slot 1..9. The character binds each with the slot
	// index as a payload so a single OnHotbarSlotInput handler routes them.
	// Leave entries null to skip wiring a particular slot in C++ (BP can
	// still drive Hotbar->SelectSlot directly).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TArray<TObjectPtr<UInputAction>> HotbarSlotActions;

	// Mouse-wheel cycle for the hotbar (optional, recommended).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> HotbarNextAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> HotbarPrevAction;

	// Right-mouse "use held item" — dispatches by item category:
	//   Weapon   → SetADS on press, clear ADS on release
	//   Food     → ConsumeFood (one-shot, press only)
	//   Medicine → ApplyHealing (one-shot)
	//   Other    → no-op
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> UseHeldAction;

	// Esc — toggles pause menu overlay.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> PauseAction;

	// K — toggles the Codex (discovery / lore) screen.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> CodexAction;

	// Class spawned by Drop when the held item's category is Wildlife.
	// Defaults to AQRWildlifeActor (static-mesh wanderer).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Items")
	TSubclassOf<class AQRWildlifeActor> WildlifeActorClass;

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

	// Fires every time the Tab/CreativeBrowserAction key is pressed. The
	// browser widget itself owns the open/closed state — it just toggles
	// on this event.
	UPROPERTY(BlueprintAssignable, Category = "Character|Events")
	FOnCreativeBrowserToggled OnCreativeBrowserToggled;

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

	// Drop the currently-equipped hotbar item into the world. Dispatches
	// by category: Wildlife → spawn wandering actor; building-prefixed
	// (BLD_) → enter build mode with that piece selected; else → drop
	// as world item.
	UFUNCTION(BlueprintCallable, Category = "Character")
	void TryDropHeld();

	UFUNCTION(Server, Reliable)
	void Server_DropHeld();

	// "Use" the held item via right-mouse. Press/release pair lets
	// weapons hold-to-aim while consumables fire once on press.
	UFUNCTION(BlueprintCallable, Category = "Character")
	void TryUseHeld(bool bPressed);

	UFUNCTION(Server, Reliable)
	void Server_UseHeld(bool bPressed);

	// Widget classes spawned for the local player on BeginPlay. Default to
	// the C++ HUD / browser; override in BP to swap in a designer-authored
	// WBP_ version.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class UQRHotbarHUDWidget> HotbarHUDClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class UQRCreativeBrowserWidget> CreativeBrowserClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class UQRVitalsHUDWidget> VitalsHUDClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class UQRPauseMenuWidget> PauseMenuClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class UQRSettingsWidget> SettingsWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class UQRCraftingWidget> CraftingWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class UQRDialogueWidget> DialogueWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class UQRBuildPieceSelectorWidget> BuildPieceSelectorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class UQRInventoryGridWidget> InventoryGridClass;

	UPROPERTY()
	TObjectPtr<class UQRInventoryGridWidget> InventoryGrid = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class UQRCodexWidget> CodexWidgetClass;

	UPROPERTY()
	TObjectPtr<class UQRCodexWidget> CodexWidget = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class UQRScopeOverlayWidget> ScopeOverlayClass;

	UPROPERTY()
	TObjectPtr<class UQRScopeOverlayWidget> ScopeOverlay = nullptr;

	// Opens the settings overlay. Routed through ConsoleCommand from
	// the pause / main menu widgets so they don't take a direct C++
	// dep on the character.
	UFUNCTION(Exec, Category = "UI")
	void QR_OpenSettings();

	// Advances a codex entry to the Researched state. Called when the
	// player completes a "study item" recipe at a workbench, or for
	// testing via the in-game console (Tilde → "QR_StudyItem PLT_LATTICE_BULB").
	UFUNCTION(Exec, BlueprintCallable, Category = "QR|Codex")
	void QR_StudyItem(FName Id);

	UPROPERTY()
	TObjectPtr<class UQRHotbarHUDWidget> HotbarHUD = nullptr;

	UPROPERTY()
	TObjectPtr<class UQRCreativeBrowserWidget> CreativeBrowser = nullptr;

	UPROPERTY()
	TObjectPtr<class UQRVitalsHUDWidget> VitalsHUD = nullptr;

	UPROPERTY()
	TObjectPtr<class UQRPauseMenuWidget> PauseMenu = nullptr;

	// ── Footsteps ────────────────────────────
	// Speed threshold (cm/s) above which a step is allowed to play.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Audio|Footsteps")
	float FootstepSpeedThreshold = 50.0f;

	// Seconds between footsteps at normal walk speed. Sprinting scales
	// this down proportionally so steps cadence with leg movement.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Audio|Footsteps")
	float FootstepWalkInterval = 0.45f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Audio|Footsteps")
	float FootstepSprintInterval = 0.30f;

	// Played at 100% volume sprinting / scaled down when walking.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Audio|Footsteps")
	float FootstepVolumeMult = 0.7f;

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
	void HandleJumpPressed();
	void HandleJumpReleased();
	void LeanLeftPressed();
	void LeanLeftReleased();
	void LeanRightPressed();
	void LeanRightReleased();
	void OnDropPressed();
	void OnCreativeBrowserPressed();
	void OnUseHeldPressed();
	void OnUseHeldReleased();
	void OnPausePressed();
	void OnInventoryPressed();
	void OnCodexPressed();

	// Per-category drop dispatch; runs on the server.
	void DoDropHeld();
	// Per-category use dispatch; runs on the server.
	void DoUseHeld(bool bPressed);
	// Tracks whether the current RMB hold started an ADS so release can clear it.
	bool bUseStartedADS = false;

	// Time until next footstep (counts down each Tick on locally-controlled,
	// grounded, moving characters).
	float FootstepTimer = 0.0f;

	// ── Biome ambient ─────────────────────────
	// Played as a looping ambient layer. Swapped when the player moves
	// between cells with different biomes (or enters/exits an
	// AQRBiomeZone). Lives on the character so it follows the listener
	// without needing world placement.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio|Biome")
	TObjectPtr<class UAudioComponent> BiomeAmbient = nullptr;

	UFUNCTION(BlueprintCallable, Category = "QR|Biome")
	void OnBiomeZoneEnter(class UQRBiomeProfile* Profile, int32 Priority);

	UFUNCTION(BlueprintCallable, BlueprintCallable, Category = "QR|Biome")
	void OnBiomeZoneExit(class UQRBiomeProfile* Profile, int32 Priority);

	// Last biome name we activated audio for, so we only swap when it
	// actually changes.
	FName ActiveBiomeName;

	// Stack of overlapping zones, highest priority wins.
	UPROPERTY()
	TArray<TWeakObjectPtr<class UQRBiomeProfile>> ActiveBiomeStack;
	TArray<int32> ActiveBiomeStackPriorities;

	float BiomePollAccum = 0.0f;

	// Apply a biome profile (swap ambient audio, set active name).
	void ApplyBiomeProfile(class UQRBiomeProfile* Profile);

	// Last observed health value — used by HandleHealthChanged to detect
	// damage (drop) vs healing (rise) so we only play the hit SFX on hits.
	float LastObservedHealth = 100.0f;

	UFUNCTION()
	void HandleHealthChanged(float NewHealth);
	void OnHotbarSlotInput(int32 SlotIndex);
	void OnHotbarNext();
	void OnHotbarPrev();

	// Refresh HeldItemMesh from the inventory's current HandSlot. Subscribed
	// to inventory OnInventoryChanged in BeginPlay.
	UFUNCTION()
	void RefreshHeldItemMesh();

	void ScanForInteractable();
	TWeakObjectPtr<AActor> CurrentInteractable;

	// Cached view component for lean routing; resolved on BeginPlay.
	UPROPERTY()
	TObjectPtr<UQRFPViewComponent> CachedView = nullptr;

	// Track which lean keys are held so releasing one doesn't cancel the
	// other if both were pressed (e.g. Q held, E tapped and released).
	bool bLeanLeftHeld = false;
	bool bLeanRightHeld = false;
	void UpdateLeanInput();
};
