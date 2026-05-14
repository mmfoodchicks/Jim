#include "QRBuildModeComponent.h"
#include "QRBuildPieceTag.h"
#include "QRInventoryComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Actor.h"

UQRBuildModeComponent::UQRBuildModeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // ticks only while in build mode
	SetIsReplicatedByDefault(false); // local-only — the placement RPC is on the player character
}

void UQRBuildModeComponent::BeginPlay()
{
	Super::BeginPlay();
}

const FQRBuildPieceRow* UQRBuildModeComponent::FindPieceRow(FName PieceId) const
{
	if (!PieceCatalog || PieceId.IsNone()) return nullptr;
	return PieceCatalog->FindRow<FQRBuildPieceRow>(PieceId, TEXT("QRBuild"), false);
}

UQRInventoryComponent* UQRBuildModeComponent::GetOwnerInventory() const
{
	if (AActor* O = GetOwner()) return O->FindComponentByClass<UQRInventoryComponent>();
	return nullptr;
}

void UQRBuildModeComponent::EnterBuildMode()
{
	if (bBuildModeActive) return;
	bBuildModeActive = true;
	SetComponentTickEnabled(true);
	OnBuildModeEntered.Broadcast();
}

void UQRBuildModeComponent::ExitBuildMode()
{
	if (!bBuildModeActive) return;
	bBuildModeActive = false;
	CurrentPieceId   = NAME_None;
	DestroyGhost();
	SetComponentTickEnabled(false);
	OnBuildModeExited.Broadcast();
}

void UQRBuildModeComponent::SelectPiece(FName PieceId)
{
	if (!bBuildModeActive) return;
	const FQRBuildPieceRow* Row = FindPieceRow(PieceId);
	if (!Row) return;

	CurrentPieceId   = PieceId;
	CurrentGhostYaw  = 0.0f;
	DestroyGhost();

	// Load the mesh synchronously for the ghost preview. Soft pointers
	// keep package references light when build mode is off, but we
	// need the actual UStaticMesh* to spawn the ghost mesh component.
	UStaticMesh* Mesh = Row->Mesh.LoadSynchronous();
	if (Mesh) SpawnGhost(Mesh);
}

void UQRBuildModeComponent::RotateGhost(float DeltaYawDeg)
{
	CurrentGhostYaw = FMath::Fmod(CurrentGhostYaw + DeltaYawDeg, 360.0f);
}

void UQRBuildModeComponent::SpawnGhost(UStaticMesh* Mesh)
{
	if (!Mesh) return;
	UWorld* W = GetWorld();
	if (!W) return;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	GhostActor = W->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	if (!GhostActor) return;

	UStaticMeshComponent* SMC = NewObject<UStaticMeshComponent>(GhostActor);
	SMC->RegisterComponent();
	SMC->SetStaticMesh(Mesh);
	SMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SMC->SetMobility(EComponentMobility::Movable);
	GhostActor->SetRootComponent(SMC);

	// Apply the valid-material as a default; UpdateGhost re-applies the
	// correct one each tick once validation runs.
	if (GhostMaterialValid)
	{
		const int32 NumSlots = SMC->GetNumMaterials();
		for (int32 i = 0; i < NumSlots; ++i)
			SMC->SetMaterial(i, GhostMaterialValid);
	}
}

void UQRBuildModeComponent::DestroyGhost()
{
	if (GhostActor)
	{
		GhostActor->Destroy();
		GhostActor = nullptr;
	}
	bCurrentPlacementValid = false;
}

void UQRBuildModeComponent::UpdateGhostMaterial(bool bValid)
{
	if (!GhostActor) return;
	UStaticMeshComponent* SMC = GhostActor->FindComponentByClass<UStaticMeshComponent>();
	if (!SMC) return;
	UMaterialInterface* Mat = bValid ? GhostMaterialValid.Get() : GhostMaterialInvalid.Get();
	if (!Mat) return;
	const int32 NumSlots = SMC->GetNumMaterials();
	for (int32 i = 0; i < NumSlots; ++i)
		SMC->SetMaterial(i, Mat);
}

void UQRBuildModeComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bBuildModeActive && !CurrentPieceId.IsNone() && GhostActor)
		UpdateGhost();
}

void UQRBuildModeComponent::UpdateGhost()
{
	const FQRBuildPieceRow* Row = FindPieceRow(CurrentPieceId);
	if (!Row || !GhostActor) return;

	// Get camera location + forward from owner. Reflectively find a
	// UCameraComponent so we don't hard-link to AQRCharacter here.
	AActor* O = GetOwner();
	if (!O) return;
	UCameraComponent* Cam = O->FindComponentByClass<UCameraComponent>();
	if (!Cam) return;

	const FVector  Start   = Cam->GetComponentLocation();
	const FVector  Forward = Cam->GetForwardVector();
	const FVector  End     = Start + Forward * PlacementDistance;

	// Default placement: at PlacementDistance straight ahead, on whatever
	// the trace hits (ground / wall / existing piece).
	FHitResult Hit;
	FCollisionQueryParams P(SCENE_QUERY_STAT(QRBuildGhost), false, O);
	UWorld* W = GetWorld();
	if (!W) return;
	const bool bHit = W->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, P);

	FVector  TargetLoc = bHit ? Hit.ImpactPoint : End;
	FRotator TargetRot(0.0f, CurrentGhostYaw, 0.0f);

	// Try to snap to a nearby existing piece's SOCKET_Snap*.
	FVector  SnapLoc; FRotator SnapRot;
	if (FindSnapTransform(TargetLoc, SnapLoc, SnapRot))
	{
		TargetLoc = SnapLoc;
		TargetRot = SnapRot;
		TargetRot.Yaw += CurrentGhostYaw;
	}

	GhostActor->SetActorLocation(TargetLoc);
	GhostActor->SetActorRotation(TargetRot);

	// Validate (no overlap with other pieces, materials available).
	FText Reason;
	UStaticMesh* Mesh = Row->Mesh.LoadSynchronous();
	const bool bGeomOK    = ValidatePlacement(TargetLoc, TargetRot, Mesh, Reason);
	const bool bMatsOK    = HasMaterialsFor(CurrentPieceId);
	if (!bMatsOK) Reason = FText::FromString(TEXT("Missing materials"));
	bCurrentPlacementValid = bGeomOK && bMatsOK;
	LastBlockerReason      = bCurrentPlacementValid ? FText::GetEmpty() : Reason;
	UpdateGhostMaterial(bCurrentPlacementValid);
}

bool UQRBuildModeComponent::FindSnapTransform(const FVector& AroundLocation,
	FVector& OutLocation, FRotator& OutRotation) const
{
	UWorld* W = GetWorld();
	if (!W) return false;

	// Overlap query for actors with UQRBuildPieceTag within snap radius.
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(QRBuildSnap), false, GetOwner());
	W->OverlapMultiByObjectType(
		Overlaps, AroundLocation, FQuat::Identity,
		FCollisionObjectQueryParams::AllStaticObjects,
		FCollisionShape::MakeSphere(SnapSearchRadius), Params);

	const TArray<FName> SnapSocketNames = {
		FName("SOCKET_SnapN"),     FName("SOCKET_SnapS"),
		FName("SOCKET_SnapE"),     FName("SOCKET_SnapW"),
		FName("SOCKET_SnapLeft"),  FName("SOCKET_SnapRight"),
		FName("SOCKET_SnapTop"),   FName("SOCKET_SnapBottom"),
		FName("SnapN"), FName("SnapS"), FName("SnapE"), FName("SnapW"),
		FName("SnapLeft"), FName("SnapRight"), FName("SnapTop"), FName("SnapBottom"),
	};

	float BestDistSq = FLT_MAX;
	bool  bFound     = false;
	for (const FOverlapResult& O : Overlaps)
	{
		AActor* Other = O.GetActor();
		if (!Other) continue;
		if (!Other->FindComponentByClass<UQRBuildPieceTag>()) continue;

		UStaticMeshComponent* SMC = Other->FindComponentByClass<UStaticMeshComponent>();
		if (!SMC) continue;

		for (FName Sock : SnapSocketNames)
		{
			if (!SMC->DoesSocketExist(Sock)) continue;
			const FTransform T = SMC->GetSocketTransform(Sock, RTS_World);
			const float D = FVector::DistSquared(T.GetLocation(), AroundLocation);
			if (D < BestDistSq)
			{
				BestDistSq  = D;
				OutLocation = T.GetLocation();
				OutRotation = T.GetRotation().Rotator();
				bFound      = true;
			}
		}
	}
	return bFound;
}

bool UQRBuildModeComponent::ValidatePlacement(const FVector& Location, const FRotator& Rotation,
	UStaticMesh* Mesh, FText& OutReason) const
{
	if (!Mesh)
	{
		OutReason = FText::FromString(TEXT("Missing mesh"));
		return false;
	}

	// Compute the mesh's AABB at the proposed transform and overlap-test
	// for other build pieces. A perfect-precision test would use a
	// component sweep; AABB is good enough for v1 placement validation
	// (false negatives mean designer-painful corner cases, not crashes).
	UWorld* W = GetWorld();
	if (!W) return false;

	const FBox LocalBounds = Mesh->GetBoundingBox();
	// Slightly shrink so face-to-face snapping doesn't false-overlap.
	const FVector  Extent   = LocalBounds.GetExtent() * 0.92f;
	const FVector  Center   = LocalBounds.GetCenter();
	const FTransform Xform(Rotation, Location + Rotation.RotateVector(Center));

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(QRBuildValidate), false, GetOwner());
	if (GhostActor) Params.AddIgnoredActor(GhostActor);
	W->OverlapMultiByObjectType(
		Overlaps, Xform.GetLocation(), Xform.GetRotation(),
		FCollisionObjectQueryParams::AllStaticObjects,
		FCollisionShape::MakeBox(Extent), Params);

	for (const FOverlapResult& O : Overlaps)
	{
		AActor* Other = O.GetActor();
		if (!Other) continue;
		if (Other->FindComponentByClass<UQRBuildPieceTag>())
		{
			OutReason = FText::FromString(TEXT("Overlapping existing build piece"));
			return false;
		}
	}
	return true;
}

int32 UQRBuildModeComponent::CountAvailable(FName ItemId) const
{
	if (UQRInventoryComponent* Inv = GetOwnerInventory()) return Inv->CountItem(ItemId);
	return 0;
}

bool UQRBuildModeComponent::HasMaterialsFor(FName PieceId) const
{
	const FQRBuildPieceRow* Row = FindPieceRow(PieceId);
	if (!Row) return false;
	for (const FQRRecipeIngredient& Cost : Row->MaterialCost)
	{
		if (Cost.bIsReusable) continue;
		if (CountAvailable(Cost.ItemId) < Cost.Quantity) return false;
	}
	return true;
}

bool UQRBuildModeComponent::ConsumeMaterials(const TArray<FQRRecipeIngredient>& Cost)
{
	UQRInventoryComponent* Inv = GetOwnerInventory();
	if (!Inv) return false;
	// First pass: verify everything is present so we don't half-consume.
	for (const FQRRecipeIngredient& C : Cost)
	{
		if (C.bIsReusable) continue;
		if (Inv->CountItem(C.ItemId) < C.Quantity) return false;
	}
	// Second pass: actually consume.
	for (const FQRRecipeIngredient& C : Cost)
	{
		if (C.bIsReusable) continue;
		Inv->TryRemoveItem(C.ItemId, C.Quantity);
	}
	return true;
}

bool UQRBuildModeComponent::TryConfirmPlacement()
{
	if (!bBuildModeActive || !bCurrentPlacementValid || !GhostActor) return false;

	const FQRBuildPieceRow* Row = FindPieceRow(CurrentPieceId);
	if (!Row) return false;

	UStaticMesh* Mesh = Row->Mesh.LoadSynchronous();
	if (!Mesh) return false;

	if (!ConsumeMaterials(Row->MaterialCost))
	{
		LastBlockerReason = FText::FromString(TEXT("Missing materials at confirm time"));
		OnPlacementBlocked.Broadcast(LastBlockerReason);
		return false;
	}

	UWorld* W = GetWorld();
	if (!W) return false;

	// Spawn the placed piece as a generic AActor + mesh component +
	// UQRBuildPieceTag. Keeps the actor class simple while letting the
	// build system find pieces later via the tag component.
	const FTransform Xform(GhostActor->GetActorRotation(), GhostActor->GetActorLocation());
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Piece = W->SpawnActor<AActor>(AActor::StaticClass(), Xform, Params);
	if (!Piece) return false;

	UStaticMeshComponent* SMC = NewObject<UStaticMeshComponent>(Piece);
	SMC->RegisterComponent();
	SMC->SetStaticMesh(Mesh);
	SMC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SMC->SetMobility(EComponentMobility::Static);
	Piece->SetRootComponent(SMC);

	UQRBuildPieceTag* Tag = NewObject<UQRBuildPieceTag>(Piece);
	Tag->PieceId   = CurrentPieceId;
	Tag->PieceGuid = FGuid::NewGuid();
	Tag->RegisterComponent();

	OnPiecePlaced.Broadcast(Piece);

	// Keep ghost up so the player can place another of the same piece
	// without re-selecting. Validation will re-run next tick.
	return true;
}
