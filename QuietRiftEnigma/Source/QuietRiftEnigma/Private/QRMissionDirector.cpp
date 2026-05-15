#include "QRMissionDirector.h"
#include "QRCharacter.h"
#include "QRInventoryComponent.h"
#include "QRItemInstance.h"
#include "QRItemDefinition.h"
#include "QRCodexSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"


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


void UQRMissionDirector::BeginPlay()
{
	Super::BeginPlay();

	// Bind to the player's components + the codex subsystem. Director
	// lives on AQRGameMode so player spawns after — defer to the
	// world's first PlayerController arriving.
	if (UWorld* W = GetWorld())
	{
		if (APlayerController* PC = W->GetFirstPlayerController())
		{
			if (AQRCharacter* Char = Cast<AQRCharacter>(PC->GetPawn()))
			{
				HookPlayerEvents(Char);
			}
		}

		// Codex events come from the world subsystem.
		if (UQRCodexSubsystem* Codex = W->GetSubsystem<UQRCodexSubsystem>())
		{
			Codex->OnEntryUpdated.AddDynamic(this, &UQRMissionDirector::HandleCodexUpdated);
		}
	}
}


void UQRMissionDirector::EndPlay(const EEndPlayReason::Type Reason)
{
	UnhookPlayerEvents();
	if (UWorld* W = GetWorld())
	{
		if (UQRCodexSubsystem* Codex = W->GetSubsystem<UQRCodexSubsystem>())
		{
			Codex->OnEntryUpdated.RemoveDynamic(this, &UQRMissionDirector::HandleCodexUpdated);
		}
	}
	Super::EndPlay(Reason);
}


void UQRMissionDirector::HookPlayerEvents(AQRCharacter* Player)
{
	UnhookPlayerEvents();
	if (!Player || !Player->Inventory) return;
	Player->Inventory->OnItemAdded.AddDynamic(this, &UQRMissionDirector::HandleItemAdded);
	HookedPlayer = Player;
}


void UQRMissionDirector::UnhookPlayerEvents()
{
	if (AQRCharacter* P = HookedPlayer.Get())
	{
		if (P->Inventory)
		{
			P->Inventory->OnItemAdded.RemoveDynamic(this, &UQRMissionDirector::HandleItemAdded);
		}
	}
	HookedPlayer = nullptr;
}


void UQRMissionDirector::HandleItemAdded(UQRItemInstance* Item, int32 SlotIndex)
{
	if (!Item || !Item->Definition) return;
	const FName ItemId = Item->Definition->ItemId;
	const int32 Qty    = Item->Quantity;

	// Find FetchItem missions matching this target.
	for (FQRActiveMission& M : ActiveMissions)
	{
		if (M.Family == EQRMissionFamily::FetchItem && M.TargetId == ItemId)
		{
			ReportProgress(M.MissionId, Qty);
		}
	}
}


void UQRMissionDirector::HandleCodexUpdated(FName EntryId, EQRCodexDiscoveryState NewState)
{
	if (NewState != EQRCodexDiscoveryState::Researched) return;
	for (FQRActiveMission& M : ActiveMissions)
	{
		if (M.Family == EQRMissionFamily::ResearchItem && M.TargetId == EntryId)
		{
			ReportProgress(M.MissionId, 1);
		}
	}
}


void UQRMissionDirector::ReportSpeciesKilled(UWorld* World, FName SpeciesId, int32 Delta)
{
	if (!World) return;
	// Find every active mission director in the world (typically one,
	// owned by the GameMode).
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (UQRMissionDirector* MD = It->FindComponentByClass<UQRMissionDirector>())
		{
			for (FQRActiveMission& M : MD->ActiveMissions)
			{
				if (M.Family == EQRMissionFamily::KillTarget && M.TargetId == SpeciesId)
				{
					MD->ReportProgress(M.MissionId, Delta);
				}
			}
		}
	}
}
