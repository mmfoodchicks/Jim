#include "QRProceduralScatterActor.h"
#include "QRBiomeProfile.h"
#include "QRWorldGenSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/HitResult.h"
#include "CollisionQueryParams.h"
#include "Math/RandomStream.h"

AQRProceduralScatterActor::AQRProceduralScatterActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	SetRootComponent(Bounds);
	Bounds->InitBoxExtent(FVector(2000.f, 2000.f, 1000.f));
	Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bounds->SetHiddenInGame(true);
}

void AQRProceduralScatterActor::BeginPlay()
{
	Super::BeginPlay();
	if (bAutoGenerateOnBeginPlay)
	{
		Generate();
	}
}

void AQRProceduralScatterActor::ClearGenerated()
{
	for (auto& Pair : MeshInstances)
	{
		if (UHierarchicalInstancedStaticMeshComponent* HISM = Pair.Value)
		{
			HISM->ClearInstances();
		}
	}
	for (TWeakObjectPtr<AActor>& W : SpawnedActors)
	{
		if (AActor* A = W.Get()) A->Destroy();
	}
	SpawnedActors.Reset();
}

void AQRProceduralScatterActor::Generate()
{
	UWorld* W = GetWorld();
	if (!W || !Bounds) return;

	// Pull palette from BiomeProfile when set so the actor's inline
	// Palette field can stay empty in the typical case.
	const TArray<FQRScatterEntry>& EffectivePalette =
		BiomeProfile && BiomeProfile->Palette.Num() > 0
			? BiomeProfile->Palette
			: Palette;

	if (EffectivePalette.Num() == 0) return;

	ClearGenerated();

	FRandomStream Rng(Seed);
	const FBox WorldBox = Bounds->GetScaledBoxExtent().IsZero()
		? FBox(GetActorLocation() - FVector(1000), GetActorLocation() + FVector(1000))
		: FBox::BuildAABB(GetActorLocation(), Bounds->GetScaledBoxExtent());

	TArray<FVector> Placed;
	Placed.Reserve(TargetCount);

	int32 Attempts = 0;
	const int32 MaxAttempts = TargetCount * 20;  // give up after 20x to spare CPU on impossible volumes

	while (Placed.Num() < TargetCount && Attempts < MaxAttempts)
	{
		++Attempts;
		TryPlaceOne(EffectivePalette, Rng, WorldBox, Placed);
	}

	UE_LOG(LogTemp, Log, TEXT("[ProcScatter] %s placed %d / %d (attempts=%d)"),
		*GetName(), Placed.Num(), TargetCount, Attempts);
}

int32 AQRProceduralScatterActor::PickPaletteIndex(
	const TArray<FQRScatterEntry>& Pool, FRandomStream& Rng) const
{
	float TotalWeight = 0.0f;
	for (const FQRScatterEntry& E : Pool) TotalWeight += FMath::Max(0.0f, E.Weight);
	if (TotalWeight <= 0.0f) return -1;

	const float Pick = Rng.FRandRange(0.0f, TotalWeight);
	float Acc = 0.0f;
	for (int32 i = 0; i < Pool.Num(); ++i)
	{
		Acc += FMath::Max(0.0f, Pool[i].Weight);
		if (Pick <= Acc) return i;
	}
	return Pool.Num() - 1;
}

bool AQRProceduralScatterActor::TryPlaceOne(
	const TArray<FQRScatterEntry>& BasePalette,
	FRandomStream& Rng, const FBox& WorldBox,
	TArray<FVector>& AlreadyPlaced)
{
	UWorld* W = GetWorld();
	if (!W) return false;

	// Random (X, Y) inside the box footprint, trace from above the box top.
	const FVector Min = WorldBox.Min;
	const FVector Max = WorldBox.Max;
	const FVector RandXY(
		Rng.FRandRange(Min.X, Max.X),
		Rng.FRandRange(Min.Y, Max.Y),
		0.0f);
	const FVector TraceStart(RandXY.X, RandXY.Y, Max.Z + 100.0f);
	const FVector TraceEnd  (RandXY.X, RandXY.Y, Min.Z - 100.0f);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(QRProcScatter), /*bComplex*/ false);
	Params.AddIgnoredActor(this);
	if (!W->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, Params))
	{
		return false;
	}

	// WorldGen integration — once the XY is known, ask the subsystem
	// what biome owns the cell and swap to that biome's palette.
	// Falls back to BasePalette when the subsystem hasn't generated
	// yet or the cell biome isn't in our map.
	const TArray<FQRScatterEntry>* ActivePalette = &BasePalette;
	if (bUseWorldGenSubsystem)
	{
		if (UQRWorldGenSubsystem* Sub = W->GetSubsystem<UQRWorldGenSubsystem>())
		{
			if (Sub->bGenerated)
			{
				const FName Biome = Sub->GetBiomeAt(Hit.ImpactPoint);
				if (TObjectPtr<UQRBiomeProfile>* Found = BiomeProfileMap.Find(Biome))
				{
					if (UQRBiomeProfile* Profile = *Found)
					{
						if (Profile->Palette.Num() > 0)
						{
							ActivePalette = &Profile->Palette;
						}
					}
				}
			}
		}
	}

	const int32 EntryIdx = PickPaletteIndex(*ActivePalette, Rng);
	if (EntryIdx == INDEX_NONE) return false;
	const FQRScatterEntry& Entry = (*ActivePalette)[EntryIdx];

	// Slope check.
	const float SlopeDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Hit.ImpactNormal.Z, -1.f, 1.f)));
	if (SlopeDeg > MaxSlopeDeg) return false;

	// Spacing check.
	const FVector Loc = Hit.ImpactPoint + FVector(0, 0, Entry.ZOffset);
	for (const FVector& Other : AlreadyPlaced)
	{
		if (FVector::DistSquared(Loc, Other) < MinSpacing * MinSpacing) return false;
	}

	// Rotation. Yaw is random; pitch/roll either zero or aligned to surface.
	FRotator Rot = FRotator::ZeroRotator;
	if (Entry.bRandomYaw) Rot.Yaw = Rng.FRandRange(0.0f, 360.0f);
	if (Entry.bAlignToSurface)
	{
		const FQuat AlignQuat = FQuat::FindBetweenNormals(FVector::UpVector, Hit.ImpactNormal);
		Rot = (AlignQuat * Rot.Quaternion()).Rotator();
	}
	const float Scale = Rng.FRandRange(Entry.MinScale, Entry.MaxScale);

	// Mesh path: HISM instance.
	if (UStaticMesh* Mesh = Entry.Mesh.LoadSynchronous())
	{
		UHierarchicalInstancedStaticMeshComponent** Found = MeshInstances.Find(Mesh);
		UHierarchicalInstancedStaticMeshComponent* HISM = Found ? *Found : nullptr;
		if (!HISM)
		{
			HISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
			HISM->RegisterComponent();
			HISM->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
			HISM->SetStaticMesh(Mesh);
			HISM->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			MeshInstances.Add(Mesh, HISM);
		}
		const FTransform InstanceXform(Rot, Loc, FVector(Scale));
		HISM->AddInstance(InstanceXform, /*bWorldSpace*/ true);

		AlreadyPlaced.Add(Loc);
		return true;
	}

	// Actor path: SpawnActor.
	if (UClass* Cls = Entry.ActorClass.LoadSynchronous())
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		AActor* Spawned = W->SpawnActor<AActor>(Cls, Loc, Rot, SpawnParams);
		if (Spawned)
		{
			Spawned->SetActorScale3D(FVector(Scale));
			SpawnedActors.Add(Spawned);
			AlreadyPlaced.Add(Loc);
			return true;
		}
	}
	return false;
}
