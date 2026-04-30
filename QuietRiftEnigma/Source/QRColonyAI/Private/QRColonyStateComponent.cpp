#include "QRColonyStateComponent.h"
#include "Net/UnrealNetwork.h"

UQRColonyStateComponent::UQRColonyStateComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 60.0f; // game-minute cadence
	SetIsReplicatedByDefault(true);
}

void UQRColonyStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UQRColonyStateComponent, SurvivorRecords);
	DOREPLIFETIME(UQRColonyStateComponent, ColonyMorale);
	DOREPLIFETIME(UQRColonyStateComponent, PopulationCount);
	DOREPLIFETIME(UQRColonyStateComponent, WorkerCount);
	DOREPLIFETIME(UQRColonyStateComponent, FoodSupplyDays);
	DOREPLIFETIME(UQRColonyStateComponent, WaterSupplyDays);
	DOREPLIFETIME(UQRColonyStateComponent, CampStyleTags);
	DOREPLIFETIME(UQRColonyStateComponent, ActiveEndingPath);
	DOREPLIFETIME(UQRColonyStateComponent, LeadershipInstabilityScore);
}

void UQRColonyStateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!GetOwner()->HasAuthority()) return;

	// Instability decays toward 0 at ~5 pts per game-minute (TickInterval = 60s)
	if (LeadershipInstabilityScore > 0.0f)
	{
		LeadershipInstabilityScore = FMath::Max(LeadershipInstabilityScore - 5.0f, 0.0f);
	}

	RecalculateColonyStats();
}

void UQRColonyStateComponent::RegisterSurvivor(FQRSurvivorRecord Record)
{
	for (const FQRSurvivorRecord& Existing : SurvivorRecords)
	{
		if (Existing.SurvivorId == Record.SurvivorId) return;
	}
	SurvivorRecords.Add(Record);
	RecalculateColonyStats();
}

void UQRColonyStateComponent::UpdateSurvivorRecord(FName SurvivorId, float NewHealth, float NewMorale, EQRNPCMoodState Mood)
{
	for (FQRSurvivorRecord& Record : SurvivorRecords)
	{
		if (Record.SurvivorId == SurvivorId)
		{
			Record.Health = NewHealth;
			Record.Morale = NewMorale;
			Record.Mood   = Mood;
			return;
		}
	}
}

void UQRColonyStateComponent::MarkSurvivorDead(FName SurvivorId)
{
	for (FQRSurvivorRecord& Record : SurvivorRecords)
	{
		if (Record.SurvivorId == SurvivorId)
		{
			Record.bIsAlive = false;
			OnSurvivorDied.Broadcast(SurvivorId);
			ApplyColonyMoraleEvent(-15.0f); // Death always hurts morale
			RecalculateColonyStats();
			return;
		}
	}
}

void UQRColonyStateComponent::RecalculateColonyStats()
{
	int32 Alive   = 0;
	int32 Workers = 0;
	float TotalMorale = 0.0f;

	for (const FQRSurvivorRecord& Record : SurvivorRecords)
	{
		if (!Record.bIsAlive) continue;
		++Alive;
		TotalMorale += Record.Morale;
		if (Record.CurrentRole != EQRNPCRole::Unassigned) ++Workers;
	}

	PopulationCount = Alive;
	WorkerCount     = Workers;

	if (Alive > 0)
	{
		float NewMorale = TotalMorale / Alive;
		// Instability drags aggregate morale down: at score 100, penalty is -10 morale
		NewMorale -= LeadershipInstabilityScore * 0.1f;
		NewMorale  = FMath::Clamp(NewMorale, 0.0f, 100.0f);
		if (!FMath::IsNearlyEqual(NewMorale, ColonyMorale, 0.5f))
		{
			ColonyMorale = NewMorale;
			OnColonyMoraleChanged.Broadcast(ColonyMorale);
		}
	}

	OnColonyReportReady.Broadcast();
}

void UQRColonyStateComponent::ApplyColonyMoraleEvent(float Delta)
{
	for (FQRSurvivorRecord& Record : SurvivorRecords)
	{
		if (!Record.bIsAlive) continue;
		Record.Morale = FMath::Clamp(Record.Morale + Delta, 0.0f, 100.0f);
	}
	RecalculateColonyStats();
}

void UQRColonyStateComponent::SetEndingPath(EQREndingPath Path)
{
	ActiveEndingPath = Path;
}

void UQRColonyStateComponent::ApplyLeadershipChurn(float Severity)
{
	const float SafeSeverity = FMath::Clamp(Severity, 0.0f, 50.0f);
	LeadershipInstabilityScore = FMath::Clamp(LeadershipInstabilityScore + SafeSeverity, 0.0f, 100.0f);

	// Immediate morale hit proportional to severity — survivors notice the leadership shake-up
	const float MoraleHit = SafeSeverity * 0.2f;
	if (MoraleHit > 0.0f)
		ApplyColonyMoraleEvent(-MoraleHit);
}

int32 UQRColonyStateComponent::CountSurvivorsWithRole(EQRNPCRole Role) const
{
	int32 Count = 0;
	for (const FQRSurvivorRecord& Record : SurvivorRecords)
	{
		if (Record.bIsAlive && Record.CurrentRole == Role) ++Count;
	}
	return Count;
}

bool UQRColonyStateComponent::HasLeaderOfType(EQRLeaderType Type) const
{
	for (const FQRSurvivorRecord& Record : SurvivorRecords)
	{
		if (Record.bIsAlive && Record.bIsLeader && Record.LeaderType == Type) return true;
	}
	return false;
}
