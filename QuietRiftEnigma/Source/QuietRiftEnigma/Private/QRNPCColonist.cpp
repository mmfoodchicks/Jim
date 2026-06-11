#include "QRNPCColonist.h"
#include "QRNPCBrainComponent.h"
#include "QRFarmPlotActor.h"
#include "QRStationBase.h"
#include "QRBuildPieceTag.h"
#include "Engine/World.h"
#include "EngineUtils.h"


AQRNPCColonist::AQRNPCColonist()
{
	DisplayName = FText::FromString(TEXT("Colonist"));
}


void AQRNPCColonist::BeginPlay()
{
	Super::BeginPlay();

	if (Brain)
	{
		// HomePosition stays at the spawn location (Brain::BeginPlay
		// already wrote it). Work post snaps to the nearest role
		// fit so the daytime loop is meaningful out of the box.
		const FVector Post = ResolveWorkPostForRole();
		if (!Post.IsZero())
		{
			Brain->AssignedWorkPost = Post;
		}
	}
}


FVector AQRNPCColonist::ResolveWorkPostForRole() const
{
	UWorld* W = GetWorld();
	if (!W) return GetActorLocation();
	const FVector MyLoc = GetActorLocation();
	const float MaxR2 = MaxClaimRangeCm * MaxClaimRangeCm;

	auto NearestOfClass = [&MyLoc, MaxR2](UWorld* World,
		TFunctionRef<bool(AActor*)> Filter) -> FVector
	{
		AActor* Best = nullptr;
		float BestD2 = MaxR2;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* A = *It;
			if (!A || !Filter(A)) continue;
			const float D2 = FVector::DistSquared(MyLoc, A->GetActorLocation());
			if (D2 < BestD2)
			{
				BestD2 = D2;
				Best = A;
			}
		}
		return Best ? Best->GetActorLocation() : MyLoc;
	};

	switch (Role)
	{
	case EQRNPCRole::Farmer:
		return NearestOfClass(W, [](AActor* A)
		{
			return A->IsA(AQRFarmPlotActor::StaticClass());
		});
	case EQRNPCRole::Engineer:
	case EQRNPCRole::Cook:
		// Both work at AQRStationBase instances; the role distinction
		// can drive station-tag filtering later. v1 just claims the
		// nearest station so a colonist tied to a kitchen is still
		// productive even if it's a forge.
		return NearestOfClass(W, [](AActor* A)
		{
			return A->IsA(AQRStationBase::StaticClass());
		});
	case EQRNPCRole::Guard:
		// Guards patrol around the nearest built fortification piece.
		return NearestOfClass(W, [](AActor* A)
		{
			return A->FindComponentByClass<UQRBuildPieceTag>() != nullptr;
		});
	default:
		// Unassigned / Hunter / Medic etc.: hold at spawn location so
		// the brain still walks a daytime loop instead of standing still.
		return MyLoc;
	}
}
