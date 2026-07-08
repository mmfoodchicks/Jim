#include "QRDressingStreamer.h"
#include "QRProceduralScatterActor.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"


AQRDressingStreamer::AQRDressingStreamer()
{
	// Per-frame tick is fine: the hot path is one FIntPoint compare;
	// actual generation is metered by TileGenInterval.
	PrimaryActorTick.bCanEverTick = true;
}


void AQRDressingStreamer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player) return;   // editor world / dedicated lull -- idle cheaply

	const FVector PlayerLoc = Player->GetActorLocation();
	const FIntPoint NowTile = TileAt(PlayerLoc);
	if (NowTile != CenterTile)
	{
		CenterTile = NowTile;
		RefreshRing(NowTile);
	}

	GenCooldown -= DeltaTime;
	if (GenCooldown <= 0.0f && PendingTiles.Num() > 0)
	{
		GenCooldown = TileGenInterval;
		GenerateOnePending(PlayerLoc);
	}
}


FIntPoint AQRDressingStreamer::TileAt(const FVector& WorldPos) const
{
	return FIntPoint(
		FMath::FloorToInt(WorldPos.X / TileSizeCm),
		FMath::FloorToInt(WorldPos.Y / TileSizeCm));
}


void AQRDressingStreamer::RefreshRing(const FIntPoint& NewCenter)
{
	TSet<FIntPoint> Wanted;
	for (int32 Dx = -RingRadiusTiles; Dx <= RingRadiusTiles; ++Dx)
	{
		for (int32 Dy = -RingRadiusTiles; Dy <= RingRadiusTiles; ++Dy)
		{
			Wanted.Add(NewCenter + FIntPoint(Dx, Dy));
		}
	}

	// Stream out everything that fell off the ring. ClearGenerated
	// first: HISM components die with the actor but palette-spawned
	// sub-ACTORS (flora nodes etc.) are independent and must be
	// destroyed explicitly or they'd leak behind the player forever.
	for (auto It = LiveTiles.CreateIterator(); It; ++It)
	{
		if (!Wanted.Contains(It->Key))
		{
			if (AQRProceduralScatterActor* Tile = It->Value)
			{
				Tile->ClearGenerated();
				Tile->Destroy();
			}
			It.RemoveCurrent();
		}
	}

	// Queue the gaps, nearest to the player first so the ground under
	// their feet dresses before the horizon does.
	PendingTiles.Reset();
	for (const FIntPoint& T : Wanted)
	{
		if (!LiveTiles.Contains(T))
		{
			PendingTiles.Add(T);
		}
	}
	PendingTiles.Sort([NewCenter](const FIntPoint& A, const FIntPoint& B)
	{
		const FIntPoint DA = A - NewCenter;
		const FIntPoint DB = B - NewCenter;
		return DA.X * DA.X + DA.Y * DA.Y < DB.X * DB.X + DB.Y * DB.Y;
	});
}


void AQRDressingStreamer::GenerateOnePending(const FVector& PlayerLoc)
{
	UWorld* W = GetWorld();
	if (!W || PendingTiles.Num() == 0) return;

	const FIntPoint T = PendingTiles[0];
	PendingTiles.RemoveAt(0);
	if (LiveTiles.Contains(T)) return;

	// Tile box straddles the player's current altitude -- placements
	// trace top-to-bottom of the box, so this window is where ground
	// can be found.
	const FVector Center(
		(T.X + 0.5f) * TileSizeCm,
		(T.Y + 0.5f) * TileSizeCm,
		PlayerLoc.Z);

	const FTransform SpawnXform(FRotator::ZeroRotator, Center);
	AQRProceduralScatterActor* Tile = W->SpawnActorDeferred<AQRProceduralScatterActor>(
		AQRProceduralScatterActor::StaticClass(), SpawnXform, this,
		nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Tile) return;

	Tile->bAutoGenerateOnBeginPlay = false;   // we call Generate() ourselves
	Tile->bUseWorldGenSubsystem    = true;
	Tile->BiomeProfileMap          = BiomeProfileMap;
	Tile->TargetCount              = InstancesPerTile;
	Tile->CullStartDistance        = CullStartDistance;
	Tile->CullEndDistance          = CullEndDistance;

	// Same shape as qr_world_dressing's per-tile formula: a pure
	// function of the tile coordinate + world seed, so leaving and
	// returning rebuilds the identical dressing.
	const int64 Mixed = (static_cast<int64>(T.X) * 7919)
	                  ^ (static_cast<int64>(T.Y) * 6151)
	                  ^ static_cast<int64>(WorldSeed);
	Tile->Seed = static_cast<int32>(Mixed & 0x7FFFFFFF);

	Tile->FinishSpawning(SpawnXform);

	if (Tile->Bounds)
	{
		Tile->Bounds->SetBoxExtent(
			FVector(TileSizeCm * 0.5f, TileSizeCm * 0.5f, TileHalfHeightCm));
	}
	Tile->Generate();

	LiveTiles.Add(T, Tile);
}


void AQRDressingStreamer::DestroyAllTiles()
{
	for (TPair<FIntPoint, TObjectPtr<AQRProceduralScatterActor>>& Pair : LiveTiles)
	{
		if (AQRProceduralScatterActor* Tile = Pair.Value)
		{
			Tile->ClearGenerated();
			Tile->Destroy();
		}
	}
	LiveTiles.Empty();
	PendingTiles.Reset();
}


void AQRDressingStreamer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyAllTiles();
	Super::EndPlay(EndPlayReason);
}
