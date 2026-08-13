#include "QRRemnantSite.h"
#include "QRSurvivalComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Components/PointLightComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

AQRRemnantSite::AQRRemnantSite()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);

	ProximitySphere = CreateDefaultSubobject<USphereComponent>(TEXT("ProximitySphere"));
	SetRootComponent(ProximitySphere);
	ProximitySphere->InitSphereRadius(1200.0f);
	ProximitySphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ProximitySphere->SetCollisionResponseToAllChannels(ECR_Overlap);
	ProximitySphere->SetGenerateOverlapEvents(true);

	StructureMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StructureMesh"));
	StructureMesh->SetupAttachment(ProximitySphere);
	StructureMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	StructureMesh->SetCollisionProfileName(TEXT("BlockAll"));

	PulseLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("PulseLight"));
	PulseLight->SetupAttachment(StructureMesh);
	PulseLight->SetIntensity(400.0f);
	PulseLight->SetLightColor(FLinearColor(0.6f, 0.85f, 1.0f));
	PulseLight->SetAttenuationRadius(800.0f);

	// Default research yield curve — Dormant baseline 1.0×, peaks at
	// Active 2.0×, Hostile drops to 0.4× since approach is dangerous.
	ResearchYieldByState.Add(EQRRemnantWakeState::Dormant,   1.0f);
	ResearchYieldByState.Add(EQRRemnantWakeState::Stirring,  1.3f);
	ResearchYieldByState.Add(EQRRemnantWakeState::Active,    2.0f);
	ResearchYieldByState.Add(EQRRemnantWakeState::Hostile,   0.4f);
	ResearchYieldByState.Add(EQRRemnantWakeState::Subsiding, 1.5f);
}

void AQRRemnantSite::BeginPlay()
{
	Super::BeginPlay();
	if (ProximitySphere)
	{
		ProximitySphere->OnComponentBeginOverlap.AddDynamic(this, &AQRRemnantSite::HandleSphereBeginOverlap);
		ProximitySphere->OnComponentEndOverlap.AddDynamic(this, &AQRRemnantSite::HandleSphereEndOverlap);
	}

	// Kind-specific structure mesh — the library ships one per wake
	// archetype but nothing ever assigned ANY mesh (invisible remnants).
	// Note: Kind is stamped by the spawner right after SpawnActor, which
	// is before the first render frame, so reading it here is safe.
	if (StructureMesh && !StructureMesh->GetStaticMesh())
	{
		const TCHAR* KindName =
			Kind == EQRRemnantStructureType::PowerCore        ? TEXT("PowerCore") :
			Kind == EQRRemnantStructureType::DataArchive      ? TEXT("DataArchive") :
			Kind == EQRRemnantStructureType::ResonanceChamber ? TEXT("ResonanceChamber") :
			TEXT("SignalSpire");
		const FString Path = FString::Printf(
			TEXT("/Game/Meshes/Remnant/SM_REM_STR_%s.SM_REM_STR_%s"), KindName, KindName);
		if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *Path))
		{
			StructureMesh->SetStaticMesh(M);
			const float MaxExtent = M->GetBounds().BoxExtent.GetMax();
			if (MaxExtent > 1.0f)
			{
				StructureMesh->SetRelativeScale3D(FVector(600.0f / MaxExtent)); // ~12 m
			}
		}
	}

	ApplyVisualForState();
}

void AQRRemnantSite::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AQRRemnantSite, WakeState);
}

void AQRRemnantSite::SetWakeState(EQRRemnantWakeState NewState)
{
	if (!HasAuthority() || WakeState == NewState) return;
	WakeState = NewState;
	OnRep_WakeState();
	if (WakeState == EQRRemnantWakeState::Hostile)
	{
		HostileTimer = 0.0f;
	}
}

void AQRRemnantSite::OnRep_WakeState()
{
	OnWakeStateChanged.Broadcast(WakeState);
	ApplyVisualForState();
}

float AQRRemnantSite::CurrentResearchYieldScalar() const
{
	if (const float* Found = ResearchYieldByState.Find(WakeState)) return *Found;
	return 1.0f;
}

void AQRRemnantSite::ApplyVisualForState()
{
	if (!PulseLight) return;
	switch (WakeState)
	{
	case EQRRemnantWakeState::Dormant:
		PulseLight->SetIntensity(150.0f);
		PulseLight->SetLightColor(FLinearColor(0.40f, 0.55f, 0.70f));
		break;
	case EQRRemnantWakeState::Stirring:
		PulseLight->SetIntensity(400.0f);
		PulseLight->SetLightColor(FLinearColor(0.60f, 0.80f, 0.95f));
		break;
	case EQRRemnantWakeState::Active:
		PulseLight->SetIntensity(900.0f);
		PulseLight->SetLightColor(FLinearColor(0.85f, 0.95f, 1.00f));
		break;
	case EQRRemnantWakeState::Hostile:
		PulseLight->SetIntensity(1500.0f);
		PulseLight->SetLightColor(FLinearColor(1.00f, 0.30f, 0.20f));
		break;
	case EQRRemnantWakeState::Subsiding:
		PulseLight->SetIntensity(700.0f);
		PulseLight->SetLightColor(FLinearColor(0.95f, 0.60f, 0.40f));
		break;
	}
}

void AQRRemnantSite::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!HasAuthority()) return;

	// Hostile state ticks damage to overlapping actors and counts down
	// to auto-subside.
	if (WakeState == EQRRemnantWakeState::Hostile)
	{
		for (auto It = OverlapsInside.CreateIterator(); It; ++It)
		{
			if (AActor* A = It->Get())
			{
				if (UQRSurvivalComponent* Surv = A->FindComponentByClass<UQRSurvivalComponent>())
				{
					Surv->ApplyDamage(HostileDamagePerSecond * DeltaTime);
				}
			}
			else
			{
				It.RemoveCurrent();
			}
		}

		if (HostileAutoSubsideSeconds > 0.0f)
		{
			HostileTimer += DeltaTime;
			if (HostileTimer >= HostileAutoSubsideSeconds)
			{
				SetWakeState(EQRRemnantWakeState::Subsiding);
			}
		}
	}
}

void AQRRemnantSite::HandleSphereBeginOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (OtherActor && OtherActor != this)
	{
		OverlapsInside.Add(OtherActor);
	}
}

void AQRRemnantSite::HandleSphereEndOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32)
{
	if (OtherActor)
	{
		OverlapsInside.Remove(OtherActor);
	}
}
