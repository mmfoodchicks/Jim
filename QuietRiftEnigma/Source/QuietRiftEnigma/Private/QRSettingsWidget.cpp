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
#include "Sound/SoundClass.h"
#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Misc/ConfigCacheIni.h"

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
	float Min, float Max, float Current, USlider*& OutSlider, UTextBlock*& OutValue)
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
}

void UQRSettingsWidget::HandleFOV(float NewValue)
{
	using namespace QRSettingsDefaults;
	const int32 Snapped = FMath::RoundToInt(NewValue);
	if (FOVValue) FOVValue->SetText(FText::AsNumber(Snapped));
	GConfig->SetFloat(kCfgSection, TEXT("FieldOfView"), static_cast<float>(Snapped), GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);

	// Push into the active camera on the local pawn so the change is
	// visible immediately.
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (APlayerCameraManager* CM = PC->PlayerCameraManager)
		{
			CM->SetFOV(static_cast<float>(Snapped));
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

void UQRSettingsWidget::HandleClose()
{
	QRUISound::PlayClick(this);
	RemoveFromParent();
}
