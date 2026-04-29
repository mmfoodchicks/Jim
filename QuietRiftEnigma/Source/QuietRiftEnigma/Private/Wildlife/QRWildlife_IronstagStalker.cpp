#include "Wildlife/QRWildlife_IronstagStalker.h"
#include "Kismet/GameplayStatics.h"

AQRWildlife_IronstagStalker::AQRWildlife_IronstagStalker()
{
	SpeciesId          = FName("PRD_IRONSTAG_STALKER");
	SpeciesDisplayName = FText::FromString("Ironstag Stalker");
	BehaviorRole       = EQRWildlifeBehaviorRole::Predator;
	MaxHealth          = 450.0f;
	MassKg             = 600.0f;
	MoveSpeedWalk      = 280.0f;
	MoveSpeedFlee      = 420.0f;
	ThreatDetectionRadius = 3000.0f;
	NoiseFactor        = 0.3f;

	DeathDrops.Add({ FName("FOD_IRONSTAG_MEAT"),    4, 7, 1.0f });
	DeathDrops.Add({ FName("MAT_FERRIC_ANTLER"),     2, 2, 1.0f });
	DeathDrops.Add({ FName("MAT_CHEST_PLATE_IRON"),  1, 1, 0.9f });
	DeathDrops.Add({ FName("MAT_IRONSTAG_HIDE"),     2, 3, 0.8f });
}

void AQRWildlife_IronstagStalker::TriggerAntlerSwipe()
{
	TArray<AActor*> HitActors;
	UGameplayStatics::GetAllActorsWithinSphere(
		GetWorld(), GetActorLocation() + GetActorForwardVector() * AntlerSwipeRadius * 0.5f,
		AntlerSwipeRadius, TArray<TSubclassOf<AActor>>{AActor::StaticClass()}, HitActors
	);
	for (AActor* Target : HitActors)
	{
		if (Target == this) continue;
		UGameplayStatics::ApplyDamage(Target, AntlerSwipeDamage, GetController(), this, nullptr);
	}
}

void AQRWildlife_IronstagStalker::TriggerStaticDischarge()
{
	float Damage = bMagneticStormActive ? StaticDischargeDamage * 2.0f : StaticDischargeDamage;
	TArray<AActor*> HitActors;
	UGameplayStatics::GetAllActorsWithinSphere(
		GetWorld(), GetActorLocation(), StaticDischargeRadius,
		TArray<TSubclassOf<AActor>>{AActor::StaticClass()}, HitActors
	);
	for (AActor* Target : HitActors)
	{
		if (Target == this) continue;
		UGameplayStatics::ApplyDamage(Target, Damage, GetController(), this, nullptr);
	}
}

void AQRWildlife_IronstagStalker::OnThreatDetected_Implementation(AActor* Threat)
{
	// Territorial: charges intruders within territory, does not retreat
	SetAIState(EQRWildlifeAIState::Attacking);
}
