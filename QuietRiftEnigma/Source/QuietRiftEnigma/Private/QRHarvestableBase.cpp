#include "QRHarvestableBase.h"
#include "QRCoreSettings.h"
#include "Net/UnrealNetwork.h"

AQRHarvestableBase::AQRHarvestableBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

void AQRHarvestableBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AQRHarvestableBase, RemainingCharges);
	DOREPLIFETIME(AQRHarvestableBase, RegrowthTimeRemaining);
	DOREPLIFETIME(AQRHarvestableBase, bIsDepleted);
}

void AQRHarvestableBase::BeginPlay()
{
	Super::BeginPlay();
	RemainingCharges = MaxHarvestCharges;
	NodeId           = FName(*FGuid::NewGuid().ToString());
}

TArray<FQRHarvestYield> AQRHarvestableBase::Harvest(FGameplayTag ToolTag, float EfficiencyMultiplier)
{
	TArray<FQRHarvestYield> Results;
	if (!CanBeHarvested()) return Results;

	for (const FQRHarvestYield& Yield : HarvestYields)
	{
		// Check tool requirement
		if (Yield.RequiredToolTag.IsValid() && !ToolTag.MatchesTag(Yield.RequiredToolTag))
			continue;

		if (FMath::FRand() > Yield.DropChance)
			continue;

		FQRHarvestYield Result = Yield;
		int32 BaseQty = FMath::RandRange(Yield.MinQuantity, Yield.MaxQuantity);
		Result.MinQuantity = FMath::RoundToInt(BaseQty * EfficiencyMultiplier);
		Result.MaxQuantity = Result.MinQuantity;
		Results.Add(Result);
	}

	--RemainingCharges;

	if (RemainingCharges <= 0)
	{
		bIsDepleted = true;
		RegrowthTimeRemaining = RegrowthTimeHours;
		OnDepleted();
	}

	OnHarvested.Broadcast(this, Results);
	return Results;
}

void AQRHarvestableBase::ForceRegrow()
{
	GetWorldTimerManager().ClearTimer(RegrowthTimerHandle);
	OnRegrowthTimerFired();
}

void AQRHarvestableBase::OnRegrowthTimerFired()
{
	if (!HasAuthority()) return;
	bIsDepleted           = false;
	RemainingCharges      = MaxHarvestCharges;
	RegrowthTimeRemaining = 0.0f;
	OnRegrown();
}

void AQRHarvestableBase::OnDepleted_Implementation()
{
	OnNodeDepleted.Broadcast(this);

	// Schedule automatic regrowth using real-world seconds derived from settings day length
	if (HasAuthority() && RegrowthTimeHours > 0.0f)
	{
		const UQRCoreSettings* S = GetDefault<UQRCoreSettings>();
		const float DayLenSec    = S ? S->DayLengthSeconds : 1200.0f;
		const float RegrowthSec  = RegrowthTimeHours * DayLenSec / 24.0f;
		GetWorldTimerManager().SetTimer(RegrowthTimerHandle, this,
			&AQRHarvestableBase::OnRegrowthTimerFired, RegrowthSec, /*bLoop=*/false);
		RegrowthTimeRemaining = RegrowthTimeHours;
	}
	// Blueprint subclass handles visual state change (stump mesh, etc.)
}

void AQRHarvestableBase::OnRegrown_Implementation()
{
	OnNodeRegrown.Broadcast(this);
	// Blueprint subclass restores visual state
}
