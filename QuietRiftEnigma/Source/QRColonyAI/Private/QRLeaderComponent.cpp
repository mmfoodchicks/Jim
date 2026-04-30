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
	DOREPLIFETIME(UQRLeaderComponent, CrossCraftPenaltyMult);
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
	// If the same directive already exists, update its severity and reset its escalation timer
	// rather than silently ignoring it (stale severity would persist otherwise)
	for (FQRLeaderDirective& Existing : ActiveDirectives)
	{
		if (Existing.DirectiveId == Directive.DirectiveId)
		{
			Existing.Severity            = Directive.Severity;
			Existing.EscalationTimeHours = Directive.EscalationTimeHours;
			Existing.bIsResolved         = false;
			return;
		}
	}
	ActiveDirectives.Add(Directive);
	OnDirectiveAdded.Broadcast(Directive);
}

void UQRLeaderComponent::ResolveDirective(FName DirectiveId, float MoralBias)
{
	// Clamp bias so a malicious or accidental extreme value can't spike MoralCompassVector
	const float ClampedBias = FMath::Clamp(MoralBias, -1.0f, 1.0f);
	for (FQRLeaderDirective& Dir : ActiveDirectives)
	{
		if (Dir.DirectiveId == DirectiveId && !Dir.bIsResolved)
		{
			Dir.bIsResolved = true;
			ApplyMoraleChange(Dir.Severity * 15.0f);
			MoralCompassVector = FMath::Clamp(MoralCompassVector + ClampedBias, -1.0f, 1.0f);
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
	// Reject negative or extreme values — no per-tick cap, but guards against runaway calls
	if (XP <= 0.0f || !FMath::IsFinite(XP)) return;
	LeaderXP = FMath::Clamp(LeaderXP + XP, 0.0f, 100000.0f);
}

float UQRLeaderComponent::GetTotalDebuff(FName StatName) const
{
	float Total = 0.0f;
	for (const FQRLeaderCondition& Cond : ActiveConditions)
	{
		if (Cond.AffectedStat == StatName)
			Total += Cond.DebuffAmount;
	}
	// Cross-craft leaders impose harsher conditions on their team: penalty 0.6 → 1.4x debuffs
	return Total * (2.0f - CrossCraftPenaltyMult);
}

void UQRLeaderComponent::RecalculateLeaderDerivedStats()
{
	// Determine cross-craft penalty before applying to buff
	const bool bHasMismatch = (NativeLeaderType != EQRLeaderType::None)
	                        && (LeaderType       != EQRLeaderType::None)
	                        && (LeaderType       != NativeLeaderType);

	if (!bHasMismatch)
	{
		CrossCraftPenaltyMult = 1.0f;
	}
	else
	{
		// World-found leaders adapt better — their experience covers the gap partially
		CrossCraftPenaltyMult = bIsWorldFoundLeader ? 0.8f : 0.6f;
	}

	const float BaseLeaderBuff = UQRMath::LeaderBuffScalar(LeadershipAptitude, SkillAptitude);
	// Cross-craft leaders can drop below 1.0, meaning they actively hurt team output
	LeaderBuff  = FMath::Clamp(BaseLeaderBuff * CrossCraftPenaltyMult, 0.5f, 1.35f);
	LeaderLevel = UQRMath::ComputeLeaderLevel(LeadershipAptitude, SkillAptitude);
	bIsInexperiencedLeader = (LeaderLevel == 1 && LeaderXP < 10.0f);
}

EQRLeaderType UQRLeaderComponent::GetNaturalLeaderTypeForRole(EQRNPCRole Role)
{
	switch (Role)
	{
	case EQRNPCRole::Guard:       return EQRLeaderType::Security;
	case EQRNPCRole::Engineer:    return EQRLeaderType::Engineering;
	case EQRNPCRole::Builder:     return EQRLeaderType::Engineering;
	case EQRNPCRole::Medic:       return EQRLeaderType::Medical;
	case EQRNPCRole::Gatherer:    return EQRLeaderType::Logistics;
	case EQRNPCRole::Hauler:      return EQRLeaderType::Logistics;
	case EQRNPCRole::Researcher:  return EQRLeaderType::Research;
	case EQRNPCRole::Farmer:      return EQRLeaderType::Agriculture;
	case EQRNPCRole::Cook:        return EQRLeaderType::Morale;
	case EQRNPCRole::Scout:       return EQRLeaderType::Survival;
	case EQRNPCRole::Miner:       return EQRLeaderType::Survival;
	default:                      return EQRLeaderType::None;
	}
}

// Maximum hours a QuestIssued state can persist without resolution before forcing a fallback
static constexpr float QuestIssuedTimeoutHours = 168.0f; // one in-game week

void UQRLeaderComponent::AdvanceIssueEscalation(float BlockerSeverity, float DeltaGameHours)
{
	// Guard against negative or NaN inputs that would corrupt the FSM
	if (DeltaGameHours <= 0.0f || !FMath::IsFinite(DeltaGameHours)) return;
	const float SafeSeverity = FMath::Clamp(BlockerSeverity, 0.0f, 10.0f);

	BlockerDurationHours += DeltaGameHours;

	IssueEscalationScore = UQRMath::IssueEscalationScore(
		SafeSeverity, BlockerDurationHours, GuidanceDelayHours, LeaderAwarenessMult);

	switch (IssueState)
	{
	case EQRLeaderIssueState::None:
	case EQRLeaderIssueState::Resolved:
		IssueState = EQRLeaderIssueState::Reported;
		break;
	case EQRLeaderIssueState::Reported:
		if (IssueEscalationScore > 10.0f)
			IssueState = EQRLeaderIssueState::Escalating;
		break;
	case EQRLeaderIssueState::Escalating:
		if (IssueEscalationScore >= 100.0f)
			IssueState = EQRLeaderIssueState::QuestIssued;
		break;
	case EQRLeaderIssueState::QuestIssued:
		// Fallback: if quest generation system never resolved this within the timeout,
		// force-resolve so the FSM doesn't get stuck permanently
		if (BlockerDurationHours >= QuestIssuedTimeoutHours)
		{
			ResolveCurrentIssue();
			// Apply a morale penalty to represent the unaddressed issue
			ApplyMoraleChange(-SafeSeverity * 5.0f);
		}
		break;
	}
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
