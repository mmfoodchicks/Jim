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
#include "QRCrashSiteActor.h"
#include "QRHUD.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Engine/SkeletalMesh.h"
#include "QRWildlifeActor.h"
#include "QRWildlifeBase.h"
#include "UObject/UObjectHash.h"
#include "QRBuildModeComponent.h"
#include "QRInputDefaults.h"
#include "QRGameplayTags.h"
#include "QRFPViewComponent.h"
#include "QRHotbarHUDWidget.h"
#include "QRCreativeBrowserWidget.h"
#include "QRVitalsHUDWidget.h"
#include "QRAmmoHUDWidget.h"
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
#include "QRMissionHUDWidget.h"
#include "QRMissionDirector.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "AudioDevice.h"
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
#include "Misc/ConfigCacheIni.h"
#include "DrawDebugHelpers.h"
#include "Engine/HitResult.h"
#include "Engine/DamageEvents.h"
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

	// LOCK exposure on the camera itself so the view can never blow out to
	// white regardless of which PostProcessVolume / auto-exposure the level
	// happens to have. Histogram auto-exposure with min == max pins the
	// camera at a constant EV (no adaptation). This lives on the camera (not
	// a level PPV) so it's always applied and survives map re-dressing.
	// Tune live in PIE with the QR_Exposure console exec.
	QR_Exposure(LockedExposureEV);

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
	// Held weapon transform in camera-local space. The previous offset
	// pushed the gun ~18 cm right of centre which read as "floating off
	// to the side"; bringing it in closer and dropping it slightly makes
	// it sit in the lower-right where FPS hands normally hold a gun.
	HeldItemBaseLocation = FVector(38.0f, 9.0f, -14.0f);
	HeldItemBaseRotation = FRotator(-2.0f, -3.0f, 0.0f);
	HeldItemMesh->SetRelativeLocation(HeldItemBaseLocation);
	HeldItemMesh->SetRelativeRotation(HeldItemBaseRotation);
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
	AmmoHUDClass          = UQRAmmoHUDWidget::StaticClass();
	MissionHUDClass       = UQRMissionHUDWidget::StaticClass();

	// Third-person mesh hidden from self
	GetMesh()->SetOwnerNoSee(true);

	// Movement
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	GetCharacterMovement()->bCanWalkOffLedges = true;
	GetCharacterMovement()->bUseFlatBaseForFloorChecks = true;
	// Climb steeper terrain before sliding. The default ~45° was letting
	// the player slide back down the lower (steeper) part of dome hills
	// they ought to be able to walk up. 52° + a higher step height makes
	// the rolling hills climbable.
	GetCharacterMovement()->SetWalkableFloorAngle(52.0f);
	GetCharacterMovement()->MaxStepHeight = 55.0f;
	// Crouch was mapped (Ctrl/C) but the movement component never allowed
	// it, and no input binding existed — completely inert until now.
	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
	GetCharacterMovement()->MaxWalkSpeedCrouched = CrouchSpeed;

	// Survival Components
	Inventory = CreateDefaultSubobject<UQRInventoryComponent>(TEXT("Inventory"));
	Survival  = CreateDefaultSubobject<UQRSurvivalComponent>(TEXT("Survival"));
	Weapon    = CreateDefaultSubobject<UQRWeaponComponent>(TEXT("Weapon"));
	Faction   = CreateDefaultSubobject<UQRFactionComponent>(TEXT("Faction"));
	Vault         = CreateDefaultSubobject<UQRVaultComponent>(TEXT("Vault"));
	Hotbar        = CreateDefaultSubobject<UQRHotbarComponent>(TEXT("Hotbar"));
	Build         = CreateDefaultSubobject<UQRBuildModeComponent>(TEXT("Build"));
	// First-person view driver. Owns ADS state + FOV interpolation. Without
	// this component RMB toggling ADS was a no-op (FindComponentByClass
	// returned null), so the weapon's spread function never knew you were
	// aiming and hipfire spread stayed maxed.
	FPView        = CreateDefaultSubobject<UQRFPViewComponent>(TEXT("FPView"));
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

	// Re-apply the exposure bias with the SERIALIZED property value — the
	// constructor call ran before property init, so an editor-tuned
	// LockedExposureEV never actually reached the camera.
	QR_Exposure(LockedExposureEV);

	// BP/editor-tuned CrouchSpeed lands after the constructor too.
	GetCharacterMovement()->MaxWalkSpeedCrouched = CrouchSpeed;

	// Bind death delegate
	if (Survival)
	{
		Survival->OnDeath.AddDynamic(this, &AQRCharacter::OnDied);
		Survival->OnHealthChanged.AddDynamic(this, &AQRCharacter::HandleHealthChanged);
		LastObservedHealth = Survival->Health;
	}

	// Remember the mesh's resting pose so Revive can undo a death ragdoll.
	if (USkeletalMeshComponent* M = GetMesh())
	{
		MeshBaseRelLocation = M->GetRelativeLocation();
		MeshBaseRelRotation = M->GetRelativeRotation();
	}

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
			// Priority 100 so the authored (or promoted-runtime) default
			// context outranks the raw runtime context Apply() adds at 0.
			// The old 0-vs-50 arrangement was inverted: runtime defaults
			// silently overrode any BP-authored rebinds.
			if (DefaultMappingContext)
				Subsystem->AddMappingContext(DefaultMappingContext, 100);
		}

		// Force input back to GameOnly. AQRMainMenuGameMode leaves the PC
		// in InputModeUIOnly with the cursor visible; non-seamless OpenLevel
		// is *supposed* to give us a fresh PC but in PIE the state often
		// leaks through, leaving WASD/Tab/etc. routed to nothing.
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor       = false;
		PC->SetIgnoreLookInput(false);
		PC->SetIgnoreMoveInput(false);
	}

	// Initialize faction as player faction
	if (Faction)
		Faction->FactionTag = QRGameplayTags::Faction_Player;

	// Cache the view component for lean routing.
	CachedView = FindComponentByClass<UQRFPViewComponent>();

	// Restore the persisted handedness so a fresh session picks up what the
	// player set in the settings widget last time. Same config block the
	// sliders use; key is "LeftHanded".
	{
		bool bLeftCfg = false;
		if (GConfig->GetBool(TEXT("/Script/QuietRiftEnigma.UserSettings"),
		                     TEXT("LeftHanded"), bLeftCfg, GGameUserSettingsIni))
		{
			bIsLeftHanded = bLeftCfg;
		}
	}

	// Apply the rest of the persisted user settings — these used to be
	// write-only: the sliders saved to config but nothing ever read it
	// back at boot (and sensitivity was never read at all).
	if (IsLocallyControlled())
	{
		const TCHAR* Section = TEXT("/Script/QuietRiftEnigma.UserSettings");
		float SensCfg = 1.0f;
		if (GConfig->GetFloat(Section, TEXT("MouseSensitivity"), SensCfg, GGameUserSettingsIni))
		{
			MouseSensitivityMult = FMath::Clamp(SensCfg, 0.1f, 4.0f);
		}
		float FOVCfg = 0.0f;
		if (GConfig->GetFloat(Section, TEXT("FieldOfView"), FOVCfg, GGameUserSettingsIni) && CachedView)
		{
			CachedView->BaseFOV = FMath::Clamp(FOVCfg, 60.0f, 120.0f);
		}
		float VolCfg = 1.0f;
		if (GConfig->GetFloat(Section, TEXT("MasterVolume"), VolCfg, GGameUserSettingsIni) && GEngine)
		{
			if (FAudioDevice* AD = GEngine->GetMainAudioDeviceRaw())
			{
				AD->SetTransientPrimaryVolume(FMath::Clamp(VolCfg, 0.0f, 1.0f));
			}
		}
	}

	// Held-item mesh follows the inventory's HandSlot. Refresh once on
	// spawn and whenever the inventory changes.
	if (Inventory)
	{
		Inventory->OnInventoryChanged.AddDynamic(this, &AQRCharacter::RefreshHeldItemMesh);
		Inventory->OnInventoryChanged.AddDynamic(this, &AQRCharacter::RefreshArmour);
	}
	RefreshHeldItemMesh();
	RefreshArmour();

	// Third-person body: assign the default mesh when the BP left the
	// slot empty, then flip to single-node animation so partners + the
	// player's own shadow get idle/walk/run instead of a T-pose. Only
	// when no AnimBP is assigned -- a designer-authored ABP wins.
	if (USkeletalMeshComponent* Body = GetMesh())
	{
		if (!Body->GetSkeletalMeshAsset() && !DefaultBodyMesh.IsNull())
		{
			if (USkeletalMesh* BodyMesh = DefaultBodyMesh.LoadSynchronous())
			{
				Body->SetSkeletalMesh(BodyMesh);
			}
		}
		if (Body->GetSkeletalMeshAsset() && !Body->GetAnimClass())
		{
			Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			if (UAnimSequence* Idle = TPIdleAnim.LoadSynchronous())
			{
				Body->PlayAnimation(Idle, /*bLooping*/ true);
				TPLastPlayed = Idle;
			}
		}
	}

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
		if (AmmoHUDClass && Weapon)
		{
			AmmoHUD = CreateWidget<UQRAmmoHUDWidget>(LocalPC, AmmoHUDClass);
			if (AmmoHUD)
			{
				AmmoHUD->AddToViewport(/*ZOrder*/ 10);
				AmmoHUD->Bind(Weapon, Hotbar, Inventory);
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
		// Mission tracker — director lives on the GameMode, so this only
		// binds on single-player / listen host (GameMode is server-only).
		if (MissionHUDClass)
		{
			if (AQRGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AQRGameMode>() : nullptr)
			{
				if (GM->MissionDirector)
				{
					MissionHUD = CreateWidget<UQRMissionHUDWidget>(LocalPC, MissionHUDClass);
					if (MissionHUD)
					{
						MissionHUD->AddToViewport(/*ZOrder*/ 10);
						MissionHUD->Bind(GM->MissionDirector);
					}
				}
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

	// Third-person locomotion loop (single-node) -- cheap velocity-edge
	// swap, and only when the mesh is in single-node mode (an authored
	// AnimBP owns the body otherwise).
	TickThirdPersonAnim();

	// Weapon recoil — decay the held-mesh kick back to its resting pose.
	if (IsLocallyControlled() && HeldItemMesh &&
		(!WeaponRecoilRot.IsNearlyZero(0.02f) || !WeaponRecoilLoc.IsNearlyZero(0.02f)))
	{
		WeaponRecoilRot = FMath::RInterpTo(WeaponRecoilRot, FRotator::ZeroRotator,
			DeltaTime, WeaponRecoilRecoverySpeed);
		WeaponRecoilLoc = FMath::VInterpTo(WeaponRecoilLoc, FVector::ZeroVector,
			DeltaTime, WeaponRecoilRecoverySpeed);
		HeldItemMesh->SetRelativeRotation(HeldItemBaseRotation + WeaponRecoilRot);
		HeldItemMesh->SetRelativeLocation(HeldItemBaseLocation + WeaponRecoilLoc);
	}

	// View-recoil recovery — after the burst pauses, walk the camera pitch
	// back down by the accumulated climb (Tarkov-style). The grace delay
	// stops recovery from fighting the climb mid-burst; the recovery rate
	// eases the muzzle home instead of snapping. If the player moves the
	// mouse during recovery their input still applies on top — we only
	// remove what the recoil added.
	if (IsLocallyControlled() && AccumulatedViewRecoilPitch > KINDA_SMALL_NUMBER)
	{
		TimeSinceLastShot += DeltaTime;
		if (TimeSinceLastShot >= ViewRecoilRecoveryDelay)
		{
			const float Step = FMath::Min(
				AccumulatedViewRecoilPitch,
				ViewRecoilRecoverySpeed * DeltaTime);
			AddControllerPitchInput(Step);   // positive = down
			AccumulatedViewRecoilPitch -= Step;
		}
	}

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

	// Full-auto fire — when the trigger is held on a full-auto weapon,
	// poll once a tick. TryFireWeapon is cadence-gated so this only
	// actually shoots at the weapon's RPM, no matter how fast Tick runs.
	if (IsLocallyControlled() && bFireHeld && Weapon && Weapon->IsFullAuto())
	{
		TryFireWeapon();
	}

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
				// Normalize against the walk→sprint band, NOT the current
				// MaxWalkSpeed — MaxWalkSpeed equals walk speed while
				// walking, so the old Speed/MaxSpeed always read ≈1.0 and
				// every walk played Run-gait steps at sprint cadence.
				const float SpeedAlpha = FMath::Clamp(
					(Speed - WalkSpeed) / FMath::Max(SprintSpeed - WalkSpeed, 1.0f),
					0.0f, 1.0f);
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

	// UE calls SetupPlayerInputComponent BEFORE BeginPlay, so any input
	// actions that BeginPlay would fill in via UQRInputDefaults::Apply
	// don't exist yet — every if (MoveAction) BindAction below would
	// no-op. Apply here so the action UPROPERTYs are populated before
	// we bind to them. Apply is idempotent (BeginPlay calls it too).
	UQRInputDefaults::Apply(this);

	if (UEnhancedInputComponent* EI = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveAction)      EI->BindAction(MoveAction,      ETriggerEvent::Triggered, this, &AQRCharacter::Move);
		if (LookAction)      EI->BindAction(LookAction,      ETriggerEvent::Triggered, this, &AQRCharacter::Look);
		if (JumpAction)      EI->BindAction(JumpAction,      ETriggerEvent::Started,   this, &AQRCharacter::HandleJumpPressed);
		if (JumpAction)      EI->BindAction(JumpAction,      ETriggerEvent::Completed, this, &AQRCharacter::HandleJumpReleased);
		if (CrouchAction)    EI->BindAction(CrouchAction,    ETriggerEvent::Started,   this, &AQRCharacter::HandleCrouchPressed);
		if (InteractAction)  EI->BindAction(InteractAction,  ETriggerEvent::Started,   this, &AQRCharacter::TryInteract);
		if (SprintAction)    EI->BindAction(SprintAction,    ETriggerEvent::Started,   this, &AQRCharacter::StartSprint);
		if (SprintAction)    EI->BindAction(SprintAction,    ETriggerEvent::Completed, this, &AQRCharacter::StopSprint);
		if (FireAction)
		{
			// Started = initial trigger pull (every fire mode fires once).
			// Completed = release. Full-auto continuous fire is driven by
			// Tick() polling bFireHeld so we don't depend on a specific
			// Enhanced Input trigger config to deliver per-tick "Triggered"
			// events (default Boolean triggers can fire just once on press).
			EI->BindAction(FireAction, ETriggerEvent::Started,   this, &AQRCharacter::OnFirePressed);
			EI->BindAction(FireAction, ETriggerEvent::Completed, this, &AQRCharacter::OnFireReleased);
		}
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

	// Slow the mouse when aiming. Resolve the view component the SAME way
	// SetADS does (FindComponentByClass), so we read the exact instance the
	// ADS state was set on -- reading the C++ FPView member could be a
	// different component than a BP-added one, which is why the slowdown
	// wasn't applying even though the FOV zoom (driven by that other
	// component) was.
	UQRFPViewComponent* View = CachedView;
	if (!View) View = FindComponentByClass<UQRFPViewComponent>();
	if (View && View->IsADS())
	{
		LookVector *= View->ADSLookSensitivityMult;
	}

	// User sensitivity from the settings widget (persisted + live-pushed).
	LookVector *= MouseSensitivityMult;

	AddControllerYawInput(LookVector.X);
	AddControllerPitchInput(LookVector.Y);
}

void AQRCharacter::StartSprint()
{
	if (!CanSprint()) return;
	SetSprinting(true);
	// Replicated bIsSprinting + server movement speed — without the RPC
	// the server never knew a client was sprinting (rubber-banding).
	if (!HasAuthority()) Server_SetSprinting(true);
}

void AQRCharacter::StopSprint()
{
	SetSprinting(false);
	if (!HasAuthority()) Server_SetSprinting(false);
}

void AQRCharacter::Server_SetSprinting_Implementation(bool bSprint)
{
	if (bSprint && !CanSprint()) return;
	SetSprinting(bSprint);
}

void AQRCharacter::HandleCrouchPressed()
{
	if (bIsCrouched) UnCrouch();
	else Crouch();
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
	// F while an interaction overlay is open closes it (toggle) and
	// restores game input. Without this the bench stacked a fresh widget
	// per press and the dialogue overlay had no exit at all.
	if ((CraftingWidgetOpen && CraftingWidgetOpen->IsInViewport()) ||
		(DialogueWidgetOpen && DialogueWidgetOpen->IsInViewport()))
	{
		if (CraftingWidgetOpen) CraftingWidgetOpen->RemoveFromParent();
		if (DialogueWidgetOpen) DialogueWidgetOpen->RemoveFromParent();
		CraftingWidgetOpen = nullptr;
		DialogueWidgetOpen = nullptr;
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			if (PC->IsLocalController())
			{
				PC->bShowMouseCursor = false;
				PC->SetInputMode(FInputModeGameOnly());
			}
		}
		return;
	}

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
					CraftingWidgetOpen = W;
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
	// mount the dialogue widget locally and let the DoInteract path below
	// actually start the conversation (component lives on the actor,
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
					DialogueWidgetOpen = W;
					// Cursor + UI input so the Continue button is actually
					// clickable — the bench branch always did this, the
					// dialogue branch never did.
					PC->bShowMouseCursor = true;
					FInputModeGameAndUI Mode;
					Mode.SetWidgetToFocus(W->TakeWidget());
					PC->SetInputMode(Mode);
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

	// Authority (single-player / listen host): run the same dispatch the
	// RPC path runs. Previously this only broadcast OnInteract — which
	// has no subscribers — so pickup / loot / dialogue / crash-breach
	// were all dead in single-player.
	DoInteract(CurrentInteractable.Get());
}

void AQRCharacter::OnFirePressed()
{
	// Build mode owns LMB: confirm the ghost placement instead of firing.
	// (Confirm/rotate/exit had NO bindings at all — entering build mode
	// was a one-way trap with an immortal ghost.)
	if (Build && Build->bBuildModeActive)
	{
		Build->TryConfirmPlacement();
		return;
	}
	bFireHeld = true;
	TryFireWeapon();
}

void AQRCharacter::OnFireReleased()
{
	bFireHeld = false;
}

void AQRCharacter::TryFireWeapon()
{
	if (!Weapon || !FirstPersonCamera) return;

	// Client-side rate-of-fire pace gate. Keeps full-auto from spamming a
	// Server_Fire RPC + recoil kick every frame, and enforces the bolt /
	// pump cycling delay locally so the feel matches the server cadence.
	// The server independently re-checks cadence in TryFire (anti-cheat).
	if (!Weapon->CanFire()) return;
	const float NowT = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (NowT < NextLocalFireTime) return;
	NextLocalFireTime = NowT + Weapon->GetFireIntervalSeconds();

	const FVector  Start   = FirstPersonCamera->GetComponentLocation();
	const FVector  Forward = FirstPersonCamera->GetForwardVector();
	const bool     bMoving = GetVelocity().Size2D() > 50.0f;
	bool           bAimed  = false;
	if (UQRFPViewComponent* View = FindComponentByClass<UQRFPViewComponent>())
	{
		bAimed = View->IsADS();
	}

	// Local cosmetic recoil — kick the held weapon mesh the instant we
	// fire so it reads without waiting for the server round-trip. Gated
	// on CanFire (replicated state) so an empty / jammed gun doesn't kick.
	if (IsLocallyControlled() && Weapon->CanFire())
	{
		const float AimMult = bAimed ? 0.5f : 1.0f;
		ApplyWeaponRecoilKick(
			Weapon->RecoilPitch * AimMult,
			FMath::FRandRange(-Weapon->RecoilYawRandomRange, Weapon->RecoilYawRandomRange) * AimMult);

		// View kick — punch the camera up so each shot moves the screen.
		// Snipers (high RecoilPitch) kick hard; the bigger the round, the
		// bigger the climb. ADS keeps the full kick (you feel the recoil
		// through the scope); hip-fire is scaled a touch lower so spray
		// weapons stay controllable. AddControllerPitchInput is negative
		// for "up". A little random yaw adds life.
		const float ViewKick = Weapon->RecoilPitch * CameraRecoilScale * (bAimed ? 1.0f : 0.8f);
		AddControllerPitchInput(-ViewKick);
		AddControllerYawInput(FMath::FRandRange(-ViewKick, ViewKick) * 0.25f);

		// Track the accumulated climb so Tick can pull the muzzle back
		// down after the burst ends (Tarkov-style recoil recovery).
		// Without this, sustained fire walks the camera up permanently
		// and the player ends up staring at the sky.
		AccumulatedViewRecoilPitch += ViewKick;
		TimeSinceLastShot = 0.0f;
	}

	if (!HasAuthority())
	{
		Server_Fire(Start, Forward, bAimed, bMoving);
		return;
	}

	const FQRFireResult Result = Weapon->TryFireFromTrace(Start, Forward, bAimed, bMoving, /*AmmoInstance*/ nullptr);
	UE_LOG(LogTemp, Log, TEXT("[QRCharacter] TryFireWeapon result: bFired=%d bHit=%d dmg=%.1f"),
		Result.bFired ? 1 : 0, Result.bHitSomething ? 1 : 0, Result.Damage);
	if (Result.bFired)
	{
		// Recoil is a local kick on the held weapon mesh — applied in
		// TryFireWeapon above via ApplyWeaponRecoilKick. The camera is
		// deliberately left untouched.

		// Debug-line tracer + hit feedback, OFF by default — the weapon
		// component's replicated Niagara muzzle/tracer/impact FX are the
		// real visuals; these pink lines were dev scaffolding that stayed
		// on top of them.
		if (bDebugTracerLines)
		if (UWorld* W = GetWorld())
		{
			const FVector Muzzle = Start + Forward * 35.0f;
			if (Result.PelletEnds.Num() > 0)
			{
				for (const FVector& End : Result.PelletEnds)
				{
					DrawDebugLine(W, Muzzle, End, FColor(255, 50, 200),
						false, 0.25f, 0, 2.0f);
				}
			}
			else
			{
				// Fallback (shouldn't normally hit): one line.
				const FVector EndPt = Result.bHitSomething
					? Result.HitLocation
					: (Start + Forward * (Weapon->MaxRangeMeters * 100.0f));
				DrawDebugLine(W, Muzzle, EndPt, FColor(255, 50, 200),
					false, 0.25f, 0, 2.0f);
			}
			if (Result.bHitSomething)
			{
				DrawDebugSphere(W, Result.HitLocation, 18.0f, 12,
					FColor::Cyan, false, 1.0f, 0, 2.0f);
			}
		}
	}
}

void AQRCharacter::Server_Fire_Implementation(FVector TraceStart, FVector TraceForward,
	bool bIsAimed, bool bIsMoving)
{
	if (!Weapon) return;
	// Authoritative shot — damage / ammo / FX. Recoil is a local cosmetic
	// kick on the firer's weapon mesh (see TryFireWeapon), not applied here.
	Weapon->TryFireFromTrace(TraceStart, TraceForward, bIsAimed, bIsMoving, nullptr);
}

void AQRCharacter::ApplyWeaponRecoilKick(float PitchUnits, float YawUnits)
{
	// Pitch the muzzle up, add a little random yaw + roll for life, and
	// jolt the mesh back toward the camera. Tick decays it all to zero.
	WeaponRecoilRot.Pitch += PitchUnits * WeaponRecoilPitchScale;
	WeaponRecoilRot.Yaw   += YawUnits   * WeaponRecoilPitchScale;
	WeaponRecoilRot.Roll  += YawUnits   * WeaponRecoilPitchScale * 0.5f;
	// -X on the camera-relative held mesh = toward the player.
	WeaponRecoilLoc.X     -= WeaponRecoilKickback;
}

void AQRCharacter::TryReload()
{
	// Build mode owns R: rotate the ghost a quarter turn.
	if (Build && Build->bBuildModeActive)
	{
		Build->RotateGhost(90.0f);
		return;
	}
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
	DoInteract(Target);
}

void AQRCharacter::DoInteract(AActor* Target)
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

	// Tool-gated crash site: F with the required tool in the pack breaches
	// the interior and scatters its held-back loot. Without it, tell the
	// player what they need (the tool is a key, not a consumable).
	if (AQRCrashSiteActor* Crash = Cast<AQRCrashSiteActor>(Target))
	{
		const bool bWasUnlocked = Crash->bUnlocked;
		if (Crash->TryUnlockWithInventory(Inventory))
		{
			if (!bWasUnlocked)
			{
				UE_LOG(LogTemp, Log, TEXT("[QRCharacter] Breached crash site %s"),
					*Crash->ArchetypeId.ToString());
				NotifyHUD(FText::Format(
					NSLOCTEXT("QR", "CrashUnlocked", "Breached {0} — interior accessible."),
					FText::FromName(Crash->ArchetypeId)));
			}
		}
		else
		{
			NotifyHUD(FText::Format(
				NSLOCTEXT("QR", "CrashLocked", "Sealed. Requires: {0}"),
				FText::FromName(Crash->RequiredToolItemId)));
		}
	}
}

void AQRCharacter::NotifyHUD(const FText& Message)
{
	// Route through AQRHUD::PushNotification when the HUD subclass is in
	// use (BP implements the visual); always mirror to the log so the
	// message is never silently lost while the widget side is unbuilt.
	UE_LOG(LogTemp, Log, TEXT("[QR HUD] %s"), *Message.ToString());
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (AQRHUD* HUD = Cast<AQRHUD>(PC->GetHUD()))
		{
			HUD->PushNotification(Message, 5.0f, false);
		}
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
	// Build mode owns G: leave build mode (and dismiss the piece picker
	// if it's still up) instead of dropping the held blueprint item.
	if (Build && Build->bBuildModeActive)
	{
		Build->ExitBuildMode();
		if (BuildSelectorOpen)
		{
			BuildSelectorOpen->RemoveFromParent();
			BuildSelectorOpen = nullptr;
		}
		return;
	}
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

	// Wildlife — spawn in front of the player and decrement one unit.
	if (Def && Def->Category == EQRItemCategory::Wildlife && WildlifeActorClass)
	{
		// Ground-trace the spawn point: the old fixed -30 cm offset left
		// creative-spawned animals hovering at chest height forever.
		FVector SpawnLoc = GetActorLocation() + GetActorForwardVector() * 300.0f;
		{
			FHitResult Ground;
			FCollisionQueryParams QP(SCENE_QUERY_STAT(QRDropGround), false, this);
			if (GetWorld()->LineTraceSingleByChannel(Ground,
				SpawnLoc + FVector(0, 0, 300.0f), SpawnLoc - FVector(0, 0, 2000.0f),
				ECC_Visibility, QP))
			{
				SpawnLoc.Z = Ground.ImpactPoint.Z;
			}
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		Params.Owner = this;

		// Prefer the REAL species pawn — AI controller, herds, predator/
		// prey hunting, proper gravity. The legacy static wanderer (no AI,
		// Z pinned at spawn) is only the fallback for wildlife items with
		// no matching AQRWildlifeBase subclass.
		UClass* SpeciesCls = nullptr;
		{
			TArray<UClass*> Derived;
			GetDerivedClasses(AQRWildlifeBase::StaticClass(), Derived, true);
			for (UClass* C : Derived)
			{
				if (C->HasAnyClassFlags(CLASS_Abstract)) continue;
				const AQRWildlifeBase* CDO = C->GetDefaultObject<AQRWildlifeBase>();
				if (CDO && CDO->SpeciesId == Def->ItemId)
				{
					SpeciesCls = C;
					break;
				}
			}
		}

		if (SpeciesCls)
		{
			const AQRWildlifeBase* CDO = SpeciesCls->GetDefaultObject<AQRWildlifeBase>();
			const float HoistCm = FMath::Max(CDO ? CDO->BodyHeightMeters : 1.0f, 0.5f) * 100.0f;
			if (GetWorld()->SpawnActor<AQRWildlifeBase>(SpeciesCls,
					SpawnLoc + FVector(0, 0, HoistCm), GetActorRotation(), Params))
			{
				Inventory->TryRemoveItem(Def->ItemId, 1);
			}
			return;
		}

		if (AQRWildlifeActor* Animal = GetWorld()->SpawnActor<AQRWildlifeActor>(
				WildlifeActorClass, SpawnLoc + FVector(0, 0, 40.0f), GetActorRotation(), Params))
		{
			Animal->InitializeFrom(Def, 1);
			// Drops are creative-mode: don't make the animal flee the
			// player or you get the marching-mirror effect.
			Animal->bIgnorePlayer = true;
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
					BuildSelectorOpen = W;
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

void AQRCharacter::QR_Exposure(float NewEV)
{
	// NewEV is the exposure COMPENSATION (bias) in stops. Higher = brighter,
	// lower = darker. Stored in LockedExposureEV for the editor knob.
	LockedExposureEV = FMath::Clamp(NewEV, -8.0f, 8.0f);
	if (!FirstPersonCamera) return;

	// Bounded histogram AUTO exposure: the camera adapts across the huge
	// day<->Jovianlight-night luminance swing instead of being pinned to a
	// single EV (which made noon wash out or night go black). The wide
	// min/max range lets it stop down fully for the bright daylit scene
	// (no white-out) and open up for night, while the clamps stop it from
	// running away. ExposureBias is the user offset on top.
	FPostProcessSettings& PP = FirstPersonCamera->PostProcessSettings;
	PP.bOverride_AutoExposureMethod = true;
	PP.AutoExposureMethod = AEM_Histogram;
	PP.bOverride_AutoExposureMinBrightness = true;
	PP.AutoExposureMinBrightness = -2.0f;   // EV100 floor (night)
	PP.bOverride_AutoExposureMaxBrightness = true;
	PP.AutoExposureMaxBrightness = 14.0f;   // EV100 ceiling (bright day)
	PP.bOverride_AutoExposureBias = true;
	PP.AutoExposureBias = LockedExposureEV;
	PP.bOverride_AutoExposureSpeedUp = true;
	PP.AutoExposureSpeedUp = 6.0f;
	PP.bOverride_AutoExposureSpeedDown = true;
	PP.AutoExposureSpeedDown = 6.0f;

	UE_LOG(LogTemp, Log, TEXT("[QR_Exposure] exposure bias %.2f (adaptive)"), LockedExposureEV);
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
	if (!HasAuthority())
	{
		// Local ADS so the owning client's view zooms immediately — the
		// server's FPView copy (set in DoUseHeld) is not the one this
		// client's camera reads.
		ApplyLocalADSPreview(bPressed);
		Server_UseHeld(bPressed);
		return;
	}
	DoUseHeld(bPressed);
}

void AQRCharacter::ApplyLocalADSPreview(bool bPressed)
{
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
	if (Def->Category == EQRItemCategory::Weapon || Def->Category == EQRItemCategory::Attachment)
	{
		if (UQRFPViewComponent* View = FindComponentByClass<UQRFPViewComponent>())
		{
			View->SetADS(true);
			bUseStartedADS = true;
		}
	}
}

void AQRCharacter::SetMouseSensitivity(float NewMult)
{
	MouseSensitivityMult = FMath::Clamp(NewMult, 0.1f, 4.0f);
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
	UE_LOG(LogTemp, Log, TEXT("[QRCharacter] DoUseHeld(pressed) — held=%s def=%s category=%d"),
		Held ? TEXT("yes") : TEXT("null"),
		Def ? *Def->ItemId.ToString() : TEXT("null"),
		Def ? (int32)Def->Category : -1);
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
	const UQRItemDefinition* HandDef = nullptr;
	if (Inventory && Inventory->HandSlot)
	{
		HandDef = Inventory->HandSlot->Definition;
		if (HandDef)
		{
			TargetMesh = HandDef->WorldMesh.LoadSynchronous();

			// Auto-resolve fallback: if the item def's WorldMesh slot is
			// empty, probe SM_<ItemId> across every mesh bucket (the old
			// weapons_assets-only probe left most non-weapon items
			// invisible in hand — playtest report).
			if (!TargetMesh)
			{
				static const TCHAR* Buckets[] = {
					TEXT("weapons_assets"), TEXT("Weapons"),
					TEXT("Handheld"), TEXT("items_handheld"),
					TEXT("Food"), TEXT("food_assets"),
					TEXT("Building"), TEXT("walls_structures"),
					TEXT("Remnant"), TEXT("remnant_assets"),
					TEXT("AmmoAttachments"), TEXT("attachments_ammo"),
					TEXT("Clothing"), TEXT("cosmetic_clothing"),
					TEXT("stations"), TEXT("POIProps"), TEXT("poi_props"),
					TEXT("wildlife"), TEXT("Flora"),
				};
				const FString Id = HandDef->ItemId.ToString();
				for (const TCHAR* Bucket : Buckets)
				{
					const FString Path = FString::Printf(
						TEXT("/Game/Meshes/%s/SM_%s.SM_%s"), Bucket, *Id, *Id);
					TargetMesh = LoadObject<UStaticMesh>(
						nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
					if (TargetMesh) break;
				}
			}
		}
	}

	// Diagnostic — fires every time a hotbar slot becomes active or the
	// inventory changes. Tells you why the held mesh might be invisible:
	//   no HandSlot      = hotbar didn't equip anything
	//   HandSlot, no Def = item instance exists but has no definition
	//   Def, no WorldMesh= definition exists but mesh slot is empty / soft-ptr unresolved
	UE_LOG(LogTemp, Log,
		TEXT("[QRCharacter] RefreshHeldItemMesh — handSlot=%s def=%s mesh=%s visible=%d"),
		(Inventory && Inventory->HandSlot) ? TEXT("yes") : TEXT("null"),
		HandDef ? *HandDef->ItemId.ToString() : TEXT("null"),
		TargetMesh ? *TargetMesh->GetName() : TEXT("null"),
		TargetMesh != nullptr ? 1 : 0);

	HeldItemMesh->SetStaticMesh(TargetMesh);
	HeldItemMesh->SetVisibility(TargetMesh != nullptr);

	// Creative-mode auto-equip for weapons: the QRWeaponComponent
	// defaults to CurrentAmmo=0 + WeaponState=Holstered, so CanFire()
	// returns false on every LMB until you "reload" — but there's no
	// real ammo pipeline yet. Top the mag off here whenever a Weapon-
	// category item becomes active so LMB actually fires. Clear back
	// to Holstered when the slot becomes empty so the component isn't
	// claiming to be ready while you're empty-handed.
	if (Weapon)
	{
		if (HandDef && HandDef->Category == EQRItemCategory::Weapon)
		{
			// Pick the fire mode + rate of fire for this specific gun
			// (full-auto SMG/carbine, semi pistol/DMR, bolt/pump sniper &
			// shotgun). Name-based until the armory DataTable is wired in C++.
			Weapon->ConfigureForWeaponId(HandDef->ItemId);
			// TESTING: unlimited ammo so every gun is range-ready, and
			// clear any leftover jam/fouling so a previously-gunked weapon
			// doesn't come back Jammed.
			Weapon->bUnlimitedAmmo = true;
			Weapon->bIsJammed = false;
			Weapon->FoulingFactor = 0.0f;
			Weapon->CurrentAmmo = Weapon->MagazineCapacity;
			Weapon->WeaponState = EQRWeaponState::Ready;
		}
		else
		{
			Weapon->WeaponState = EQRWeaponState::Holstered;
		}
	}

	// Uniform held-item scale: drive every weapon / prop down to a
	// consistent ~25 cm visible footprint regardless of how the source
	// FBX was authored. SM_WPN_LONGRANGE_SNIPER imports at real-world
	// metres (~150 cm) and at scale 1.0 it fills the whole screen;
	// the pistol mesh is ~20 cm so a fixed scale wouldn't suit both.
	// Compute from the mesh's bounds so every item lands at the same
	// visible size.
	if (TargetMesh)
	{
		// Scale by the mesh's LONGEST horizontal extent (X) so guns sit at
		// roughly the right length in first-person view (the previous
		// max-of-3-axes scaling shrunk long thin weapons to read tiny).
		// Target ~30 cm half-length = 60 cm gun in hand, which matches a
		// real carbine / SMG silhouette.
		const FBoxSphereBounds B = TargetMesh->GetBounds();
		const float LongExtent = FMath::Max(B.BoxExtent.X, B.BoxExtent.Y);
		const float TargetHalfLengthCm = 30.0f;
		const float S = (LongExtent > 0.01f) ? (TargetHalfLengthCm / LongExtent) : 1.0f;
		HeldItemMesh->SetRelativeScale3D(FVector(S));
	}
	else
	{
		HeldItemMesh->SetRelativeScale3D(FVector(1.0f));
	}

	// Scope detection — long-range sniper or any weapon with ItemId
	// containing SNIPER / DMR / SCOPE. The v8 patch's long-range sniper
	// and the 8X / 16X optic attachments drive the magnification tier:
	//   LONGRANGE  → 4× (ScopeFOV baseline)
	//   ATT_8X     → 2× ScopeFOV (=10° effective)
	//   ATT_16X    → 4× ScopeFOV (=5° effective)
	// Name-based until the attachment runtime exposes EquippedAttachmentIds.
	bool bHasScope = false;
	float ScopeZoom = 1.0f;
	if (Inventory && Inventory->HandSlot && Inventory->HandSlot->Definition)
	{
		const FString Id = Inventory->HandSlot->Definition->ItemId.ToString().ToUpper();
		bHasScope = Id.Contains(TEXT("SNIPER"))
				 || Id.Contains(TEXT("DMR"))
				 || Id.Contains(TEXT("SCOPE"))
				 || Id.Contains(TEXT("LONGRANGE"));
		if (Id.Contains(TEXT("LONGRANGE")) || Id.Contains(TEXT("16X")))
		{
			ScopeZoom = 4.0f;
		}
		else if (Id.Contains(TEXT("8X")))
		{
			ScopeZoom = 2.0f;
		}
	}
	if (CachedView)
	{
		CachedView->SetScopeAvailable(bHasScope);
		CachedView->SetScopeZoomMultiplier(ScopeZoom);
	}

	// Handedness applied last so the negative-Y scale flip composes with
	// the uniform bounds-based scale set above. Position + rotation are
	// mirrored too -- the recoil delta in Tick adds atop the mirrored base.
	ApplyHandednessToHeldMesh();
}

void AQRCharacter::ApplyHandednessToHeldMesh()
{
	if (!HeldItemMesh) return;

	// Right-handed defaults -- camera-local: +X forward, +Y right, +Z up.
	// Kept in sync with the constructor's initial values for HeldItemBase*.
	// Mirroring across the XZ plane (negate Y) flips the gun to the left
	// hand and inverts Yaw + Roll so e.g. the muzzle still points away.
	HeldItemBaseLocation = FVector(38.0f, 9.0f, -14.0f);
	HeldItemBaseRotation = FRotator(-2.0f, -3.0f, 0.0f);

	if (bIsLeftHanded)
	{
		HeldItemBaseLocation.Y    = -HeldItemBaseLocation.Y;
		HeldItemBaseRotation.Yaw  = -HeldItemBaseRotation.Yaw;
		HeldItemBaseRotation.Roll = -HeldItemBaseRotation.Roll;
	}

	HeldItemMesh->SetRelativeLocation(HeldItemBaseLocation);
	HeldItemMesh->SetRelativeRotation(HeldItemBaseRotation);

	// Mirror the geometry itself so the ejection port, charging handle,
	// scope offset etc. land on the visually-correct side. Preserves the
	// uniform scale magnitude set above; only flips the sign on Y.
	FVector Scale = HeldItemMesh->GetRelativeScale3D();
	Scale.Y = FMath::Abs(Scale.Y) * (bIsLeftHanded ? -1.0f : 1.0f);
	HeldItemMesh->SetRelativeScale3D(Scale);
}

void AQRCharacter::SetLeftHanded(bool bLeft)
{
	if (bIsLeftHanded == bLeft) return;
	bIsLeftHanded = bLeft;
	RefreshHeldItemMesh();
}

void AQRCharacter::TickThirdPersonAnim()
{
	USkeletalMeshComponent* Body = GetMesh();
	if (!Body || !Body->GetSkeletalMeshAsset()) return;
	if (Body->GetAnimationMode() != EAnimationMode::AnimationSingleNode) return;

	const float Speed = GetVelocity().Size2D();

	// Pick by movement state; each slot falls back to the previous tier
	// so a partially-authored set still animates.
	TSoftObjectPtr<UAnimSequence> Want = TPIdleAnim;
	if (bIsCrouched && !TPCrouchAnim.IsNull())
	{
		Want = TPCrouchAnim;
	}
	else if (Speed >= SprintSpeed * 0.75f && !TPRunAnim.IsNull())
	{
		Want = TPRunAnim;
	}
	else if (Speed >= 15.0f && !TPWalkAnim.IsNull())
	{
		Want = TPWalkAnim;
	}

	// Scale loop speed to actual velocity every tick so feet track the
	// ground instead of moonwalking — walk clip is authored for
	// WalkSpeed, run clip for SprintSpeed.
	if (UAnimSingleNodeInstance* Single = Body->GetSingleNodeInstance())
	{
		float Rate = 1.0f;
		if (Speed >= 15.0f)
		{
			const float Ref = (Speed >= SprintSpeed * 0.75f) ? SprintSpeed : WalkSpeed;
			Rate = FMath::Clamp(Speed / FMath::Max(Ref, 1.0f), 0.6f, 1.6f);
		}
		Single->SetPlayRate(Rate);
	}

	if (Want.IsNull() || Want == TPLastPlayed) return;
	if (UAnimSequence* Seq = Want.LoadSynchronous())
	{
		Body->PlayAnimation(Seq, /*bLooping*/ true);
		TPLastPlayed = Want;
	}
}

void AQRCharacter::RefreshArmour()
{
	if (!Inventory || !Survival) return;

	// Per-piece protection by slot, in stops of damage reduction (additive).
	// e.g. chest is the biggest cover, helm + legs add a smaller share.
	auto SlotShare = [](const FString& Id) -> float
	{
		if (Id.Contains(TEXT("CHEST"))) return 0.55f;
		if (Id.Contains(TEXT("LEGS")))  return 0.25f;
		if (Id.Contains(TEXT("HELM")))  return 0.20f;
		return 0.0f;
	};
	// Metal tier sets the absolute fraction blocked at "full coverage".
	// Stacked piece shares scale this, so a full set hits the metal's cap.
	auto MetalCap = [](const FString& Id) -> float
	{
		if (Id.Contains(TEXT("REMNANT")))    return 0.80f;
		if (Id.Contains(TEXT("SPARKSTONE"))) return 0.65f;
		if (Id.Contains(TEXT("FROSTSPARK")))return 0.55f;
		if (Id.Contains(TEXT("BLACKGLASS")))return 0.45f;
		if (Id.Contains(TEXT("SUNWIRE")))    return 0.45f;
		if (Id.Contains(TEXT("MAGNET")))     return 0.40f;
		if (Id.Contains(TEXT("FERRIC")))     return 0.30f;
		return 0.0f;
	};

	float HeadCov = 0.0f, ChestCov = 0.0f, LegsCov = 0.0f;
	float HeadCap = 0.0f, ChestCap = 0.0f, LegsCap = 0.0f;

	// Only count armour in dedicated worn slots; loose pieces in the body
	// grid don't protect (you have to actually equip them).
	TArray<UQRItemInstance*> Worn;
	Worn.Reserve(3);
	if (UQRItemInstance* H = Inventory->GetEquippedArmour(EQRArmourSlot::Helm))  Worn.Add(H);
	if (UQRItemInstance* C = Inventory->GetEquippedArmour(EQRArmourSlot::Chest)) Worn.Add(C);
	if (UQRItemInstance* L = Inventory->GetEquippedArmour(EQRArmourSlot::Legs))  Worn.Add(L);
	for (UQRItemInstance* Inst : Worn)
	{
		if (!Inst || !Inst->Definition) continue;
		const FString Id = Inst->Definition->ItemId.ToString().ToUpper();
		if (!Id.StartsWith(TEXT("ARM_"))) continue;
		const float Share = SlotShare(Id);
		const float Cap   = MetalCap(Id);
		if (Id.Contains(TEXT("HELM")))  { HeadCov  = FMath::Max(HeadCov,  Share); HeadCap  = FMath::Max(HeadCap,  Cap); }
		if (Id.Contains(TEXT("CHEST"))) { ChestCov = FMath::Max(ChestCov, Share); ChestCap = FMath::Max(ChestCap, Cap); }
		if (Id.Contains(TEXT("LEGS")))  { LegsCov  = FMath::Max(LegsCov,  Share); LegsCap  = FMath::Max(LegsCap,  Cap); }
	}

	// Weighted contribution: each slot covers its share, capped to the metal
	// tier of THAT slot. Full set = HeadCap*0.20 + ChestCap*0.55 + LegsCap*0.25.
	const float Total = (HeadCap * HeadCov) + (ChestCap * ChestCov) + (LegsCap * LegsCov);
	Survival->SetArmourDamageReduction(Total);
	UE_LOG(LogTemp, Log, TEXT("[QRCharacter] Armour refreshed: %.0f%% reduction"), Total * 100.0f);
}

void AQRCharacter::HandleHealthChanged(float NewHealth)
{
	if (NewHealth < LastObservedHealth - KINDA_SMALL_NUMBER)
	{
		QRUISound::PlayHitImpact(this, GetActorLocation());
	}
	LastObservedHealth = NewHealth;
}

float AQRCharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	const float Actual = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	float Incoming = DamageAmount;

	// Shield block: if a shield is equipped and raised (ADS/RMB), mitigate
	// frontal damage. Only blocks hits coming from in front of the player --
	// you can't block what's behind you.
	if (Incoming > 0.0f && Weapon && Weapon->bIsShield && FPView && FPView->IsADS())
	{
		bool bFrontal = true;
		if (DamageCauser)
		{
			const FVector ToThreat = (DamageCauser->GetActorLocation() - GetActorLocation()).GetSafeNormal();
			bFrontal = FVector::DotProduct(GetActorForwardVector(), ToThreat) > 0.25f;
		}
		if (bFrontal)
		{
			const float Before = Incoming;
			Incoming *= (1.0f - FMath::Clamp(Weapon->ShieldDamageReduction, 0.0f, 1.0f));
			UE_LOG(LogTemp, Log, TEXT("[QRCharacter] SHIELD blocked %.0f -> %.0f"),
				Before, Incoming);
		}
	}

	// Only the server mutates vitals; clients see the change via OnRep_Health.
	if (HasAuthority() && Survival && Incoming > 0.0f)
	{
		Survival->ApplyDamage(Incoming, EQRInjuryType::Bleeding);
	}
	return Actual;
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

void AQRCharacter::Revive(FVector Location, FRotator Rotation)
{
	if (!HasAuthority()) return;

	// Refill vitals (clears bIsDead so the survival component ticks again).
	if (Survival)
	{
		Survival->Revive();
		LastObservedHealth = Survival->Health;
	}

	// Undo the death ragdoll: stop simulating, reattach the mesh to the
	// capsule, and snap it back to its resting pose.
	if (USkeletalMeshComponent* M = GetMesh())
	{
		M->SetSimulatePhysics(false);
		M->AttachToComponent(GetCapsuleComponent(),
			FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		M->SetRelativeLocationAndRotation(MeshBaseRelLocation, MeshBaseRelRotation);
	}

	// Restore capsule collision + walking movement.
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Walking);
	}

	// Teleport to the respawn point.
	SetActorLocationAndRotation(Location, Rotation, /*bSweep*/ false,
		/*OutHit*/ nullptr, ETeleportType::TeleportPhysics);

	// Re-enable input on the controlling PC (OnDied disabled it).
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->EnableInput(PC);
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
