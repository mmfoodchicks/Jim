#include "QRWildlifeBase.h"
#include "QRWildlifeAIController.h"
#include "QRWorldItem.h"
#include "QRItemDefinition.h"
#include "QRMissionDirector.h"
#include "Engine/AssetManager.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/DamageEvents.h"
#include "Animation/AnimSequence.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "TimerManager.h"

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

	// NOTE: the capsule's ECC_Visibility response is set in BeginPlay, NOT
	// here. Setting it in the constructor gets wiped when the component
	// registers and re-applies the "Pawn" collision profile, which is why
	// bullets were still passing through. See SetupHitCollision().
}

void AQRWildlifeBase::SetupHitCollision()
{
	// The player's weapon line-traces on ECC_Visibility. ACharacter
	// capsules use the "Pawn" profile, which IGNORES Visibility -- so
	// weapon traces fly straight through animals and hit whatever's
	// behind them. Force the capsule (and the visible body mesh) to BLOCK
	// Visibility so shots land and route through TakeDamage. Done in
	// BeginPlay so it survives the profile being applied at registration.
	if (UCapsuleComponent* Cap = GetCapsuleComponent())
	{
		Cap->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}
	// Also make the body itself shootable so hits register on the whole
	// silhouette, not just the small centre capsule. This runs whether
	// the FallbackMesh is the VISIBLE placeholder OR an invisible
	// body-sized hitbox behind a skinned mesh (see SetupFallbackVisual)
	// -- gate on "has a mesh", NOT on visibility, or skinned species end
	// up with only the capsule catching traces and a long/low animal's
	// head, tail and flanks take no damage. Query-only (no physics)
	// keeps it out of movement/overlap logic while catching weapon
	// traces on ECC_Visibility.
	if (FallbackMesh && FallbackMesh->GetStaticMesh())
	{
		FallbackMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		FallbackMesh->SetCollisionObjectType(ECC_WorldDynamic);
		FallbackMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		FallbackMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}
}

void AQRWildlifeBase::SetupSkinnedBody()
{
	USkeletalMeshComponent* SMC = GetMesh();
	if (!SMC) return;

	// Respect a designer-assigned mesh; only fill an empty slot.
	if (!SMC->GetSkeletalMeshAsset())
	{
		if (USkeletalMesh* Body = DefaultBodyMesh.LoadSynchronous())
		{
			SMC->SetSkeletalMesh(Body);
		}
	}
	if (!SMC->GetSkeletalMeshAsset()) return;   // still nothing -- placeholder path

	// Single-node anims only when the species ships an idle and no
	// designer AnimBP is in charge of the mesh.
	UAnimSequence* Idle = IdleAnim.LoadSynchronous();
	if (!Idle || SMC->GetAnimClass()) return;

	SMC->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	SMC->PlayAnimation(Idle, /*bLooping*/ true);
	LastPlayedAnim = Idle;

	GetWorldTimerManager().SetTimer(AnimSwapTimer, this,
		&AQRWildlifeBase::TickAnimSwap, 0.15f, /*bLoop*/ true);
}

void AQRWildlifeBase::TickAnimSwap()
{
	USkeletalMeshComponent* SMC = GetMesh();
	if (!SMC || bIsDead) return;

	const float Speed = GetVelocity().Size2D();
	UAnimSequence* Want = nullptr;
	if (Speed >= RunAnimSpeedThreshold)
	{
		Want = RunAnim.LoadSynchronous();
	}
	if (!Want && Speed >= WalkAnimSpeedThreshold)
	{
		Want = WalkAnim.LoadSynchronous();
	}
	if (!Want)
	{
		Want = IdleAnim.LoadSynchronous();
	}
	if (!Want || LastPlayedAnim.Get() == Want) return;

	SMC->PlayAnimation(Want, /*bLooping*/ true);
	LastPlayedAnim = Want;
}

void AQRWildlifeBase::ApplyBodySizing()
{
	const float HeightCm = FMath::Max(BodyHeightMeters * 100.0f, 20.0f);
	const float LengthCm = FMath::Max(BodyLengthMeters * 100.0f, 20.0f);
	const float HalfHeight = HeightCm * 0.5f;

	// Radius from body length, capped below the half-height so the capsule
	// stays geometrically valid (UE requires radius <= half-height).
	const float Radius = FMath::Clamp(LengthCm * 0.25f, 10.0f, HalfHeight - 1.0f);

	// Auto crit zone. Default = the head (forward + up). Armoured-head
	// species put the weak point at the soft underbelly instead (lower,
	// slightly rear) so you have to flank them, not just aim for the face.
	if (bAutoCritFromBody)
	{
		if (bArmoredHead)
		{
			CritZoneCenterLocal = FVector(-LengthCm * 0.10f, 0.0f, -HeightCm * 0.28f);
			CritZoneRadiusCm = FMath::Max(HeightCm * 0.20f, 18.0f);
		}
		else
		{
			CritZoneCenterLocal = FVector(LengthCm * 0.42f, 0.0f, HeightCm * 0.30f);
			CritZoneRadiusCm = FMath::Max(HeightCm * 0.22f, 20.0f);
		}
	}

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
	if (!MeshComp || !MeshComp->GetSkeletalMeshAsset()) return;

	// ACharacter sets a default -90 deg yaw on GetMesh() (the Mannequin
	// faces -Y). Our wildlife are authored nose-forward (+X), so clear it
	// or every animal renders 90 deg off its travel direction.
	MeshComp->SetRelativeRotation(FRotator::ZeroRotator);
	MeshComp->SetRelativeLocation(FVector(0.0f, 0.0f, -HalfHeight));

	if (bAutoFitMeshToBody)
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

	// Ground the mesh's ACTUAL bottom (not its pivot) to the capsule
	// bottom -- the static FallbackMesh path already does this, but the
	// skinned path skipped it, so any species whose skeletal pivot isn't
	// at its lowest vertex (a hip-pivoted Fab mesh like the German
	// Shepherd, or the hover-authored floaters) rendered above -- or sunk
	// into -- the ground even though the capsule is correctly grounded.
	// That mesh-vs-capsule offset is exactly the "animal floats, ignores
	// gravity" report. Skipped for canon floaters (bGroundBodyToFeet=false).
	if (bGroundBodyToFeet)
	{
		const FBoxSphereBounds GB = MeshComp->CalcBounds(MeshComp->GetRelativeTransform());
		const float MeshBottomZ = GB.Origin.Z - GB.BoxExtent.Z;
		MeshComp->AddLocalOffset(FVector(0.0f, 0.0f, -HalfHeight - MeshBottomZ));
	}
}

void AQRWildlifeBase::SetupFallbackVisual()
{
	if (!FallbackMesh) return;

	// If a real skeletal mesh is the visible body, the placeholder isn't
	// shown -- but keep the FallbackMesh as an INVISIBLE, body-sized
	// collision hull. The rigged wildlife import uses
	// create_physics_asset=False, so a simple ECC_Visibility weapon trace
	// cannot hit the skeletal mesh at all; without this hull only the thin
	// vertical capsule is shootable and most shots on a horizontal animal
	// pass straight through (this is why animals took no damage).
	USkeletalMeshComponent* SkelComp = GetMesh();
	if (SkelComp && SkelComp->GetSkeletalMeshAsset())
	{
		if (UStaticMesh* Hull = LoadObject<UStaticMesh>(
				nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
		{
			const float LenM = FMath::Max(BodyLengthMeters, 0.2f);
			const float HtM  = FMath::Max(BodyHeightMeters, 0.2f);
			const float WidM = FMath::Max(LenM * 0.45f, 0.15f);
			FallbackMesh->SetStaticMesh(Hull);
			FallbackMesh->SetRelativeRotation(FRotator::ZeroRotator);
			FallbackMesh->SetRelativeLocation(FVector::ZeroVector);  // centred on capsule
			FallbackMesh->SetRelativeScale3D(FVector(LenM, WidM, HtM));
		}
		FallbackMesh->SetVisibility(false);
		FallbackMesh->SetHiddenInGame(true);   // invisible, still collides
		FallbackMesh->SetCastShadow(false);
		return;                                // SetupHitCollision arms the hull
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
	// LOAD_NoWarn keeps the log clean when a species' FBX hasn't been
	// baked yet -- we're probing several candidate paths by design and the
	// "first hit wins" pattern would otherwise spam Warning entries for
	// every miss on every wildlife spawn.
	const uint32 QuietLoadFlags = LOAD_NoWarn | LOAD_Quiet;
	UStaticMesh* RealMesh = nullptr;
	if (!FallbackMeshPath.IsEmpty())
	{
		RealMesh = LoadObject<UStaticMesh>(nullptr, *FallbackMeshPath, nullptr, QuietLoadFlags);
	}
	if (!RealMesh)
	{
		FString ClassName = GetClass()->GetName(); // e.g. "QRWildlife_AshbackBoar"
		const FString Prefix = TEXT("QRWildlife_");
		if (ClassName.StartsWith(Prefix))
		{
			// Species suffix in the class's CamelCase form (e.g.
			// "NestweaverDrifter") and its UPPER_SNAKE EntityId form
			// ("NESTWEAVER_DRIFTER"). The legacy generator wrote
			// CamelCase filenames (SM_ANM_AshbackBoar) while the v15 pass
			// writes the EntityId verbatim (SM_ANI_NESTWEAVER_DRIFTER), so
			// we probe both.
			const FString SpeciesCamel = ClassName.RightChop(Prefix.Len());
			FString SpeciesSnake;
			for (int32 i = 0; i < SpeciesCamel.Len(); ++i)
			{
				const TCHAR Ch = SpeciesCamel[i];
				if (i > 0 && FChar::IsUpper(Ch)) SpeciesSnake.AppendChar(TEXT('_'));
				SpeciesSnake.AppendChar(FChar::ToUpper(Ch));
			}

			// Import path varies by tool: UE auto-import mirrors the disk
			// folder ("wildlife", lowercase); qr_seed_items.py buckets to
			// "Wildlife" (capital). Probe both. Prefixes ANM/ANI/PRD and
			// both name casings round out the matrix.
			const TCHAR* Folders[] = { TEXT("wildlife"), TEXT("Wildlife") };
			const TCHAR* Prefixes[] = { TEXT("SM_ANM_"), TEXT("SM_ANI_"), TEXT("SM_PRD_") };
			const FString Names[] = { SpeciesCamel, SpeciesSnake };

			for (const TCHAR* Folder : Folders)
			{
				for (const TCHAR* Pfx : Prefixes)
				{
					for (const FString& Nm : Names)
					{
						const FString Asset = FString::Printf(TEXT("%s%s"), Pfx, *Nm);
						const FString Path = FString::Printf(
							TEXT("/Game/Meshes/%s/%s.%s"), Folder, *Asset, *Asset);
						RealMesh = LoadObject<UStaticMesh>(nullptr, *Path, nullptr, QuietLoadFlags);
						if (RealMesh) break;
					}
					if (RealMesh) break;
				}
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

	// Assign the skinned body FIRST so the sizing / fallback / collision
	// passes below all see the real mesh instead of the placeholder.
	SetupSkinnedBody();
	// Size the capsule + mesh to the species' real-world dimensions, and
	// push the walk speed onto the movement component (subclass constructors
	// set MoveSpeedWalk after the base constructor ran).
	ApplyBodySizing();
	// Show a placeholder body so an animal without a skeletal mesh is
	// visible instead of an invisible biting capsule.
	SetupFallbackVisual();
	// Make the animal shootable (must run after registration + after the
	// visible mesh is set up).
	SetupHitCollision();
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
		float FinalDamage = DamageAmount;

		// Headshot / crit zone: point-damage events carry the hit location.
		// Transform it into actor-local space and check the distance to the
		// species' crit zone; a hit inside multiplies the damage.
		if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
		{
			const FPointDamageEvent& Pt = static_cast<const FPointDamageEvent&>(DamageEvent);
			const FVector LocalHit = GetActorTransform().InverseTransformPosition(Pt.HitInfo.ImpactPoint);
			const float DistToCrit = FVector::Dist(LocalHit, CritZoneCenterLocal);
			if (DistToCrit <= CritZoneRadiusCm)
			{
				FinalDamage *= CritDamageMultiplier;
				UE_LOG(LogTemp, Log, TEXT("[Wildlife] %s CRIT HIT x%.1f (%.0f -> %.0f)"),
					*GetName(), CritDamageMultiplier, DamageAmount, FinalDamage);
			}
		}

		AActor* Causer = DamageCauser;
		if (!Causer && EventInstigator)
		{
			Causer = EventInstigator->GetPawn();
		}
		TakeDamage_Wildlife(FinalDamage, Causer);
	}
	return Actual;
}

void AQRWildlifeBase::TakeDamage_Wildlife(float Amount, AActor* DamageCauser)
{
	UE_LOG(LogTemp, Log, TEXT("[Wildlife] %s TakeDamage_Wildlife amt=%.1f hp=%.1f/%.1f"),
		*GetName(), Amount, CurrentHealth, MaxHealth);
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

	UE_LOG(LogTemp, Log, TEXT("[Wildlife] %s died (killer=%s)"),
		*GetName(), Killer ? *Killer->GetName() : TEXT("<none>"));

	// Disable collision + movement so the corpse doesn't bump the player.
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}

	// Skinned species: stop the swap loop and hold the death pose.
	GetWorldTimerManager().ClearTimer(AnimSwapTimer);
	bool bPlayedDeathAnim = false;
	if (USkeletalMeshComponent* SMC = GetMesh())
	{
		if (SMC->GetSkeletalMeshAsset())
		{
			if (UAnimSequence* Death = DeathAnim.LoadSynchronous())
			{
				SMC->PlayAnimation(Death, /*bLooping*/ false);
				bPlayedDeathAnim = true;
			}
		}
	}

	// Topple the visible body. GetMesh() ragdolls only work if a real
	// skeletal mesh is assigned -- the v15 species don't ship one, so
	// the body is on FallbackMesh. Flip it onto its side so death reads
	// clearly even without an animated death.
	if (FallbackMesh && FallbackMesh->IsVisible())
	{
		FallbackMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		// Rotate the corpse 80° around X (rolled onto its side) and drop
		// half a body-height so it lies on the ground.
		const float DropCm = FMath::Max(BodyHeightMeters, 0.5f) * 40.0f;
		FRotator R = FallbackMesh->GetRelativeRotation();
		R.Roll = 80.0f;
		FallbackMesh->SetRelativeRotation(R);
		FallbackMesh->AddLocalOffset(FVector(0.0f, 0.0f, -DropCm));
	}
	// Real skeletal mesh path -- ragdoll, but only when no DeathAnim is
	// holding the pose: SetSimulatePhysics takes the mesh out of
	// animation-driven pose the same frame, so a configured death anim
	// would never visibly play. Anim wins when authored; physics is the
	// fallback for meshes that ship a PhysicsAsset but no death clip.
	if (!bPlayedDeathAnim)
	{
		if (USkeletalMeshComponent* SkelComp = GetMesh())
		{
			if (SkelComp->GetSkeletalMeshAsset())
			{
				SkelComp->SetSimulatePhysics(true);
			}
		}
	}

	// Detach AI controller so the corpse doesn't keep ticking decisions.
	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		AIC->UnPossess();
	}

	// Roll the death drop table and scatter the loot as pickupable world
	// items. The tables existed but nothing ever rolled them — killing
	// any animal yielded literally nothing before the corpse despawned.
	if (HasAuthority() && GetWorld())
	{
		int32 SpawnedStacks = 0;
		for (const FQRWildlifeDrop& Drop : Harvest())
		{
			if (Drop.ItemId.IsNone() || Drop.MinQuantity <= 0) continue;

			FActorSpawnParameters SP;
			SP.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			const FVector Loc = GetActorLocation()
				+ FVector(FMath::FRandRange(-40.0f, 40.0f),
				          FMath::FRandRange(-40.0f, 40.0f), 20.0f);
			AQRWorldItem* Item = GetWorld()->SpawnActor<AQRWorldItem>(
				AQRWorldItem::StaticClass(), Loc, FRotator::ZeroRotator, SP);
			if (!Item) continue;

			// Harvest() collapses the roll into MinQuantity.
			Item->ItemId   = Drop.ItemId;
			Item->Quantity = Drop.MinQuantity;

			// Resolve the definition for the visual mesh. Pickup re-resolves
			// through the asset manager, so a missing def still leaves a
			// functional (meshless) pickup instead of vanishing loot.
			if (UAssetManager* AM = UAssetManager::GetIfInitialized())
			{
				const FPrimaryAssetId AssetId(TEXT("QRItem"), Drop.ItemId);
				if (const UQRItemDefinition* Def =
					Cast<UQRItemDefinition>(AM->GetPrimaryAssetPath(AssetId).TryLoad()))
				{
					Item->InitializeFrom(Def, Item->Quantity);
				}
			}
			++SpawnedStacks;
		}
		UE_LOG(LogTemp, Log, TEXT("[Wildlife] %s death drops: %d stacks"),
			*GetName(), SpawnedStacks);

		// KillTarget mission progress — the director's static hook was only
		// reachable from the legacy AQRWildlifeActor path before.
		UQRMissionDirector::ReportSpeciesKilled(GetWorld(),
			!SpeciesId.IsNone() ? SpeciesId : ItemId, 1);
	}

	// Despawn the corpse after a delay so the world stays tidy.
	if (GetWorld())
	{
		FTimerHandle DespawnHandle;
		GetWorld()->GetTimerManager().SetTimer(DespawnHandle,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				if (IsValid(this)) Destroy();
			}), 20.0f, false);
	}
}
