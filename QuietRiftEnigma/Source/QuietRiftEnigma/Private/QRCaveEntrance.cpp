#include "QRCaveEntrance.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"

AQRCaveEntrance::AQRCaveEntrance()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	InteractSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractSphere"));
	SetRootComponent(InteractSphere);
	InteractSphere->InitSphereRadius(200.0f);
	InteractSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractSphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	EntranceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EntranceMesh"));
	EntranceMesh->SetupAttachment(InteractSphere);
	EntranceMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	// Dim interior glow makes a cave mouth read at distance even
	// without authored decoration.
	InteriorGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("InteriorGlow"));
	InteriorGlow->SetupAttachment(EntranceMesh);
	InteriorGlow->SetIntensity(120.0f);
	InteriorGlow->SetLightColor(FLinearColor(0.35f, 0.55f, 0.85f));
	InteriorGlow->SetAttenuationRadius(500.0f);
	InteriorGlow->SetRelativeLocation(FVector(0, 0, -50.0f));

	DisplayName = FText::FromString(TEXT("Cave Mouth"));
}
