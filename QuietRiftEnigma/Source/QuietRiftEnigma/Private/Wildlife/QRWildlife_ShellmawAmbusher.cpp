#include "Wildlife/QRWildlife_ShellmawAmbusher.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"

AQRWildlife_ShellmawAmbusher::AQRWildlife_ShellmawAmbusher()
{
	SpeciesId          = FName("PRD_SHELLMAW_AMBUSHER");
	SpeciesDisplayName = FText::FromString("Shellmaw Ambusher");
	BehaviorRole       = EQRWildlifeBehaviorRole::Predator;
	MaxHealth          = 380.0f;
	MassKg             = 700.0f;
	MoveSpeedWalk      = 120.0f;
	MoveSpeedFlee      = 200.0f;
	ThreatDetectionRadius = BuriedDetectionRadius;
	NoiseFactor        = 0.05f;

	// Real-world size + attack tuning (burrowing armoured ambusher)
	BodyLengthMeters   = 3.6f;
	BodyHeightMeters   = 1.7f;
	AttackDamage       = 38.0f;
	bArmoredHead       = true;   // armoured shell -- weak spot is the underside

	DeathDrops.Add({ FName("FOD_SHELLMAW_MEAT"),   3, 5, 1.0f });
	DeathDrops.Add({ FName("MAT_SHELLMAW_PLATE"),   2, 3, 0.9f });
	DeathDrops.Add({ FName("MAT_MINERAL_CRUST"),    2, 4, 0.7f });
}

void AQRWildlife_ShellmawAmbusher::Emerge()
{
	if (!bIsBuried) return;
	bIsBuried = false;
	ThreatDetectionRadius = SurfacedDetectionRadius;
}

void AQRWildlife_ShellmawAmbusher::Rebury()
{
	if (bIsBuried) return;
	bIsBuried = true;
	ThreatDetectionRadius = BuriedDetectionRadius;
}

void AQRWildlife_ShellmawAmbusher::TriggerMawSnap(AActor* Target)
{
	if (!Target) return;
	UGameplayStatics::ApplyDamage(Target, MawSnapDamage, GetController(), this, nullptr);
	// Grab/bleed loop handled by BT task or Blueprint event
}

void AQRWildlife_ShellmawAmbusher::OnThreatDetected_Implementation(AActor* Threat)
{
	// Herd alerts can report threats far outside snap reach — a buried
	// ambusher stays hidden for those instead of surfacing and landing
	// an any-range maw snap.
	const bool bInSnapRange = Threat &&
		FVector::Dist(Threat->GetActorLocation(), GetActorLocation()) <= BuriedDetectionRadius * 2.0f;
	if (bIsBuried)
	{
		if (!bInSnapRange) return;
		Emerge();
		TriggerMawSnap(Threat);
	}
	SetAIState(EQRWildlifeAIState::Attacking);
}

void AQRWildlife_ShellmawAmbusher::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AQRWildlife_ShellmawAmbusher, bIsBuried);
}
