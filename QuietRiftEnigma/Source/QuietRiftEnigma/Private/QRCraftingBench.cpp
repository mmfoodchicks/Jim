#include "QRCraftingBench.h"
#include "QRCraftingComponent.h"
#include "QRInventoryComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"

AQRCraftingBench::AQRCraftingBench()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	InteractSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractSphere"));
	SetRootComponent(InteractSphere);
	InteractSphere->InitSphereRadius(120.0f);
	InteractSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractSphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	InteractSphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComp->SetupAttachment(InteractSphere);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	Crafting = CreateDefaultSubobject<UQRCraftingComponent>(TEXT("Crafting"));
	Storage  = CreateDefaultSubobject<UQRInventoryComponent>(TEXT("Storage"));

	DisplayName = FText::FromString(TEXT("Workbench"));
}
