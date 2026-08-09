#include "QRNPCColonist.h"
#include "QRNPCBrainComponent.h"
#include "QRHaulerComponent.h"
#include "QRFarmPlotActor.h"
#include "QRStationBase.h"
#include "QRBuildPieceTag.h"
#include "QRDepotActor.h"
#include "QRInventoryComponent.h"
#include "QRItemDefinition.h"
#include "QRMountHusbandryComponent.h"
#include "QRSaveSnapshotLibrary.h"
#include "QRSurvivalComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"


AQRNPCColonist::AQRNPCColonist()
{
	DisplayName = FText::FromString(TEXT("Colonist"));

	// Job duties run on a slow heartbeat -- 5s is fast enough that farm
	// plots never sit ripe for long and slow enough to be free.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 5.0f;
}


void AQRNPCColonist::BeginPlay()
{
	Super::BeginPlay();

	// Hauler colonists get the logistics FSM. The component existed but
	// was never attached to ANY actor, so depot→station hauling never
	// ran in any session.
	if (ColonistRole == EQRNPCRole::Hauler &&
		!FindComponentByClass<UQRHaulerComponent>())
	{
		UQRHaulerComponent* Hauler = NewObject<UQRHaulerComponent>(this, TEXT("Hauler_RT"));
		Hauler->RegisterComponent();
	}

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


void AQRNPCColonist::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!HasAuthority() || !Brain) return;
	if (Brain->State != EQRNPCBrainState::Work) return;

	const bool bAtPost =
		FVector::Dist2D(GetActorLocation(), Brain->AssignedWorkPost) <= JobReachCm;

	switch (ColonistRole)
	{
	case EQRNPCRole::Farmer:
		if (bAtPost) TickFarmerJob();
		break;
	case EQRNPCRole::Guard:
		if (bAtPost) TickGuardJob();
		break;
	case EQRNPCRole::Medic:
		// Medics roam to their patients; gating on the post would strand
		// them the moment they left it to treat someone.
		TickMedicJob();
		break;
	default:
		break;
	}
}


void AQRNPCColonist::TickFarmerJob()
{
	UWorld* W = GetWorld();
	if (!W) return;

	const FVector MyLoc = GetActorLocation();
	const float Reach2 = JobReachCm * JobReachCm * 4.0f;  // whole plot cluster

	for (TActorIterator<AQRFarmPlotActor> It(W); It; ++It)
	{
		AQRFarmPlotActor* Plot = *It;
		if (!Plot) continue;
		if (FVector::DistSquared(MyLoc, Plot->GetActorLocation()) > Reach2) continue;

		if (Plot->bHarvestable)
		{
			// Read the yield BEFORE Harvest() soft-resets the plot.
			const FName Yield = Plot->CurrentYieldItemId;
			Plot->Harvest();
			DepositToNearestDepot(Yield, 2);
		}
		else if (Plot->PlantedSeedId.IsNone())
		{
			// Nobody in the codebase calls Plant() but the player --
			// without this replant, every plot sits fallow forever.
			Plot->Plant(!Plot->DefaultYieldItemId.IsNone()
				? Plot->DefaultYieldItemId : FallbackSeedId);
		}
	}

	// Animal care doubles as the farmer duty: FeedOrPet advances taming
	// and defuses stress bucking on every mount in reach.
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		AActor* A = *It;
		if (!A) continue;
		if (FVector::DistSquared(MyLoc, A->GetActorLocation()) > Reach2) continue;
		if (UQRMountHusbandryComponent* H = A->FindComponentByClass<UQRMountHusbandryComponent>())
		{
			// FeedOrPet has no internal cooldown and one call = one day of
			// taming progress -- gate on the care clock or a farmer ticking
			// every 5s would insta-tame every wild animal in reach.
			if (H->HoursSinceLastCare >= H->FeedIntervalHours)
			{
				H->FeedOrPet();
			}
		}
	}
}


void AQRNPCColonist::TickGuardJob()
{
	UWorld* W = GetWorld();
	if (!W || !Brain) return;

	// Collect fortification waypoints around home, name-sorted so the
	// patrol traces the same loop every day instead of jittering.
	TArray<AActor*> Posts;
	const float MaxR2 = MaxClaimRangeCm * MaxClaimRangeCm;
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		AActor* A = *It;
		if (!A || !A->FindComponentByClass<UQRBuildPieceTag>()) continue;
		if (FVector::DistSquared(Brain->HomePosition, A->GetActorLocation()) > MaxR2) continue;
		Posts.Add(A);
	}
	if (Posts.Num() == 0) return;
	Posts.Sort([](const AActor& A, const AActor& B) { return A.GetName() < B.GetName(); });

	// This only fires while standing at the current post (bAtPost gate),
	// so each 5s tick at a waypoint advances to the next -- a patrol.
	PatrolCursor = (PatrolCursor + 1) % Posts.Num();
	Brain->AssignedWorkPost = Posts[PatrolCursor]->GetActorLocation();
	Brain->CurrentTarget    = Brain->AssignedWorkPost;
}


void AQRNPCColonist::TickMedicJob()
{
	UWorld* W = GetWorld();
	if (!W || !Brain) return;

	const FVector MyLoc  = GetActorLocation();
	const float   Range2 = MaxClaimRangeCm * MaxClaimRangeCm;

	// Triage: the lowest health fraction in claim range wins.
	UQRSurvivalComponent* Worst = nullptr;
	AActor* WorstActor = nullptr;
	float WorstPct = 2.0f;
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		AActor* A = *It;
		if (!A || A == this) continue;
		UQRSurvivalComponent* S = A->FindComponentByClass<UQRSurvivalComponent>();
		if (!S) continue;
		if (FVector::DistSquared(MyLoc, A->GetActorLocation()) > Range2) continue;

		const float Pct = S->GetHealthPercent();
		if (Pct >= 0.999f && S->ActiveInjuries.Num() == 0) continue;  // healthy
		if (Pct < WorstPct)
		{
			WorstPct   = Pct;
			Worst      = S;
			WorstActor = A;
		}
	}

	if (!Worst || !WorstActor)
	{
		// Ward is empty -- return to the home post so the brain doesn't
		// hold station at the last patient's location.
		Brain->AssignedWorkPost = Brain->HomePosition;
		return;
	}

	// Walk to the patient; treat once within arm's reach. Think() re-reads
	// AssignedWorkPost every planner tick, so moving the post moves the NPC.
	Brain->AssignedWorkPost = WorstActor->GetActorLocation();
	if (FVector::Dist2D(MyLoc, WorstActor->GetActorLocation()) > 250.0f) return;

	Worst->ApplyHealing(MedicHealPerTick);
	if (Worst->ActiveInjuries.Num() > 0)
	{
		Worst->TreatInjury(Worst->ActiveInjuries[0].Type, 0.5f);
	}
}


void AQRNPCColonist::DepositToNearestDepot(FName ItemId, int32 Quantity)
{
	if (ItemId.IsNone() || Quantity <= 0) return;
	UWorld* W = GetWorld();
	if (!W) return;

	AQRDepotActor* Best = nullptr;
	float BestD2 = MaxClaimRangeCm * MaxClaimRangeCm;
	const FVector MyLoc = GetActorLocation();
	for (TActorIterator<AQRDepotActor> It(W); It; ++It)
	{
		AQRDepotActor* D = *It;
		if (!D || !D->Storage) continue;
		const float D2 = FVector::DistSquared(MyLoc, D->GetActorLocation());
		if (D2 < BestD2)
		{
			BestD2 = D2;
			Best   = D;
		}
	}
	if (!Best) return;  // no depot in range: the colony eats it on the spot

	if (UQRItemDefinition* Def = FQRSaveSnapshot::ResolveItemDefinition(ItemId))
	{
		int32 Remainder = 0;
		Best->Storage->TryAddByDefinition(Def, Quantity, Remainder);
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

	switch (ColonistRole)
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
