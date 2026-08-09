#include "QRMissionDirector.h"
#include "QRCharacter.h"
#include "QRInventoryComponent.h"
#include "QRItemInstance.h"
#include "QRItemDefinition.h"
#include "QRCodexSubsystem.h"
#include "QRColonyStateComponent.h"
#include "QRLeaderComponent.h"
#include "QRDepotActor.h"
#include "QRDepotComponent.h"
#include "QRFactionCamp.h"
#include "QRWorldItem.h"
#include "QRSaveSnapshotLibrary.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"


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

	InstantiateMission(ChosenId, *ChosenRow);
	return ChosenId;
}


bool UQRMissionDirector::StartMissionById(FName MissionId)
{
	if (!MissionTemplateTable || MissionId.IsNone()) return false;
	if (IsMissionActive(MissionId)) return false;
	if (ActiveMissions.Num() >= MaxConcurrentMissions) return false;

	const FQRMissionTemplateRow* Row =
		MissionTemplateTable->FindRow<FQRMissionTemplateRow>(MissionId, TEXT("QRMissionStart"), false);
	if (!Row) return false;

	InstantiateMission(MissionId, *Row);
	return true;
}


FName UQRMissionDirector::IssueDirectiveMission(EQRLeaderType LeaderType, FName AffectedStat)
{
	if (!MissionTemplateTable) return NAME_None;
	if (ActiveMissions.Num() >= MaxConcurrentMissions) return NAME_None;

	// Pick a template family that matches the leader's domain. The
	// priority list's mission templates have descriptive ids
	// (PT_COLLECT_RESEARCH, PT_DEFEND, ...); the director picks the
	// best family for the leader type and the FQRMissionTemplateRow's
	// Family enum, falling back to a generic FetchItem.
	EQRMissionFamily PreferredFamily = EQRMissionFamily::FetchItem;
	switch (LeaderType)
	{
	case EQRLeaderType::Research:    PreferredFamily = EQRMissionFamily::ResearchItem; break;
	case EQRLeaderType::Military:    PreferredFamily = EQRMissionFamily::KillTarget;   break;
	case EQRLeaderType::Security:    PreferredFamily = EQRMissionFamily::KillTarget;   break;
	case EQRLeaderType::Survival:    PreferredFamily = EQRMissionFamily::ScoutPOI;     break;
	case EQRLeaderType::Logistics:
	case EQRLeaderType::Agriculture:
	case EQRLeaderType::Engineering: PreferredFamily = EQRMissionFamily::FetchItem;    break;
	default: break;
	}

	// First pass: look for a non-active template with the matching family.
	// Second pass: any non-active template (so the directive system
	// always tries to spawn something).
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		for (auto& Pair : MissionTemplateTable->GetRowMap())
		{
			const FName Id = Pair.Key;
			if (IsMissionActive(Id)) continue;
			FQRMissionTemplateRow* Row = reinterpret_cast<FQRMissionTemplateRow*>(Pair.Value);
			if (!Row) continue;
			if (Pass == 0 && Row->Family != PreferredFamily) continue;

			// Stamp the blocker's affected stat onto the template's
			// TargetId so the mission tracks the actual scarce thing
			// (food id, predator species id, ...) rather than whatever
			// the template defaulted to.
			FQRMissionTemplateRow Patched = *Row;
			if (!AffectedStat.IsNone())
			{
				Patched.TargetId = AffectedStat;
			}
			InstantiateMission(Id, Patched);
			UE_LOG(LogTemp, Log,
				TEXT("[QRMissionDirector] directive mission '%s' issued (leader=%d, stat=%s)"),
				*Id.ToString(), static_cast<int32>(LeaderType), *AffectedStat.ToString());
			return Id;
		}
	}
	return NAME_None;
}


void UQRMissionDirector::InstantiateMission(FName Id, const FQRMissionTemplateRow& Row)
{
	FQRActiveMission M;
	M.MissionId        = Id;
	M.TargetId         = Row.TargetId;
	M.TargetQuantity   = FMath::Max(1, Row.TargetQuantity);
	M.CurrentProgress  = 0;
	M.Family           = Row.Family;
	M.IssuedAt         = FDateTime::UtcNow();
	M.ScoutRadiusMeters = FMath::Max(5.0f, Row.ScoutRadiusMeters);

	// Location-bearing families resolve a world position now, through the
	// MissionLocationFallbackRule cascade — a named POI that didn't spawn
	// in this world degrades gracefully instead of producing an
	// uncompletable mission.
	if (Row.Family == EQRMissionFamily::ScoutPOI ||
		Row.Family == EQRMissionFamily::EscortNPC)
	{
		FVector Loc;
		if (ResolveMissionLocation(Row, Row.TargetId, Loc))
		{
			M.TargetLocation     = Loc;
			M.bHasTargetLocation = true;
		}
	}

	ActiveMissions.Add(M);
	OnMissionIssued.Broadcast(Id);
	UE_LOG(LogTemp, Log, TEXT("[QRMissionDirector] issued '%s' (target %s × %d%s)"),
		*Id.ToString(), *M.TargetId.ToString(), M.TargetQuantity,
		M.bHasTargetLocation ? *FString::Printf(TEXT(" @ %s"), *M.TargetLocation.ToCompactString()) : TEXT(""));
}


bool UQRMissionDirector::ResolveMissionLocation(const FQRMissionTemplateRow& Row,
	FName TargetId, FVector& OutLocation) const
{
	UWorld* W = GetWorld();
	if (!W) return false;

	// Anchor for ring fallbacks: the player if hooked, else the owner
	// (GameMode has no transform — owner fallback handles other hosts).
	FVector Anchor = FVector::ZeroVector;
	if (const AQRCharacter* P = HookedPlayer.Get())
	{
		Anchor = P->GetActorLocation();
	}
	else if (const AActor* O = GetOwner())
	{
		Anchor = O->GetActorLocation();
	}

	auto RingPoint = [&Anchor](float MinM, float MaxM) -> FVector
	{
		const float Ang  = FMath::FRandRange(0.0f, 2.0f * PI);
		const float Dist = FMath::FRandRange(MinM, MaxM) * 100.0f;   // m → cm
		return Anchor + FVector(FMath::Cos(Ang) * Dist, FMath::Sin(Ang) * Dist, 0.0f);
	};

	// Cascade: each rule tries, then falls through to the next-loosest.
	EQRMissionLocationRule Rule = Row.LocationRule;

	if (Rule == EQRMissionLocationRule::ExactPOI)
	{
		// A POI "spawned" if some actor carries the target id as an actor
		// tag (worldgen stamps these on placed POIs).
		for (TActorIterator<AActor> It(W); It; ++It)
		{
			if (It->ActorHasTag(TargetId))
			{
				OutLocation = It->GetActorLocation();
				return true;
			}
		}
		UE_LOG(LogTemp, Log, TEXT("[QRMissionDirector] ExactPOI '%s' not in world — falling back"),
			*TargetId.ToString());
		Rule = EQRMissionLocationRule::FactionOwned;
	}

	if (Rule == EQRMissionLocationRule::FactionOwned)
	{
		float BestDistSq = TNumericLimits<float>::Max();
		bool bFound = false;
		for (TActorIterator<AQRFactionCamp> It(W); It; ++It)
		{
			const float D = FVector::DistSquared(Anchor, It->GetActorLocation());
			if (D < BestDistSq)
			{
				BestDistSq = D;
				OutLocation = It->GetActorLocation();
				bFound = true;
			}
		}
		if (bFound) return true;
		Rule = EQRMissionLocationRule::RegionHint;
	}

	if (Rule == EQRMissionLocationRule::RegionHint ||
		Rule == EQRMissionLocationRule::BiomeHint)
	{
		// v1: biome lookup isn't queryable by point yet, so BiomeHint
		// shares the region ring. 300–800 m out — a real trek, not a
		// doorstep objective.
		OutLocation = RingPoint(300.0f, 800.0f);
		return true;
	}

	// GenerateMinorPOI — terminal rule, always produces a point.
	OutLocation = RingPoint(500.0f, 1200.0f);
	UE_LOG(LogTemp, Log, TEXT("[QRMissionDirector] generated minor POI for '%s'"),
		*TargetId.ToString());
	return true;
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

	// Copy out before removal — GrantRewards needs the instance and may
	// broadcast events that re-enter the mission list.
	const FQRActiveMission Completed = ActiveMissions[Idx];
	ActiveMissions.RemoveAt(Idx);

	GrantRewards(Completed);
	OnMissionCompleted.Broadcast(MissionId);
}


void UQRMissionDirector::GrantRewards(const FQRActiveMission& Mission)
{
	if (!MissionTemplateTable) return;
	const FQRMissionTemplateRow* Row =
		MissionTemplateTable->FindRow<FQRMissionTemplateRow>(Mission.MissionId, TEXT("QRMissionReward"), false);
	if (!Row) return;

	UWorld* W = GetWorld();
	AQRCharacter* Player = HookedPlayer.Get();

	// ── XP — flows to the leadership layer, not a player level bar.
	if (Row->RewardXP > 0 && W)
	{
		if (AGameStateBase* GS = W->GetGameState())
		{
			if (UQRLeaderComponent* Leader = GS->FindComponentByClass<UQRLeaderComponent>())
			{
				Leader->GainLeaderXP(static_cast<float>(Row->RewardXP));
			}
		}
	}

	// ── Items — the No-Pocket-OP law. Each source has its own physics.
	switch (Row->RewardSource)
	{
	case EQRRewardSource::Knowledge:
	{
		// Knowledge rewards advance the codex, never the pocket.
		if (Row->RewardItems.Num() > 0)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[QRMissionDirector] '%s' authored items on a Knowledge reward — stripped (No-Pocket-OP)"),
				*Mission.MissionId.ToString());
		}
		if (W)
		{
			if (UQRCodexSubsystem* Codex = W->GetSubsystem<UQRCodexSubsystem>())
			{
				Codex->Record(Mission.TargetId, TEXT("Item"),
					FText::FromName(Mission.TargetId), EQRCodexDiscoveryState::Known);
			}
		}
		break;
	}
	case EQRRewardSource::Morale:
	{
		if (Row->RewardItems.Num() > 0)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[QRMissionDirector] '%s' authored items on a Morale reward — stripped (No-Pocket-OP)"),
				*Mission.MissionId.ToString());
		}
		if (W)
		{
			if (AGameStateBase* GS = W->GetGameState())
			{
				if (UQRColonyStateComponent* Colony = GS->FindComponentByClass<UQRColonyStateComponent>())
				{
					Colony->ColonyMorale = FMath::Clamp(Colony->ColonyMorale + 5.0f, 0.0f, 100.0f);
				}
			}
		}
		break;
	}
	case EQRRewardSource::Infrastructure:
	{
		// The reward is the unblocked structure/system itself; nothing
		// goes into a pocket here.
		if (Row->RewardItems.Num() > 0)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[QRMissionDirector] '%s' authored items on an Infrastructure reward — stripped (No-Pocket-OP)"),
				*Mission.MissionId.ToString());
		}
		break;
	}
	case EQRRewardSource::NPCPersonal:
	{
		// A person hands over a few things from their own pack — small
		// quantities only. The cap is the validation.
		constexpr int32 PersonalCap = 3;
		if (Player && Player->Inventory)
		{
			for (const TPair<FName, int32>& R : Row->RewardItems)
			{
				const int32 Qty = FMath::Min(R.Value, PersonalCap);
				if (R.Value > PersonalCap)
				{
					UE_LOG(LogTemp, Warning,
						TEXT("[QRMissionDirector] '%s' NPCPersonal reward %s x%d capped to %d (No-Pocket-OP)"),
						*Mission.MissionId.ToString(), *R.Key.ToString(), R.Value, PersonalCap);
				}
				if (const UQRItemDefinition* Def = FQRSaveSnapshot::ResolveItemDefinition(R.Key))
				{
					int32 Remainder = 0;
					Player->Inventory->TryAddByDefinition(Def, Qty, Remainder);
				}
			}
		}
		break;
	}
	case EQRRewardSource::FactionStockpile:
	{
		// Items must come OUT of a real depot. Empty stockpile = no
		// reward — scarcity is the game.
		if (Player && Player->Inventory)
		{
			for (const TPair<FName, int32>& R : Row->RewardItems)
			{
				const int32 Granted = WithdrawFromStockpile(R.Key, R.Value);
				if (Granted < R.Value)
				{
					UE_LOG(LogTemp, Log,
						TEXT("[QRMissionDirector] stockpile only had %d/%d of %s for '%s'"),
						Granted, R.Value, *R.Key.ToString(), *Mission.MissionId.ToString());
				}
				if (Granted > 0)
				{
					if (const UQRItemDefinition* Def = FQRSaveSnapshot::ResolveItemDefinition(R.Key))
					{
						int32 Remainder = 0;
						Player->Inventory->TryAddByDefinition(Def, Granted, Remainder);
					}
				}
			}
		}
		break;
	}
	case EQRRewardSource::SiteContainer:
	{
		// Recovered from the site — spawns as world pickups at the
		// mission location, never directly into the pocket.
		SpawnSiteCache(Mission, Row->RewardItems);
		break;
	}
	default:
		break;
	}
}


int32 UQRMissionDirector::WithdrawFromStockpile(FName ItemId, int32 Quantity)
{
	UWorld* W = GetWorld();
	if (!W || Quantity <= 0) return 0;

	// AQRDepotActor::Storage is a plain UQRInventoryComponent, so the
	// withdrawal is count-then-remove (same pattern the hauler uses).
	int32 Taken = 0;
	for (TActorIterator<AQRDepotActor> It(W); It && Taken < Quantity; ++It)
	{
		if (!It->Storage) continue;
		const int32 Avail = It->Storage->CountItem(ItemId);
		const int32 Take  = FMath::Min(Avail, Quantity - Taken);
		if (Take > 0 && It->Storage->TryRemoveItem(ItemId, Take))
		{
			Taken += Take;
		}
	}
	return Taken;
}


void UQRMissionDirector::SpawnSiteCache(const FQRActiveMission& Mission,
	const TMap<FName, int32>& Items)
{
	UWorld* W = GetWorld();
	if (!W || Items.Num() == 0) return;

	// Site missions resolved a location at issue time; anything else
	// drops the cache near the player (recovered and carried back).
	FVector Base = Mission.bHasTargetLocation ? Mission.TargetLocation : FVector::ZeroVector;
	if (!Mission.bHasTargetLocation)
	{
		if (const AQRCharacter* P = HookedPlayer.Get())
		{
			Base = P->GetActorLocation() + P->GetActorForwardVector() * 150.0f;
		}
	}

	int32 Offset = 0;
	for (const TPair<FName, int32>& R : Items)
	{
		const UQRItemDefinition* Def = FQRSaveSnapshot::ResolveItemDefinition(R.Key);
		if (!Def)
		{
			UE_LOG(LogTemp, Warning, TEXT("[QRMissionDirector] site cache item '%s' has no definition"),
				*R.Key.ToString());
			continue;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		const FVector Loc = Base + FVector(60.0f * Offset, 0.0f, 30.0f);
		if (AQRWorldItem* Drop = W->SpawnActor<AQRWorldItem>(
			AQRWorldItem::StaticClass(), Loc, FRotator::ZeroRotator, Params))
		{
			Drop->InitializeFrom(Def, FMath::Max(1, R.Value));
		}
		++Offset;
	}
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

		// ScoutPOI proximity check — 1 Hz is plenty for "did the player
		// reach the marker", and far cheaper than per-tick distance math.
		W->GetTimerManager().SetTimer(ScoutTimerHandle, this,
			&UQRMissionDirector::TickScoutCheck, 1.0f, /*bLoop*/ true);

		// Listen for leader-issued quests. Every leader component in the
		// world routes its FSM transition through OnQuestIssued; the
		// director materializes the mission.
		for (TActorIterator<AActor> It(W); It; ++It)
		{
			if (UQRLeaderComponent* Leader = It->FindComponentByClass<UQRLeaderComponent>())
			{
				Leader->OnQuestIssued.AddDynamic(this, &UQRMissionDirector::HandleLeaderQuestIssued);
			}
		}
	}
}


void UQRMissionDirector::HandleLeaderQuestIssued(EQRLeaderType LeaderType, FName AffectedStat)
{
	IssueDirectiveMission(LeaderType, AffectedStat);
}


void UQRMissionDirector::TickScoutCheck()
{
	const AQRCharacter* P = HookedPlayer.Get();
	if (!P) return;
	const FVector PlayerLoc = P->GetActorLocation();

	// Collect ids first — ReportProgress can complete + remove a mission,
	// which would invalidate iteration over ActiveMissions.
	TArray<FName> Reached;
	for (const FQRActiveMission& M : ActiveMissions)
	{
		if (M.Family != EQRMissionFamily::ScoutPOI || !M.bHasTargetLocation) continue;
		const float RadiusCm = M.ScoutRadiusMeters * 100.0f;
		if (FVector::DistSquared2D(PlayerLoc, M.TargetLocation) <= RadiusCm * RadiusCm)
		{
			Reached.Add(M.MissionId);
		}
	}
	for (const FName& Id : Reached)
	{
		ReportProgress(Id, 1);
	}
}


void UQRMissionDirector::EndPlay(const EEndPlayReason::Type Reason)
{
	UnhookPlayerEvents();
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(ScoutTimerHandle);
		if (UQRCodexSubsystem* Codex = W->GetSubsystem<UQRCodexSubsystem>())
		{
			Codex->OnEntryUpdated.RemoveDynamic(this, &UQRMissionDirector::HandleCodexUpdated);
		}
		for (TActorIterator<AActor> It(W); It; ++It)
		{
			if (UQRLeaderComponent* Leader = It->FindComponentByClass<UQRLeaderComponent>())
			{
				Leader->OnQuestIssued.RemoveDynamic(this, &UQRMissionDirector::HandleLeaderQuestIssued);
			}
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

	// Collect ids FIRST — ReportProgress→CompleteMission RemoveAt()s
	// from ActiveMissions, and GrantRewards can re-enter this handler
	// via the inventory OnItemAdded broadcast. Iterating live while
	// that happens trips the ranged-for checker on the final fetch.
	TArray<FName> Matching;
	for (const FQRActiveMission& M : ActiveMissions)
	{
		if (M.Family == EQRMissionFamily::FetchItem && M.TargetId == ItemId)
		{
			Matching.Add(M.MissionId);
		}
	}
	for (const FName& Id : Matching)
	{
		ReportProgress(Id, Qty);
	}
}


void UQRMissionDirector::HandleCodexUpdated(FName EntryId, EQRCodexDiscoveryState NewState)
{
	if (NewState != EQRCodexDiscoveryState::Known) return;
	// Collect-then-report — see HandleItemAdded.
	TArray<FName> Matching;
	for (const FQRActiveMission& M : ActiveMissions)
	{
		if (M.Family == EQRMissionFamily::ResearchItem && M.TargetId == EntryId)
		{
			Matching.Add(M.MissionId);
		}
	}
	for (const FName& Id : Matching)
	{
		ReportProgress(Id, 1);
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
			// Collect-then-report — see HandleItemAdded.
			TArray<FName> Matching;
			for (const FQRActiveMission& M : MD->ActiveMissions)
			{
				if (M.Family == EQRMissionFamily::KillTarget && M.TargetId == SpeciesId)
				{
					Matching.Add(M.MissionId);
				}
			}
			for (const FName& Id : Matching)
			{
				MD->ReportProgress(Id, Delta);
			}
		}
	}
}
