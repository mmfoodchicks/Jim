#include "QRWorldGenSpawner.h"
#include "QRWorldGenSubsystem.h"
#include "QRRemnantSite.h"
#include "QRCrashSiteActor.h"
#include "QRCaveEntrance.h"
#include "QRWildlifeActor.h"
#include "QRNPCActor.h"
#include "QRFactionCamp.h"
#include "QRCampSimComponent.h"
#include "QRGameMode.h"
#include "QRResearchComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/HitResult.h"
#include "CollisionQueryParams.h"
#include "Math/RandomStream.h"


AQRWorldGenSpawner::AQRWorldGenSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	RemnantSiteClass     = AQRRemnantSite::StaticClass();
	CrashSiteClass       = AQRCrashSiteActor::StaticClass();
	CaveEntranceClass    = AQRCaveEntrance::StaticClass();
	WildlifeFallbackClass = AQRWildlifeActor::StaticClass();
	FactionSatelliteClass = AQRFactionCamp::StaticClass();

	PopulateDefaultCrashTemplates();
}


void AQRWorldGenSpawner::BeginPlay()
{
	Super::BeginPlay();
	if (bSpawnOnBeginPlay) SpawnAll();

	// Subscribe to research events so Rift-tier unlocks bump every
	// spawned RemnantSite to the next wake state.
	if (AQRGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AQRGameMode>() : nullptr)
	{
		if (UQRResearchComponent* Research = GM->Research)
		{
			Research->OnTechNodeUnlocked.AddDynamic(this, &AQRWorldGenSpawner::HandleTechUnlocked);
		}
	}
}

void AQRWorldGenSpawner::HandleTechUnlocked(FName TechNodeId)
{
	const int32 Idx = RiftTechNodeProgression.IndexOfByKey(TechNodeId);
	if (Idx == INDEX_NONE) return;

	// Map index → wake state. Surface (0) doesn't escalate; first Rift
	// node bumps Dormant→Stirring (1), second→Active (2), third→Hostile
	// (3, dangerous!), fourth→Subsiding (4).
	static const EQRRemnantWakeState States[] = {
		EQRRemnantWakeState::Stirring,
		EQRRemnantWakeState::Active,
		EQRRemnantWakeState::Hostile,
		EQRRemnantWakeState::Subsiding,
	};
	const int32 StateIdx = FMath::Clamp(Idx, 0, UE_ARRAY_COUNT(States) - 1);
	BumpAllRemnantsToState(States[StateIdx]);

	UE_LOG(LogTemp, Log, TEXT("[QRWorldGenSpawner] Rift research '%s' advanced remnants to state %d"),
		*TechNodeId.ToString(), static_cast<int32>(States[StateIdx]));
}

void AQRWorldGenSpawner::BumpAllRemnantsToState(EQRRemnantWakeState NewState)
{
	for (TWeakObjectPtr<AActor>& W : SpawnedActors)
	{
		if (AQRRemnantSite* R = Cast<AQRRemnantSite>(W.Get()))
		{
			R->SetWakeState(NewState);
		}
	}
}


// ─── Canonical crash-site loot templates ────────────────────────────
// Per Master GDD §11 + Visual World Bible §4. Item ids assume the
// canonical UQRItemDefinition naming under /Game/QuietRift/Data/Items/.

void AQRWorldGenSpawner::PopulateDefaultCrashTemplates()
{
	auto MakeEntry = [](const FName& Id, int32 MinQ, int32 MaxQ, float Chance) -> FQRCrashLootEntry
	{
		FQRCrashLootEntry E;
		E.ItemId = Id; E.MinQty = MinQ; E.MaxQty = MaxQ; E.SpawnChance = Chance;
		return E;
	};

	// ArmoryWreck — military hardware
	{
		FQRCrashLootTemplate T;
		T.Entries.Add(MakeEntry(TEXT("WPN_SERVICE_PISTOL"),  1, 2, 0.85f));
		T.Entries.Add(MakeEntry(TEXT("WPN_CARBINE"),          1, 1, 0.45f));
		T.Entries.Add(MakeEntry(TEXT("WPN_BOLT_SNIPER"),      1, 1, 0.15f));
		T.Entries.Add(MakeEntry(TEXT("ATT_SUPPRESSOR"),       1, 2, 0.55f));
		T.Entries.Add(MakeEntry(TEXT("ATT_OPTIC_RDS"),        1, 2, 0.55f));
		T.Entries.Add(MakeEntry(TEXT("AMM_9MM"),              20, 60, 0.95f));
		T.Entries.Add(MakeEntry(TEXT("AMM_556"),              10, 40, 0.85f));
		T.Entries.Add(MakeEntry(TEXT("TOL_CLEANING_KIT"),     1, 2, 0.70f));
		CrashLootTemplates.Add(TEXT("ArmoryWreck"), T);
	}

	// MedBayWreck — medical supplies
	{
		FQRCrashLootTemplate T;
		T.Entries.Add(MakeEntry(TEXT("MED_PAINKILLERS"),      2, 8, 0.95f));
		T.Entries.Add(MakeEntry(TEXT("MED_BANDAGE"),          3, 10, 0.95f));
		T.Entries.Add(MakeEntry(TEXT("MED_ANTIBIOTIC"),       1, 4, 0.65f));
		T.Entries.Add(MakeEntry(TEXT("MED_BLOOD_BAG"),        1, 2, 0.40f));
		T.Entries.Add(MakeEntry(TEXT("MED_SUTURE_KIT"),       1, 3, 0.55f));
		T.Entries.Add(MakeEntry(TEXT("TOL_SCALPEL"),          1, 1, 0.45f));
		CrashLootTemplates.Add(TEXT("MedBayWreck"), T);
	}

	// GalleyWreck — food + cookware
	{
		FQRCrashLootTemplate T;
		T.Entries.Add(MakeEntry(TEXT("FOD_MRE_ASSORTED"),     3, 8, 0.95f));
		T.Entries.Add(MakeEntry(TEXT("FOD_RATION_BAR"),       4, 12, 0.95f));
		T.Entries.Add(MakeEntry(TEXT("FOD_DRIED_FRUIT"),      2, 6, 0.70f));
		T.Entries.Add(MakeEntry(TEXT("RAW_SALT"),             1, 3, 0.55f));
		T.Entries.Add(MakeEntry(TEXT("RAW_GRAIN"),            2, 6, 0.55f));
		T.Entries.Add(MakeEntry(TEXT("TOL_COOKWARE"),         1, 2, 0.45f));
		CrashLootTemplates.Add(TEXT("GalleyWreck"), T);
	}

	// EngineeringWreck — tools + raw materials
	{
		FQRCrashLootTemplate T;
		T.Entries.Add(MakeEntry(TEXT("TOL_WRENCH"),           1, 2, 0.75f));
		T.Entries.Add(MakeEntry(TEXT("TOL_SCREWDRIVER_SET"),  1, 2, 0.75f));
		T.Entries.Add(MakeEntry(TEXT("TOL_SOLDERING_IRON"),   1, 1, 0.45f));
		T.Entries.Add(MakeEntry(TEXT("RAW_METAL_SCRAP"),      4, 12, 0.95f));
		T.Entries.Add(MakeEntry(TEXT("RAW_WIRE"),             3, 10, 0.85f));
		T.Entries.Add(MakeEntry(TEXT("RAW_DUCT_TAPE"),        1, 4, 0.55f));
		T.Entries.Add(MakeEntry(TEXT("RAW_GLUE"),             1, 3, 0.45f));
		CrashLootTemplates.Add(TEXT("EngineeringWreck"), T);
	}

	// AvionicsWreck — electronics + early Remnant artifacts
	{
		FQRCrashLootTemplate T;
		T.Entries.Add(MakeEntry(TEXT("RAW_CIRCUIT_BOARD"),    2, 5, 0.85f));
		T.Entries.Add(MakeEntry(TEXT("ATT_OPTIC_SCOPE"),      1, 1, 0.35f));
		T.Entries.Add(MakeEntry(TEXT("REM_ART_DATA_SHARD"),   1, 2, 0.25f));
		T.Entries.Add(MakeEntry(TEXT("RAW_BATTERY"),          1, 4, 0.65f));
		T.Entries.Add(MakeEntry(TEXT("RAW_ANTENNA_PARTS"),    1, 3, 0.55f));
		CrashLootTemplates.Add(TEXT("AvionicsWreck"), T);
	}

	// LuggageWreck — civilian / personal
	{
		FQRCrashLootTemplate T;
		T.Entries.Add(MakeEntry(TEXT("CLT_JACKET"),           1, 1, 0.55f));
		T.Entries.Add(MakeEntry(TEXT("CLT_BACKPACK"),         1, 1, 0.45f));
		T.Entries.Add(MakeEntry(TEXT("CSM_HAT"),              1, 1, 0.35f));
		T.Entries.Add(MakeEntry(TEXT("FOD_RATION_BAR"),       1, 4, 0.55f));
		T.Entries.Add(MakeEntry(TEXT("RAW_FABRIC"),           2, 6, 0.75f));
		T.Entries.Add(MakeEntry(TEXT("TOL_LIGHTER"),          1, 1, 0.45f));
		CrashLootTemplates.Add(TEXT("LuggageWreck"), T);
	}

	// PowerModuleWreck — high-tier power components
	{
		FQRCrashLootTemplate T;
		T.Entries.Add(MakeEntry(TEXT("RAW_POWER_CELL"),       1, 3, 0.85f));
		T.Entries.Add(MakeEntry(TEXT("RAW_REGULATOR"),        1, 2, 0.55f));
		T.Entries.Add(MakeEntry(TEXT("RAW_CAPACITOR"),        2, 6, 0.85f));
		T.Entries.Add(MakeEntry(TEXT("REM_ART_POWER_CELL"),   1, 1, 0.15f));
		T.Entries.Add(MakeEntry(TEXT("RAW_METAL_INGOT"),      2, 5, 0.65f));
		CrashLootTemplates.Add(TEXT("PowerModuleWreck"), T);
	}
}


// ─── Top-level ──────────────────────────────────────────────────────

void AQRWorldGenSpawner::SpawnAll()
{
	ClearAll();
	SpawnPOIs();
	SpawnFauna();
	SpawnCaves();
	UE_LOG(LogTemp, Log, TEXT("[QRWorldGenSpawner] SpawnAll done — %d actors live"),
		SpawnedActors.Num());
}


void AQRWorldGenSpawner::ClearAll()
{
	for (TWeakObjectPtr<AActor>& W : SpawnedActors)
	{
		if (AActor* A = W.Get()) A->Destroy();
	}
	SpawnedActors.Reset();
}


void AQRWorldGenSpawner::SpawnPOIsOnly() { ClearAll(); SpawnPOIs(); }
void AQRWorldGenSpawner::SpawnFaunaOnly() { /* keep POIs */ SpawnFauna(); }
void AQRWorldGenSpawner::SpawnCavesOnly() { /* keep POIs */ SpawnCaves(); }


// ─── POIs ───────────────────────────────────────────────────────────

void AQRWorldGenSpawner::SpawnPOIs()
{
	UWorld* W = GetWorld();
	if (!W) return;
	UQRWorldGenSubsystem* Sub = W->GetSubsystem<UQRWorldGenSubsystem>();
	if (!Sub || !Sub->bGenerated)
	{
		UE_LOG(LogTemp, Warning, TEXT("[QRWorldGenSpawner] worldgen subsystem not generated; run AQRWorldGenSeedActor::Generate first"));
		return;
	}

	int32 RemnantCycleIdx = 0;
	const TArray<EQRRemnantStructureType> RemnantCycle = {
		EQRRemnantStructureType::SignalSpire,
		EQRRemnantStructureType::PowerCore,
		EQRRemnantStructureType::DataArchive,
		EQRRemnantStructureType::ResonanceChamber,
	};

	for (const FQRPOIPlacement& P : Sub->POIPlacements)
	{
		FVector SpawnLoc = P.WorldLocation;
		FVector GroundHit;
		if (TraceGround(P.WorldLocation, GroundHit)) SpawnLoc = GroundHit;
		const FRotator Rot(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f);

		// 1. POIArchetypeTable override path — if set and a row matches
		//    the archetype id, use the table's actor class + loot.
		UClass*                       OverrideCls    = nullptr;
		const FQRCrashLootTemplate*   OverrideTemplate = nullptr;
		UStaticMesh*                  OverrideMesh   = nullptr;
		if (POIArchetypeTable)
		{
			if (FQRPOIArchetypeRow* Row = POIArchetypeTable->FindRow<FQRPOIArchetypeRow>(P.ArchetypeId, TEXT("")))
			{
				OverrideCls = Row->ActorClass.LoadSynchronous();
				if (Row->LootTemplate.Entries.Num() > 0) OverrideTemplate = &Row->LootTemplate;
				if (Row->MeshOptions.Num() > 0)
				{
					const int32 PickIdx = FMath::Abs(GetTypeHash(P.WorldLocation.ToString())) % Row->MeshOptions.Num();
					OverrideMesh = Row->MeshOptions[PickIdx].LoadSynchronous();
				}
			}
		}

		if (OverrideCls)
		{
			if (AActor* A = SpawnAt(OverrideCls, SpawnLoc, Rot))
			{
				// Common cases: remnant kind cycling + crash site loot.
				if (AQRRemnantSite* R = Cast<AQRRemnantSite>(A))
				{
					R->Kind = RemnantCycle[RemnantCycleIdx++ % RemnantCycle.Num()];
				}
				if (AQRCrashSiteActor* Crash = Cast<AQRCrashSiteActor>(A))
				{
					Crash->ArchetypeId = P.ArchetypeId;
					const int32 LootSeed = Sub->WorldSeed
						^ GetTypeHash(P.ArchetypeId)
						^ GetTypeHash(P.WorldLocation.ToString());
					if (OverrideTemplate)
					{
						Crash->PopulateLoot(*OverrideTemplate, LootSeed);
					}
					else if (CrashLootTemplates.Contains(P.ArchetypeId))
					{
						Crash->PopulateLoot(CrashLootTemplates[P.ArchetypeId], LootSeed);
					}
				}
				if (AQRFactionCamp* Camp = Cast<AQRFactionCamp>(A))
				{
					const int32 Idx = SpawnedActors.Num();
					const FString IdStr = FString::Printf(TEXT("Camp_%03d"), Idx);
					Camp->DisplayName = FText::FromString(IdStr);
					if (Camp->Sim)
					{
						Camp->Sim->CampId = FName(*IdStr);
						const int32 LeadHash = FMath::Abs(GetTypeHash(P.WorldLocation.ToString()) ^ Sub->WorldSeed);
						Camp->Sim->FallbackLeadership = (LeadHash % 100) / 10.0f;
					}
				}
				// Optional mesh override — assigns to the first
				// UStaticMeshComponent on the spawned actor.
				if (OverrideMesh)
				{
					if (UStaticMeshComponent* SMC = A->FindComponentByClass<UStaticMeshComponent>())
					{
						SMC->SetStaticMesh(OverrideMesh);
					}
				}
			}
			continue;
		}

		// 2. Hardcoded fallback path — same logic as before.
		if (P.ArchetypeId == TEXT("ConcordatCapital") && ConcordatCapitalClass)
		{
			SpawnAt(ConcordatCapitalClass, SpawnLoc, Rot);
		}
		else if (P.ArchetypeId == TEXT("RemnantSite") && RemnantSiteClass)
		{
			if (AActor* A = SpawnAt(RemnantSiteClass, SpawnLoc, Rot))
			{
				if (AQRRemnantSite* R = Cast<AQRRemnantSite>(A))
				{
					R->Kind = RemnantCycle[RemnantCycleIdx % RemnantCycle.Num()];
					++RemnantCycleIdx;
				}
			}
		}
		else if (P.ArchetypeId == TEXT("FactionSatellite") && FactionSatelliteClass)
		{
			if (AActor* A = SpawnAt(FactionSatelliteClass, SpawnLoc, Rot))
			{
				// Seed each camp's sim with a deterministic but varied
				// CampId + leadership. Same WorldSeed → same camps.
				if (AQRFactionCamp* Camp = Cast<AQRFactionCamp>(A))
				{
					const int32 Idx = SpawnedActors.Num();  // grows as we spawn
					const FString IdStr = FString::Printf(TEXT("Camp_%03d"), Idx);
					Camp->DisplayName = FText::FromString(IdStr);
					if (Camp->Sim)
					{
						Camp->Sim->CampId = FName(*IdStr);
						// Spread leadership across the camps so some are
						// dangerous strategists, others rush blindly.
						// Seeded by world+position so it's deterministic.
						const int32 LeadHash = FMath::Abs(GetTypeHash(P.WorldLocation.ToString()) ^ Sub->WorldSeed);
						const float Leadership = (LeadHash % 100) / 10.0f;  // 0..10
						Camp->Sim->FallbackLeadership = Leadership;
					}
				}
			}
		}
		else if (CrashSiteClass && CrashLootTemplates.Contains(P.ArchetypeId))
		{
			if (AActor* A = SpawnAt(CrashSiteClass, SpawnLoc, Rot))
			{
				if (AQRCrashSiteActor* Crash = Cast<AQRCrashSiteActor>(A))
				{
					Crash->ArchetypeId = P.ArchetypeId;
					const int32 LootSeed = Sub->WorldSeed
						^ GetTypeHash(P.ArchetypeId)
						^ GetTypeHash(P.WorldLocation.ToString());
					const FQRCrashLootTemplate& Template = CrashLootTemplates[P.ArchetypeId];
					Crash->PopulateLoot(Template, LootSeed);
				}
			}
		}
	}
}


// ─── Fauna ──────────────────────────────────────────────────────────

void AQRWorldGenSpawner::SpawnFauna()
{
	UWorld* W = GetWorld();
	if (!W) return;
	UQRWorldGenSubsystem* Sub = W->GetSubsystem<UQRWorldGenSubsystem>();
	if (!Sub || !Sub->bGenerated) return;

	FRandomStream Rng(Sub->WorldSeed ^ 0x0F4B0FAB);  // "FaB" — fauna seed

	const float WorldKm2 = Sub->WorldMapSizeKm * Sub->WorldMapSizeKm;
	const int32 TotalDesired = FMath::FloorToInt(FaunaPerKm2Base * WorldKm2);
	if (TotalDesired <= 0) return;

	// Sample cells at random and place a group of the appropriate
	// biome's fauna there.
	int32 Placed = 0;
	int32 Attempts = 0;
	const int32 MaxAttempts = TotalDesired * 8;

	while (Placed < TotalDesired && Attempts < MaxAttempts)
	{
		++Attempts;
		const int32 CellIdx = Rng.RandRange(0, Sub->Cells.Num() - 1);
		const FQRWorldCell& Cell = Sub->Cells[CellIdx];
		if (Cell.MacroBiome == TEXT("HazardBelt")) continue;

		TObjectPtr<UQRFaunaSpawnRule>* RuleFound = FaunaRulesPerBiome.Find(Cell.MacroBiome);
		const UQRFaunaSpawnRule* Rule = RuleFound ? RuleFound->Get() : nullptr;
		if (!Rule || Rule->Entries.Num() == 0)
		{
			// No rules for this biome — use a sparse fallback if there's
			// a wildlife class wired so the world isn't completely empty.
			if (!WildlifeFallbackClass) continue;
			if (Rng.FRand() > 0.05f) continue;  // 5% sparse fallback
			FVector WorldXY(
				((Cell.X - Sub->GridW * 0.5f) + Rng.FRand()) * Sub->CellSizeMeters * 100.0f,
				((Cell.Y - Sub->GridH * 0.5f) + Rng.FRand()) * Sub->CellSizeMeters * 100.0f,
				0.0f);
			FVector Hit;
			if (!TraceGround(WorldXY, Hit)) continue;
			if (AActor* A = SpawnAt(WildlifeFallbackClass, Hit, FRotator(0, Rng.FRandRange(0.0f, 360.0f), 0)))
				++Placed;
			continue;
		}

		// Weighted pick.
		float TotalWeight = 0.0f;
		for (const FQRFaunaEntry& E : Rule->Entries) TotalWeight += FMath::Max(0.0f, E.Weight);
		if (TotalWeight <= 0.0f) continue;
		const float Pick = Rng.FRandRange(0.0f, TotalWeight);
		float Acc = 0.0f;
		int32 EntryIdx = Rule->Entries.Num() - 1;
		for (int32 i = 0; i < Rule->Entries.Num(); ++i)
		{
			Acc += FMath::Max(0.0f, Rule->Entries[i].Weight);
			if (Pick <= Acc) { EntryIdx = i; break; }
		}
		const FQRFaunaEntry& Entry = Rule->Entries[EntryIdx];

		// Predator density cap.
		if (Entry.bIsPredator && Rng.FRand() > Rule->MaxPredatorFraction) continue;

		// Anchor location within this cell, ground-traced.
		FVector WorldXY(
			((Cell.X - Sub->GridW * 0.5f) + Rng.FRand()) * Sub->CellSizeMeters * 100.0f,
			((Cell.Y - Sub->GridH * 0.5f) + Rng.FRand()) * Sub->CellSizeMeters * 100.0f,
			0.0f);
		FVector Hit;
		if (!TraceGround(WorldXY, Hit)) continue;

		const int32 GroupSize = Rng.RandRange(Entry.MinGroupSize,
			FMath::Max(Entry.MinGroupSize, Entry.MaxGroupSize));

		UClass* Cls = Entry.ActorClass.IsNull() ? *WildlifeFallbackClass : Entry.ActorClass.LoadSynchronous();
		if (!Cls) continue;

		for (int32 g = 0; g < GroupSize && Placed < TotalDesired; ++g)
		{
			const FVector Offset(Rng.FRandRange(-200.0f, 200.0f),
				Rng.FRandRange(-200.0f, 200.0f), 0.0f);
			if (AActor* A = SpawnAt(Cls, Hit + Offset, FRotator(0, Rng.FRandRange(0.0f, 360.0f), 0)))
				++Placed;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[QRWorldGenSpawner] SpawnFauna placed %d / %d"),
		Placed, TotalDesired);
}


// ─── Caves ──────────────────────────────────────────────────────────

void AQRWorldGenSpawner::SpawnCaves()
{
	UWorld* W = GetWorld();
	if (!W || !CaveEntranceClass) return;
	UQRWorldGenSubsystem* Sub = W->GetSubsystem<UQRWorldGenSubsystem>();
	if (!Sub || !Sub->bGenerated) return;

	FRandomStream Rng(Sub->WorldSeed ^ 0x0CAFE100);

	int32 SinceLastSpawn = 0;
	for (const FQRWorldCell& Cell : Sub->Cells)
	{
		if (!(Cell.HabitatFlags & static_cast<uint8>(EQRHabitatFlag::Caves))) continue;
		++SinceLastSpawn;
		if (SinceLastSpawn < OneCavePerNFlaggedCells) continue;
		SinceLastSpawn = 0;

		FVector WorldXY(
			((Cell.X - Sub->GridW * 0.5f) + 0.5f) * Sub->CellSizeMeters * 100.0f,
			((Cell.Y - Sub->GridH * 0.5f) + 0.5f) * Sub->CellSizeMeters * 100.0f,
			0.0f);
		FVector Hit;
		if (!TraceGround(WorldXY, Hit)) continue;
		SpawnAt(CaveEntranceClass, Hit, FRotator(0, Rng.FRandRange(0.0f, 360.0f), 0));
	}
}


// ─── Helpers ────────────────────────────────────────────────────────

AActor* AQRWorldGenSpawner::SpawnAt(UClass* Cls, const FVector& Loc, const FRotator& Rot)
{
	if (!Cls) return nullptr;
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	if (AActor* A = GetWorld()->SpawnActor<AActor>(Cls, Loc, Rot, Params))
	{
		SpawnedActors.Add(A);
		return A;
	}
	return nullptr;
}


bool AQRWorldGenSpawner::TraceGround(const FVector& XY, FVector& OutHit) const
{
	UWorld* W = GetWorld();
	if (!W) return false;
	const FVector Start(XY.X, XY.Y, 50000.0f);
	const FVector End  (XY.X, XY.Y, -50000.0f);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(QRSpawnerTrace), /*bComplex*/ false);
	Params.AddIgnoredActor(this);
	if (W->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
	{
		OutHit = Hit.ImpactPoint;
		return true;
	}
	return false;
}
