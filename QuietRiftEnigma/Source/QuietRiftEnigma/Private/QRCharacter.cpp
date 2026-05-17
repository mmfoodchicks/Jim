#include "QRCharacter.h"
#include "QRInventoryComponent.h"
#include "QRItemInstance.h"
#include "QRItemDefinition.h"
#include "QRSurvivalComponent.h"
#include "QRWeaponComponent.h"
#include "QRFactionComponent.h"
#include "QRDialogueComponent.h"
#include "QRLootContainerComponent.h"
#include "QRVaultComponent.h"
#include "QRHotbarComponent.h"
#include "QRWorldItem.h"
#include "QRWildlifeActor.h"
#include "QRBuildModeComponent.h"
#include "QRInputDefaults.h"
#include "QRGameplayTags.h"
#include "QRFPViewComponent.h"
#include "QRHotbarHUDWidget.h"
#include "QRCreativeBrowserWidget.h"
#include "QRVitalsHUDWidget.h"
#include "QRPauseMenuWidget.h"
#include "QRSettingsWidget.h"
#include "QRCraftingWidget.h"
#include "QRCraftingBench.h"
#include "QRDialogueWidget.h"
#include "QRBuildPieceSelectorWidget.h"
#include "QRInventoryGridWidget.h"
#include "QRBiomeProfile.h"
#include "QRWorldGenSubsystem.h"
#include "QRCodexSubsystem.h"
#include "QRCodexWidget.h"
#include "QRScopeOverlayWidget.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "QRGameMode.h"
#include "QRUISound.h"
#include "Kismet/GameplayStatics.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Net/UnrealNetwork.h"
#include "DrawDebugHelpers.h"
#include "Engine/HitResult.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

AQRCharacter::AQRCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	// First-person camera on root
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(-10.0f, 0.0f, 60.0f));
	FirstPersonCamera->bUsePawnControlRotation = true;

	// Arm mesh (visible only to local player)
	ArmsMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ArmsMesh"));
	ArmsMesh->SetupAttachment(FirstPersonCamera);
	ArmsMesh->SetOnlyOwnerSee(true);
	ArmsMesh->SetCastShadow(false);
	ArmsMesh->SetRelativeLocation(FVector(-30.0f, 0.0f, -150.0f));

	// Held-item mesh: attached directly to the first-person camera so it's
	// visible without needing an authored arms skeletal mesh. Offsets place
	// it in classic first-person held-weapon position (forward, slightly
	// right and down). Once you author a proper arms skeleton with a
	// SOCKET_GripPoint, reattach this to ArmsMesh in a Blueprint subclass.
	HeldItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeldItemMesh"));
	HeldItemMesh->SetupAttachment(FirstPersonCamera);
	HeldItemMesh->SetOnlyOwnerSee(true);
	HeldItemMesh->SetCastShadow(false);
	HeldItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeldItemMesh->SetVisibility(false);
	HeldItemMesh->SetRelativeLocation(FVector(35.0f, 12.0f, -12.0f));
	HeldItemMesh->SetRelativeRotation(FRotator(-5.0f, -8.0f, 0.0f));
	HeldItemMesh->SetRelativeScale3D(FVector(1.0f));

	// UI defaults — local C++ widgets unless overridden in BP.
	HotbarHUDClass        = UQRHotbarHUDWidget::StaticClass();
	CreativeBrowserClass  = UQRCreativeBrowserWidget::StaticClass();
	VitalsHUDClass        = UQRVitalsHUDWidget::StaticClass();
	PauseMenuClass        = UQRPauseMenuWidget::StaticClass();
	SettingsWidgetClass   = UQRSettingsWidget::StaticClass();
	CraftingWidgetClass   = UQRCraftingWidget::StaticClass();
	DialogueWidgetClass   = UQRDialogueWidget::StaticClass();
	BuildPieceSelectorClass = UQRBuildPieceSelectorWidget::StaticClass();
	InventoryGridClass    = UQRInventoryGridWidget::StaticClass();
	CodexWidgetClass      = UQRCodexWidget::StaticClass();
	ScopeOverlayClass     = UQRScopeOverlayWidget::StaticClass();

	// Third-person mesh hidden from self
	GetMesh()->SetOwnerNoSee(true);

	// Movement
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	GetCharacterMovement()->bCanWalkOffLedges = true;
	GetCharacterMovement()->bUseFlatBaseForFloorChecks = true;

	// Survival Components
	Inventory = CreateDefaultSubobject<UQRInventoryComponent>(TEXT("Inventory"));
	Survival  = CreateDefaultSubobject<UQRSurvivalComponent>(TEXT("Survival"));
	Weapon    = CreateDefaultSubobject<UQRWeaponComponent>(TEXT("Weapon"));
	Faction   = CreateDefaultSubobject<UQRFactionComponent>(TEXT("Faction"));
	Vault         = CreateDefaultSubobject<UQRVaultComponent>(TEXT("Vault"));
	Hotbar        = CreateDefaultSubobject<UQRHotbarComponent>(TEXT("Hotbar"));
	Build         = CreateDefaultSubobject<UQRBuildModeComponent>(TEXT("Build"));
	BiomeAmbient  = CreateDefaultSubobject<UAudioComponent>(TEXT("BiomeAmbient"));
	if (BiomeAmbient)
	{
		BiomeAmbient->SetupAttachment(RootComponent);
		BiomeAmbient->bAutoActivate = false;
		BiomeAmbient->VolumeMultiplier = 0.5f;
	}

	WildlifeActorClass = AQRWildlifeActor::StaticClass();

	SurvivorId = FName("PLR_0001");
}

void AQRCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AQRCharacter, bIsSprinting);
	DOREPLIFETIME_CONDITION(AQRCharacter, bIsOverEncumbered, COND_OwnerOnly);
	DOREPLIFETIME(AQRCharacter, PlayerIdentity);
}

void AQRCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Bind death delegate
	if (Survival)
		Survival->OnDeath.AddDynamic(this, &AQRCharacter::OnDied);
		Survival->OnHealthChanged.AddDynamic(this, &AQRCharacter::HandleHealthChanged);
		LastObservedHealth = Survival->Health;

	// Fill any unset input action slots + build a runtime mapping context
	// with sensible defaults (WASD / mouse / F / G / Tab / 1-9 / etc).
	// If DefaultMappingContext is authored in BP it keeps higher priority.
	UQRInputDefaults::Apply(this);

	// Add the authored mapping context if there is one — Apply() already
	// pushed its runtime context at lower priority.
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}

	// Initialize faction as player faction
	if (Faction)
		Faction->FactionTag = QRGameplayTags::Faction_Player;

	// Cache the view component for lean routing.
	CachedView = FindComponentByClass<UQRFPViewComponent>();

	// Held-item mesh follows the inventory's HandSlot. Refresh once on
	// spawn and whenever the inventory changes.
	if (Inventory)
	{
		Inventory->OnInventoryChanged.AddDynamic(this, &AQRCharacter::RefreshHeldItemMesh);
	}
	RefreshHeldItemMesh();

	// Spawn the runtime UI on the local player. Skip on dedicated server
	// pawns and remote clients (each client makes its own).
	APlayerController* LocalPC = Cast<APlayerController>(GetController());
	if (LocalPC && LocalPC->IsLocalController())
	{
		if (HotbarHUDClass)
		{
			HotbarHUD = CreateWidget<UQRHotbarHUDWidget>(LocalPC, HotbarHUDClass);
			if (HotbarHUD)
			{
				HotbarHUD->AddToViewport(/*ZOrder*/ 10);
				HotbarHUD->Bind(Hotbar);
			}
		}
		if (CreativeBrowserClass)
		{
			CreativeBrowser = CreateWidget<UQRCreativeBrowserWidget>(LocalPC, CreativeBrowserClass);
			if (CreativeBrowser)
			{
				CreativeBrowser->AddToViewport(/*ZOrder*/ 50);
				CreativeBrowser->SetVisibility(ESlateVisibility::Collapsed);
				CreativeBrowser->Bind(Hotbar);
			}
		}
		if (VitalsHUDClass && Survival)
		{
			VitalsHUD = CreateWidget<UQRVitalsHUDWidget>(LocalPC, VitalsHUDClass);
			if (VitalsHUD)
			{
				VitalsHUD->AddToViewport(/*ZOrder*/ 10);
				VitalsHUD->Bind(Survival);
			}
		}
		if (ScopeOverlayClass && CachedView)
		{
			ScopeOverlay = CreateWidget<UQRScopeOverlayWidget>(LocalPC, ScopeOverlayClass);
			if (ScopeOverlay)
			{
				ScopeOverlay->AddToViewport(/*ZOrder*/ 400);
				ScopeOverlay->Bind(CachedView);
			}
		}
	}

	// Pull any pending save snapshot from the game mode (server only —
	// it owns the save state; clients get vitals via replication and
	// inventory via the standard inventory replication once the
	// authoritative pawn is populated).
	if (HasAuthority())
	{
		if (AQRGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AQRGameMode>() : nullptr)
		{
			GM->ApplyLoadedDataToPlayer(this);
		}
	}
}

void AQRCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Update encumbrance state
	if (Inventory)
	{
		bool bOver = Inventory->IsOverEncumbered();
		if (bOver != bIsOverEncumbered)
		{
			bIsOverEncumbered = bOver;
			GetCharacterMovement()->MaxWalkSpeed = bOver ? WalkSpeed * 0.5f : WalkSpeed;
		}
	}

	// Interaction scan (local only)
	if (IsLocallyControlled())
		ScanForInteractable();

	// Footsteps — local-only, grounded, moving above threshold. Cadence
	// interpolates between walk and sprint interval based on current
	// horizontal speed vs. max speed.
	if (IsLocallyControlled())
	{
		UCharacterMovementComponent* CMC = GetCharacterMovement();
		const bool bGrounded = CMC && !CMC->IsFalling();
		FVector Vel = GetVelocity();
		Vel.Z = 0.0f;
		const float Speed = Vel.Size();

		if (bGrounded && Speed > FootstepSpeedThreshold)
		{
			FootstepTimer -= DeltaTime;
			if (FootstepTimer <= 0.0f)
			{
				const float MaxSpeed = CMC ? CMC->MaxWalkSpeed : WalkSpeed;
				const float SpeedAlpha = MaxSpeed > 0.0f ? FMath::Clamp(Speed / MaxSpeed, 0.0f, 1.0f) : 0.0f;
				const float Interval = FMath::Lerp(FootstepWalkInterval, FootstepSprintInterval, SpeedAlpha);
				const float Vol      = FootstepVolumeMult * FMath::Lerp(0.6f, 1.0f, SpeedAlpha);

				const float HalfH = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.0f;
				FVector FootLoc = GetActorLocation();
				FootLoc.Z -= HalfH;

				// Surface detection: short line trace from feet downward,
				// read the hit's physical material, map to our footstep
				// surface enum. No PhysMat → falls back to Concrete inside
				// QRUISound::SurfaceFromPhysMat.
				EQRFootSurface Surface = EQRFootSurface::Concrete;
				FHitResult Hit;
				FCollisionQueryParams Params(SCENE_QUERY_STAT(QRFootstepTrace), /*bComplex*/ true);
				Params.AddIgnoredActor(this);
				Params.bReturnPhysicalMaterial = true;
				const FVector TraceStart = GetActorLocation();
				const FVector TraceEnd   = TraceStart - FVector(0, 0, HalfH + 30.0f);
				if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
				{
					if (UPhysicalMaterial* PM = Hit.PhysMaterial.Get())
					{
						Surface = QRUISound::SurfaceFromPhysMat(static_cast<uint8>(PM->SurfaceType));
					}
				}

				// Gait selection — walk for slow, jog mid, run fast.
				EQRFootGait Gait = EQRFootGait::Walk;
				if (SpeedAlpha > 0.66f)      Gait = EQRFootGait::Run;
				else if (SpeedAlpha > 0.33f) Gait = EQRFootGait::Jog;

				QRUISound::PlayFootstep(this, FootLoc, Surface, Gait, Vol);

				FootstepTimer = Interval;
			}
		}
		else
		{
			// Reset so the first step after standing still doesn't fire instantly.
			FootstepTimer = 0.0f;
		}

		// Biome poll — once per second, query the worldgen subsystem at
		// our current position. If a manually-placed zone is already
		// active, defer to it (ActiveBiomeStack non-empty).
		BiomePollAccum += DeltaTime;
		if (BiomePollAccum >= 1.0f && ActiveBiomeStack.Num() == 0)
		{
			BiomePollAccum = 0.0f;
			if (UWorld* W = GetWorld())
			{
				if (UQRWorldGenSubsystem* Sub = W->GetSubsystem<UQRWorldGenSubsystem>())
				{
					if (Sub->bGenerated)
					{
						const FName Biome = Sub->GetBiomeAt(GetActorLocation());
						if (Biome != ActiveBiomeName)
						{
							// Look up the BP_<biome> data asset under the
							// canonical path and swap ambient if found.
							const FString AssetPath = FString::Printf(
								TEXT("/Game/QuietRift/Data/Biomes/BP_%s.BP_%s"),
								*Biome.ToString(), *Biome.ToString());
							if (UQRBiomeProfile* Profile = LoadObject<UQRBiomeProfile>(nullptr, *AssetPath))
							{
								ApplyBiomeProfile(Profile);
							}
						}
					}
				}
			}
		}
	}
}

void AQRCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EI = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveAction)      EI->BindAction(MoveAction,      ETriggerEvent::Triggered, this, &AQRCharacter::Move);
		if (LookAction)      EI->BindAction(LookAction,      ETriggerEvent::Triggered, this, &AQRCharacter::Look);
		if (JumpAction)      EI->BindAction(JumpAction,      ETriggerEvent::Started,   this, &AQRCharacter::HandleJumpPressed);
		if (JumpAction)      EI->BindAction(JumpAction,      ETriggerEvent::Completed, this, &AQRCharacter::HandleJumpReleased);
		if (InteractAction)  EI->BindAction(InteractAction,  ETriggerEvent::Started,   this, &AQRCharacter::TryInteract);
		if (SprintAction)    EI->BindAction(SprintAction,    ETriggerEvent::Started,   this, &AQRCharacter::StartSprint);
		if (SprintAction)    EI->BindAction(SprintAction,    ETriggerEvent::Completed, this, &AQRCharacter::StopSprint);
		if (FireAction)      EI->BindAction(FireAction,      ETriggerEvent::Started,   this, &AQRCharacter::TryFireWeapon);
		if (ReloadAction)    EI->BindAction(ReloadAction,    ETriggerEvent::Started,   this, &AQRCharacter::TryReload);
		if (LeanLeftAction)  EI->BindAction(LeanLeftAction,  ETriggerEvent::Started,   this, &AQRCharacter::LeanLeftPressed);
		if (LeanLeftAction)  EI->BindAction(LeanLeftAction,  ETriggerEvent::Completed, this, &AQRCharacter::LeanLeftReleased);
		if (LeanRightAction) EI->BindAction(LeanRightAction, ETriggerEvent::Started,   this, &AQRCharacter::LeanRightPressed);
		if (LeanRightAction) EI->BindAction(LeanRightAction, ETriggerEvent::Completed, this, &AQRCharacter::LeanRightReleased);

		if (DropAction)             EI->BindAction(DropAction,             ETriggerEvent::Started, this, &AQRCharacter::OnDropPressed);
		if (CreativeBrowserAction)  EI->BindAction(CreativeBrowserAction,  ETriggerEvent::Started, this, &AQRCharacter::OnCreativeBrowserPressed);
		if (HotbarNextAction)       EI->BindAction(HotbarNextAction,       ETriggerEvent::Started, this, &AQRCharacter::OnHotbarNext);
		if (HotbarPrevAction)       EI->BindAction(HotbarPrevAction,       ETriggerEvent::Started, this, &AQRCharacter::OnHotbarPrev);

		if (UseHeldAction)
		{
			EI->BindAction(UseHeldAction, ETriggerEvent::Started,   this, &AQRCharacter::OnUseHeldPressed);
			EI->BindAction(UseHeldAction, ETriggerEvent::Completed, this, &AQRCharacter::OnUseHeldReleased);
		}
		if (PauseAction)
		{
			EI->BindAction(PauseAction, ETriggerEvent::Started, this, &AQRCharacter::OnPausePressed);
		}
		if (InventoryAction)
		{
			EI->BindAction(InventoryAction, ETriggerEvent::Started, this, &AQRCharacter::OnInventoryPressed);
		}
		if (CodexAction)
		{
			EI->BindAction(CodexAction, ETriggerEvent::Started, this, &AQRCharacter::OnCodexPressed);
		}

		// Per-slot bindings carry the slot index as a payload, so the same
		// handler routes all 9 keys without 9 trampoline functions.
		for (int32 i = 0; i < HotbarSlotActions.Num(); ++i)
		{
			if (HotbarSlotActions[i])
			{
				EI->BindAction(HotbarSlotActions[i], ETriggerEvent::Started,
					this, &AQRCharacter::OnHotbarSlotInput, i);
			}
		}
	}
}

void AQRCharacter::Move(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();
	if (!Controller) return;

	AddMovementInput(GetActorForwardVector(), MovementVector.Y);
	AddMovementInput(GetActorRightVector(),   MovementVector.X);
}

void AQRCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookVector = Value.Get<FVector2D>();
	AddControllerYawInput(LookVector.X);
	AddControllerPitchInput(LookVector.Y);
}

void AQRCharacter::StartSprint()
{
	if (!CanSprint()) return;
	SetSprinting(true);
}

void AQRCharacter::StopSprint()
{
	SetSprinting(false);
}

void AQRCharacter::SetSprinting(bool bSprint)
{
	bIsSprinting = bSprint;
	float NewSpeed = bSprint ? SprintSpeed : WalkSpeed;
	if (bIsOverEncumbered) NewSpeed *= 0.5f;
	GetCharacterMovement()->MaxWalkSpeed = NewSpeed;

	// Sprinting drains fatigue faster (handled in survival tick through locomotion tags)
}

bool AQRCharacter::CanSprint() const
{
	if (!Survival) return !bIsOverEncumbered;
	// Survival owns the full block check (exhausted / suffocating / severe
	// fracture) so the character only has to also enforce encumbrance.
	return !Survival->IsSprintBlockedByCondition() && !bIsOverEncumbered;
}

void AQRCharacter::ScanForInteractable()
{
	if (!FirstPersonCamera) return;

	FVector Start = FirstPersonCamera->GetComponentLocation();
	FVector End   = Start + FirstPersonCamera->GetForwardVector() * InteractDistance;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

	AActor* NewInteractable = bHit ? Hit.GetActor() : nullptr;

	if (NewInteractable != CurrentInteractable.Get())
	{
		CurrentInteractable = NewInteractable;
		if (NewInteractable) OnInteractableFound(NewInteractable);
	}
}

void AQRCharacter::TryInteract()
{
	if (!CurrentInteractable.IsValid()) return;

	// Client-side: if the focus is a crafting bench, open the local UI.
	// This is purely cosmetic; the queue/cancel buttons inside the
	// widget call into the component which RPCs the server.
	if (CraftingWidgetClass)
	{
		if (AQRCraftingBench* Bench = Cast<AQRCraftingBench>(CurrentInteractable.Get()))
		{
			APlayerController* PC = Cast<APlayerController>(GetController());
			if (PC && PC->IsLocalController())
			{
				UQRCraftingWidget* W = CreateWidget<UQRCraftingWidget>(PC, CraftingWidgetClass);
				if (W)
				{
					W->AddToViewport(/*ZOrder*/ 200);
					W->Bind(Bench);
					PC->bShowMouseCursor = true;
					FInputModeGameAndUI Mode;
					Mode.SetWidgetToFocus(W->TakeWidget());
					PC->SetInputMode(Mode);
				}
				return;
			}
		}
	}

	// Client-side dialogue overlay: if the focus has a UQRDialogueComponent,
	// mount the dialogue widget locally and let the standard Server_Interact
	// path actually start the conversation (component lives on the actor,
	// the widget just subscribes to its events).
	if (DialogueWidgetClass)
	{
		if (UQRDialogueComponent* Dlg = CurrentInteractable->FindComponentByClass<UQRDialogueComponent>())
		{
			APlayerController* PC = Cast<APlayerController>(GetController());
			if (PC && PC->IsLocalController())
			{
				UQRDialogueWidget* W = CreateWidget<UQRDialogueWidget>(PC, DialogueWidgetClass);
				if (W)
				{
					W->AddToViewport(/*ZOrder*/ 150);
					W->Bind(Dlg);
				}
			}
		}
	}

	// Trigger interaction on server if client
	if (!HasAuthority())
	{
		Server_Interact(CurrentInteractable.Get());
		return;
	}

	OnInteract.Broadcast(CurrentInteractable.Get());
}

void AQRCharacter::TryFireWeapon()
{
	if (!Weapon || !FirstPersonCamera) return;

	const FVector  Start   = FirstPersonCamera->GetComponentLocation();
	const FVector  Forward = FirstPersonCamera->GetForwardVector();
	const bool     bMoving = GetVelocity().Size2D() > 50.0f;
	bool           bAimed  = false;
	if (UQRFPViewComponent* View = FindComponentByClass<UQRFPViewComponent>())
	{
		bAimed = View->IsADS();
	}

	if (!HasAuthority())
	{
		Server_Fire(Start, Forward, bAimed, bMoving);
		return;
	}

	const FQRFireResult Result = Weapon->TryFireFromTrace(Start, Forward, bAimed, bMoving, /*AmmoInstance*/ nullptr);
	if (Result.bFired)
	{
		// Apply kick on the firing controller. Pitch is up (negative
		// camera pitch input in UE convention), yaw is +/- random.
		AddControllerPitchInput(-Result.RecoilPitch);
		AddControllerYawInput(Result.RecoilYaw);
	}
}

void AQRCharacter::Server_Fire_Implementation(FVector TraceStart, FVector TraceForward,
	bool bIsAimed, bool bIsMoving)
{
	if (!Weapon) return;
	const FQRFireResult Result = Weapon->TryFireFromTrace(TraceStart, TraceForward, bIsAimed, bIsMoving, nullptr);
	if (Result.bFired)
	{
		AddControllerPitchInput(-Result.RecoilPitch);
		AddControllerYawInput(Result.RecoilYaw);
	}
}

void AQRCharacter::TryReload()
{
	if (!Weapon) return;
	if (!HasAuthority()) { Server_Reload(); return; }
	Weapon->BeginReload();
	// FinishReload(NewAmmoCount) is called by the animation/notify after
	// ReloadTimeSeconds; gameplay code can also call it directly for an
	// instant reload (debug / cheats). Hooking the animation up to call
	// FinishReload at the end of the reload pose is a BP-side task.
}

void AQRCharacter::Server_Reload_Implementation()
{
	if (Weapon) Weapon->BeginReload();
}

void AQRCharacter::Server_Interact_Implementation(AActor* Target)
{
	if (!Target) return;
	OnInteract.Broadcast(Target);

	// Auto-start a conversation if the target has a UQRDialogueComponent.
	// Reflective lookup avoids a hard module dep on the dialogue header
	// here — anything subscribed to OnInteract can still handle the event
	// however it likes; this is just the default convenience for the
	// most common case (NPC with dialogue).
	if (UActorComponent* DlgComp = Target->FindComponentByClass(
		UQRDialogueComponent::StaticClass()))
	{
		if (UQRDialogueComponent* Dialogue = Cast<UQRDialogueComponent>(DlgComp))
		{
			Dialogue->StartConversation(this);
		}
	}

	// Auto-loot if the target carries a UQRLootContainerComponent.
	if (UActorComponent* LootComp = Target->FindComponentByClass(
		UQRLootContainerComponent::StaticClass()))
	{
		if (UQRLootContainerComponent* Container = Cast<UQRLootContainerComponent>(LootComp))
		{
			Container->TryLoot(this);
		}
	}

	// Auto-pickup if the target is an AQRWorldItem (dropped / placed item).
	if (AQRWorldItem* WorldItem = Cast<AQRWorldItem>(Target))
	{
		WorldItem->TryPickup(this);
	}
}

void AQRCharacter::HandleJumpPressed()
{
	// Try a vault first; fall through to Jump only if no obstacle ahead.
	if (Vault && Vault->TryVault()) return;
	Jump();
}

void AQRCharacter::HandleJumpReleased()
{
	StopJumping();
}

void AQRCharacter::LeanLeftPressed()  { bLeanLeftHeld  = true;  UpdateLeanInput(); }
void AQRCharacter::LeanLeftReleased() { bLeanLeftHeld  = false; UpdateLeanInput(); }
void AQRCharacter::LeanRightPressed() { bLeanRightHeld = true;  UpdateLeanInput(); }
void AQRCharacter::LeanRightReleased(){ bLeanRightHeld = false; UpdateLeanInput(); }

void AQRCharacter::UpdateLeanInput()
{
	// Both held → cancel out, neither held → 0, one held → +/-1.
	const float Target = (bLeanRightHeld ? 1.0f : 0.0f) - (bLeanLeftHeld ? 1.0f : 0.0f);
	if (CachedView) CachedView->SetLeanInput(Target);
}

void AQRCharacter::OnDropPressed()
{
	TryDropHeld();
}

void AQRCharacter::TryDropHeld()
{
	if (!Hotbar) return;
	if (!HasAuthority()) { Server_DropHeld(); return; }
	DoDropHeld();
}

void AQRCharacter::Server_DropHeld_Implementation()
{
	DoDropHeld();
}

void AQRCharacter::DoDropHeld()
{
	if (!Hotbar || !Inventory) return;

	UQRItemInstance* Held = Hotbar->GetActiveItem();
	const UQRItemDefinition* Def = (Held && Held->IsValid()) ? Held->Definition : nullptr;

	// Wildlife — spawn a wandering actor in front of the player and
	// decrement one unit from the held stack.
	if (Def && Def->Category == EQRItemCategory::Wildlife && WildlifeActorClass)
	{
		const FVector SpawnLoc = GetActorLocation()
			+ GetActorForwardVector() * 150.0f
			+ FVector(0, 0, -30.0f);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		Params.Owner = this;
		if (AQRWildlifeActor* Animal = GetWorld()->SpawnActor<AQRWildlifeActor>(
				WildlifeActorClass, SpawnLoc, GetActorRotation(), Params))
		{
			Animal->InitializeFrom(Def, 1);
			Inventory->TryRemoveItem(Def->ItemId, 1);
		}
		return;
	}

	// Building piece — enter build mode with this piece selected, only
	// if Build component actually has a catalog to look it up in.
	if (Def && Build && Build->PieceCatalog &&
		Def->ItemId.ToString().StartsWith(TEXT("BLD_")))
	{
		Build->EnterBuildMode();
		Build->SelectPiece(Def->ItemId);

		// Pop the piece selector so the player can swap to a different
		// piece without leaving build mode. Local-only UI.
		if (BuildPieceSelectorClass)
		{
			APlayerController* PC = Cast<APlayerController>(GetController());
			if (PC && PC->IsLocalController())
			{
				UQRBuildPieceSelectorWidget* W = CreateWidget<UQRBuildPieceSelectorWidget>(PC, BuildPieceSelectorClass);
				if (W)
				{
					W->AddToViewport(/*ZOrder*/ 220);
					W->Bind(Build);
				}
			}
		}
		return;
	}

	// Default: drop as a world item.
	Hotbar->DropActiveItem(-1);
}

void AQRCharacter::OnUseHeldPressed()  { TryUseHeld(true);  }
void AQRCharacter::OnUseHeldReleased() { TryUseHeld(false); }

void AQRCharacter::OnInventoryPressed()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->IsLocalController()) return;

	// Toggle behavior: close if already open, otherwise mount.
	if (InventoryGrid && InventoryGrid->IsInViewport())
	{
		InventoryGrid->RemoveFromParent();
		PC->bShowMouseCursor = false;
		PC->SetInputMode(FInputModeGameOnly());
		return;
	}

	if (!InventoryGridClass || !Inventory) return;
	InventoryGrid = CreateWidget<UQRInventoryGridWidget>(PC, InventoryGridClass);
	if (!InventoryGrid) return;

	InventoryGrid->AddToViewport(/*ZOrder*/ 300);
	InventoryGrid->Bind(Inventory);

	PC->bShowMouseCursor = true;
	FInputModeGameAndUI Mode;
	Mode.SetWidgetToFocus(InventoryGrid->TakeWidget());
	PC->SetInputMode(Mode);
}

void AQRCharacter::OnCodexPressed()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->IsLocalController()) return;

	if (CodexWidget && CodexWidget->IsInViewport())
	{
		CodexWidget->RemoveFromParent();
		PC->bShowMouseCursor = false;
		PC->SetInputMode(FInputModeGameOnly());
		return;
	}

	if (!CodexWidgetClass) return;
	CodexWidget = CreateWidget<UQRCodexWidget>(PC, CodexWidgetClass);
	if (!CodexWidget) return;
	CodexWidget->AddToViewport(/*ZOrder*/ 350);

	PC->bShowMouseCursor = true;
	FInputModeGameAndUI Mode;
	Mode.SetWidgetToFocus(CodexWidget->TakeWidget());
	PC->SetInputMode(Mode);
}

void AQRCharacter::QR_StudyItem(FName Id)
{
	if (UWorld* W = GetWorld())
	{
		if (UQRCodexSubsystem* Codex = W->GetSubsystem<UQRCodexSubsystem>())
		{
			const FQRCodexEntry Existing = Codex->GetEntry(Id);
			const FText DisplayName = Existing.DisplayName.IsEmpty()
				? FText::FromName(Id)
				: Existing.DisplayName;
			// Auto-infer category: if the existing entry has one, use it;
			// else default to Item (player typically studies inventory items).
			const FName Category = Existing.Category.IsNone()
				? FName(TEXT("Item"))
				: Existing.Category;
			Codex->Record(Id, Category, DisplayName, EQRCodexDiscoveryState::Known);
		}
	}
}

void AQRCharacter::QR_OpenSettings()
{
	if (!SettingsWidgetClass) return;
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->IsLocalController()) return;

	UQRSettingsWidget* W = CreateWidget<UQRSettingsWidget>(PC, SettingsWidgetClass);
	if (!W) return;
	W->AddToViewport(/*ZOrder*/ 600);

	// Keep cursor + UI input while open. If launched from the pause
	// menu, the game is already paused; otherwise pause now so the
	// world freezes while the player tweaks knobs.
	PC->bShowMouseCursor = true;
	FInputModeGameAndUI Mode;
	Mode.SetWidgetToFocus(W->TakeWidget());
	PC->SetInputMode(Mode);
}

void AQRCharacter::OnPausePressed()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->IsLocalController()) return;

	const bool bCurrentlyPaused = UGameplayStatics::IsGamePaused(this);
	if (bCurrentlyPaused)
	{
		// Resume — pause widget handles its own teardown via Resume button,
		// but Esc-while-open should also dismiss it.
		UGameplayStatics::SetGamePaused(this, false);
		PC->bShowMouseCursor = false;
		PC->SetInputMode(FInputModeGameOnly());
		if (PauseMenu)
		{
			PauseMenu->RemoveFromParent();
			PauseMenu = nullptr;
		}
		return;
	}

	if (!PauseMenuClass) return;
	PauseMenu = CreateWidget<UQRPauseMenuWidget>(PC, PauseMenuClass);
	if (PauseMenu)
	{
		PauseMenu->AddToViewport(/*ZOrder*/ 500);
		UGameplayStatics::SetGamePaused(this, true);
		PC->bShowMouseCursor = true;
		FInputModeGameAndUI Mode;
		Mode.SetWidgetToFocus(PauseMenu->TakeWidget());
		PC->SetInputMode(Mode);
	}
}

void AQRCharacter::TryUseHeld(bool bPressed)
{
	if (!HasAuthority()) { Server_UseHeld(bPressed); return; }
	DoUseHeld(bPressed);
}

void AQRCharacter::Server_UseHeld_Implementation(bool bPressed)
{
	DoUseHeld(bPressed);
}

void AQRCharacter::DoUseHeld(bool bPressed)
{
	// On release, only clear ADS if we started it on the matching press.
	if (!bPressed)
	{
		if (bUseStartedADS)
		{
			if (UQRFPViewComponent* View = FindComponentByClass<UQRFPViewComponent>())
				View->SetADS(false);
		}
		bUseStartedADS = false;
		return;
	}

	if (!Hotbar) return;
	UQRItemInstance* Held = Hotbar->GetActiveItem();
	const UQRItemDefinition* Def = (Held && Held->IsValid()) ? Held->Definition : nullptr;
	if (!Def) return;

	switch (Def->Category)
	{
	case EQRItemCategory::Weapon:
	case EQRItemCategory::Attachment:
		if (UQRFPViewComponent* View = FindComponentByClass<UQRFPViewComponent>())
		{
			View->SetADS(true);
			bUseStartedADS = true;
		}
		break;

	case EQRItemCategory::Food:
		if (Survival) Survival->ConsumeFood(Held);
		// ConsumeFood is expected to decrement the stack; if it leaves
		// the instance valid we trust the API. No double-remove here.
		break;

	case EQRItemCategory::Medicine:
		if (Survival)
		{
			Survival->ApplyHealing(25.0f);
			if (Inventory) Inventory->TryRemoveItem(Def->ItemId, 1);
		}
		break;

	default:
		break;
	}
}

void AQRCharacter::OnCreativeBrowserPressed()
{
	OnCreativeBrowserToggled.Broadcast();
	if (CreativeBrowser) CreativeBrowser->Toggle();
}

void AQRCharacter::OnHotbarSlotInput(int32 SlotIndex)
{
	if (!Hotbar) return;
	Hotbar->SelectSlot(SlotIndex);
	if (IsLocallyControlled()) QRUISound::PlayClick(this);
}

void AQRCharacter::OnHotbarNext()
{
	if (!Hotbar) return;
	Hotbar->SelectNext();
	if (IsLocallyControlled()) QRUISound::PlayClick(this);
}

void AQRCharacter::OnHotbarPrev()
{
	if (!Hotbar) return;
	Hotbar->SelectPrev();
	if (IsLocallyControlled()) QRUISound::PlayClick(this);
}

void AQRCharacter::RefreshHeldItemMesh()
{
	if (!HeldItemMesh) return;

	UStaticMesh* TargetMesh = nullptr;
	if (Inventory && Inventory->HandSlot)
	{
		if (const UQRItemDefinition* Def = Inventory->HandSlot->Definition)
		{
			TargetMesh = Def->WorldMesh.LoadSynchronous();
		}
	}

	HeldItemMesh->SetStaticMesh(TargetMesh);
	HeldItemMesh->SetVisibility(TargetMesh != nullptr);

	// Scope detection — long-range sniper or any weapon with ItemId
	// containing SNIPER or with a scope attachment in tags. Designer
	// can override via per-weapon tags later. For now: name-based.
	bool bHasScope = false;
	if (Inventory && Inventory->HandSlot && Inventory->HandSlot->Definition)
	{
		const FString Id = Inventory->HandSlot->Definition->ItemId.ToString();
		bHasScope = Id.Contains(TEXT("SNIPER"))
				 || Id.Contains(TEXT("DMR"))
				 || Id.Contains(TEXT("SCOPE"));
	}
	if (CachedView) CachedView->SetScopeAvailable(bHasScope);
}

void AQRCharacter::HandleHealthChanged(float NewHealth)
{
	if (NewHealth < LastObservedHealth - KINDA_SMALL_NUMBER)
	{
		QRUISound::PlayHitImpact(this, GetActorLocation());
	}
	LastObservedHealth = NewHealth;
}

void AQRCharacter::OnDied_Implementation()
{
	// Death cry, then teardown.
	QRUISound::PlayDeathCry(this, GetActorLocation());

	// Disable input, collapse physics, notify game mode
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
		PC->DisableInput(PC);

	GetMesh()->SetSimulatePhysics(true);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Hand off to the game mode to mount the death screen + schedule a
	// respawn. Server authority only — clients receive the new pawn via
	// the standard PlayerController possession path.
	if (HasAuthority())
	{
		if (AQRGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AQRGameMode>() : nullptr)
		{
			GM->HandlePlayerDied(this);
		}
	}
}

// ─── Biome ambient audio ─────────────────────────────────────────────

void AQRCharacter::ApplyBiomeProfile(UQRBiomeProfile* Profile)
{
	if (!Profile || !BiomeAmbient) return;
	if (Profile->BiomeTag == ActiveBiomeName) return;
	ActiveBiomeName = Profile->BiomeTag;

	USoundBase* Sound = Profile->AmbientLoop.LoadSynchronous();
	BiomeAmbient->Stop();
	if (Sound)
	{
		BiomeAmbient->SetSound(Sound);
		BiomeAmbient->Play();
	}

	// Codex: record biome on first contact.
	if (UWorld* W = GetWorld())
	{
		if (UQRCodexSubsystem* Codex = W->GetSubsystem<UQRCodexSubsystem>())
		{
			Codex->Record(Profile->BiomeTag, TEXT("Biome"), Profile->DisplayName,
				EQRCodexDiscoveryState::Observed);
		}
	}
}

void AQRCharacter::OnBiomeZoneEnter(UQRBiomeProfile* Profile, int32 Priority)
{
	if (!Profile) return;
	ActiveBiomeStack.Add(Profile);
	ActiveBiomeStackPriorities.Add(Priority);

	// Highest-priority active zone wins.
	int32 BestIdx = INDEX_NONE;
	int32 BestPriority = TNumericLimits<int32>::Min();
	for (int32 i = 0; i < ActiveBiomeStack.Num(); ++i)
	{
		if (ActiveBiomeStack[i].IsValid() && ActiveBiomeStackPriorities[i] > BestPriority)
		{
			BestPriority = ActiveBiomeStackPriorities[i];
			BestIdx = i;
		}
	}
	if (BestIdx != INDEX_NONE) ApplyBiomeProfile(ActiveBiomeStack[BestIdx].Get());
}

void AQRCharacter::OnBiomeZoneExit(UQRBiomeProfile* Profile, int32 Priority)
{
	for (int32 i = ActiveBiomeStack.Num() - 1; i >= 0; --i)
	{
		if (ActiveBiomeStack[i].Get() == Profile && ActiveBiomeStackPriorities[i] == Priority)
		{
			ActiveBiomeStack.RemoveAt(i);
			ActiveBiomeStackPriorities.RemoveAt(i);
			break;
		}
	}

	// Fall back to worldgen biome at current position (handled on tick).
	if (ActiveBiomeStack.Num() == 0)
	{
		ActiveBiomeName = NAME_None;  // forces tick to re-apply
	}
}
