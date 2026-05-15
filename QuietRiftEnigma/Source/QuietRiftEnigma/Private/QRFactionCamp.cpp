#include "QRFactionCamp.h"
#include "QRCampSimComponent.h"
#include "QRLeaderComponent.h"
#include "QRFactionComponent.h"
#include "QRNPCActor.h"
#include "QRRaidPartyAI.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"


AQRFactionCamp::AQRFactionCamp()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	AreaSphere = CreateDefaultSubobject<USphereComponent>(TEXT("AreaSphere"));
	SetRootComponent(AreaSphere);
	AreaSphere->InitSphereRadius(800.0f);
	AreaSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	AreaSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	AreaSphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	CampMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CampMesh"));
	CampMesh->SetupAttachment(AreaSphere);
	CampMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	Sim     = CreateDefaultSubobject<UQRCampSimComponent>(TEXT("Sim"));
	Leader  = CreateDefaultSubobject<UQRLeaderComponent>(TEXT("Leader"));
	Faction = CreateDefaultSubobject<UQRFactionComponent>(TEXT("Faction"));

	RaiderClass = AQRNPCActor::StaticClass();
	DisplayName = FText::FromString(TEXT("Enemy Camp"));
}


void AQRFactionCamp::BeginPlay()
{
	Super::BeginPlay();
	if (Sim)
	{
		Sim->OnRaidLaunched.AddDynamic(this, &AQRFactionCamp::HandleRaidLaunched);
	}
}


void AQRFactionCamp::HandleRaidLaunched(FQRRaidPlan Plan)
{
	if (!HasAuthority() || !RaiderClass) return;
	UWorld* W = GetWorld();
	if (!W) return;

	const FVector Origin = Plan.OriginLocation;

	// Spawn the party in a small ring around the camp so they don't
	// pile on top of each other. Each gets its own UQRRaidPartyAI.
	for (int32 i = 0; i < Plan.PartySize; ++i)
	{
		const float Angle = (i / static_cast<float>(FMath::Max(1, Plan.PartySize))) * 2.0f * PI;
		const FVector Offset(FMath::Cos(Angle) * 250.0f, FMath::Sin(Angle) * 250.0f, 0.0f);

		FActorSpawnParameters SP;
		SP.Owner = this;
		SP.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		AQRNPCActor* Raider = W->SpawnActor<AQRNPCActor>(RaiderClass,
			Origin + Offset, FRotator::ZeroRotator, SP);
		if (!Raider) continue;

		// Attach raid AI dynamically.
		UQRRaidPartyAI* AI = NewObject<UQRRaidPartyAI>(Raider);
		AI->RegisterComponent();
		AI->TargetLocation = Plan.TargetLocation;
		AI->CampOrigin     = Origin;
		AI->SourceCampId   = Plan.SourceCampId;
	}
}


void AQRFactionCamp::OnLeaderDied()
{
	if (!bLeaderAlive) return;
	bLeaderAlive = false;
	if (Sim)
	{
		// Drop fallback leadership so subsequent raids are reckless.
		Sim->FallbackLeadership = 0.0f;
		// Knock hostility down — leaderless camps fragment internally
		// before resuming aggression.
		Sim->ModifyHostility(-0.20f);
	}
	if (Leader)
	{
		Leader->LeadershipAptitude = 0.0f;
	}
	UE_LOG(LogTemp, Log, TEXT("[AQRFactionCamp %s] leader died — camp regresses"),
		*GetName());
}
