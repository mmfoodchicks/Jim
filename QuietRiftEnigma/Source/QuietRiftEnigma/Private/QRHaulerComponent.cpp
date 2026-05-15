#include "QRHaulerComponent.h"
#include "QRDepotActor.h"
#include "QRInventoryComponent.h"
#include "QRItemDefinition.h"
#include "QRItemInstance.h"
#include "QRCraftingComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "EngineUtils.h"


UQRHaulerComponent::UQRHaulerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;  // 10 Hz is plenty for routing
}


void UQRHaulerComponent::BeginPlay()
{
	Super::BeginPlay();
	SetState(EQRHaulerState::Idle);
}


void UQRHaulerComponent::SetState(EQRHaulerState NewState)
{
	State = NewState;
	StateTimer = 0.0f;
}


void UQRHaulerComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	switch (State)
	{
	case EQRHaulerState::Idle:          TickIdle(DeltaTime); break;
	case EQRHaulerState::SeekDemand:    TickSeekDemand(); break;
	case EQRHaulerState::MoveToDepot:   TickMoveTo(ActiveDepot.Get(),  EQRHaulerState::PickUp, DeltaTime); break;
	case EQRHaulerState::PickUp:        TickPickUp(); break;
	case EQRHaulerState::MoveToStation: TickMoveTo(ActiveStation.Get(), EQRHaulerState::Deliver, DeltaTime); break;
	case EQRHaulerState::Deliver:       TickDeliver(); break;
	}
}


void UQRHaulerComponent::TickIdle(float DeltaTime)
{
	StateTimer += DeltaTime;
	if (StateTimer >= IdleSeconds) SetState(EQRHaulerState::SeekDemand);
}


void UQRHaulerComponent::TickSeekDemand()
{
	FName WantedItem;
	int32 WantedQty = 0;
	AActor* Station = FindStationWithDemand(WantedItem, WantedQty);
	if (!Station || WantedItem.IsNone() || WantedQty <= 0)
	{
		SetState(EQRHaulerState::Idle);
		return;
	}

	AQRDepotActor* Depot = FindDepotWithItem(WantedItem, FMath::Min(WantedQty, CarryCapacity));
	if (!Depot)
	{
		SetState(EQRHaulerState::Idle);
		return;
	}

	ActiveDepot     = Depot;
	ActiveStation   = Station;
	ActiveItemId    = WantedItem;
	ActiveQuantity  = FMath::Min(WantedQty, CarryCapacity);
	SetState(EQRHaulerState::MoveToDepot);
}


void UQRHaulerComponent::TickMoveTo(AActor* Target, EQRHaulerState OnArrivedState,
	float DeltaTime)
{
	if (!Target)
	{
		SetState(EQRHaulerState::Idle);
		return;
	}
	if (MoveToward(Target->GetActorLocation(), DeltaTime))
	{
		SetState(OnArrivedState);
	}
}


void UQRHaulerComponent::TickPickUp()
{
	AQRDepotActor* Depot = ActiveDepot.Get();
	if (!Depot || !Depot->Storage)
	{
		SetState(EQRHaulerState::Idle);
		return;
	}

	UQRInventoryComponent* MyInv = GetOwner() ? GetOwner()->FindComponentByClass<UQRInventoryComponent>() : nullptr;
	if (!MyInv)
	{
		SetState(EQRHaulerState::Idle);
		return;
	}

	// Withdraw from the depot. UQRInventoryComponent has no transfer-
	// between-components API in v1, so we do the equivalent via remove
	// + add by id.
	const int32 Available = Depot->Storage->CountItem(ActiveItemId);
	const int32 ToMove = FMath::Min(ActiveQuantity, Available);
	if (ToMove <= 0)
	{
		SetState(EQRHaulerState::Idle);
		return;
	}

	if (!Depot->Storage->TryRemoveItem(ActiveItemId, ToMove))
	{
		SetState(EQRHaulerState::Idle);
		return;
	}

	// Resolve the definition by id for TryAddByDefinition.
	const FString DefPath = FString::Printf(TEXT("/Game/QuietRift/Data/Items/%s.%s"),
		*ActiveItemId.ToString(), *ActiveItemId.ToString());
	const UQRItemDefinition* Def = LoadObject<UQRItemDefinition>(nullptr, *DefPath);
	if (Def)
	{
		int32 Remainder = 0;
		MyInv->TryAddByDefinition(Def, ToMove, Remainder);
	}

	ActiveQuantity = ToMove;
	SetState(EQRHaulerState::MoveToStation);
}


void UQRHaulerComponent::TickDeliver()
{
	AActor* Station = ActiveStation.Get();
	if (!Station)
	{
		SetState(EQRHaulerState::Idle);
		return;
	}

	UQRInventoryComponent* MyInv      = GetOwner()->FindComponentByClass<UQRInventoryComponent>();
	UQRInventoryComponent* StationInv = Station->FindComponentByClass<UQRInventoryComponent>();
	if (!MyInv || !StationInv)
	{
		SetState(EQRHaulerState::Idle);
		return;
	}

	if (MyInv->TryRemoveItem(ActiveItemId, ActiveQuantity))
	{
		const FString DefPath = FString::Printf(TEXT("/Game/QuietRift/Data/Items/%s.%s"),
			*ActiveItemId.ToString(), *ActiveItemId.ToString());
		const UQRItemDefinition* Def = LoadObject<UQRItemDefinition>(nullptr, *DefPath);
		if (Def)
		{
			int32 Remainder = 0;
			StationInv->TryAddByDefinition(Def, ActiveQuantity, Remainder);
		}
	}

	// Clear state and idle.
	ActiveDepot    = nullptr;
	ActiveStation  = nullptr;
	ActiveItemId   = NAME_None;
	ActiveQuantity = 0;
	SetState(EQRHaulerState::Idle);
}


bool UQRHaulerComponent::MoveToward(const FVector& Destination, float DeltaTime)
{
	AActor* Owner = GetOwner();
	if (!Owner) return false;
	FVector Cur = Owner->GetActorLocation();
	FVector To  = Destination - Cur;
	To.Z = 0.0f;
	const float Dist = To.Size();
	if (Dist < InteractRange) return true;

	const FVector Dir = To.GetSafeNormal();
	FVector NewLoc = Cur + Dir * MoveSpeed * DeltaTime;
	NewLoc.Z = Cur.Z;  // 2D pathing — no falling
	Owner->SetActorLocation(NewLoc);
	if (!Dir.IsNearlyZero())
	{
		const FRotator R = Dir.Rotation();
		Owner->SetActorRotation(FRotator(0.0f, R.Yaw, 0.0f));
	}
	return false;
}


AActor* UQRHaulerComponent::FindStationWithDemand(FName& OutItemId, int32& OutQuantity) const
{
	UWorld* W = GetWorld();
	if (!W) return nullptr;
	AActor* Owner = GetOwner();
	if (!Owner) return nullptr;
	const FVector OwnerLoc = Owner->GetActorLocation();

	// Look for crafting stations with stalled recipes. The UQRCrafting
	// Component exposes RecipeQueue; if the head recipe has missing
	// ingredients we surface the first one as the demand.
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		AActor* A = *It;
		if (!A) continue;
		if (FVector::DistSquared(OwnerLoc, A->GetActorLocation()) > SearchRadiusCm * SearchRadiusCm) continue;

		UQRCraftingComponent* Crafting = A->FindComponentByClass<UQRCraftingComponent>();
		if (!Crafting) continue;
		// CanCraft uses the station's own inventory; if it fails on a
		// missing ingredient the reason text + RecipeQueue head tell us
		// what to fetch. v1 takes a shortcut: if there's a queued
		// recipe but IsCrafting is false, we assume the head is stalled
		// and try to find anything that recipe needs.
		if (Crafting->IsCrafting()) continue;
		if (Crafting->RecipeQueue.Num() == 0) continue;
		// We don't have a public "what ingredient is missing" API; for
		// v1 set OutItemId to a generic RAW_METAL_SCRAP demand so the
		// hauler at least moves SOMETHING. Designer can extend Crafting
		// Component to expose CurrentDemandItem in a small follow-up.
		OutItemId = TEXT("RAW_METAL_SCRAP");
		OutQuantity = CarryCapacity;
		return A;
	}
	return nullptr;
}


AQRDepotActor* UQRHaulerComponent::FindDepotWithItem(FName ItemId, int32 MinQuantity) const
{
	UWorld* W = GetWorld();
	if (!W) return nullptr;
	AActor* Owner = GetOwner();
	if (!Owner) return nullptr;
	const FVector OwnerLoc = Owner->GetActorLocation();

	AQRDepotActor* Best = nullptr;
	float BestDistSq = SearchRadiusCm * SearchRadiusCm;
	int32 BestPriority = TNumericLimits<int32>::Min();

	for (TActorIterator<AQRDepotActor> It(W); It; ++It)
	{
		AQRDepotActor* D = *It;
		if (!D || !D->Storage) continue;
		const float DistSq = FVector::DistSquared(OwnerLoc, D->GetActorLocation());
		if (DistSq > BestDistSq) continue;
		if (D->Storage->CountItem(ItemId) < MinQuantity) continue;
		// Higher priority wins; ties go to nearest.
		if (D->PullPriority > BestPriority ||
			(D->PullPriority == BestPriority && DistSq < BestDistSq))
		{
			Best = D;
			BestPriority = D->PullPriority;
			BestDistSq   = DistSq;
		}
	}
	return Best;
}
