#include "QRStationBase.h"
#include "QRDepotComponent.h"
#include "QRItemInstance.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"

AQRStationBase::AQRStationBase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 1.0f;
	bReplicates = true;
}

void AQRStationBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AQRStationBase, bIsPowered);
	DOREPLIFETIME(AQRStationBase, bIsStaffed);
	DOREPLIFETIME(AQRStationBase, bIsBlocked);
	DOREPLIFETIME(AQRStationBase, CurrentBlockerReason);
	DOREPLIFETIME(AQRStationBase, CurrentProgress);
	DOREPLIFETIME(AQRStationBase, QueueSize);
}

void AQRStationBase::BeginPlay()
{
	Super::BeginPlay();
}

void AQRStationBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!HasAuthority() || !CanOperate()) return;

	OnWorkTick(DeltaTime);
}

bool AQRStationBase::CanOperate() const
{
	if (bIsBlocked) return false;
	if (bRequiresWorker && !bIsStaffed) return false;
	if (RequiredPowerQuality != EQRPowerQuality::None && !bIsPowered) return false;
	return true;
}

void AQRStationBase::SetPowered(bool bNewPowered)
{
	bIsPowered = bNewPowered;
}

void AQRStationBase::SetStaffed(bool bNewStaffed)
{
	bIsStaffed = bNewStaffed;
}

void AQRStationBase::SetBlocker(FText Reason)
{
	bIsBlocked = true;
	CurrentBlockerReason = Reason;
	OnBlockerChanged.Broadcast(Reason);
}

void AQRStationBase::ClearBlocker()
{
	bIsBlocked = false;
	CurrentBlockerReason = FText::GetEmpty();
	OnBlockerChanged.Broadcast(FText::GetEmpty());
}

TArray<UQRDepotComponent*> AQRStationBase::FindNearbyDepots(FGameplayTag DepotCategory) const
{
	TArray<UQRDepotComponent*> Result;
	if (!GetWorld()) return Result;

	// Gather all depot components in the world within pull radius
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor) continue;

		UQRDepotComponent* Depot = Actor->FindComponentByClass<UQRDepotComponent>();
		if (Depot && Depot->AcceptedCategory == DepotCategory && Depot->IsWithinPullRadius(this))
		{
			Result.Add(Depot);
		}
	}

	// Sort by priority (highest first)
	Result.Sort([](const UQRDepotComponent& A, const UQRDepotComponent& B)
	{
		return A.PullPriority > B.PullPriority;
	});

	return Result;
}

bool AQRStationBase::PullFromDepots(FName ItemId, FGameplayTag Category, int32 Quantity, TArray<UQRItemInstance*>& OutItems)
{
	TArray<UQRDepotComponent*> Depots = FindNearbyDepots(Category);
	int32 Remaining = Quantity;

	for (UQRDepotComponent* Depot : Depots)
	{
		if (Remaining <= 0) break;
		if (Depot->CountItem(ItemId) > 0)
		{
			int32 ToTake = FMath::Min(Remaining, Depot->CountItem(ItemId));
			UQRItemInstance* Pulled = Depot->WithdrawItem(ItemId, ToTake);
			if (Pulled)
			{
				OutItems.Add(Pulled);
				Remaining -= Pulled->Quantity;
			}
		}
	}

	return Remaining == 0;
}

void AQRStationBase::OnTaskCompleted_Implementation(FName OutputItemId, int32 OutputQuantity)
{
	OnOutputReady.Broadcast(OutputItemId, OutputQuantity);
}
