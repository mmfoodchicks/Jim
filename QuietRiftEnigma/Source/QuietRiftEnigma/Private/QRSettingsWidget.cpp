#include "QRSettingsWidget.h"
#include "QRUISound.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Sound/SoundClass.h"
#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Misc/ConfigCacheIni.h"
#include "QRCharacter.h"
#include "QRFPViewComponent.h"
#include "QRPauseMenuWidget.h"

namespace QRSettingsDefaults
{
	constexpr float SensitivityMin = 0.1f, SensitivityMax = 4.0f, SensitivityDef = 1.0f;
	constexpr float FOVMin         = 60.0f, FOVMax         = 120.0f, FOVDef         = 90.0f;
	constexpr float VolumeMin      = 0.0f, VolumeMax      = 1.0f,  VolumeDef      = 1.0f;

	const TCHAR* kCfgSection = TEXT("/Script/QuietRiftEnigma.UserSettings");
}

UQRSettingsWidget::UQRSettingsWidget(const FObjectInitializer& OI)
	: Super(OI)
{
}

TSharedRef<SWidget> UQRSettingsWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		using namespace QRSettingsDefaults;

		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		WidgetTree->RootWidget = Canvas;

		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Panel->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));
		UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(Panel);
		if (PanelSlot)
		{
			PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			PanelSlot->SetSize(FVector2D(520.0f, 360.0f));
		}

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Panel->SetContent(Column);
		Panel->SetPadding(FMargin(24.0f));

		UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Title->SetText(FText::FromString(TEXT("Settings")));
		Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		{
			FSlateFontInfo Font = Title->GetFont();
			Font.Size = 28;
			Title->SetFont(Font);
		}
		UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(Title);
		if (TitleSlot)
		{
			TitleSlot->SetHorizontalAlignment(HAlign_Center);
			TitleSlot->SetPadding(FMargin(0, 0, 0, 16));
		}

		float SensCur = SensitivityDef, FOVCur = FOVDef, VolCur = VolumeDef;
		GConfig->GetFloat(kCfgSection, TEXT("MouseSensitivity"), SensCur, GGameUserSettingsIni);
		GConfig->GetFloat(kCfgSection, TEXT("FieldOfView"),      FOVCur,  GGameUserSettingsIni);
		GConfig->GetFloat(kCfgSection, TEXT("MasterVolume"),     VolCur,  GGameUserSettingsIni);

		MakeSliderRow(Column, TEXT("Mouse Sensitivity"),  SensitivityMin, SensitivityMax, SensCur,
			SensitivitySlider, SensitivityValue);
		MakeSliderRow(Column, TEXT("Field of View"),       FOVMin,         FOVMax,         FOVCur,
			FOVSlider, FOVValue);
		MakeSliderRow(Column, TEXT("Master Volume"),       VolumeMin,      VolumeMax,      VolCur,
			VolumeSlider, VolumeValue);

		if (SensitivitySlider) SensitivitySlider->OnValueChanged.AddDynamic(this, &UQRSettingsWidget::HandleSensitivity);
		if (FOVSlider)         FOVSlider->OnValueChanged.AddDynamic(this,         &UQRSettingsWidget::HandleFOV);
		if (VolumeSlider)      VolumeSlider->OnValueChanged.AddDynamic(this,      &UQRSettingsWidget::HandleVolume);

		// Left-handed checkbox row. CheckBox + label sit on the same horizontal
		// row so the layout matches the sliders above.
		{
			UHorizontalBox* LeftRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

			LeftHandedCheck = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass());
			bool bLeftCur = false;
			GConfig->GetBool(kCfgSection, TEXT("LeftHanded"), bLeftCur, GGameUserSettingsIni);
			LeftHandedCheck->SetIsChecked(bLeftCur);
			LeftHandedCheck->OnCheckStateChanged.AddDynamic(this, &UQRSettingsWidget::HandleLeftHanded);
			LeftRow->AddChildToHorizontalBox(LeftHandedCheck);

			UTextBlock* LeftLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			LeftLabel->SetText(FText::FromString(TEXT("  Left-Handed (mirror held weapon)")));
			LeftLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			UHorizontalBoxSlot* LblSlot = LeftRow->AddChildToHorizontalBox(LeftLabel);
			if (LblSlot) LblSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

			UVerticalBoxSlot* LeftRowSlot = Column->AddChildToVerticalBox(LeftRow);
			if (LeftRowSlot) LeftRowSlot->SetPadding(FMargin(0, 12, 0, 4));
		}

		// Close button.
		CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		UTextBlock* CloseText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		CloseText->SetText(FText::FromString(TEXT("Close")));
		CloseText->SetJustification(ETextJustify::Center);
		CloseText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		{
			FSlateFontInfo Font = CloseText->GetFont();
			Font.Size = 16;
			CloseText->SetFont(Font);
		}
		CloseButton->AddChild(CloseText);
		UVerticalBoxSlot* CloseSlot = Column->AddChildToVerticalBox(CloseButton);
		if (CloseSlot)
		{
			CloseSlot->SetPadding(FMargin(0, 24, 0, 0));
			CloseSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		CloseButton->OnClicked.AddDynamic(this, &UQRSettingsWidget::HandleClose);
	}
	return Super::RebuildWidget();
}

void UQRSettingsWidget::MakeSliderRow(UVerticalBox* Parent, const FString& Label,
	float Min, float Max, float Current, TObjectPtr<USlider>& OutSlider, TObjectPtr<UTextBlock>& OutValue)
{
	if (!Parent || !WidgetTree) return;

	UVerticalBox* Row = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

	// Label + numeric readout horizontally.
	UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

	UTextBlock* Lab = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Lab->SetText(FText::FromString(Label));
	Lab->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	UHorizontalBoxSlot* LabSlot = HeaderRow->AddChildToHorizontalBox(Lab);
	if (LabSlot) LabSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	OutValue = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	OutValue->SetText(FText::AsNumber(Current));
	OutValue->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	HeaderRow->AddChildToHorizontalBox(OutValue);

	UVerticalBoxSlot* HeaderSlot = Row->AddChildToVerticalBox(HeaderRow);
	if (HeaderSlot) HeaderSlot->SetPadding(FMargin(0, 0, 0, 4));

	// Slider below.
	OutSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass());
	OutSlider->SetMinValue(Min);
	OutSlider->SetMaxValue(Max);
	OutSlider->SetValue(Current);
	Row->AddChildToVerticalBox(OutSlider);

	UVerticalBoxSlot* RowSlot = Parent->AddChildToVerticalBox(Row);
	if (RowSlot) RowSlot->SetPadding(FMargin(0, 8));
}

void UQRSettingsWidget::HandleSensitivity(float NewValue)
{
	using namespace QRSettingsDefaults;
	if (SensitivityValue) SensitivityValue->SetText(FText::AsNumber(FMath::RoundToFloat(NewValue * 100.0f) / 100.0f));
	GConfig->SetFloat(kCfgSection, TEXT("MouseSensitivity"), NewValue, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);

	// Push live to the local pawn — before this the slider only wrote a
	// config value nothing ever read, so sensitivity was a no-op.
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (AQRCharacter* Char = Cast<AQRCharacter>(PC->GetPawn()))
		{
			Char->SetMouseSensitivity(NewValue);
		}
	}
}

void UQRSettingsWidget::HandleFOV(float NewValue)
{
	using namespace QRSettingsDefaults;
	const int32 Snapped = FMath::RoundToInt(NewValue);
	if (FOVValue) FOVValue->SetText(FText::AsNumber(Snapped));
	GConfig->SetFloat(kCfgSection, TEXT("FieldOfView"), static_cast<float>(Snapped), GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);

	// Drive the view component's BaseFOV (its interp target) instead of
	// PlayerCameraManager::SetFOV — the manager override PINS the FOV and
	// silently killed ADS / scope / sprint zoom for the rest of the session.
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (APawn* P = PC->GetPawn())
		{
			if (UQRFPViewComponent* View = P->FindComponentByClass<UQRFPViewComponent>())
			{
				View->BaseFOV = static_cast<float>(Snapped);
			}
		}
	}
}

void UQRSettingsWidget::HandleVolume(float NewValue)
{
	using namespace QRSettingsDefaults;
	if (VolumeValue) VolumeValue->SetText(FText::AsNumber(FMath::RoundToFloat(NewValue * 100.0f) / 100.0f));
	GConfig->SetFloat(kCfgSection, TEXT("MasterVolume"), NewValue, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);

	// Push live to the audio device so the slider is audible immediately.
	if (GEngine)
	{
		if (FAudioDevice* AD = GEngine->GetMainAudioDeviceRaw())
		{
			AD->SetTransientPrimaryVolume(NewValue);
		}
	}
}

void UQRSettingsWidget::HandleLeftHanded(bool bNew)
{
	using namespace QRSettingsDefaults;
	GConfig->SetBool(kCfgSection, TEXT("LeftHanded"), bNew, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);

	// Push live to the local pawn so the held weapon flips immediately.
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (AQRCharacter* Char = Cast<AQRCharacter>(PC->GetPawn()))
		{
			Char->SetLeftHanded(bNew);
		}
	}
}

void UQRSettingsWidget::HandleClose()
{
	QRUISound::PlayClick(this);

	// QR_OpenSettings switched the controller to GameAndUI; without undoing
	// that here, closing Settings opened from gameplay (console command, no
	// pause menu underneath) left the player stuck in UI input — WASD dead.
	// If the pause menu is still up, hand focus back to it instead.
	if (APlayerController* PC = GetOwningPlayer())
	{
		UQRPauseMenuWidget* Pause = nullptr;
		if (AQRCharacter* Char = Cast<AQRCharacter>(PC->GetPawn()))
		{
			Pause = (Char->PauseMenu && Char->PauseMenu->IsInViewport())
				? Char->PauseMenu.Get() : nullptr;
		}
		if (Pause)
		{
			FInputModeGameAndUI Mode;
			Mode.SetWidgetToFocus(Pause->TakeWidget());
			PC->SetInputMode(Mode);
			PC->bShowMouseCursor = true;
		}
		else if (Cast<AQRCharacter>(PC->GetPawn()))
		{
			PC->SetInputMode(FInputModeGameOnly());
			PC->bShowMouseCursor = false;
		}
		else
		{
			// Opened from the MAIN MENU: there is no gameplay pawn, so
			// dropping to GameOnly + hidden cursor made every menu button
			// unclickable — a hard soft-lock. Stay in UI-only input.
			PC->SetInputMode(FInputModeUIOnly());
			PC->bShowMouseCursor = true;
		}
	}

	RemoveFromParent();
}
