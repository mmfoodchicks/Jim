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
	// Pre-allocate 8-axis innate compass vector matching EQRMoralCompassAxis
	InnateCompassVector.Init(0.0f, 8);
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
	DOREPLIFETIME(AQRNPCSurvivor, CampAlignmentScore);
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
		// Auto-derive native affinity on first promotion if not already set by the spawner.
		// World-found leaders have NativeLeaderType set explicitly before PromoteToLeader is called.
		if (LeaderComp->NativeLeaderType == EQRLeaderType::None && RoleComp)
		{
			LeaderComp->NativeLeaderType = UQRLeaderComponent::GetNaturalLeaderTypeForRole(RoleComp->PrimaryRole);
		}
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

EQRCivilianRaidState AQRNPCSurvivor::DetermineRaidResponse() const
{
	// High morale survivors fight back, mid morale hide, low morale flee.
	// Guards are still routed through their normal combat orders by the role component;
	// this only governs civilian (non-guard) reaction.
	if (IndividualMorale >= 65.0f) return EQRCivilianRaidState::Defending;
	if (IndividualMorale >= 30.0f) return EQRCivilianRaidState::Hiding;
	return EQRCivilianRaidState::Fleeing;
}

void AQRNPCSurvivor::RespondToRaid()
{
	SetRaidState(DetermineRaidResponse());
}

bool AQRNPCSurvivor::IsAlive() const
{
	return Survival && !Survival->bIsDead;
}

float AQRNPCSurvivor::UpdateCampAlignment(const TArray<float>& CampPolicyVector)
{
	if (InnateCompassVector.Num() != CampPolicyVector.Num() || InnateCompassVector.Num() == 0)
	{
		CampAlignmentScore = 0.0f;
		return CampAlignmentScore;
	}

	float Dot = 0.0f, MagInnate = 0.0f, MagCamp = 0.0f;
	for (int32 i = 0; i < InnateCompassVector.Num(); ++i)
	{
		Dot       += InnateCompassVector[i] * CampPolicyVector[i];
		MagInnate += InnateCompassVector[i] * InnateCompassVector[i];
		MagCamp   += CampPolicyVector[i]    * CampPolicyVector[i];
	}

	const float Denom = FMath::Sqrt(MagInnate) * FMath::Sqrt(MagCamp);
	CampAlignmentScore = (Denom > SMALL_NUMBER)
		? FMath::Clamp(Dot / Denom, -1.0f, 1.0f)
		: 0.0f;

	return CampAlignmentScore;
}

void AQRNPCSurvivor::OnSurvivalDeath()
{
	// Notify colony state — handled via GameMode in Blueprint
}
