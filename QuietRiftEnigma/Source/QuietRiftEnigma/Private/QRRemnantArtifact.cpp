#include "QRRemnantArtifact.h"
#include "Net/UnrealNetwork.h"

AQRRemnantArtifact::AQRRemnantArtifact()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

void AQRRemnantArtifact::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AQRRemnantArtifact, bCollected);
}

float AQRRemnantArtifact::Collect(AActor* Collector)
{
	if (bCollected || !HasAuthority()) return 0.0f;

	bCollected = true;

	OnCollected.Broadcast(this, Collector);
	OnArtifactCollected(Collector);

	return ResearchPointsOnCollect;
}
