#include "QRMissionDirector.h"


UQRMissionDirector::UQRMissionDirector()
{
	PrimaryComponentTick.bCanEverTick = false;
}


FName UQRMissionDirector::RollNewMission()
{
	if (!MissionTemplateTable) return NAME_None;
	if (ActiveMissions.Num() >= MaxConcurrentMissions) return NAME_None;

	// Build a weighted candidate list, skipping any template already
	// active.
	TArray<TPair<FName, FQRMissionTemplateRow*>> Candidates;
	float TotalWeight = 0.0f;
	for (auto& Pair : MissionTemplateTable->GetRowMap())
	{
		const FName Id = Pair.Key;
		if (IsMissionActive(Id)) continue;
		FQRMissionTemplateRow* Row = reinterpret_cast<FQRMissionTemplateRow*>(Pair.Value);
		if (!Row || Row->RollWeight <= 0.0f) continue;
		Candidates.Add({Id, Row});
		TotalWeight += Row->RollWeight;
	}
	if (Candidates.Num() == 0 || TotalWeight <= 0.0f) return NAME_None;

	const float Pick = FMath::FRandRange(0.0f, TotalWeight);
	float Acc = 0.0f;
	FName ChosenId = NAME_None;
	FQRMissionTemplateRow* ChosenRow = nullptr;
	for (auto& C : Candidates)
	{
		Acc += C.Value->RollWeight;
		if (Pick <= Acc)
		{
			ChosenId  = C.Key;
			ChosenRow = C.Value;
			break;
		}
	}
	if (!ChosenRow)
	{
		ChosenId  = Candidates.Last().Key;
		ChosenRow = Candidates.Last().Value;
	}

	FQRActiveMission M;
	M.MissionId       = ChosenId;
	M.TargetId        = ChosenRow->TargetId;
	M.TargetQuantity  = FMath::Max(1, ChosenRow->TargetQuantity);
	M.CurrentProgress = 0;
	M.Family          = ChosenRow->Family;
	M.IssuedAt        = FDateTime::UtcNow();
	ActiveMissions.Add(M);

	OnMissionIssued.Broadcast(ChosenId);
	UE_LOG(LogTemp, Log, TEXT("[QRMissionDirector] issued '%s' (target %s × %d)"),
		*ChosenId.ToString(), *M.TargetId.ToString(), M.TargetQuantity);
	return ChosenId;
}


void UQRMissionDirector::ReportProgress(FName MissionId, int32 Delta)
{
	for (FQRActiveMission& M : ActiveMissions)
	{
		if (M.MissionId == MissionId)
		{
			M.CurrentProgress = FMath::Clamp(M.CurrentProgress + Delta, 0, M.TargetQuantity);
			OnMissionProgress.Broadcast(MissionId, M.CurrentProgress);
			if (M.CurrentProgress >= M.TargetQuantity)
			{
				CompleteMission(MissionId);
			}
			return;
		}
	}
}


void UQRMissionDirector::CompleteMission(FName MissionId)
{
	const int32 Idx = ActiveMissions.IndexOfByPredicate(
		[&](const FQRActiveMission& M) { return M.MissionId == MissionId; });
	if (Idx == INDEX_NONE) return;
	ActiveMissions.RemoveAt(Idx);
	OnMissionCompleted.Broadcast(MissionId);
}


void UQRMissionDirector::AbandonMission(FName MissionId)
{
	ActiveMissions.RemoveAll([&](const FQRActiveMission& M) {
		return M.MissionId == MissionId;
	});
}


bool UQRMissionDirector::IsMissionActive(FName MissionId) const
{
	return ActiveMissions.ContainsByPredicate(
		[&](const FQRActiveMission& M) { return M.MissionId == MissionId; });
}
