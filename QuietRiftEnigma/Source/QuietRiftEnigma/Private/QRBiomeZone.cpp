#include "QRBiomeZone.h"
#include "QRBiomeProfile.h"
#include "QRCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"

AQRBiomeZone::AQRBiomeZone()
{
	PrimaryActorTick.bCanEverTick = false;

	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	SetRootComponent(Bounds);
	Bounds->InitBoxExtent(FVector(2000.f, 2000.f, 1000.f));
	Bounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Bounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	Bounds->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Bounds->SetGenerateOverlapEvents(true);
	Bounds->SetHiddenInGame(true);
}

void AQRBiomeZone::BeginPlay()
{
	Super::BeginPlay();
	if (Bounds)
	{
		Bounds->OnComponentBeginOverlap.AddDynamic(this, &AQRBiomeZone::HandleBeginOverlap);
		Bounds->OnComponentEndOverlap.AddDynamic(this, &AQRBiomeZone::HandleEndOverlap);
	}
}

void AQRBiomeZone::HandleBeginOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	AQRCharacter* Char = Cast<AQRCharacter>(OtherActor);
	if (!Char || !BiomeProfile) return;
	// Forward to the character — it owns the ambient audio component
	// and decides priority across multiple active zones.
	Char->OnBiomeZoneEnter(BiomeProfile, Priority);
}

void AQRBiomeZone::HandleEndOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32)
{
	AQRCharacter* Char = Cast<AQRCharacter>(OtherActor);
	if (!Char || !BiomeProfile) return;
	Char->OnBiomeZoneExit(BiomeProfile, Priority);
}
