#include "QRGameMode.h"
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
#include "QRItemInstance.h"
#include "QRItemDefinition.h"
#include "QRSaveTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

AQRGameMode::AQRGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 1.0f;

	DefaultPawnClass = AQRCharacter::StaticClass();

	SaveSystem = CreateDefaultSubobject<UQRSaveGameSystem>(TEXT("SaveSystem"));

	// Default death-screen widget class — C++ placeholder, swap via BP.
	DeathScreenClass = UQRDeathScreenWidget::StaticClass();
}

void AQRGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Colony state, research, and weather components live on the GameState actor
	if (AGameStateBase* GS = GetGameState<AGameStateBase>())
	{
		ColonyState = GS->FindComponentByClass<UQRColonyStateComponent>();
		Research    = GS->FindComponentByClass<UQRResearchComponent>();
		Weather     = GS->FindComponentByClass<UQRWeatherComponent>();
	}

	// The Concordat is placed once by the level designer; locate it by class.
	VanguardConcordat = Cast<AQRVanguardColony>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AQRVanguardColony::StaticClass()));

	// Activate tutorial mission
	ActivateMission(FName("MQ_000"));

	// Auto-load on session start. Async — applies to player pawn from
	// ApplyLoadedDataToPlayer once the character spawns + asks for its
	// share of the snapshot.
	if (SaveSystem && SaveSystem->DoesSaveExist(AutosaveSlotName))
	{
		SaveSystem->OnLoadComplete.AddUObject(this, &AQRGameMode::HandleLoadComplete);
		SaveSystem->LoadGame(AutosaveSlotName);
	}

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
	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("[QR] Save load failed for slot '%s'"), *AutosaveSlotName);
		return;
	}
	PendingLoadedData     = Data;
	bHasPendingLoadedData = true;

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

	// Survival vitals — restore to whatever the save says.
	if (UQRSurvivalComponent* Surv = Player->Survival)
	{
		Surv->Health   = PendingLoadedData.PlayerData.Health;
		Surv->Hunger   = PendingLoadedData.PlayerData.Hunger;
		Surv->Thirst   = PendingLoadedData.PlayerData.Thirst;
		Surv->Fatigue  = PendingLoadedData.PlayerData.Fatigue;
		// Oxygen / temp aren't on the survivor save struct; leave defaults.
	}

	// Inventory contents — clear what's there, then rebuild from save.
	// We can't materially restore stack containers/cells without the
	// spatial grid metadata which the save struct doesn't carry, so
	// items are re-added via TryAddItem so they pack the grid afresh.
	if (UQRInventoryComponent* Inv = Player->Inventory)
	{
		// Walk a copy because TryAddItem mutates Items.
		TArray<FQRItemSaveData> ToRestore = PendingLoadedData.PlayerInventory.Items;
		for (const FQRItemSaveData& Saved : ToRestore)
		{
			int32 Remainder = 0;
			// We need the UQRItemDefinition for Saved.ItemId. Resolve
			// via FindObject — definition assets are loaded once on
			// startup so a global FindObject hit is cheap.
			const FString DefPath = FString::Printf(
				TEXT("/Game/QuietRift/Data/Items/%s.%s"), *Saved.ItemId.ToString(), *Saved.ItemId.ToString());
			const UQRItemDefinition* Def = LoadObject<UQRItemDefinition>(nullptr, *DefPath);
			if (!Def) continue;
			Inv->TryAddByDefinition(Def, FMath::Max(1, Saved.Quantity), Remainder);
		}
	}

	// Hand slot restore — pull the matching definition out of inventory
	// (just re-added above) and equip it. Save struct only carries ItemId
	// and Quantity, so a fresh-equipped instance is acceptable for v1.
	if (UQRInventoryComponent* Inv = Player->Inventory)
	{
		if (PendingLoadedData.PlayerInventory.bHasHandSlot)
		{
			const FName HandId = PendingLoadedData.PlayerInventory.HandSlot.ItemId;
			for (UQRItemInstance* Inst : Inv->Items)
			{
				if (Inst && Inst->Definition && Inst->Definition->ItemId == HandId)
				{
					Inv->TryEquipToHandSlot(Inst);
					break;
				}
			}
		}
	}

	// Identity (name + pronouns + voice profile). Appearance lives on
	// the character creator flow and isn't restored mid-session — the
	// creator runs only at New Game.
	Player->PlayerIdentity = PendingLoadedData.PlayerIdentity;

	// Move the pawn to the saved location if non-zero. Avoids zeroing
	// out a freshly-spawned PlayerStart when there's no saved transform.
	const FVector& SavedLoc = PendingLoadedData.PlayerData.WorldLocation;
	if (!SavedLoc.IsNearlyZero())
	{
		Player->SetActorLocation(SavedLoc);
	}

	UE_LOG(LogTemp, Log, TEXT("[QR] Applied save snapshot to player pawn '%s'"), *Player->GetName());
}

void AQRGameMode::QR_Save() { QuickSave(); }
void AQRGameMode::QR_Load() { QuickLoad(); }

void AQRGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	WorldTimeSeconds += DeltaTime;

	// Convert real-seconds elapsed into game-hours for time-driven subsystems
	const float GameHoursElapsed = DeltaTime * 24.0f / DayLengthRealSeconds;
	if (Weather)           Weather->AdvanceByHours(GameHoursElapsed);
	if (VanguardConcordat) VanguardConcordat->AdvanceTime(GameHoursElapsed);

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
	FQRGameSaveData Data;
	Data.SaveSlotName        = AutosaveSlotName;
	Data.SaveTimestamp       = FDateTime::Now();
	Data.WorldSeed           = 0; // Blueprint fills from world generator
	Data.WorldTimeSeconds    = WorldTimeSeconds;
	Data.DayNumber           = DayNumber;
	Data.CompletedMissionIds = CompletedMissionIds;
	Data.ActiveMissionIds    = ActiveMissionIds;
	if (ColonyState) Data.ColonyMorale = ColonyState->ColonyMorale;

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

			if (UQRSurvivalComponent* Surv = Player->Survival)
			{
				Data.PlayerData.Health  = Surv->Health;
				Data.PlayerData.Hunger  = Surv->Hunger;
				Data.PlayerData.Thirst  = Surv->Thirst;
				Data.PlayerData.Fatigue = Surv->Fatigue;
				Data.PlayerData.bIsAlive = !Surv->bIsDead;
			}

			if (UQRInventoryComponent* Inv = Player->Inventory)
			{
				FQRInventorySaveData InvSave;
				for (UQRItemInstance* Inst : Inv->Items)
				{
					if (!Inst || !Inst->Definition) continue;
					FQRItemSaveData ItemSave;
					ItemSave.ItemId   = Inst->Definition->ItemId;
					ItemSave.Quantity = Inst->Quantity;
					InvSave.Items.Add(ItemSave);
				}
				if (UQRItemInstance* Held = Inv->HandSlot)
				{
					if (Held->Definition)
					{
						InvSave.HandSlot.ItemId   = Held->Definition->ItemId;
						InvSave.HandSlot.Quantity = Held->Quantity;
						InvSave.bHasHandSlot      = true;
					}
				}
				Data.PlayerInventory = InvSave;
			}
		}
	}

	SaveSystem->SaveGame(Data, AutosaveSlotName, 0);
	UE_LOG(LogTemp, Log, TEXT("[QR] QuickSave -> '%s' (Day %d)"), *AutosaveSlotName, DayNumber);
}

void AQRGameMode::QuickLoad()
{
	if (!SaveSystem) return;
	// Subscribe each call — HandleLoadComplete is idempotent and the
	// delegate dedupes on identical bindings.
	SaveSystem->OnLoadComplete.AddUObject(this, &AQRGameMode::HandleLoadComplete);
	SaveSystem->LoadGame(AutosaveSlotName, 0);
}

void AQRGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	// New player joined — in listen-server co-op, sync their initial state
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
	if (DeathScreenClass)
	{
		if (UQRDeathScreenWidget* W = CreateWidget<UQRDeathScreenWidget>(PC, DeathScreenClass))
		{
			W->AddToViewport(/*ZOrder*/ 1000);
			W->Initialize(RespawnDelaySeconds);
		}
	}

	// Defer the actual respawn so the death screen can play its fade.
	// A lambda captures the PC and the dying pawn via weak pointers so
	// teardown failures (PC quits mid-fade) don't crash the respawn.
	TWeakObjectPtr<APlayerController> WeakPC   = PC;
	TWeakObjectPtr<AQRCharacter>      WeakDead = DeadPawn;
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda(
		[this, WeakPC, WeakDead]()
		{
			APlayerController* P = WeakPC.Get();
			if (!P) return;

			// Tear down the corpse before spawning a new pawn so we
			// don't end up with two characters owned by the same PC.
			if (AQRCharacter* Corpse = WeakDead.Get())
			{
				Corpse->Destroy();
			}

			// Standard GameModeBase respawn — picks a PlayerStart and
			// possesses a freshly spawned DefaultPawnClass.
			RestartPlayer(P);
		}), RespawnDelaySeconds, /*bLoop*/ false);
}
