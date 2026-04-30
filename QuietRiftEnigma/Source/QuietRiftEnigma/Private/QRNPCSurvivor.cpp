#include "QRNPCSurvivor.h"
#include "QRInventoryComponent.h"
#include "QRSurvivalComponent.h"
#include "QRNPCRoleComponent.h"
#include "QRLeaderComponent.h"
#include "QRColonyStateComponent.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "GameFramework/GameStateBase.h"
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
	DOREPLIFETIME(AQRNPCSurvivor, LeaderPromotionCount);
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

	if (LeaderComp)
	{
		LeaderComp->LeaderType = Type;
		// NativeLeaderType is set once (during creation or world-find) and never overwritten here
		LeaderComp->RecalculateLeaderDerivedStats();
	}

	// Determine churn severity to report to the colony.
	// World-found leaders joining fresh: no churn — the colony respects their credentials.
	// First-ever promotion of a local survivor: no churn (new leadership, no instability yet).
	// Each subsequent swap escalates: +15 per prior promotion, capped at 50.
	const bool bIsWorldFound = LeaderComp && LeaderComp->bIsWorldFoundLeader;
	float ChurnSeverity = 0.0f;
	if (!bIsWorldFound && LeaderPromotionCount > 0)
	{
		ChurnSeverity = FMath::Min(15.0f * LeaderPromotionCount, 50.0f);
	}

	if (ChurnSeverity > 0.0f)
	{
		if (AGameStateBase* GS = GetWorld()->GetGameState<AGameStateBase>())
		{
			if (UQRColonyStateComponent* Colony = GS->FindComponentByClass<UQRColonyStateComponent>())
				Colony->ApplyLeadershipChurn(ChurnSeverity);
		}
	}

	++LeaderPromotionCount;
}

void AQRNPCSurvivor::DemoteFromLeader()
{
	if (!bIsLeader) return;

	bIsLeader  = false;
	LeaderType = EQRLeaderType::None;

	// Demotion always adds mild instability — survivors notice when someone loses the title
	if (AGameStateBase* GS = GetWorld()->GetGameState<AGameStateBase>())
	{
		if (UQRColonyStateComponent* Colony = GS->FindComponentByClass<UQRColonyStateComponent>())
			Colony->ApplyLeadershipChurn(5.0f);
	}
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
