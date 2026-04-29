#include "QRLeaderComponent.h"
#include "Net/UnrealNetwork.h"
#include "QRMath.h"

UQRLeaderComponent::UQRLeaderComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 60.0f; // tick once per in-game minute
	SetIsReplicatedByDefault(true);

	// Pre-allocate 8-axis policy vector (one slot per EQRMoralCompassAxis)
	CampPolicyVector.Init(0.0f, 8);
}

void UQRLeaderComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UQRLeaderComponent, LeadershipAptitude);
	DOREPLIFETIME(UQRLeaderComponent, SkillAptitude);
	DOREPLIFETIME(UQRLeaderComponent, Composure);
	DOREPLIFETIME(UQRLeaderComponent, LeaderBuff);
	DOREPLIFETIME(UQRLeaderComponent, LeaderLevel);
	DOREPLIFETIME(UQRLeaderComponent, bIsInexperiencedLeader);
	DOREPLIFETIME(UQRLeaderComponent, MoraleIndex);
	DOREPLIFETIME(UQRLeaderComponent, MoraleResilience);
	DOREPLIFETIME(UQRLeaderComponent, MoraleGradient);
	DOREPLIFETIME(UQRLeaderComponent, LeaderXP);
	DOREPLIFETIME(UQRLeaderComponent, DefectionRisk);
	DOREPLIFETIME(UQRLeaderComponent, MoralCompassVector);
	DOREPLIFETIME(UQRLeaderComponent, IssueState);
	DOREPLIFETIME(UQRLeaderComponent, IssueEscalationScore);
	DOREPLIFETIME(UQRLeaderComponent, BlockerDurationHours);
	DOREPLIFETIME(UQRLeaderComponent, CampAlignmentScore);
	DOREPLIFETIME(UQRLeaderComponent, ActiveDirectives);
	DOREPLIFETIME(UQRLeaderComponent, ActiveConditions);
}

void UQRLeaderComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!GetOwner()->HasAuthority()) return;

	TickDirectiveEscalation(DeltaTime);
	TickConditionRecovery(DeltaTime);
	RecalculateDefectionRisk();

	// Passive morale decay toward 50
	float Target = 50.0f;
	float Prev   = MoraleIndex;
	MoraleIndex  = FMath::FInterpTo(MoraleIndex, Target, DeltaTime, 0.001f);
	MoraleGradient = MoraleIndex - Prev;
}

void UQRLeaderComponent::TickDirectiveEscalation(float DeltaTime)
{
	const float GameHoursPerTick = DeltaTime / 3600.0f;

	for (FQRLeaderDirective& Dir : ActiveDirectives)
	{
		if (Dir.bIsResolved) continue;

		Dir.EscalationTimeHours -= GameHoursPerTick;
		if (Dir.EscalationTimeHours <= 0.0f)
		{
			// Escalate — apply morale penalty
			ApplyMoraleChange(-Dir.Severity * 10.0f);
			Dir.EscalationTimeHours = 24.0f; // re-escalate after another day
		}
	}
}

void UQRLeaderComponent::TickConditionRecovery(float DeltaTime)
{
	const float GameHoursPerTick = DeltaTime / 3600.0f;

	for (int32 i = ActiveConditions.Num() - 1; i >= 0; --i)
	{
		FQRLeaderCondition& Cond = ActiveConditions[i];
		if (Cond.RemainingHours > 0.0f)
		{
			Cond.RemainingHours -= GameHoursPerTick;
			if (Cond.RemainingHours <= 0.0f)
				ActiveConditions.RemoveAt(i);
		}
	}
}

void UQRLeaderComponent::RecalculateDefectionRisk()
{
	// Risk rises sharply when MI < 20, falls when MI > 60
	float RiskFromMorale = 0.0f;
	if (MoraleIndex < 20.0f)
		RiskFromMorale = (20.0f - MoraleIndex) / 20.0f;

	// Unresolved high-severity directives add risk
	float DirRisk = 0.0f;
	for (const FQRLeaderDirective& Dir : ActiveDirectives)
	{
		if (!Dir.bIsResolved && Dir.Severity > 0.7f)
			DirRisk = FMath::Min(DirRisk + 0.1f, 0.5f);
	}

	DefectionRisk = FMath::Clamp(RiskFromMorale + DirRisk, 0.0f, 1.0f);
}

void UQRLeaderComponent::AddDirective(FQRLeaderDirective Directive)
{
	// Don't duplicate
	for (const FQRLeaderDirective& Existing : ActiveDirectives)
	{
		if (Existing.DirectiveId == Directive.DirectiveId) return;
	}
	ActiveDirectives.Add(Directive);
	OnDirectiveAdded.Broadcast(Directive);
}

void UQRLeaderComponent::ResolveDirective(FName DirectiveId, float MoralBias)
{
	for (FQRLeaderDirective& Dir : ActiveDirectives)
	{
		if (Dir.DirectiveId == DirectiveId && !Dir.bIsResolved)
		{
			Dir.bIsResolved = true;
			ApplyMoraleChange(Dir.Severity * 15.0f); // reward MI for resolution
			MoralCompassVector = FMath::Clamp(MoralCompassVector + MoralBias, -1.0f, 1.0f);
			OnDirectiveResolved.Broadcast(DirectiveId);
			return;
		}
	}
}

void UQRLeaderComponent::AddCondition(FQRLeaderCondition Condition)
{
	for (const FQRLeaderCondition& Existing : ActiveConditions)
	{
		if (Existing.ConditionId == Condition.ConditionId) return;
	}
	ActiveConditions.Add(Condition);
	OnConditionAdded.Broadcast(Condition);
}

void UQRLeaderComponent::HealCondition(FName ConditionId)
{
	for (int32 i = 0; i < ActiveConditions.Num(); ++i)
	{
		if (ActiveConditions[i].ConditionId == ConditionId)
		{
			ActiveConditions.RemoveAt(i);
			return;
		}
	}
}

void UQRLeaderComponent::ApplyMoraleChange(float Delta, bool bIsEvent)
{
	MoraleIndex = FMath::Clamp(MoraleIndex + Delta, 0.0f, 100.0f);
}

void UQRLeaderComponent::GainLeaderXP(float XP)
{
	LeaderXP += XP;
}

float UQRLeaderComponent::GetTotalDebuff(FName StatName) const
{
	float Total = 0.0f;
	for (const FQRLeaderCondition& Cond : ActiveConditions)
	{
		if (Cond.AffectedStat == StatName)
			Total += Cond.DebuffAmount;
	}
	return Total;
}

void UQRLeaderComponent::RecalculateLeaderDerivedStats()
{
	LeaderBuff  = UQRMath::LeaderBuffScalar(LeadershipAptitude, SkillAptitude);
	LeaderLevel = UQRMath::ComputeLeaderLevel(LeadershipAptitude, SkillAptitude);
	bIsInexperiencedLeader = (LeaderLevel == 1 && LeaderXP < 10.0f);
}

void UQRLeaderComponent::AdvanceIssueEscalation(float BlockerSeverity, float DeltaGameHours)
{
	BlockerDurationHours += DeltaGameHours;

	IssueEscalationScore = UQRMath::IssueEscalationScore(
		BlockerSeverity, BlockerDurationHours, GuidanceDelayHours, LeaderAwarenessMult);

	if (IssueState == EQRLeaderIssueState::None)
		IssueState = EQRLeaderIssueState::Reported;
	else if (IssueState == EQRLeaderIssueState::Reported && IssueEscalationScore > 10.0f)
		IssueState = EQRLeaderIssueState::Escalating;
	else if (IssueState == EQRLeaderIssueState::Escalating && IssueEscalationScore >= 100.0f)
		IssueState = EQRLeaderIssueState::QuestIssued;
}

void UQRLeaderComponent::ResolveCurrentIssue()
{
	IssueState           = EQRLeaderIssueState::Resolved;
	IssueEscalationScore = 0.0f;
	BlockerDurationHours = 0.0f;
	// Let morale recover on next directive resolution call
}

void UQRLeaderComponent::UpdateCampAlignment(const TArray<float>& CampPreferenceVector)
{
	if (CampPreferenceVector.Num() != CampPolicyVector.Num()) return;

	float Dot = 0.0f;
	float MagPolicy = 0.0f, MagCamp = 0.0f;
	for (int32 i = 0; i < CampPolicyVector.Num(); ++i)
	{
		Dot       += CampPolicyVector[i] * CampPreferenceVector[i];
		MagPolicy += CampPolicyVector[i] * CampPolicyVector[i];
		MagCamp   += CampPreferenceVector[i] * CampPreferenceVector[i];
	}

	const float Denom = FMath::Sqrt(MagPolicy) * FMath::Sqrt(MagCamp);
	CampAlignmentScore = (Denom > SMALL_NUMBER) ? FMath::Clamp(Dot / Denom, -1.0f, 1.0f) : 0.0f;
}
