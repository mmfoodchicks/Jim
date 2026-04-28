#include "QRNPCRoleComponent.h"
#include "Net/UnrealNetwork.h"

UQRNPCRoleComponent::UQRNPCRoleComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UQRNPCRoleComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UQRNPCRoleComponent, RoleSkillLevels);
	DOREPLIFETIME(UQRNPCRoleComponent, CurrentRole);
	DOREPLIFETIME(UQRNPCRoleComponent, CurrentTaskId);
	DOREPLIFETIME(UQRNPCRoleComponent, bIsWorking);
	DOREPLIFETIME(UQRNPCRoleComponent, RaidState);
}

void UQRNPCRoleComponent::AssignRole(EQRNPCRole NewRole)
{
	if (CurrentRole == NewRole) return;

	EQRNPCRole Old = CurrentRole;
	CurrentRole    = NewRole;
	OnRoleChanged.Broadcast(Old, NewRole);
}

void UQRNPCRoleComponent::SetRaidState(EQRCivilianRaidState NewRaidState)
{
	RaidState = NewRaidState;
}

void UQRNPCRoleComponent::SelectBestTask(const FGameplayTagContainer& WorldConditions)
{
	// Score all tasks in fallback array and pick best matching one
	FName BestTaskId  = NAME_None;
	float BestScore   = -1.0f;

	for (const FQRNPCTask& Task : FallbackTaskArray)
	{
		if (!Task.RequiredConditionTags.IsEmpty() && !WorldConditions.HasAll(Task.RequiredConditionTags))
			continue;

		float Score = Task.Priority;
		if (Score > BestScore)
		{
			BestScore   = Score;
			BestTaskId  = Task.TaskId;
		}
	}

	if (!BestTaskId.IsNone() && BestTaskId != CurrentTaskId)
		SetCurrentTask(BestTaskId);
}

void UQRNPCRoleComponent::SetCurrentTask(FName TaskId)
{
	CurrentTaskId = TaskId;
	bIsWorking    = !TaskId.IsNone();
	OnTaskChanged.Broadcast(TaskId);
}

float UQRNPCRoleComponent::GetSkillLevel(EQRNPCRole Role) const
{
	const float* Found = RoleSkillLevels.Find(Role);
	return Found ? *Found : 0.0f;
}

void UQRNPCRoleComponent::GainSkillXP(EQRNPCRole Role, float XP)
{
	float& Skill = RoleSkillLevels.FindOrAdd(Role);
	Skill = FMath::Clamp(Skill + XP, 0.0f, 100.0f);
}

float UQRNPCRoleComponent::GetEfficiencyMultiplier() const
{
	float Skill = GetSkillLevel(CurrentRole);
	// Skill 0 = 0.5x, Skill 50 = 1.0x, Skill 100 = 2.0x
	return FMath::GetMappedRangeValueClamped(FVector2f(0.0f, 100.0f), FVector2f(0.5f, 2.0f), Skill);
}
