#include "QRNPCSurvivor.h"
#include "QRInventoryComponent.h"
#include "QRSurvivalComponent.h"
#include "QRNPCRoleComponent.h"
#include "QRLeaderComponent.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "Net/UnrealNetwork.h"

AQRNPCSurvivor::AQRNPCSurvivor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	Inventory   = CreateDefaultSubobject<UQRInventoryComponent>(TEXT("Inventory"));
	Survival    = CreateDefaultSubobject<UQRSurvivalComponent>(TEXT("Survival"));
	RoleComp    = CreateDefaultSubobject<UQRNPCRoleComponent>(TEXT("RoleComp"));
	LeaderComp  = CreateDefaultSubobject<UQRLeaderComponent>(TEXT("LeaderComp"));

	// Reduce inventory capacity for NPCs vs player
	Inventory->MaxCarryWeightKg = 20.0f;
	Inventory->MaxSlots         = 10;
}

void AQRNPCSurvivor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AQRNPCSurvivor, SurvivorId);
	DOREPLIFETIME(AQRNPCSurvivor, DisplayName);
	DOREPLIFETIME(AQRNPCSurvivor, bIsLeader);
	DOREPLIFETIME(AQRNPCSurvivor, LeaderType);
	DOREPLIFETIME(AQRNPCSurvivor, IndividualMorale);
	DOREPLIFETIME(AQRNPCSurvivor, Mood);
	DOREPLIFETIME(AQRNPCSurvivor, bHasNightPanic);
}

void AQRNPCSurvivor::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		// Bind death
		if (Survival)
			Survival->OnDeath.AddDynamic(this, &AQRNPCSurvivor::OnSurvivalDeath);

		// Start behavior tree
		if (AAIController* AIC = Cast<AAIController>(GetController()))
		{
			if (SurvivorBehaviorTree)
				AIC->RunBehaviorTree(SurvivorBehaviorTree);
		}
	}
}

void AQRNPCSurvivor::PromoteToLeader(EQRLeaderType Type)
{
	bIsLeader  = true;
	LeaderType = Type;
	if (LeaderComp) LeaderComp->LeaderType = Type;
}

void AQRNPCSurvivor::DemoteFromLeader()
{
	bIsLeader  = false;
	LeaderType = EQRLeaderType::None;
}

void AQRNPCSurvivor::SetMorale(float NewMorale)
{
	IndividualMorale = FMath::Clamp(NewMorale, 0.0f, 100.0f);

	if      (IndividualMorale >= 70.0f)  Mood = EQRNPCMoodState::Motivated;
	else if (IndividualMorale >= 50.0f)  Mood = EQRNPCMoodState::Stable;
	else if (IndividualMorale >= 30.0f)  Mood = EQRNPCMoodState::Anxious;
	else if (IndividualMorale >= 15.0f)  Mood = EQRNPCMoodState::Panicked;
	else
	{
		Mood = EQRNPCMoodState::Resigned;
		OnMoraleCollapsed();
	}
}

void AQRNPCSurvivor::ApplyMoraleEvent(float Delta, FText EventDescription)
{
	SetMorale(IndividualMorale + Delta);
}

void AQRNPCSurvivor::SetRaidState(EQRCivilianRaidState NewState)
{
	if (RoleComp) RoleComp->SetRaidState(NewState);
}

bool AQRNPCSurvivor::IsAlive() const
{
	return Survival && !Survival->bIsDead;
}

void AQRNPCSurvivor::OnSurvivalDeath()
{
	// Notify colony state — handled via GameMode in Blueprint
}
