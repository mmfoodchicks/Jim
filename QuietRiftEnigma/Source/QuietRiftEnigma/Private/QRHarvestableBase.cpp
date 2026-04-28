#include "QRHarvestableBase.h"
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

void AQRHarvestableBase::AdvanceRegrowth(float GameHoursElapsed)
{
	if (!bIsDepleted || RegrowthTimeHours <= 0.0f) return;

	RegrowthTimeRemaining -= GameHoursElapsed;
	if (RegrowthTimeRemaining <= 0.0f)
	{
		bIsDepleted           = false;
		RemainingCharges      = MaxHarvestCharges;
		RegrowthTimeRemaining = 0.0f;
		OnRegrown();
	}
}

void AQRHarvestableBase::OnDepleted_Implementation()
{
	OnNodeDepleted.Broadcast(this);
	// Blueprint subclass handles visual state change
}

void AQRHarvestableBase::OnRegrown_Implementation()
{
	OnNodeRegrown.Broadcast(this);
	// Blueprint subclass restores visual state
}
