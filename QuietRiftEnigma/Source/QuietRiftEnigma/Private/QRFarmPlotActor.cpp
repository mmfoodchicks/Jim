#include "QRFarmPlotActor.h"
#include "QRMath.h"
#include "QRCodexSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"


AQRFarmPlotActor::AQRFarmPlotActor()
{
	PrimaryActorTick.bCanEverTick = false;

	PlotMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlotMesh"));
	PlotMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetRootComponent(PlotMesh);
}


void AQRFarmPlotActor::Plant(FName SeedId)
{
	PlantedSeedId       = SeedId;
	CurrentYieldItemId  = SeedId.IsNone() ? DefaultYieldItemId : SeedId;
	GrowthProgressHours = 0.0f;
	BaseState           = EQREarthCropBaseState::Stable;
	bHarvestable        = false;
}


void AQRFarmPlotActor::Harvest()
{
	if (!bHarvestable || CurrentYieldItemId.IsNone()) return;
	OnCropHarvested.Broadcast(CurrentYieldItemId);

	// Soft-reset: same seed, cleared progress. Mutation carries forward
	// in CurrentYieldItemId until the player explicitly replants.
	GrowthProgressHours = 0.0f;
	bHarvestable        = false;
}


void AQRFarmPlotActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}


void AQRFarmPlotActor::TickGameHours(float DeltaGameHours)
{
	if (DeltaGameHours <= 0.0f) return;
	if (PlantedSeedId.IsNone() || bHarvestable) return;

	RecomputeSporeLoad();

	// Contamination only escalates while in Contaminated/Stable. A
	// Mutated plot has already branched -- further roll would loop.
	if (BaseState != EQREarthCropBaseState::Mutated)
	{
		const float Exposure = UQRMath::CrossContamExposure(
			ToxicSoil, InfectedWater, SporeLoad, FarmerCrossContamScore);
		if (Exposure >= CropMutationThreshold * 0.5f && BaseState == EQREarthCropBaseState::Stable)
		{
			BaseState = EQREarthCropBaseState::Contaminated;
		}
	}

	GrowthProgressHours += DeltaGameHours;
	if (GrowthProgressHours < GrowthHours) return;

	// Growth complete -- roll mutation once. The mutated id is
	// MUT_<original> so designers can author paired definitions
	// (toxic broccoli, sparkstone-laced grain) in the item table.
	const float Exposure = UQRMath::CrossContamExposure(
		ToxicSoil, InfectedWater, SporeLoad, FarmerCrossContamScore);
	if (Exposure >= CropMutationThreshold && BaseState != EQREarthCropBaseState::Mutated)
	{
		BaseState = EQREarthCropBaseState::Mutated;
		const FName MutId(*FString::Printf(TEXT("MUT_%s"), *CurrentYieldItemId.ToString()));
		CurrentYieldItemId = MutId;

		// Codex registers the mutated cultivar as Observed -- player
		// has to research it to clear the "safe to eat" gate per the
		// food-safety pipeline.
		if (UWorld* W = GetWorld())
		{
			if (UQRCodexSubsystem* Codex = W->GetSubsystem<UQRCodexSubsystem>())
			{
				Codex->Record(MutId, TEXT("Flora"), FText::FromName(MutId),
					EQRCodexDiscoveryState::Observed);
			}
		}

		OnCropMutated.Broadcast(MutId);
		UE_LOG(LogTemp, Log,
			TEXT("[QRFarm] %s mutated -> %s (exposure %.2f >= %.2f)"),
			*PlantedSeedId.ToString(), *MutId.ToString(),
			Exposure, CropMutationThreshold);
	}

	bHarvestable = true;
}


void AQRFarmPlotActor::RecomputeSporeLoad()
{
	UWorld* W = GetWorld();
	if (!W) return;

	const FVector MyLoc = GetActorLocation();
	const float R2 = SporePressureRadiusCm * SporePressureRadiusCm;

	float Load = 0.0f;
	for (TActorIterator<AQRFarmPlotActor> It(W); It; ++It)
	{
		AQRFarmPlotActor* Other = *It;
		if (!Other || Other == this) continue;
		if (Other->BaseState != EQREarthCropBaseState::Mutated) continue;
		if (FVector::DistSquared(MyLoc, Other->GetActorLocation()) > R2) continue;
		// Each nearby mutated plot adds 1.0 — three within radius is
		// enough to push a baseline-clean plot over the threshold.
		Load += 1.0f;
	}
	SporeLoad = FMath::Min(Load, 5.0f);
}
