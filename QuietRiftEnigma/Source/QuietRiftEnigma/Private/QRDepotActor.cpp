#include "QRDepotActor.h"
#include "QRInventoryComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

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
	// Visible default — depots had no mesh asset on any code path.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultCrate(
		TEXT("/Game/Meshes/Food/SM_FOD_TUBER_CRATE"));
	if (DefaultCrate.Succeeded())
	{
		DepotMesh->SetStaticMesh(DefaultCrate.Object);
		const float MaxExtent = DefaultCrate.Object->GetBounds().BoxExtent.GetMax();
		if (MaxExtent > 1.0f)
		{
			DepotMesh->SetRelativeScale3D(FVector(120.0f / MaxExtent)); // ~2.4 m crate
		}
	}

	// Depot inventory is generous by default — a stockpile, not a
	// player pocket. Designer can tune the grid + carry capacity.
	Storage = CreateDefaultSubobject<UQRInventoryComponent>(TEXT("Storage"));

	DisplayName = FText::FromString(TEXT("Storage Depot"));
}
