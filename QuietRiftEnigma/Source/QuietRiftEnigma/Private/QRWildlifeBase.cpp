#include "QRWildlifeBase.h"
#include "QRWildlifeAIController.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/DamageEvents.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"

AQRWildlifeBase::AQRWildlifeBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AQRWildlifeAIController::StaticClass();

	// Placeholder visible body. The species classes don't assign a
	// SkeletalMesh, so without this a spawned animal is an invisible
	// capsule that can still bite the player ("died to nothing"). Attach
	// to the capsule root; sized + shown/hidden in SetupFallbackVisual.
	FallbackMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FallbackMesh"));
	FallbackMesh->SetupAttachment(GetCapsuleComponent());
	FallbackMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FallbackMesh->SetCastShadow(true);

	// Walk on the ground under gravity and conform to terrain. Without
	// this an animal dropped on a slope would slide/float along a flat
	// plane instead of following the hill. Movement mode is set to walking
	// so CharacterMovement keeps the capsule glued to the floor every tick.
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->GravityScale            = 1.0f;
		Move->DefaultLandMovementMode = MOVE_Walking;
		Move->bConstrainToPlane       = false;
		Move->SetWalkableFloorAngle(50.0f);   // climb fairly steep terrain
		Move->bUseRVOAvoidance        = false;
		Move->bOrientRotationToMovement = true; // face travel direction
		Move->RotationRate            = FRotator(0.0f, 360.0f, 0.0f);
	}

	// Let the controller, not the spawn rotation, drive facing.
	bUseControllerRotationYaw = false;
}

void AQRWildlifeBase::ApplyBodySizing()
{
	const float HeightCm = FMath::Max(BodyHeightMeters * 100.0f, 20.0f);
	const float LengthCm = FMath::Max(BodyLengthMeters * 100.0f, 20.0f);
	const float HalfHeight = HeightCm * 0.5f;

	// Radius from body length, capped below the half-height so the capsule
	// stays geometrically valid (UE requires radius <= half-height).
	const float Radius = FMath::Clamp(LengthCm * 0.25f, 10.0f, HalfHeight - 1.0f);

	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (Capsule)
	{
		Capsule->SetCapsuleSize(Radius, HalfHeight);
	}

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		// Bigger animals step over taller obstacles so they don't get
		// snagged on terrain bumps that are trivial relative to their size.
		Move->MaxStepHeight = FMath::Clamp(HeightCm * 0.25f, 45.0f, 400.0f);

		// Keep the *nav agent* footprint modest even for megafauna. The
		// project ships one default RecastNavMesh; a 2.5 m-radius agent would
		// have no matching nav data and the animal would never path. The
		// collision capsule above is still full size — only the pathfinding
		// footprint is capped so large animals keep moving.
		Move->NavAgentProps.AgentRadius = FMath::Min(Radius, 60.0f);
		Move->NavAgentProps.AgentHeight = FMath::Min(HeightCm, 200.0f);
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp) return;

	// Drop the mesh so its feet rest at the bottom of the capsule.
	MeshComp->SetRelativeLocation(FVector(0.0f, 0.0f, -HalfHeight));

	if (bAutoFitMeshToBody && MeshComp->GetSkeletalMeshAsset())
	{
		// Measure the mesh at unit scale, then rescale so its rendered
		// height matches BodyHeightMeters. The source FBX scale is then
		// irrelevant — the animal always shows at its canonical size.
		MeshComp->SetRelativeScale3D(FVector::OneVector);
		const FBoxSphereBounds B = MeshComp->CalcBounds(FTransform::Identity);
		const float MeshHeightCm = FMath::Max(B.BoxExtent.Z * 2.0f, 1.0f);
		const float Fit = HeightCm / MeshHeightCm;
		MeshComp->SetRelativeScale3D(FVector(Fit));
	}
}

void AQRWildlifeBase::SetupFallbackVisual()
{
	if (!FallbackMesh) return;

	// If a real skeletal mesh is assigned (designer wired one in a BP),
	// the placeholder isn't needed — hide it and bail.
	USkeletalMeshComponent* SkelComp = GetMesh();
	if (SkelComp && SkelComp->GetSkeletalMeshAsset())
	{
		FallbackMesh->SetVisibility(false);
		return;
	}

	const float HalfHeightCm = GetCapsuleComponent()
		? GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
		: BodyHeightMeters * 50.0f;
	const float TargetHeightCm = FMath::Max(BodyHeightMeters * 100.0f, 20.0f);

	// 1) Try the real species mesh. Order of attempts:
	//      a. FallbackMeshPath override (designer-set on a BP),
	//      b. Auto-derived path from the C++ class name. Subclasses are
	//         named "AQRWildlife_<Species>"; the static-mesh assets shipped
	//         from the Blender generator under /Game/Meshes/wildlife/ are
	//         SM_ANM_<Species> (older naming) or SM_ANI_<Species> (newer).
	//      Auto-derivation means every species class that has a mesh on
	//      disk gets it without any per-subclass wiring.
	UStaticMesh* RealMesh = nullptr;
	if (!FallbackMeshPath.IsEmpty())
	{
		RealMesh = LoadObject<UStaticMesh>(nullptr, *FallbackMeshPath);
	}
	if (!RealMesh)
	{
		FString ClassName = GetClass()->GetName(); // e.g. "QRWildlife_AshbackBoar"
		const FString Prefix = TEXT("QRWildlife_");
		if (ClassName.StartsWith(Prefix))
		{
			const FString Species = ClassName.RightChop(Prefix.Len());
			const TCHAR* Folders[] = { TEXT("SM_ANM_"), TEXT("SM_ANI_"), TEXT("SM_PRD_") };
			for (const TCHAR* Pfx : Folders)
			{
				const FString Path = FString::Printf(
					TEXT("/Game/Meshes/wildlife/%s%s.%s%s"),
					Pfx, *Species, Pfx, *Species);
				RealMesh = LoadObject<UStaticMesh>(nullptr, *Path);
				if (RealMesh) break;
			}
		}
	}

	if (RealMesh)
	{
		FallbackMesh->SetStaticMesh(RealMesh);
		FallbackMesh->SetVisibility(true);

		// Measure at unit scale, then rescale so the rendered height
		// matches BodyHeightMeters (matches the auto-fit logic on the
		// skeletal-mesh path). Align the mesh's bottom to the capsule
		// bottom, regardless of where the source FBX's pivot landed.
		FallbackMesh->SetRelativeRotation(FRotator::ZeroRotator);
		FallbackMesh->SetRelativeScale3D(FVector::OneVector);
		FallbackMesh->SetRelativeLocation(FVector::ZeroVector);
		const FBoxSphereBounds B = FallbackMesh->CalcBounds(FTransform::Identity);
		const float MeshHeightCm = FMath::Max(B.BoxExtent.Z * 2.0f, 1.0f);
		const float Fit = TargetHeightCm / MeshHeightCm;
		FallbackMesh->SetRelativeScale3D(FVector(Fit));

		// After scaling, recompute bounds to figure out the mesh-bottom
		// offset from its pivot, then drop it so its feet sit at the
		// capsule bottom.
		const FBoxSphereBounds B2 = FallbackMesh->CalcBounds(FallbackMesh->GetRelativeTransform());
		const float MeshBottomZ = B2.Origin.Z - B2.BoxExtent.Z;
		FallbackMesh->AddLocalOffset(FVector(0.0f, 0.0f, -HalfHeightCm - MeshBottomZ));
		return;
	}

	// 2) No real mesh available — shape-code by behaviour role so even a
	//    placeholder reads clearly: Cube = predator, Cylinder = prey,
	//    Sphere = everything else.
	const TCHAR* ShapePath = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	switch (BehaviorRole)
	{
	case EQRWildlifeBehaviorRole::Predator:
		ShapePath = TEXT("/Engine/BasicShapes/Cube.Cube");
		break;
	case EQRWildlifeBehaviorRole::Prey:
		ShapePath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
		break;
	default:
		ShapePath = TEXT("/Engine/BasicShapes/Sphere.Sphere");
		break;
	}

	UStaticMesh* Shape = LoadObject<UStaticMesh>(nullptr, ShapePath);
	if (!Shape)
	{
		Shape = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	}
	if (!Shape)
	{
		FallbackMesh->SetVisibility(false);
		return;
	}

	FallbackMesh->SetStaticMesh(Shape);
	FallbackMesh->SetVisibility(true);

	// Engine basic shapes are 100 cm, so scale = body dimension in metres.
	// Length along X, narrower width along Y, height along Z. Centre it.
	const float LenM = FMath::Max(BodyLengthMeters, 0.2f);
	const float HtM  = FMath::Max(BodyHeightMeters, 0.2f);
	const float WidM = FMath::Max(LenM * 0.45f, 0.15f);
	FallbackMesh->SetRelativeRotation(FRotator::ZeroRotator);
	FallbackMesh->SetRelativeLocation(FVector::ZeroVector);
	FallbackMesh->SetRelativeScale3D(FVector(LenM, WidM, HtM));
}

void AQRWildlifeBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AQRWildlifeBase, CurrentHealth);
	DOREPLIFETIME(AQRWildlifeBase, AIState);
	DOREPLIFETIME(AQRWildlifeBase, bIsDead);
	DOREPLIFETIME(AQRWildlifeBase, HerdGroupId);
}

void AQRWildlifeBase::BeginPlay()
{
	Super::BeginPlay();
	CurrentHealth = MaxHealth;

	// Size the capsule + mesh to the species' real-world dimensions, and
	// push the walk speed onto the movement component (subclass constructors
	// set MoveSpeedWalk after the base constructor ran).
	ApplyBodySizing();
	// Show a placeholder body so an animal without a skeletal mesh is
	// visible instead of an invisible biting capsule.
	SetupFallbackVisual();
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
		Move->MaxWalkSpeed = MoveSpeedWalk;

	if (HasAuthority())
	{
		// Launch behavior tree via AI controller
		if (AAIController* AIC = Cast<AAIController>(GetController()))
		{
			if (BehaviorTree)
				AIC->RunBehaviorTree(BehaviorTree);
		}
	}
}

float AQRWildlifeBase::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	const float Actual = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	if (HasAuthority() && DamageAmount > 0.0f)
	{
		AActor* Causer = DamageCauser;
		if (!Causer && EventInstigator)
		{
			Causer = EventInstigator->GetPawn();
		}
		TakeDamage_Wildlife(DamageAmount, Causer);
	}
	return Actual;
}

void AQRWildlifeBase::TakeDamage_Wildlife(float Amount, AActor* DamageCauser)
{
	if (!HasAuthority() || bIsDead || Amount <= 0.0f) return;

	CurrentHealth = FMath::Max(0.0f, CurrentHealth - Amount);

	if (CurrentHealth <= 0.0f)
		OnDied(DamageCauser);
	else if (AIState != EQRWildlifeAIState::Fleeing_Injured && CurrentHealth / MaxHealth < 0.3f)
		SetAIState(EQRWildlifeAIState::Fleeing_Injured);
}

void AQRWildlifeBase::SetAIState(EQRWildlifeAIState NewState)
{
	if (AIState == NewState) return;
	AIState = NewState;

	float NewSpeed = MoveSpeedWalk;
	switch (NewState)
	{
	case EQRWildlifeAIState::Fleeing:
	case EQRWildlifeAIState::Fleeing_Injured:
		NewSpeed = MoveSpeedFlee;
		break;
	case EQRWildlifeAIState::Charging:
	case EQRWildlifeAIState::Attacking:
		NewSpeed = MoveSpeedCharge;
		break;
	default:
		NewSpeed = MoveSpeedWalk;
		break;
	}
	GetCharacterMovement()->MaxWalkSpeed = NewSpeed;

	// Propagate to BT blackboard via AI controller
	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		if (UBlackboardComponent* BB = AIC->GetBlackboardComponent())
			BB->SetValueAsEnum(FName("AIState"), static_cast<uint8>(NewState));
	}
}

void AQRWildlifeBase::AlertHerd(AActor* Threat)
{
	if (HerdGroupId == 0 || !GetWorld()) return;

	// Find all wildlife in same herd group and alert them
	for (TActorIterator<AQRWildlifeBase> It(GetWorld()); It; ++It)
	{
		AQRWildlifeBase* Other = *It;
		if (Other && Other != this && Other->HerdGroupId == HerdGroupId && !Other->bIsDead)
			Other->OnThreatDetected(Threat);
	}
}

TArray<FQRWildlifeDrop> AQRWildlifeBase::Harvest()
{
	TArray<FQRWildlifeDrop> Result;
	TArray<FQRWildlifeDrop>& Source = bIsDead ? DeathDrops : HarvestDrops;

	for (const FQRWildlifeDrop& Drop : Source)
	{
		if (FMath::FRand() <= Drop.DropChance)
		{
			FQRWildlifeDrop Actual = Drop;
			Actual.MinQuantity = FMath::RandRange(Drop.MinQuantity, Drop.MaxQuantity);
			Actual.MaxQuantity = Actual.MinQuantity;
			Result.Add(Actual);
		}
	}

	return Result;
}

void AQRWildlifeBase::OnDied_Implementation(AActor* Killer)
{
	bIsDead = true;
	SetAIState(EQRWildlifeAIState::Dead);

	// Ragdoll
	GetMesh()->SetSimulatePhysics(true);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Detach AI controller
	if (AAIController* AIC = Cast<AAIController>(GetController()))
		AIC->UnPossess();

	// TODO: Start carcass despawn timer
}
