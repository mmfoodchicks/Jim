#include "QRFactionCamp.h"
#include "QRCampSimComponent.h"
#include "QRLeaderComponent.h"
#include "QRFactionComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"


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

	DisplayName = FText::FromString(TEXT("Enemy Camp"));
}
