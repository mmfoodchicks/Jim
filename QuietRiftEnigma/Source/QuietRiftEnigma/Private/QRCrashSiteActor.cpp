#include "QRCrashSiteActor.h"
#include "QRWorldItem.h"
#include "QRItemDefinition.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Engine/HitResult.h"
#include "CollisionQueryParams.h"

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

	WorldItemClass = AQRWorldItem::StaticClass();
}

void AQRCrashSiteActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AQRCrashSiteActor, ArchetypeId);
}

void AQRCrashSiteActor::ClearScatteredLoot()
{
	for (TWeakObjectPtr<AActor>& W : ScatteredLoot)
	{
		if (AActor* A = W.Get()) A->Destroy();
	}
	ScatteredLoot.Reset();
}

void AQRCrashSiteActor::PopulateLoot(const FQRCrashLootTemplate& Template, int32 Seed)
{
	if (!HasAuthority()) return;
	UWorld* W = GetWorld();
	if (!W || !WorldItemClass) return;

	ClearScatteredLoot();
	FRandomStream Rng(Seed);
	const FVector Center = GetActorLocation();

	for (const FQRCrashLootEntry& Entry : Template.Entries)
	{
		if (Entry.ItemId.IsNone()) continue;
		if (Rng.FRand() > Entry.SpawnChance) continue;

		// Resolve item definition by id. Convention follows seeder
		// output: /Game/QuietRift/Data/Items/<Id>.<Id>
		const FString DefPath = FString::Printf(TEXT("/Game/QuietRift/Data/Items/%s.%s"),
			*Entry.ItemId.ToString(), *Entry.ItemId.ToString());
		UQRItemDefinition* Def = LoadObject<UQRItemDefinition>(nullptr, *DefPath);
		if (!Def) continue;

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
