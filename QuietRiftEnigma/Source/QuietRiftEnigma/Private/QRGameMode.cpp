#include "QRGameMode.h"
#include "QRGameState.h"
#include "QRCheatManager.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Blueprint/UserWidget.h"
#include "QRColonyStateComponent.h"
#include "QRResearchComponent.h"
#include "QRWeatherComponent.h"
#include "QRSaveGameSystem.h"
#include "QRVanguardColony.h"
#include "QRCharacter.h"
#include "QRDeathScreenWidget.h"
#include "QRSurvivalComponent.h"
#include "QRInventoryComponent.h"
#include "QRMissionDirector.h"
#include "QRFactionCamp.h"
#include "QRCampSimComponent.h"
#include "QRSkyManager.h"
#include "QRWeatherFXManager.h"
#include "QRWorldGenSubsystem.h"
#include "QRWorldGenSeedActor.h"
#include "QRWorldGenSpawner.h"
#include "QRItemInstance.h"
#include "QRItemDefinition.h"
#include "QRSaveTypes.h"
#include "QRSaveSnapshotLibrary.h"
#include "QRBuildPieceTag.h"
#include "QRBuildModeComponent.h"
#include "QRLootedRegistry.h"
#include "QRCodexSubsystem.h"
#include "QRMountHusbandryComponent.h"
#include "QRFarmPlotActor.h"
#include "QRNPCActor.h"
#include "QRNPCColonist.h"
#include "QRNPCBrainComponent.h"
#include "QRRaidPartyAI.h"
#include "QRHotbarComponent.h"
#include "QRWildlifeBase.h"
#include "UObject/UObjectHash.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

AQRGameMode::AQRGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 1.0f;

	// Prefer the editor-side Blueprint child (BP_QRCharacter) when it's
	// present — that's where the SkeletalMesh + AnimBP get wired by
	// qr_create_player_blueprint.py. Fall back to the bare C++ class
	// (invisible body) when the BP hasn't been created yet.
	static ConstructorHelpers::FClassFinder<APawn> BPCharacter(
		TEXT("/Game/QuietRift/Characters/BP_QRCharacter"));
	if (BPCharacter.Class)
	{
		DefaultPawnClass = BPCharacter.Class;
	}
	else
	{
		DefaultPawnClass = AQRCharacter::StaticClass();
	}

	// Carries ColonyState / Research / Weather — without it those three
	// components never existed anywhere and every FindComponentByClass
	// lookup (here, crafting, camp sim) returned null forever.
	GameStateClass = AQRGameState::StaticClass();

	SaveSystem      = CreateDefaultSubobject<UQRSaveGameSystem>(TEXT("SaveSystem"));
	MissionDirector = CreateDefaultSubobject<UQRMissionDirector>(TEXT("MissionDirector"));

	// Mission template table — NOTHING assigned this before, so every
	// director entry point early-returned and no procedural mission
	// could ever exist. The table is imported by qr_import_datatables.py
	// from DT_MissionTemplates.csv; absence just leaves missions off.
	static ConstructorHelpers::FObjectFinder<UDataTable> MissionTable(
		TEXT("/Game/QuietRift/Data/DT_MissionTemplates"));
	if (MissionTable.Succeeded() && MissionDirector)
	{
		MissionDirector->MissionTemplateTable = MissionTable.Object;
	}

	// Default death-screen widget class — C++ placeholder, swap via BP.
	DeathScreenClass = UQRDeathScreenWidget::StaticClass();

	// Atmosphere defaults.
	SkyManagerClass       = AQRSkyManager::StaticClass();
	WeatherFXManagerClass = AQRWeatherFXManager::StaticClass();
}

void AQRGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Colony state, research, and weather components live on the GameState
	// actor (AQRGameState carries all three by default). If a BP override
	// swapped in a different GameState class, create whatever is missing
	// so these systems can never silently go inert again.
	if (AGameStateBase* GS = GetGameState<AGameStateBase>())
	{
		ColonyState = GS->FindComponentByClass<UQRColonyStateComponent>();
		Research    = GS->FindComponentByClass<UQRResearchComponent>();
		Weather     = GS->FindComponentByClass<UQRWeatherComponent>();

		if (!ColonyState)
		{
			ColonyState = NewObject<UQRColonyStateComponent>(GS, TEXT("ColonyState_RT"));
			ColonyState->RegisterComponent();
		}
		if (!Research)
		{
			Research = NewObject<UQRResearchComponent>(GS, TEXT("Research_RT"));
			Research->RegisterComponent();
		}
		if (!Weather)
		{
			Weather = NewObject<UQRWeatherComponent>(GS, TEXT("Weather_RT"));
			Weather->RegisterComponent();
		}
	}

	// The Concordat is placed once by the level designer; locate it by class.
	VanguardConcordat = Cast<AQRVanguardColony>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AQRVanguardColony::StaticClass()));

	// Activate tutorial mission
	ActivateMission(FName("MQ_000"));

	// Auto-spawn atmosphere managers if the level didn't pre-place them.
	// Designer-placed instances win (TActorIterator finds them first).
	if (bAutoSpawnAtmosphere && GetWorld())
	{
		if (SkyManagerClass && !SkyManager)
		{
			for (TActorIterator<AQRSkyManager> It(GetWorld()); It; ++It) { SkyManager = *It; break; }
			if (!SkyManager)
			{
				SkyManager = GetWorld()->SpawnActor<AQRSkyManager>(SkyManagerClass,
					FVector::ZeroVector, FRotator::ZeroRotator);
			}
		}
		if (WeatherFXManagerClass && !WeatherFXManager)
		{
			for (TActorIterator<AQRWeatherFXManager> It(GetWorld()); It; ++It) { WeatherFXManager = *It; break; }
			if (!WeatherFXManager)
			{
				WeatherFXManager = GetWorld()->SpawnActor<AQRWeatherFXManager>(WeatherFXManagerClass,
					FVector::ZeroVector, FRotator::ZeroRotator);
			}
		}
	}

	// Auto-bootstrap the procedural world. Fresh boot only — a save
	// defers to HandleLoadComplete so generation can use the SAVED seed.
	if (bAutoBootstrapWorld && GetWorld())
	{
		const bool bResumingSave = SaveSystem && SaveSystem->DoesSaveExist(AutosaveSlotName);
		if (!bResumingSave)
		{
			EnsureWorldBootstrapped(BootstrapWorldSeed);
			PlacePlayerAtSurfaceStart();
			SpawnStarterVillageAtStart();
			SpawnStarterFaunaBurst();
		}
	}

	// Auto-load on session start. Async — applies to player pawn from
	// ApplyLoadedDataToPlayer once the character spawns + asks for its
	// share of the snapshot.
	if (SaveSystem && SaveSystem->DoesSaveExist(AutosaveSlotName))
	{
		if (!bLoadDelegateBound)
		{
			SaveSystem->OnLoadComplete.AddUObject(this, &AQRGameMode::HandleLoadComplete);
			bLoadDelegateBound = true;
		}
		bLoadInFlight = true;
		SaveSystem->LoadGame(AutosaveSlotName);
	}

	// Roll procedural missions on a cadence — RollNewMission had ZERO
	// callers, so even with a template table nothing ever started.
	// The director self-caps at MaxConcurrentMissions.
	GetWorldTimerManager().SetTimer(MissionRollTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (MissionDirector) MissionDirector->RollNewMission();
		}), 120.0f, /*bLoop*/ true, /*FirstDelay*/ 30.0f);

	// Periodic background autosave. Disabled if AutosaveIntervalSeconds
	// is 0 — manual + lifecycle saves still work.
	if (AutosaveIntervalSeconds > 0.0f)
	{
		GetWorldTimerManager().SetTimer(
			AutosaveTimerHandle,
			this, &AQRGameMode::HandleAutosaveTick,
			AutosaveIntervalSeconds,
			/*bLoop*/ true,
			/*FirstDelay*/ AutosaveIntervalSeconds);
	}
}

void AQRGameMode::HandleAutosaveTick()
{
	UE_LOG(LogTemp, Log, TEXT("[QR] Periodic autosave triggered"));
	QuickSave();
}

void AQRGameMode::Logout(AController* Exiting)
{
	// In listen-server / dedicated co-op, save the world snapshot when
	// any player exits so their progress isn't lost if the host quits
	// next. v1 keeps a single shared slot; per-PC slots come later.
	if (Exiting && Exiting->IsPlayerController())
	{
		QuickSave();
	}
	Super::Logout(Exiting);
}

void AQRGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
	// Cancel the autosave loop so the destroyed game mode doesn't keep
	// pulling on a freed timer manager.
	GetWorldTimerManager().ClearTimer(AutosaveTimerHandle);
	GetWorldTimerManager().ClearTimer(MissionRollTimerHandle);

	// Autosave on graceful shutdown. Quit / level-travel / PIE-stop
	// all route through EndPlay, so this gives us a single hook that
	// covers every "session is ending" path.
	if (Reason != EEndPlayReason::Destroyed)
	{
		QuickSave();
	}
	Super::EndPlay(Reason);
}

void AQRGameMode::HandleLoadComplete(bool bSuccess, const FQRGameSaveData& Data)
{
	bLoadInFlight = false;
	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("[QR] Save load failed for slot '%s'"), *AutosaveSlotName);

		// The BeginPlay bootstrap was skipped because a save EXISTED —
		// if that save turns out corrupt/unreadable, degrade to a fresh
		// New Game instead of an empty floor.
		if (bAutoBootstrapWorld && GetWorld())
		{
			EnsureWorldBootstrapped(BootstrapWorldSeed);
			PlacePlayerAtSurfaceStart();
		}
		return;
	}
	// Legacy-save cleanse: saves written before the seed-capture fix
	// carry WorldSeed==0 and predate the Surface-ring start — resuming
	// them strands the player at the origin (the DEEPEST zone) in a
	// world we can't reconstruct. Treat them as New Game instead of
	// poisoning every session with stale state.
	if (Data.WorldSeed == 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[QR] Save predates world-seed capture — starting fresh instead of resuming it"));
		if (bAutoBootstrapWorld && GetWorld())
		{
			EnsureWorldBootstrapped(BootstrapWorldSeed);
			PlacePlayerAtSurfaceStart();
			SpawnStarterVillageAtStart();
			SpawnStarterFaunaBurst();
		}
		return;
	}

	PendingLoadedData     = Data;
	bHasPendingLoadedData = true;

	// Resuming skipped the BeginPlay world bootstrap on purpose — we
	// needed the SAVED seed first. Regenerate/populate the deterministic
	// world now (granular: a level-saved seed actor no longer blocks the
	// POI/fauna population pass).
	if (bAutoBootstrapWorld && GetWorld())
	{
		EnsureWorldBootstrapped(Data.WorldSeed);
	}

	// Restore world-level state that doesn't need a player pawn.
	WorldTimeSeconds      = Data.WorldTimeSeconds;
	DayNumber             = FMath::Max(1, Data.DayNumber);
	CompletedMissionIds   = Data.CompletedMissionIds;
	ActiveMissionIds      = Data.ActiveMissionIds;
	if (ColonyState) ColonyState->ColonyMorale = Data.ColonyMorale;

	UE_LOG(LogTemp, Log, TEXT("[QR] Loaded save '%s' (Day %d, %.0fs)"),
		*AutosaveSlotName, DayNumber, WorldTimeSeconds);

	// If a player pawn already exists (e.g. reloading mid-session), push
	// the snapshot to them now. Otherwise AQRCharacter::BeginPlay will
	// pull it on its own.
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		if (AQRCharacter* Player = Cast<AQRCharacter>(PC->GetPawn()))
		{
			ApplyLoadedDataToPlayer(Player);
		}
	}
}

void AQRGameMode::ApplyLoadedDataToPlayer(AQRCharacter* Player)
{
	if (!bHasPendingLoadedData || !Player) return;

	// Survival vitals + active injuries — a fracture survives a reload
	// instead of healing for free.
	FQRSaveSnapshot::ApplySurvival(Player->Survival, PendingLoadedData.PlayerData);

	// Inventory — full restore: grid placement, equipped armour/containers,
	// hand + offhand, durability/spoil. The snapshot library also resolves
	// item definitions through the asset registry, so defs seeded into
	// nested buckets (Items/Weapons, Items/Containers, ...) restore too —
	// the old root-path LoadObject silently dropped all of those.
	FQRSaveSnapshot::ApplyInventory(Player->Inventory, PendingLoadedData.PlayerInventory);

	// Research / tech tree / codex — was never restored before v2.
	FQRSaveSnapshot::ApplyResearch(Research, PendingLoadedData.ResearchData);

	// In-flight procedural missions resume with their saved progress.
	if (MissionDirector)
	{
		for (const TPair<FName, int32>& Pair : PendingLoadedData.DirectorMissionProgress)
		{
			if (MissionDirector->StartMissionById(Pair.Key))
			{
				for (FQRActiveMission& M : MissionDirector->ActiveMissions)
				{
					if (M.MissionId == Pair.Key)
					{
						M.CurrentProgress = FMath::Clamp(Pair.Value, 0, M.TargetQuantity);
						break;
					}
				}
			}
		}
	}

	// World-state restore: looted containers stay empty, codex keeps its
	// discovery history, and the placed base comes back.
	if (UWorld* W = GetWorld())
	{
		if (UQRLootedRegistry* Looted = W->GetSubsystem<UQRLootedRegistry>())
		{
			Looted->ImportLootedIds(PendingLoadedData.LootedContainerIds);
		}
		if (UQRCodexSubsystem* Codex = W->GetSubsystem<UQRCodexSubsystem>())
		{
			Codex->ImportEntries(PendingLoadedData.CodexEntries);
		}
		{
			// Run UNCONDITIONALLY — RestoreFromSave owns the teardown of
			// existing tagged pieces, so a zero-piece save must still
			// clear the world (the old >0 gate duplicated pieces when a
			// pieceless save loaded mid-session). Unresolvable entries
			// are carried forward so the next autosave keeps them.
			UDataTable* Catalog = (Player->Build) ? Player->Build->PieceCatalog.Get() : nullptr;
			const int32 N = UQRBuildModeComponent::RestoreFromSave(
				W, Catalog, PendingLoadedData.ColonyBuildables, &UnrestoredBuildables);
			UE_LOG(LogTemp, Log, TEXT("[QR] Restored %d/%d build pieces (%d carried forward)"),
				N, PendingLoadedData.ColonyBuildables.Num(), UnrestoredBuildables.Num());
		}

		// Despawn any AQRNPCActor that's still in the level from the
		// fresh load, then respawn from the save. Without the wipe,
		// loading mid-session would double the village.
		TArray<AActor*> ToKill;
		for (TActorIterator<AQRNPCActor> It(W); It; ++It) ToKill.Add(*It);
		for (AActor* A : ToKill) A->Destroy();

		for (const FQRNPCSaveData& N : PendingLoadedData.NPCActors)
		{
			UClass* Cls = AQRNPCActor::StaticClass();
			if (!N.NPCClassPath.IsEmpty())
			{
				if (UClass* Loaded = LoadObject<UClass>(nullptr, *N.NPCClassPath))
				{
					Cls = Loaded;
				}
			}
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			AQRNPCActor* NPC = W->SpawnActor<AQRNPCActor>(Cls, N.Location, N.Rotation, Params);
			if (!NPC) continue;
#if WITH_EDITOR
			if (!N.ActorLabel.IsEmpty()) NPC->SetActorLabel(N.ActorLabel);
#endif
			NPC->DisplayName  = N.DisplayName;
			if (UQRNPCBrainComponent* Brain = NPC->Brain)
			{
				Brain->HomePosition     = N.HomePosition;
				Brain->AssignedWorkPost = N.AssignedWorkPost;
				Brain->AssignedBed      = N.AssignedBed;
				Brain->State            = static_cast<EQRNPCBrainState>(N.BrainState);
			}
			// v3: restore the colonist's job so the farm/guard/medic AI
			// resumes instead of everyone reverting to Unassigned.
			if (AQRNPCColonist* Col = Cast<AQRNPCColonist>(NPC))
			{
				Col->ColonistRole = static_cast<EQRNPCRole>(N.ColonistRole);
			}
		}
		UE_LOG(LogTemp, Log, TEXT("[QR] Restored %d NPC actors"),
			PendingLoadedData.NPCActors.Num());
	}

	// Identity (name + pronouns + voice profile). Appearance lives on
	// the character creator flow and isn't restored mid-session — the
	// creator runs only at New Game.
	Player->PlayerIdentity = PendingLoadedData.PlayerIdentity;

	// Hotbar rebinding (v3): match saved per-slot item ids against the
	// restored inventory instances. First unclaimed instance with the id
	// wins, so two stacks of the same item fill two slots correctly.
	if (Player->Hotbar && Player->Inventory &&
		PendingLoadedData.HotbarSlotItemIds.Num() > 0)
	{
		TSet<UQRItemInstance*> Claimed;
		const int32 N = FMath::Min(Player->Hotbar->Slots.Num(),
			PendingLoadedData.HotbarSlotItemIds.Num());
		for (int32 i = 0; i < N; ++i)
		{
			Player->Hotbar->Slots[i] = nullptr;
			const FName WantId = PendingLoadedData.HotbarSlotItemIds[i];
			if (WantId.IsNone()) continue;
			for (UQRItemInstance* Inst : Player->Inventory->Items)
			{
				if (Inst && Inst->IsValid() && Inst->Definition &&
					Inst->Definition->ItemId == WantId && !Claimed.Contains(Inst))
				{
					Player->Hotbar->Slots[i] = Inst;
					Claimed.Add(Inst);
					break;
				}
			}
		}
		const int32 WantActive = PendingLoadedData.HotbarActiveSlot;
		if (WantActive >= 0 && WantActive < Player->Hotbar->Slots.Num())
		{
			Player->Hotbar->SelectSlot(WantActive);
		}
	}

	// Move the pawn to the saved location if non-zero. Avoids zeroing
	// out a freshly-spawned PlayerStart when there's no saved transform.
	const FVector& SavedLoc = PendingLoadedData.PlayerData.WorldLocation;
	if (!SavedLoc.IsNearlyZero())
	{
		Player->SetActorLocation(SavedLoc);
	}

	// One-shot: without clearing, every later authoritative pawn BeginPlay
	// (respawns, co-op joins) re-applied this stale snapshot.
	bHasPendingLoadedData = false;

	UE_LOG(LogTemp, Log, TEXT("[QR] Applied save snapshot to player pawn '%s'"), *Player->GetName());
}

void AQRGameMode::QR_Save() { QuickSave(); }
void AQRGameMode::QR_Load() { QuickLoad(); }

void AQRGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	WorldTimeSeconds += DeltaTime;

	// Guard the divisor — a 0 typed into the editor (or a cheat) would be a
	// division-by-zero crash on the very next tick.
	DayLengthRealSeconds = FMath::Max(DayLengthRealSeconds, 1.0f);

	// Convert real-seconds elapsed into game-hours for time-driven subsystems
	const float GameHoursElapsed = DeltaTime * 24.0f / DayLengthRealSeconds;
	if (Weather)           Weather->AdvanceByHours(GameHoursElapsed);
	if (VanguardConcordat) VanguardConcordat->AdvanceTime(GameHoursElapsed);

	// Each AI camp ticks its own sim — grand-strategy style. Camps
	// grow population, train military, accumulate hostility, and
	// launch raids independently. The Concordat above is the mega-
	// faction special-case; these are the regular satellite camps.
	for (TActorIterator<AQRFactionCamp> It(GetWorld()); It; ++It)
	{
		if (AQRFactionCamp* Camp = *It)
		{
			if (Camp->Sim) Camp->Sim->AdvanceGameHours(GameHoursElapsed);
		}
	}

	// Drive mount husbandry on every tameable animal — taming days
	// advance, stress decays, panic fires.
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (UQRMountHusbandryComponent* H = It->FindComponentByClass<UQRMountHusbandryComponent>())
		{
			H->TickGameHours(GameHoursElapsed);
		}
	}

	// Advance every farm plot's grow cycle + run the cross-contam
	// mutation roll.
	for (TActorIterator<AQRFarmPlotActor> It(GetWorld()); It; ++It)
	{
		if (AQRFarmPlotActor* Plot = *It)
		{
			Plot->TickGameHours(GameHoursElapsed);
		}
	}

	float DayProgress = FMath::Fmod(WorldTimeSeconds, DayLengthRealSeconds) / DayLengthRealSeconds;
	bool bWasNight    = bIsNight;
	bIsNight          = DayProgress > 0.5f; // Night is second half of cycle

	// Day rollover
	int32 NewDay = FMath::FloorToInt(WorldTimeSeconds / DayLengthRealSeconds) + 1;
	if (NewDay != DayNumber)
	{
		DayNumber = NewDay;
		OnDayStarted(DayNumber);
	}

	// Night transition
	if (bIsNight != bWasNight)
	{
		if (bIsNight) OnNightStarted();
	}
}

int32 AQRGameMode::GetStartingNPCCount() const
{
	// Solo = 3, 2 players = 2, 3 players = 1, 4+ players = 0
	return FMath::Max(0, 4 - MaxPlayers);
}

float AQRGameMode::GetDayProgress() const
{
	return FMath::Fmod(WorldTimeSeconds, DayLengthRealSeconds) / DayLengthRealSeconds;
}

void AQRGameMode::CompleteMission(FName MissionId)
{
	if (CompletedMissionIds.Contains(MissionId)) return;

	ActiveMissionIds.Remove(MissionId);
	CompletedMissionIds.Add(MissionId);
	OnMissionCompleted(MissionId);

	// Auto-unlock next mission in sequence (handled in Blueprint subclass for flexibility)
}

void AQRGameMode::ActivateMission(FName MissionId)
{
	if (ActiveMissionIds.Contains(MissionId) || CompletedMissionIds.Contains(MissionId)) return;
	ActiveMissionIds.Add(MissionId);
}

bool AQRGameMode::IsMissionComplete(FName MissionId) const
{
	return CompletedMissionIds.Contains(MissionId);
}

void AQRGameMode::QuickSave()
{
	if (!SaveSystem) return;

	// Never overwrite the slot while its own async load is still in
	// flight — EndPlay / the autosave timer could clobber a good save
	// with fresh-spawn state before the snapshot applies.
	if (bLoadInFlight)
	{
		UE_LOG(LogTemp, Log, TEXT("[QR] QuickSave skipped — load in flight"));
		return;
	}

	// Never snapshot mid-death: restoring a 0-HP pawn produces an
	// unkillable zombie with no death flow. The previous autosave stands;
	// respawn re-enables saving a few seconds later.
	if (APlayerController* DeadPC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		if (AQRCharacter* P = Cast<AQRCharacter>(DeadPC->GetPawn()))
		{
			if (P->Survival && P->Survival->bIsDead)
			{
				UE_LOG(LogTemp, Log, TEXT("[QR] QuickSave skipped — player dead (respawn pending)"));
				return;
			}
		}
	}

	FQRGameSaveData Data;
	Data.SaveSlotName        = AutosaveSlotName;
	Data.SaveTimestamp       = FDateTime::Now();

	// Real seed from the worldgen subsystem so resume can regenerate the
	// same world. Was hardcoded 0 ("Blueprint fills") — nothing ever did.
	Data.WorldSeed = BootstrapWorldSeed;
	if (UQRWorldGenSubsystem* WG = GetWorld() ? GetWorld()->GetSubsystem<UQRWorldGenSubsystem>() : nullptr)
	{
		if (WG->bGenerated) Data.WorldSeed = WG->WorldSeed;
	}
	Data.WorldTimeSeconds    = WorldTimeSeconds;
	Data.DayNumber           = DayNumber;
	Data.CompletedMissionIds = CompletedMissionIds;
	Data.ActiveMissionIds    = ActiveMissionIds;
	if (ColonyState) Data.ColonyMorale = ColonyState->ColonyMorale;
	if (MissionDirector)
	{
		for (const FQRActiveMission& M : MissionDirector->ActiveMissions)
		{
			Data.DirectorMissionProgress.Add(M.MissionId, M.CurrentProgress);
		}
	}

	// Snapshot the first local player's vitals + inventory. Multi-player
	// per-PC save expansion goes here later.
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		if (AQRCharacter* Player = Cast<AQRCharacter>(PC->GetPawn()))
		{
			Data.PlayerData.SurvivorId    = Player->SurvivorId;
			Data.PlayerData.WorldLocation = Player->GetActorLocation();
			Data.PlayerData.bIsAlive      = true;
			Data.PlayerIdentity           = Player->PlayerIdentity;

			// Snapshot library captures vitals + injuries, the full spatial
			// inventory (placement, equipped slots, durability), so a system
			// is saved iff it has a Capture/Apply pair — fields can't fall
			// out of the save by someone forgetting to extend this function.
			FQRSaveSnapshot::CaptureSurvival(Player->Survival, Data.PlayerData);
			FQRSaveSnapshot::CaptureInventory(Player->Inventory, Data.PlayerInventory);

			// Hotbar bindings (v3). Lives here, not in FQRSaveSnapshot:
			// UQRHotbarComponent is game-module, unreachable from QRSaveNet.
			if (Player->Hotbar)
			{
				Data.HotbarSlotItemIds.Reset();
				for (UQRItemInstance* Slot : Player->Hotbar->Slots)
				{
					Data.HotbarSlotItemIds.Add(
						(Slot && Slot->IsValid() && Slot->Definition)
							? Slot->Definition->ItemId : NAME_None);
				}
				Data.HotbarActiveSlot = Player->Hotbar->ActiveSlotIndex;
			}
		}
	}

	// Research / tech tree / codex — the ResearchData field existed since
	// v1 but nothing ever filled it, so research was lost on every reload.
	FQRSaveSnapshot::CaptureResearch(Research, Data.ResearchData);

	// World-state persistence (save v2): placed build pieces, looted-
	// container registry, the full codex (SeenCount / FirstSeen), and
	// every brain-carrying NPC actor so colonies survive reload.
	if (UWorld* W = GetWorld())
	{
		for (TActorIterator<AActor> It(W); It; ++It)
		{
			if (UQRBuildPieceTag* Tag = It->FindComponentByClass<UQRBuildPieceTag>())
			{
				FQRBuildableSaveData B;
				B.BuildableGuid = Tag->PieceGuid;
				B.PieceId       = Tag->PieceId;
				B.Location      = It->GetActorLocation();
				B.Rotation      = It->GetActorRotation();
				Data.ColonyBuildables.Add(MoveTemp(B));
			}

			if (AQRNPCActor* NPC = Cast<AQRNPCActor>(*It))
			{
				// Transient actors don't persist: corpses, and raid-party
				// members mid-raid — restoring raiders as brain-driven
				// villagers turned an autosave-during-a-raid into a squad
				// of permanent hostile "residents".
				if (NPC->Survival && NPC->Survival->bIsDead) continue;
				if (NPC->FindComponentByClass<UQRRaidPartyAI>()) continue;

				FQRNPCSaveData N;
#if WITH_EDITOR
				// Actor labels are editor-only; packaged builds key off
				// DisplayName instead.
				N.ActorLabel    = NPC->GetActorLabel();
#endif
				N.NPCClassPath  = NPC->GetClass()->GetPathName();
				N.DisplayName   = NPC->DisplayName;
				N.Location      = NPC->GetActorLocation();
				N.Rotation      = NPC->GetActorRotation();
				if (UQRNPCBrainComponent* Brain = NPC->Brain)
				{
					N.HomePosition     = Brain->HomePosition;
					N.AssignedWorkPost = Brain->AssignedWorkPost;
					N.AssignedBed      = Brain->AssignedBed;
					N.BrainState       = static_cast<uint8>(Brain->State);
				}
				if (const AQRNPCColonist* Col = Cast<AQRNPCColonist>(NPC))
				{
					N.ColonistRole = static_cast<uint8>(Col->ColonistRole);
				}
				Data.NPCActors.Add(MoveTemp(N));
			}
		}

		// Pieces that failed to respawn on the last load (missing catalog
		// row / mesh) ride along so they aren't erased from the save.
		Data.ColonyBuildables.Append(UnrestoredBuildables);

		if (UQRLootedRegistry* Looted = W->GetSubsystem<UQRLootedRegistry>())
		{
			Data.LootedContainerIds = Looted->ExportLootedIds();
		}
		if (UQRCodexSubsystem* Codex = W->GetSubsystem<UQRCodexSubsystem>())
		{
			Codex->ExportEntries(Data.CodexEntries);
		}
	}

	SaveSystem->SaveGame(Data, AutosaveSlotName, 0);
	UE_LOG(LogTemp, Log, TEXT("[QR] QuickSave -> '%s' (Day %d)"), *AutosaveSlotName, DayNumber);
}

void AQRGameMode::QuickLoad()
{
	if (!SaveSystem) return;
	// Bind exactly once — AddUObject does NOT dedupe (the old comment
	// claiming it does was wrong; repeated QuickLoads stacked handlers).
	if (!bLoadDelegateBound)
	{
		SaveSystem->OnLoadComplete.AddUObject(this, &AQRGameMode::HandleLoadComplete);
		bLoadDelegateBound = true;
	}
	bLoadInFlight = true;
	SaveSystem->LoadGame(AutosaveSlotName, 0);
}

void AQRGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// Register the QR cheat manager. The class existed but nothing ever
	// assigned it to a PlayerController, so every qr.* console command
	// was unreachable. PIE builds the default UCheatManager before
	// PostLogin, so force-recreate with ours.
	if (NewPlayer)
	{
		NewPlayer->CheatClass = UQRCheatManager::StaticClass();
		if (!NewPlayer->CheatManager ||
			!NewPlayer->CheatManager->IsA(UQRCheatManager::StaticClass()))
		{
			NewPlayer->CheatManager = nullptr;
			NewPlayer->AddCheats(/*bForce*/ true);
		}
	}
}

void AQRGameMode::HandlePlayerDied(AQRCharacter* DeadPawn)
{
	if (!DeadPawn) return;
	APlayerController* PC = Cast<APlayerController>(DeadPawn->GetController());
	if (!PC) return;

	// Mount the death-screen widget on the dying player's client.
	// CreateWidget on the server with a PC argument routes correctly to
	// that PC's local viewport — for listen-server hosts that's the host
	// screen; for remote clients the widget is created via the standard
	// owning-PC replication.
	TWeakObjectPtr<UQRDeathScreenWidget> WeakWidget;
	if (DeathScreenClass)
	{
		if (UQRDeathScreenWidget* W = CreateWidget<UQRDeathScreenWidget>(PC, DeathScreenClass))
		{
			W->AddToViewport(/*ZOrder*/ 1000);
			W->Initialize(RespawnDelaySeconds);
			WeakWidget = W;
		}
	}

	// Defer the actual respawn so the death screen can play its fade.
	// A lambda captures the PC and the dying pawn via weak pointers so
	// teardown failures (PC quits mid-fade) don't crash the respawn.
	TWeakObjectPtr<APlayerController> WeakPC   = PC;
	TWeakObjectPtr<AQRCharacter>      WeakDead = DeadPawn;
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda(
		[this, WeakPC, WeakDead, WeakWidget]()
		{
			APlayerController* P = WeakPC.Get();
			if (!P) return;

			// Remove the death overlay BEFORE respawning. Without this the
			// widget sits on screen forever showing "Respawning in 0…",
			// covering the revived pawn.
			if (UQRDeathScreenWidget* DeadUI = WeakWidget.Get())
			{
				DeadUI->RemoveFromParent();
			}

			AQRCharacter* Pawn = WeakDead.Get();
			if (!Pawn) return;

			// Respawn-in-place: revive the SAME pawn rather than spawning a
			// fresh one via RestartPlayer. RestartPlayer would have wiped the
			// player's inventory and left the dead pawn's HUD widgets stranded
			// in the viewport (stale 0-HP bar + old hotbar that wouldn't
			// cycle). Reusing the pawn keeps inventory, HUDs, and input bindings
			// intact — Revive just refills vitals, un-ragdolls, re-enables input
			// and teleports to a PlayerStart.
			FVector  SpawnLoc = Pawn->GetActorLocation();
			FRotator SpawnRot = Pawn->GetActorRotation();
			if (AActor* Start = FindPlayerStart(P))
			{
				SpawnLoc = Start->GetActorLocation();
				SpawnRot = Start->GetActorRotation();
			}
			Pawn->Revive(SpawnLoc, SpawnRot);
		}), RespawnDelaySeconds, /*bLoop*/ false);
}

void AQRGameMode::EnsureWorldBootstrapped(int32 Seed)
{
	UWorld* W = GetWorld();
	if (!W) return;
	UQRWorldGenSubsystem* WG = W->GetSubsystem<UQRWorldGenSubsystem>();
	if (!WG) return;

	// 1. Generation — find-or-spawn the seed actor and Generate. A seed
	//    actor saved into the level (the dressing script does this) only
	//    carries CONFIG; the subsystem's cell grid is per-session memory.
	if (!WG->bGenerated)
	{
		AQRWorldGenSeedActor* SeedActor = nullptr;
		for (TActorIterator<AQRWorldGenSeedActor> It(W); It; ++It) { SeedActor = *It; break; }
		if (!SeedActor)
		{
			SeedActor = W->SpawnActor<AQRWorldGenSeedActor>(
				AQRWorldGenSeedActor::StaticClass(),
				FVector::ZeroVector, FRotator::ZeroRotator);
			if (SeedActor)
			{
				SeedActor->WorldMapSizeKm = BootstrapMapSizeKm;
				SeedActor->CellSizeMeters = BootstrapCellSizeMeters;
			}
		}
		if (SeedActor)
		{
			SeedActor->WorldSeed = Seed;
			SeedActor->Generate();
		}
	}

	// 2. Population — POIs + fauna. A level-saved seed actor used to
	//    make the old all-or-nothing bootstrap skip this entirely: every
	//    PIE session ran with zero POIs and zero fauna.
	if (WG->bGenerated)
	{
		AQRWorldGenSpawner* WSpawner = nullptr;
		for (TActorIterator<AQRWorldGenSpawner> It(W); It; ++It) { WSpawner = *It; break; }
		if (!WSpawner)
		{
			WSpawner = W->SpawnActor<AQRWorldGenSpawner>(
				AQRWorldGenSpawner::StaticClass(),
				FVector::ZeroVector, FRotator::ZeroRotator);
			if (WSpawner) WSpawner->FaunaPerKm2Base = BootstrapFaunaPerKm2;
		}
		// A level-saved spawner has bSpawnOnBeginPlay=false and an empty
		// SpawnedActors every session — merely EXISTING is not populated.
		// This is why PIE kept running with zero POIs and zero fauna.
		if (WSpawner && !WSpawner->HasPopulated())
		{
			WSpawner->SpawnAll();
		}
		UE_LOG(LogTemp, Log,
			TEXT("[QRGameMode] world bootstrapped (seed %d, %.0fkm, %d POI actors live)"),
			Seed, WG->WorldMapSizeKm, WSpawner ? WSpawner->SpawnedActors.Num() : -1);
	}
}

void AQRGameMode::PlacePlayerAtSurfaceStart()
{
	UWorld* W = GetWorld();
	if (!W) return;
	UQRWorldGenSubsystem* WG = W->GetSubsystem<UQRWorldGenSubsystem>();
	if (!WG || !WG->bGenerated) return;

	// Seeded azimuth on the outer Surface ring (band Frac > 0.70).
	FRandomStream Rng(WG->WorldSeed ^ 0x57A27000);
	const float PlayableRadCm = WG->WorldMapSizeKm * 1000.0f * 0.5f * 100.0f;
	const float Az = Rng.FRandRange(0.0f, 2.0f * PI);
	FVector Start(FMath::Cos(Az), FMath::Sin(Az), 0.0f);
	Start *= PlayableRadCm * 0.78f;

	FHitResult Hit;
	FCollisionQueryParams QP(SCENE_QUERY_STAT(QRSurfaceStart), false);
	if (W->LineTraceSingleByChannel(Hit,
		Start + FVector(0, 0, 200000.0f), Start - FVector(0, 0, 200000.0f),
		ECC_Visibility, QP))
	{
		Start.Z = Hit.ImpactPoint.Z + 150.0f;
	}
	else
	{
		Start.Z = 300.0f;
	}

	// Move the respawn anchor and any already-spawned pawn.
	for (TActorIterator<APlayerStart> It(W); It; ++It)
	{
		It->SetActorLocation(Start);
		break;
	}
	if (APlayerController* PC = W->GetFirstPlayerController())
	{
		if (APawn* P = PC->GetPawn())
		{
			P->SetActorLocation(Start);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[QRGameMode] surface start at (%.0f, %.0f) — %.1f km from center"),
		Start.X, Start.Y, Start.Size2D() / 100000.0f);
}

void AQRGameMode::SpawnStarterVillageAtStart()
{
	UWorld* W = GetWorld();
	if (!W) return;

	// Anchor on the (already relocated) PlayerStart.
	FVector Center = FVector::ZeroVector;
	for (TActorIterator<APlayerStart> It(W); It; ++It) { Center = It->GetActorLocation(); break; }

	// Don't double a village that already exists near the start (editor-
	// authored, or an earlier call).
	for (TActorIterator<AQRNPCColonist> It(W); It; ++It)
	{
		if (FVector::DistSquared2D(It->GetActorLocation(), Center) < FMath::Square(300000.0f))
		{
			return;
		}
	}

	// The appearance script stamps meshes onto spawned actors, not the
	// C++ CDO — so stamp the shared Mannequin here or runtime colonists
	// spawn invisible. Deferred spawn: DefaultSkeletalMesh resolves in
	// BeginPlay, which for plain SpawnActor runs before we could set it.
	const TCHAR* MannequinPaths[] = {
		TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"),
		TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"),
		TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn.SKM_Quinn"),
		TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny.SKM_Manny"),
	};
	FSoftObjectPath MannequinPath;
	for (const TCHAR* P : MannequinPaths)
	{
		if (LoadObject<USkeletalMesh>(nullptr, P)) { MannequinPath = FSoftObjectPath(P); break; }
	}

	static const EQRNPCRole Roles[] = {
		EQRNPCRole::Farmer, EQRNPCRole::Guard, EQRNPCRole::Medic,
		EQRNPCRole::Farmer, EQRNPCRole::Builder, EQRNPCRole::Cook,
		EQRNPCRole::Guard, EQRNPCRole::Hauler,
	};
	static const TCHAR* Names[] = {
		TEXT("Asha"), TEXT("Bram"), TEXT("Cyra"), TEXT("Dev"),
		TEXT("Enna"), TEXT("Frey"), TEXT("Goran"), TEXT("Hale"),
	};

	int32 Placed = 0;
	const int32 Count = 8;
	for (int32 i = 0; i < Count; ++i)
	{
		const float Angle = (i / static_cast<float>(Count)) * 2.0f * PI;
		FVector Loc = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * 4000.0f;
		FHitResult Hit;
		FCollisionQueryParams QP(SCENE_QUERY_STAT(QRVillageGround), false);
		if (W->LineTraceSingleByChannel(Hit, Loc + FVector(0, 0, 100000.0f),
			Loc - FVector(0, 0, 100000.0f), ECC_Visibility, QP))
		{
			Loc.Z = Hit.ImpactPoint.Z + 95.0f;
		}

		FTransform Xform(FRotator(0, FMath::FRandRange(0.0f, 360.0f), 0), Loc);
		AQRNPCColonist* NPC = W->SpawnActorDeferred<AQRNPCColonist>(
			AQRNPCColonist::StaticClass(), Xform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (!NPC) continue;
		NPC->ColonistRole = Roles[i % UE_ARRAY_COUNT(Roles)];
		NPC->DisplayName  = FText::FromString(Names[i % UE_ARRAY_COUNT(Names)]);
		if (MannequinPath.IsValid())
		{
			NPC->DefaultSkeletalMesh = TSoftObjectPtr<USkeletalMesh>(MannequinPath);
		}
		NPC->FinishSpawning(Xform);
		++Placed;
	}

	// One colony dog, per tradition.
	{
		TArray<UClass*> Derived;
		GetDerivedClasses(AQRWildlifeBase::StaticClass(), Derived, true);
		for (UClass* C : Derived)
		{
			if (C->GetName().Contains(TEXT("ColonyDog")))
			{
				FVector Loc = Center + FVector(600.0f, 0.0f, 100.0f);
				W->SpawnActor<AQRWildlifeBase>(C, Loc, FRotator::ZeroRotator);
				break;
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[QRGameMode] runtime starter village: %d colonists at (%.0f, %.0f)"),
		Placed, Center.X, Center.Y);
}

void AQRGameMode::SpawnStarterFaunaBurst()
{
	UWorld* W = GetWorld();
	if (!W) return;
	FVector Center = FVector::ZeroVector;
	for (TActorIterator<APlayerStart> It(W); It; ++It) { Center = It->GetActorLocation(); break; }

	// Starter ecology: mostly grazers, one predator to make it honest.
	static const TCHAR* StarterSpecies[] = {
		TEXT("AshbackBoar"), TEXT("GlasshornRunner"), TEXT("RidgebackGrazer"),
		TEXT("ShardbackGrazer"), TEXT("ThornhideDray"), TEXT("HookjawStalker"),
	};
	static const int32 CountPer[] = { 3, 3, 2, 2, 1, 1 };

	TArray<UClass*> Derived;
	GetDerivedClasses(AQRWildlifeBase::StaticClass(), Derived, true);

	FRandomStream Rng(0xFA0A);
	int32 Spawned = 0;
	for (int32 s = 0; s < UE_ARRAY_COUNT(StarterSpecies); ++s)
	{
		UClass* Cls = nullptr;
		for (UClass* C : Derived)
		{
			if (!C->HasAnyClassFlags(CLASS_Abstract) && C->GetName().Contains(StarterSpecies[s]))
			{
				Cls = C;
				break;
			}
		}
		if (!Cls) continue;
		const AQRWildlifeBase* CDO = Cls->GetDefaultObject<AQRWildlifeBase>();
		const float HoistCm = FMath::Max(CDO ? CDO->BodyHeightMeters : 1.0f, 0.5f) * 100.0f;
		const int32 HerdId = 9000 + s;
		for (int32 i = 0; i < CountPer[s]; ++i)
		{
			const float Ang  = Rng.FRandRange(0.0f, 2.0f * PI);
			const float Dist = Rng.FRandRange(15000.0f, 50000.0f);   // 150–500 m
			FVector Loc = Center + FVector(FMath::Cos(Ang) * Dist, FMath::Sin(Ang) * Dist, 0.0f);
			FHitResult Hit;
			FCollisionQueryParams QP(SCENE_QUERY_STAT(QRFaunaGround), false);
			if (W->LineTraceSingleByChannel(Hit, Loc + FVector(0, 0, 100000.0f),
				Loc - FVector(0, 0, 100000.0f), ECC_Visibility, QP))
			{
				Loc.Z = Hit.ImpactPoint.Z;
			}
			FActorSpawnParameters SP;
			SP.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			if (AQRWildlifeBase* A = W->SpawnActor<AQRWildlifeBase>(
				Cls, Loc + FVector(0, 0, HoistCm), FRotator(0, Rng.FRandRange(0.0f, 360.0f), 0), SP))
			{
				A->HerdGroupId = (CountPer[s] > 1) ? HerdId : 0;
				++Spawned;
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[QRGameMode] starter fauna burst: %d animals near spawn"), Spawned);
}
