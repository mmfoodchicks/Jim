#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QRMissionHUDWidget.generated.h"

class UVerticalBox;
class UQRMissionDirector;

/**
 * Always-on mission tracker, top-right of the HUD. One line per active
 * mission: "Display Name        2/5". Rebuilds on the director's
 * issue / progress / complete events — no per-tick work.
 *
 * Programmatic UMG like every other widget in the project; designer can
 * subclass to restyle. Mounted by AQRCharacter::BeginPlay on the local
 * player (single-player / listen-host v1 — remote co-op clients get it
 * when mission state replicates in a later pass).
 */
UCLASS()
class QUIETRIFTENIGMA_API UQRMissionHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "QR|UI")
	void Bind(UQRMissionDirector* InDirector);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;

private:
	UPROPERTY()
	TObjectPtr<UQRMissionDirector> Director = nullptr;

	UPROPERTY()
	TObjectPtr<UVerticalBox> MissionList = nullptr;

	UFUNCTION() void HandleIssued(FName MissionId);
	UFUNCTION() void HandleCompleted(FName MissionId);
	UFUNCTION() void HandleProgress(FName MissionId, int32 NewProgress);

	void Refresh();
};
