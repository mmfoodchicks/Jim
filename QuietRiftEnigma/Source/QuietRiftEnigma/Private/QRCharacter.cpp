#include "QRCharacter.h"
#include "QRInventoryComponent.h"
#include "QRSurvivalComponent.h"
#include "QRWeaponComponent.h"
#include "QRFactionComponent.h"
#include "QRGameplayTags.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Net/UnrealNetwork.h"
#include "DrawDebugHelpers.h"
#include "Engine/HitResult.h"

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

	SurvivorId = FName("PLR_0001");
}

void AQRCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AQRCharacter, bIsSprinting);
	DOREPLIFETIME_CONDITION(AQRCharacter, bIsOverEncumbered, COND_OwnerOnly);
}

void AQRCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Bind death delegate
	if (Survival)
		Survival->OnDeath.AddDynamic(this, &AQRCharacter::OnDied);

	// Add input mapping context
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
}

void AQRCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EI = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveAction)     EI->BindAction(MoveAction,     ETriggerEvent::Triggered, this, &AQRCharacter::Move);
		if (LookAction)     EI->BindAction(LookAction,     ETriggerEvent::Triggered, this, &AQRCharacter::Look);
		if (JumpAction)     EI->BindAction(JumpAction,     ETriggerEvent::Started,   this, &ACharacter::Jump);
		if (JumpAction)     EI->BindAction(JumpAction,     ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		if (InteractAction) EI->BindAction(InteractAction, ETriggerEvent::Started,   this, &AQRCharacter::TryInteract);
		if (SprintAction)   EI->BindAction(SprintAction,   ETriggerEvent::Started,   this, &AQRCharacter::StartSprint);
		if (SprintAction)   EI->BindAction(SprintAction,   ETriggerEvent::Completed, this, &AQRCharacter::StopSprint);
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
	if (!Survival) return true;
	return !Survival->IsExhausted() && !bIsOverEncumbered;
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

	// Trigger interaction on server if client
	if (!HasAuthority())
	{
		Server_Interact(CurrentInteractable.Get());
		return;
	}

	OnInteract.Broadcast(CurrentInteractable.Get());
}

void AQRCharacter::Server_Interact_Implementation(AActor* Target)
{
	if (!Target) return;
	OnInteract.Broadcast(Target);
}

void AQRCharacter::OnDied_Implementation()
{
	// Disable input, collapse physics, notify game mode
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
		PC->DisableInput(PC);

	GetMesh()->SetSimulatePhysics(true);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
