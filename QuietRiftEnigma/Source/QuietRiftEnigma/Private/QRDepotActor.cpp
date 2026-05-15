#include "QRDepotActor.h"
#include "QRInventoryComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"

AQRDepotActor::AQRDepotActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	InteractSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractSphere"));
	SetRootComponent(InteractSphere);
	InteractSphere->InitSphereRadius(150.0f);
	InteractSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractSphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	DepotMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DepotMesh"));
	DepotMesh->SetupAttachment(InteractSphere);
	DepotMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	// Depot inventory is generous by default — a stockpile, not a
	// player pocket. Designer can tune the grid + carry capacity.
	Storage = CreateDefaultSubobject<UQRInventoryComponent>(TEXT("Storage"));

	DisplayName = FText::FromString(TEXT("Storage Depot"));
}
