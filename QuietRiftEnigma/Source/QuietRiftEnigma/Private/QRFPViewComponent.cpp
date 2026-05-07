#include "QRFPViewComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/UnrealType.h"

UQRFPViewComponent::UQRFPViewComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	// Tick after the camera has updated so our writes win for the frame.
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UQRFPViewComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerCharacter = Cast<ACharacter>(GetOwner());

	// If the character has only one camera component, auto-grab it. Caller
	// can still override via SetCameraTarget for multi-camera setups.
	if (!CameraTarget && OwnerCharacter)
	{
		CameraTarget = OwnerCharacter->FindComponentByClass<UCameraComponent>();
	}
	if (CameraTarget)
	{
		BaseCameraRelLocation = CameraTarget->GetRelativeLocation();
	}
}

void UQRFPViewComponent::SetCameraTarget(UCameraComponent* InCamera)
{
	CameraTarget = InCamera;
	if (CameraTarget)
	{
		BaseCameraRelLocation = CameraTarget->GetRelativeLocation();
	}
}

void UQRFPViewComponent::SetADS(bool bAiming)
{
	bIsADS = bAiming;
}

bool UQRFPViewComponent::QueryIsSprinting() const
{
	// Reflective bIsSprinting lookup so we don't hard-link to AQRCharacter.
	if (!OwnerCharacter) return false;
	const FBoolProperty* Prop = FindFProperty<FBoolProperty>(OwnerCharacter->GetClass(), TEXT("bIsSprinting"));
	if (!Prop) return false;
	return Prop->GetPropertyValue_InContainer(OwnerCharacter);
}

void UQRFPViewComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                        FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!CameraTarget || !OwnerCharacter) return;

	const bool bSprinting = QueryIsSprinting();
	const bool bCrouched  = OwnerCharacter->bIsCrouched;

	// ── 1. FOV blend ────────────────────────────
	// ADS wins over sprint (you can't aim while sprinting in most games anyway).
	float TargetFOV = BaseFOV;
	if (bIsADS)        TargetFOV = ADSFOV;
	else if (bSprinting) TargetFOV = SprintFOV;

	const float NewFOV = FMath::FInterpTo(CameraTarget->FieldOfView, TargetFOV,
	                                        DeltaTime, FOVInterpSpeed);
	CameraTarget->SetFieldOfView(NewFOV);

	// ── 2. Eye-height blend (crouch) ────────────
	const float TargetEyeZ = bCrouched ? CrouchedEyeHeight : StandingEyeHeight;
	FVector RelLoc = BaseCameraRelLocation;
	RelLoc.Z = FMath::FInterpTo(CameraTarget->GetRelativeLocation().Z, TargetEyeZ,
	                             DeltaTime, CrouchInterpSpeed);

	// ── 3. ADS camera offset ────────────────────
	const FVector TargetADSOffset = bIsADS ? ADSCameraOffset : FVector::ZeroVector;
	// We blend ADS offset on X and Y; Z is owned by the eye-height path above.
	const FVector CurOffset(
		CameraTarget->GetRelativeLocation().X - BaseCameraRelLocation.X,
		CameraTarget->GetRelativeLocation().Y - BaseCameraRelLocation.Y,
		0.0f
	);
	const FVector NewOffset(
		FMath::FInterpTo(CurOffset.X, TargetADSOffset.X, DeltaTime, ADSInterpSpeed),
		FMath::FInterpTo(CurOffset.Y, TargetADSOffset.Y, DeltaTime, ADSInterpSpeed),
		0.0f
	);
	RelLoc.X = BaseCameraRelLocation.X + NewOffset.X;
	RelLoc.Y = BaseCameraRelLocation.Y + NewOffset.Y;
	// ADS also pushes Z slightly — fold it into the eye-height target so the
	// two systems compose without fighting.
	if (bIsADS)
	{
		RelLoc.Z += ADSCameraOffset.Z * 1.0f;
	}

	// ── 4. Head bob ─────────────────────────────
	if (bHeadBobEnabled && !bIsADS)
	{
		// Amplitude scales with speed, frequency stays constant.
		const float Speed = OwnerCharacter->GetVelocity().Size2D();
		const float MaxSpeed = OwnerCharacter->GetCharacterMovement()
		                       ? OwnerCharacter->GetCharacterMovement()->MaxWalkSpeed * 1.5f
		                       : 600.0f;
		const float SpeedFactor = FMath::Clamp(Speed / FMath::Max(MaxSpeed, 1.0f), 0.0f, 1.0f);
		// Don't bob when essentially still.
		if (SpeedFactor > 0.05f)
		{
			BobPhase += DeltaTime * BobFrequency * (1.0f + SpeedFactor) * (2.0f * PI);
			const float BobZ = FMath::Sin(BobPhase * 2.0f) * BobAmplitudeZ * SpeedFactor;
			const float BobY = FMath::Sin(BobPhase)        * BobAmplitudeY * SpeedFactor;
			RelLoc.Z += BobZ;
			RelLoc.Y += BobY;
		}
	}
	else
	{
		// Reset phase when not bobbing so resuming starts at zero.
		BobPhase = 0.0f;
	}

	CameraTarget->SetRelativeLocation(RelLoc);
}
