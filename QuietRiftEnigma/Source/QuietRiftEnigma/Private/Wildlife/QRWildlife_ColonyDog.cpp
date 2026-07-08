#include "Wildlife/QRWildlife_ColonyDog.h"

AQRWildlife_ColonyDog::AQRWildlife_ColonyDog()
{
	SpeciesId          = FName("ANM_ColonyDog");
	SpeciesDisplayName = FText::FromString("German Shepherd");
	BehaviorRole       = EQRWildlifeBehaviorRole::Ambient;
	PreferredBiome     = EQRBiomeType::Grassland;
	MaxHealth          = 60.0f;
	MassKg             = 32.0f;
	MoveSpeedWalk      = 220.0f;
	MoveSpeedFlee      = 850.0f;   // shepherds sprint ~45 km/h
	MoveSpeedCharge    = 700.0f;
	ThreatDetectionRadius = 1800.0f;
	NoiseFactor        = 0.5f;

	// Real-world shepherd proportions (nose-to-tail ~1.1 m, ~0.62 m at
	// the withers). Never fights, so attack stats stay at prey floor.
	BodyLengthMeters   = 1.1f;
	BodyHeightMeters   = 0.62f;
	AttackDamage       = 0.0f;

	// The Fab pack ships mesh + loops on its own SKEL_Dogs_type1
	// skeleton, so the whole visual wires up in the constructor --
	// no Python stamping, no retarget, no AnimBP.
	DefaultBodyMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(
		TEXT("/Game/German_Shepherd_3D_Model/Models/SK_GermanShepherd_01.SK_GermanShepherd_01")));
	IdleAnim = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(
		TEXT("/Game/German_Shepherd_3D_Model/Animations/A_type1_Idle_Playing_v01.A_type1_Idle_Playing_v01")));
	WalkAnim = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(
		TEXT("/Game/German_Shepherd_3D_Model/Animations/A_type1_Walk_Loop_v01.A_type1_Walk_Loop_v01")));
	RunAnim = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(
		TEXT("/Game/German_Shepherd_3D_Model/Animations/A_type1_Run_Loop_v01.A_type1_Run_Loop_v01")));
	// Pack ships no death anim; the base holds the last pose instead.

	// The pack's loops are authored around walking gait speeds --
	// swap to run earlier than the default so the sprint reads right.
	RunAnimSpeedThreshold = 400.0f;

	// Companion animal: no drops. The colony remembers.
	DeathDrops.Reset();
	HarvestDrops.Reset();
}

void AQRWildlife_ColonyDog::OnThreatDetected_Implementation(AActor* Threat)
{
	// Dogs don't fight raiders (yet) -- they bolt for a moment and
	// circle back once the reaction cooldown clears.
	SetAIState(EQRWildlifeAIState::Fleeing);
}
