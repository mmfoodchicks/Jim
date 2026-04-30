#include "QRGameMode.h"
#include "GameFramework/GameStateBase.h"
#include "QRColonyStateComponent.h"
#include "QRResearchComponent.h"
#include "QRWeatherComponent.h"
#include "QRSaveGameSystem.h"
#include "QRCharacter.h"
#include "Kismet/GameplayStatics.h"

AQRGameMode::AQRGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 1.0f;

	DefaultPawnClass = AQRCharacter::StaticClass();

	SaveSystem = CreateDefaultSubobject<UQRSaveGameSystem>(TEXT("SaveSystem"));
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

	// Activate tutorial mission
	ActivateMission(FName("MQ_000"));
}

void AQRGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	WorldTimeSeconds += DeltaTime;

	// Convert real-seconds elapsed into game-hours for time-driven subsystems
	const float GameHoursElapsed = DeltaTime * 24.0f / DayLengthRealSeconds;
	if (Weather) Weather->AdvanceByHours(GameHoursElapsed);

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
	Data.WorldSeed        = 0; // Blueprint fills from world generator
	Data.WorldTimeSeconds = WorldTimeSeconds;
	Data.DayNumber        = DayNumber;
	Data.CompletedMissionIds = CompletedMissionIds;
	Data.ActiveMissionIds    = ActiveMissionIds;
	if (ColonyState) Data.ColonyMorale = ColonyState->ColonyMorale;

	SaveSystem->SaveGame(Data, TEXT("QuickSave"), 0);
}

void AQRGameMode::QuickLoad()
{
	if (!SaveSystem) return;
	SaveSystem->LoadGame(TEXT("QuickSave"), 0);
}

void AQRGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	// New player joined — in listen-server co-op, sync their initial state
}
