#include "QRCrashSiteActor.h"
#include "QRWorldItem.h"
#include "QRItemDefinition.h"
#include "QRInventoryComponent.h"
#include "QRLootedRegistry.h"
#include "QRSaveSnapshotLibrary.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Engine/HitResult.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "CollisionQueryParams.h"

void AQRCrashSiteActor::ApplyArchetypeVisual()
{
	if (!WreckMesh) return;
	const FString Id = ArchetypeId.ToString();
	const TCHAR* MeshName =
		Id.Contains(TEXT("Armory"))      ? TEXT("SM_POI_ArmoryWreck") :
		Id.Contains(TEXT("MedBay"))      ? TEXT("SM_POI_MedBayWreck") :
		Id.Contains(TEXT("Galley"))      ? TEXT("SM_POI_GalleyWreck") :
		Id.Contains(TEXT("Food"))        ? TEXT("SM_POI_FoodWreck") :
		Id.Contains(TEXT("Rover"))       ? TEXT("SM_POI_RoverBayWreck") :
		Id.Contains(TEXT("Power"))       ? TEXT("SM_POI_PowerModuleWreck") :
		Id.Contains(TEXT("Comms"))       ? TEXT("SM_POI_CommsRelayDebris") :
		Id.Contains(TEXT("Cryo"))        ? TEXT("SM_POI_CryoPodCluster") :
		Id.Contains(TEXT("Avionics"))    ? TEXT("SM_POI_AvionicsWreck") :
		Id.Contains(TEXT("CommandBridge")) ? TEXT("SM_POI_AvionicsWreck") :
		Id.Contains(TEXT("Engineering")) ? TEXT("SM_POI_EngineeringWreck") :
		TEXT("SM_POI_LuggageWreck");
	const FString Path = FString::Printf(
		TEXT("/Game/Meshes/POIProps/%s.%s"), MeshName, MeshName);
	if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *Path))
	{
		WreckMesh->SetStaticMesh(M);
	}
}

AQRCrashSiteActor::AQRCrashSiteActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	ProximitySphere = CreateDefaultSubobject<USphereComponent>(TEXT("ProximitySphere"));
	SetRootComponent(ProximitySphere);
	ProximitySphere->InitSphereRadius(800.0f);
	ProximitySphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ProximitySphere->SetCollisionResponseToAllChannels(ECR_Overlap);

	WreckMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WreckMesh"));
	WreckMesh->SetupAttachment(ProximitySphere);
	WreckMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	// Visible default — no code path ever assigned a mesh, so every
	// wreck rendered as nothing. ApplyArchetypeVisual refines per type.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultWreck(
		TEXT("/Game/Meshes/POIProps/SM_POI_LuggageWreck"));
	if (DefaultWreck.Succeeded())
	{
		WreckMesh->SetStaticMesh(DefaultWreck.Object);
	}

	WorldItemClass = AQRWorldItem::StaticClass();
}

void AQRCrashSiteActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AQRCrashSiteActor, ArchetypeId);
	DOREPLIFETIME(AQRCrashSiteActor, RequiredToolItemId);
	DOREPLIFETIME(AQRCrashSiteActor, bUnlocked);
}

void AQRCrashSiteActor::ClearScatteredLoot()
{
	for (TWeakObjectPtr<AActor>& W : ScatteredLoot)
	{
		if (AActor* A = W.Get()) A->Destroy();
	}
	ScatteredLoot.Reset();
}

bool AQRCrashSiteActor::TryUnlockWithInventory(UQRInventoryComponent* Inventory)
{
	if (bUnlocked) return true;
	if (!RequiredToolItemId.IsNone())
	{
		if (!Inventory) return false;
		if (Inventory->CountItem(RequiredToolItemId) <= 0) return false;
	}
	bUnlocked = true;

	// First unlock scatters the held-back interior loot. The tool is a
	// KEY, not a consumable -- breaching the armory with a cutting torch
	// doesn't destroy the torch.
	if (bHasPendingLoot)
	{
		bHasPendingLoot = false;
		PopulateLoot(PendingLootTemplate, PendingLootSeed);
	}
	return true;
}


void AQRCrashSiteActor::PopulateLoot(const FQRCrashLootTemplate& Template, int32 Seed)
{
	if (!HasAuthority()) return;
	UWorld* W = GetWorld();
	if (!W || !WorldItemClass) return;

	// One scatter per wreck per SAVE, ever. Resume regeneration re-runs
	// SpawnAll → PopulateLoot; without this guard every reload re-rolled
	// the full hero-wreck haul (an infinite loot exploit). Keyed by a
	// deterministic id from the wreck's placement, persisted through
	// UQRLootedRegistry alongside container looted-state.
	const FString WreckKey = FString::Printf(TEXT("WRECK_%s_%d_%d"),
		*ArchetypeId.ToString(),
		FMath::RoundToInt(GetActorLocation().X),
		FMath::RoundToInt(GetActorLocation().Y));
	const FGuid WreckGuid(GetTypeHash(WreckKey),
		GetTypeHash(WreckKey + TEXT("::QR")), 0x57524B21u,
		static_cast<uint32>(WreckKey.Len()));
	if (UQRLootedRegistry* Registry = W->GetSubsystem<UQRLootedRegistry>())
	{
		if (Registry->HasBeenLooted(WreckGuid)) return;
		Registry->MarkLooted(WreckGuid);
	}

	ClearScatteredLoot();
	FRandomStream Rng(Seed);
	const FVector Center = GetActorLocation();

	for (const FQRCrashLootEntry& Entry : Template.Entries)
	{
		if (Entry.ItemId.IsNone()) continue;
		if (Rng.FRand() > Entry.SpawnChance) continue;

		// Asset-registry resolver — seeders put every definition in a
		// bucket subfolder (Items/<Bucket>/<Id>), so the old flat-path
		// LoadObject returned null for EVERY entry and no wreck ever
		// scattered a single item (including the only TOL_MED_KEY).
		UQRItemDefinition* Def = FQRSaveSnapshot::ResolveItemDefinition(Entry.ItemId);
		if (!Def)
		{
			UE_LOG(LogTemp, Warning, TEXT("[QRCrash] no item def for '%s' — entry skipped"),
				*Entry.ItemId.ToString());
			continue;
		}

		const int32 Quantity = Rng.RandRange(Entry.MinQty, FMath::Max(Entry.MinQty, Entry.MaxQty));

		// Random offset within scatter radius, ground-traced.
		const float Angle = Rng.FRandRange(0.0f, 2.0f * PI);
		const float Dist  = Rng.FRandRange(80.0f, ScatterRadiusCm);
		const FVector XYOffset(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, 0.0f);
		const FVector TraceStart = Center + XYOffset + FVector(0, 0, 500.0f);
		const FVector TraceEnd   = Center + XYOffset - FVector(0, 0, 500.0f);

		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(QRCrashScatter));
		Params.AddIgnoredActor(this);
		FVector SpawnLoc = Center + XYOffset;
		if (W->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, Params))
		{
			SpawnLoc = Hit.ImpactPoint + FVector(0, 0, 5.0f);
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		const FRotator SpawnRot(0.0f, Rng.FRandRange(0.0f, 360.0f), 0.0f);
		AQRWorldItem* Spawned = W->SpawnActor<AQRWorldItem>(
			WorldItemClass, SpawnLoc, SpawnRot, SpawnParams);
		if (Spawned)
		{
			Spawned->InitializeFrom(Def, Quantity);
			ScatteredLoot.Add(Spawned);
		}
	}
}
