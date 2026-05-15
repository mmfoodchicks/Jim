#include "QRWorldGenSubsystem.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "ImageUtils.h"
#include "Math/UnrealMathUtility.h"


// ─── Canonical biome → color palette for minimap output ─────────────
// Lifted from Visual World Bible §3 Biome Visual Table palette
// descriptions, condensed to single RGBs.
static const TMap<FName, FColor>& BiomeColorPalette()
{
	static const TMap<FName, FColor> Map = {
		// Surface tier — pale / black / slate
		{ TEXT("BasaltShelf"),    FColor(48,  48,  56)  },
		{ TEXT("WindPlains"),     FColor(110, 120, 130) },
		{ TEXT("MeltlineEdges"),  FColor(96,  82,  60)  },
		{ TEXT("CraterFloors"),   FColor(126, 96,  72)  },
		// Mid tier — rust / sulfur / teal moss / amber vent
		{ TEXT("WetBasins"),      FColor(72,  102, 80)  },
		{ TEXT("ShallowFens"),    FColor(54,  84,  60)  },
		{ TEXT("ThermalCracks"),  FColor(180, 134, 50)  },
		{ TEXT("GlassDunes"),     FColor(170, 178, 188) },
		{ TEXT("MossFields"),     FColor(50,  90,  70)  },
		// Deep tier — magnetic red / cold / impossible color highlights
		{ TEXT("MagneticRidges"), FColor(150, 50,  46)  },
		{ TEXT("HighRims"),       FColor(176, 192, 200) },
		{ TEXT("ColdBasins"),     FColor(170, 198, 218) },
		{ TEXT("CanyonWebs"),     FColor(48,  40,  40)  },
		{ TEXT("RidgeShadows"),   FColor(40,  44,  62)  },
		// Hazard belt + unmapped fallback
		{ TEXT("HazardBelt"),     FColor(80,  10,  10)  },
	};
	return Map;
}


// ─── Lifecycle ──────────────────────────────────────────────────────

void UQRWorldGenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	PopulateDefaultBandPools();
}


void UQRWorldGenSubsystem::PopulateDefaultBandPools()
{
	// GDD §4 canonical biome assignment by depth band.
	BandPools.Reset();

	FQRBiomeBandPool Surface;
	Surface.Biomes = {
		TEXT("BasaltShelf"),
		TEXT("WindPlains"),
		TEXT("MeltlineEdges"),
		TEXT("CraterFloors"),
	};
	BandPools.Add(EQRDepthBand::Surface, Surface);

	FQRBiomeBandPool Mid;
	Mid.Biomes = {
		TEXT("WetBasins"),
		TEXT("ShallowFens"),
		TEXT("ThermalCracks"),
		TEXT("GlassDunes"),
		TEXT("MossFields"),
	};
	BandPools.Add(EQRDepthBand::Mid, Mid);

	FQRBiomeBandPool Deep;
	Deep.Biomes = {
		TEXT("MagneticRidges"),
		TEXT("HighRims"),
		TEXT("ColdBasins"),
		TEXT("CanyonWebs"),
		TEXT("RidgeShadows"),
	};
	BandPools.Add(EQRDepthBand::Deep, Deep);

	// Remnant band uses Deep palette + extra weight on CanyonWebs /
	// CraterWalls; we keep it simple and reuse Deep biomes here.
	FQRBiomeBandPool Remnant;
	Remnant.Biomes = {
		TEXT("CanyonWebs"),
		TEXT("MagneticRidges"),
		TEXT("HighRims"),
	};
	BandPools.Add(EQRDepthBand::Remnant, Remnant);
}


// ─── Top-level Generate ─────────────────────────────────────────────

void UQRWorldGenSubsystem::Generate(int32 InWorldSeed,
	float InWorldMapSizeKm, float InCellSizeMeters)
{
	WorldSeed       = InWorldSeed;
	WorldMapSizeKm  = FMath::Clamp(InWorldMapSizeKm, 16.0f, 256.0f);
	CellSizeMeters  = FMath::Clamp(InCellSizeMeters, 32.0f, 1024.0f);

	GridW = FMath::Max(1, FMath::FloorToInt(WorldMapSizeKm * 1000.0f / CellSizeMeters));
	GridH = GridW;

	// Derive sub-stream seeds via combinatorial scramble — bias-free
	// for our purposes and keeps shareable seeds deterministic.
	BiomeRng.Initialize(  WorldSeed ^ 0xA1B2C3D4);
	POIRng.Initialize(    WorldSeed ^ 0xB2C3D4E5);
	EcologyRng.Initialize(WorldSeed ^ 0xC3D4E5F6);
	FactionRng.Initialize(WorldSeed ^ 0xD4E5F607);
	LootRng.Initialize(   WorldSeed ^ 0xE5F60718);

	Cells.Reset();
	Cells.SetNum(GridW * GridH);
	POIPlacements.Reset();

	GenerateCellGrid();
	PlacePOIs();

	bGenerated = true;
	OnWorldGenComplete.Broadcast();

	UE_LOG(LogTemp, Log,
		TEXT("[QRWorldGen] seed=%d size=%.0fkm grid=%dx%d cells=%d POIs=%d"),
		WorldSeed, WorldMapSizeKm, GridW, GridH,
		Cells.Num(), POIPlacements.Num());
}


// ─── Cell grid ──────────────────────────────────────────────────────

void UQRWorldGenSubsystem::GenerateCellGrid()
{
	const float HalfWorldM    = WorldMapSizeKm * 1000.0f * 0.5f;
	const float PlayableRadM  = HalfWorldM;
	const float HazardInnerM  = FMath::Max(0.0f, PlayableRadM - HazardBeltMeters);

	for (int32 Y = 0; Y < GridH; ++Y)
	{
		for (int32 X = 0; X < GridW; ++X)
		{
			const int32 Idx = Y * GridW + X;
			FQRWorldCell& Cell = Cells[Idx];
			Cell.X = X;
			Cell.Y = Y;

			// World position at cell center (origin = world center).
			const float WX = ((X - GridW * 0.5f) + 0.5f) * CellSizeMeters;
			const float WY = ((Y - GridH * 0.5f) + 0.5f) * CellSizeMeters;
			const float DistM = FMath::Sqrt(WX * WX + WY * WY);
			Cell.DistKm = DistM / 1000.0f;

			// Sample macro noise to distort the band boundary so the
			// tier rings aren't perfect circles.
			const float MNoise = FMath::PerlinNoise2D(
				FVector2D(WX * MacroNoiseScale, WY * MacroNoiseScale));
			const float DistortedDistM = DistM + MNoise * 4000.0f;
			Cell.DepthBand = DepthBandForDistanceKm(DistortedDistM / 1000.0f);

			// Hazard belt at the outer edge — set BiomeTag to HazardBelt
			// regardless of band so spawning code can skip it cleanly.
			if (DistM >= HazardInnerM)
			{
				Cell.MacroBiome = TEXT("HazardBelt");
				continue;
			}

			// Pick a biome from the band pool using a Voronoi-feeling
			// sample: combine two noise scales so adjacent cells stick
			// together into patches.
			const float V1 = FMath::PerlinNoise2D(FVector2D(
				WX * MacroNoiseScale * 1.7f + 1000.0f,
				WY * MacroNoiseScale * 1.7f - 1000.0f));
			const float V2 = FMath::PerlinNoise2D(FVector2D(
				WX * MacroNoiseScale * 3.1f - 2000.0f,
				WY * MacroNoiseScale * 3.1f + 2000.0f));
			const float Sample = (V1 + V2 * 0.5f);

			Cell.MacroBiome = PickBiomeForBand(Cell.DepthBand, Sample);

			// Micro overlay sampling.
			const float UNoise = FMath::PerlinNoise2D(
				FVector2D(WX * MicroNoiseScale, WY * MicroNoiseScale));
			Cell.MicroBiome = PickMicroOverlay(Cell, UNoise);

			// Habitat flags via thresholded micro noise.
			const float HabSample = FMath::PerlinNoise2D(
				FVector2D(WX * MicroNoiseScale * 2.0f + 5000.0f,
				          WY * MicroNoiseScale * 2.0f - 5000.0f));
			if (HabSample > 0.45f)
			{
				Cell.HabitatFlags |= static_cast<uint8>(EQRHabitatFlag::Caves);
			}
			if (HabSample < -0.55f)
			{
				Cell.HabitatFlags |= static_cast<uint8>(EQRHabitatFlag::AbandonedStructure);
			}
		}
	}
}


EQRDepthBand UQRWorldGenSubsystem::DepthBandForDistanceKm(float DistKm) const
{
	// Concentric rings — the Concordat sits at center, so deeper
	// (smaller DistKm) means harder tier. Boundaries at quartiles of
	// the playable radius.
	const float HalfWorldKm = WorldMapSizeKm * 0.5f;
	const float Frac = FMath::Clamp(DistKm / HalfWorldKm, 0.0f, 1.0f);
	if (Frac <= 0.15f) return EQRDepthBand::Remnant;
	if (Frac <= 0.40f) return EQRDepthBand::Deep;
	if (Frac <= 0.70f) return EQRDepthBand::Mid;
	return EQRDepthBand::Surface;
}


FName UQRWorldGenSubsystem::PickBiomeForBand(EQRDepthBand Band, float NoiseSample) const
{
	const FQRBiomeBandPool* Pool = BandPools.Find(Band);
	if (!Pool || Pool->Biomes.Num() == 0)
	{
		return TEXT("BasaltShelf");
	}
	// Map noise (-1..1) → index. Adding 1.0 then multiplying by
	// half count keeps the distribution uniform-ish across the pool.
	const int32 N = Pool->Biomes.Num();
	const float Mapped = FMath::Clamp((NoiseSample + 1.0f) * 0.5f, 0.0f, 0.9999f);
	const int32 Idx = FMath::FloorToInt(Mapped * N);
	return Pool->Biomes[Idx];
}


FName UQRWorldGenSubsystem::PickMicroOverlay(const FQRWorldCell& Cell, float NoiseSample) const
{
	// Only thermal/vent biomes get vent-rim micro overlays; ridge
	// biomes get IronBasalt; cold biomes get IceCaves at thresholds.
	if (Cell.MacroBiome == TEXT("ThermalCracks") && NoiseSample > 0.35f) return TEXT("VentRims");
	if (Cell.MacroBiome == TEXT("ThermalCracks") && NoiseSample > 0.55f) return TEXT("SteamVents");
	if (Cell.MacroBiome == TEXT("MagneticRidges") && NoiseSample < -0.30f) return TEXT("IronBasalt");
	if (Cell.MacroBiome == TEXT("ColdBasins")     && NoiseSample > 0.50f) return TEXT("IceCaves");
	if (Cell.MacroBiome == TEXT("WetBasins")      && NoiseSample < -0.40f) return TEXT("ShadowFens");
	return NAME_None;
}


// ─── POI placement ──────────────────────────────────────────────────

void UQRWorldGenSubsystem::PlacePOIs()
{
	// Reusable scratch helpers.
	auto WorldPosForCell = [&](int32 X, int32 Y) -> FVector
	{
		const float WX = ((X - GridW * 0.5f) + 0.5f) * CellSizeMeters * 100.0f;  // m → cm
		const float WY = ((Y - GridH * 0.5f) + 0.5f) * CellSizeMeters * 100.0f;
		return FVector(WX, WY, 0.0f);
	};

	auto MinSpacingOK = [&](const FVector& Loc, float MinDistCm) -> bool
	{
		for (const FQRPOIPlacement& P : POIPlacements)
		{
			if (FVector::DistSquared(P.WorldLocation, Loc) < MinDistCm * MinDistCm)
				return false;
		}
		return true;
	};

	auto SampleRandomCellInBand = [&](EQRDepthBand Band, int32 MaxTries,
		FName RequireBiome = NAME_None) -> int32
	{
		for (int32 Try = 0; Try < MaxTries; ++Try)
		{
			const int32 Idx = POIRng.RandRange(0, Cells.Num() - 1);
			const FQRWorldCell& C = Cells[Idx];
			if (C.DepthBand != Band) continue;
			if (C.MacroBiome == TEXT("HazardBelt")) continue;
			if (!RequireBiome.IsNone() && C.MacroBiome != RequireBiome) continue;
			return Idx;
		}
		return INDEX_NONE;
	};

	auto AddPlacement = [&](FName Archetype, const FVector& Loc, float Radius,
		const FQRWorldCell& Cell)
	{
		FQRPOIPlacement P;
		P.ArchetypeId   = Archetype;
		P.WorldLocation = Loc;
		P.RadiusCm      = Radius;
		P.BiomeTag      = Cell.MacroBiome;
		P.DepthBand     = Cell.DepthBand;
		POIPlacements.Add(P);
	};

	// ── 1. Concordat capital — exact center.
	{
		const FVector Center(0, 0, 0);
		const int32 CenterIdx = (GridH / 2) * GridW + (GridW / 2);
		if (Cells.IsValidIndex(CenterIdx))
		{
			AddPlacement(TEXT("ConcordatCapital"), Center, 2500.0f, Cells[CenterIdx]);
		}
	}

	// ── 2. Remnant sites — 4 in the Remnant band.
	for (int32 i = 0; i < 4; ++i)
	{
		const int32 Idx = SampleRandomCellInBand(EQRDepthBand::Remnant, 200);
		if (Idx == INDEX_NONE) break;
		const FQRWorldCell& C = Cells[Idx];
		const FVector Loc = WorldPosForCell(C.X, C.Y);
		if (Loc.SizeSquared() < StartSafeRadiusMeters * StartSafeRadiusMeters * 10000.0f) continue;
		if (!MinSpacingOK(Loc, 8000.0f * 100.0f)) continue;  // 8km apart
		AddPlacement(TEXT("RemnantSite"), Loc, 2000.0f, C);
	}

	// ── 3. Faction satellites — ring in Deep band.
	for (int32 i = 0; i < 8; ++i)
	{
		const int32 Idx = SampleRandomCellInBand(EQRDepthBand::Deep, 200);
		if (Idx == INDEX_NONE) break;
		const FQRWorldCell& C = Cells[Idx];
		const FVector Loc = WorldPosForCell(C.X, C.Y);
		if (!MinSpacingOK(Loc, 4000.0f * 100.0f)) continue;
		AddPlacement(TEXT("FactionSatellite"), Loc, 1500.0f, C);
	}

	// ── 4. Major crash wrecks — Surface band, scattered.
	static const TArray<FName> WreckTypes = {
		TEXT("ArmoryWreck"),
		TEXT("MedBayWreck"),
		TEXT("GalleyWreck"),
		TEXT("EngineeringWreck"),
		TEXT("AvionicsWreck"),
		TEXT("LuggageWreck"),
		TEXT("PowerModuleWreck"),
	};
	for (int32 i = 0; i < 12; ++i)
	{
		const int32 Idx = SampleRandomCellInBand(EQRDepthBand::Surface, 200);
		if (Idx == INDEX_NONE) break;
		const FQRWorldCell& C = Cells[Idx];
		const FVector Loc = WorldPosForCell(C.X, C.Y);
		if (Loc.SizeSquared() < StartSafeRadiusMeters * StartSafeRadiusMeters * 10000.0f) continue;
		if (!MinSpacingOK(Loc, 1500.0f * 100.0f)) continue;
		const FName Archetype = WreckTypes[POIRng.RandRange(0, WreckTypes.Num() - 1)];
		AddPlacement(Archetype, Loc, 800.0f, C);
	}

	// ── 5. Minor POIs — biome-specific filler.
	for (int32 i = 0; i < 20; ++i)
	{
		const int32 Idx = POIRng.RandRange(0, Cells.Num() - 1);
		if (!Cells.IsValidIndex(Idx)) continue;
		const FQRWorldCell& C = Cells[Idx];
		if (C.MacroBiome == TEXT("HazardBelt")) continue;
		const FVector Loc = WorldPosForCell(C.X, C.Y);
		if (!MinSpacingOK(Loc, 800.0f * 100.0f)) continue;

		FName Archetype = TEXT("SurveyMarker");
		if (C.MacroBiome == TEXT("ThermalCracks"))   Archetype = TEXT("ThermalVentField");
		else if (C.MacroBiome == TEXT("MagneticRidges")) Archetype = TEXT("RazorstoneRidge");
		else if (C.MacroBiome == TEXT("WetBasins"))      Archetype = TEXT("FloodedSinkhole");
		else if (C.MacroBiome == TEXT("ColdBasins"))     Archetype = TEXT("IceTunnel");
		AddPlacement(Archetype, Loc, 500.0f, C);
	}
}


// ─── Lookups ────────────────────────────────────────────────────────

bool UQRWorldGenSubsystem::WorldToCell(FVector WorldPos, int32& OutX, int32& OutY) const
{
	if (!bGenerated || GridW == 0 || GridH == 0) return false;
	const float CellCm = CellSizeMeters * 100.0f;
	const float FX = WorldPos.X / CellCm + GridW * 0.5f;
	const float FY = WorldPos.Y / CellCm + GridH * 0.5f;
	OutX = FMath::FloorToInt(FX);
	OutY = FMath::FloorToInt(FY);
	return (OutX >= 0 && OutX < GridW && OutY >= 0 && OutY < GridH);
}


FQRWorldCell UQRWorldGenSubsystem::GetCellAt(FVector WorldPos) const
{
	int32 X = 0, Y = 0;
	if (WorldToCell(WorldPos, X, Y))
	{
		const int32 Idx = Y * GridW + X;
		if (Cells.IsValidIndex(Idx)) return Cells[Idx];
	}
	return FQRWorldCell();
}


FName UQRWorldGenSubsystem::GetBiomeAt(FVector WorldPos) const
{
	return GetCellAt(WorldPos).MacroBiome;
}


EQRDepthBand UQRWorldGenSubsystem::GetDepthBandAt(FVector WorldPos) const
{
	return GetCellAt(WorldPos).DepthBand;
}


TArray<FQRPOIPlacement> UQRWorldGenSubsystem::GetPOIs(FName Archetype) const
{
	if (Archetype.IsNone()) return POIPlacements;
	TArray<FQRPOIPlacement> Out;
	for (const FQRPOIPlacement& P : POIPlacements)
	{
		if (P.ArchetypeId == Archetype) Out.Add(P);
	}
	return Out;
}


// ─── Minimap texture ────────────────────────────────────────────────

UTexture2D* UQRWorldGenSubsystem::BuildMinimapTexture(int32 PixelsPerCell) const
{
	if (!bGenerated || Cells.Num() == 0) return nullptr;
	PixelsPerCell = FMath::Clamp(PixelsPerCell, 1, 8);

	const int32 W = GridW * PixelsPerCell;
	const int32 H = GridH * PixelsPerCell;
	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(W * H);

	const TMap<FName, FColor>& Palette = BiomeColorPalette();
	const FColor Fallback(140, 0, 140);  // magenta on unmapped biome

	for (int32 Y = 0; Y < GridH; ++Y)
	{
		for (int32 X = 0; X < GridW; ++X)
		{
			const FQRWorldCell& Cell = Cells[Y * GridW + X];
			const FColor* Found = Palette.Find(Cell.MacroBiome);
			const FColor Color = Found ? *Found : Fallback;

			// Splat PixelsPerCell × PixelsPerCell pixel block.
			for (int32 dy = 0; dy < PixelsPerCell; ++dy)
			{
				for (int32 dx = 0; dx < PixelsPerCell; ++dx)
				{
					const int32 PX = X * PixelsPerCell + dx;
					const int32 PY = Y * PixelsPerCell + dy;
					Pixels[PY * W + PX] = Color;
				}
			}
		}
	}

	// Overlay POIs as bright dots.
	for (const FQRPOIPlacement& P : POIPlacements)
	{
		int32 PX = FMath::FloorToInt(P.WorldLocation.X / (CellSizeMeters * 100.0f) + GridW * 0.5f) * PixelsPerCell;
		int32 PY = FMath::FloorToInt(P.WorldLocation.Y / (CellSizeMeters * 100.0f) + GridH * 0.5f) * PixelsPerCell;
		FColor Dot = FColor::Yellow;
		if (P.ArchetypeId == TEXT("ConcordatCapital")) Dot = FColor::Red;
		else if (P.ArchetypeId == TEXT("RemnantSite")) Dot = FColor(255, 100, 200);
		else if (P.ArchetypeId == TEXT("FactionSatellite")) Dot = FColor::Orange;
		const int32 R = FMath::Max(1, PixelsPerCell);
		for (int32 dy = -R; dy <= R; ++dy)
		{
			for (int32 dx = -R; dx <= R; ++dx)
			{
				const int32 X = PX + dx, Y = PY + dy;
				if (X >= 0 && X < W && Y >= 0 && Y < H)
				{
					Pixels[Y * W + X] = Dot;
				}
			}
		}
	}

	UTexture2D* Tex = FImageUtils::CreateTexture2D(
		W, H, Pixels, GetTransientPackage(),
		TEXT("T_QRMinimap"), RF_Transient,
		FCreateTexture2DParameters());
	return Tex;
}
