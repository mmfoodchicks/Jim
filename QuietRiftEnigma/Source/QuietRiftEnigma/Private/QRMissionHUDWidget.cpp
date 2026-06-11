#include "QRMissionHUDWidget.h"
#include "QRMissionDirector.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"

TSharedRef<SWidget> UQRMissionHUDWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		WidgetTree->RootWidget = Canvas;

		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Panel->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.35f));
		Panel->SetPadding(FMargin(10.0f, 8.0f));
		UCanvasPanelSlot* PS = Canvas->AddChildToCanvas(Panel);
		if (PS)
		{
			// Top-right, under the vitals area.
			PS->SetAnchors(FAnchors(1.0f, 0.0f));
			PS->SetAlignment(FVector2D(1.0f, 0.0f));
			PS->SetPosition(FVector2D(-16.0f, 120.0f));
			PS->SetAutoSize(true);
		}

		MissionList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Panel->SetContent(MissionList);
	}
	return Super::RebuildWidget();
}

void UQRMissionHUDWidget::Bind(UQRMissionDirector* InDirector)
{
	if (Director)
	{
		Director->OnMissionIssued.RemoveDynamic(this,    &UQRMissionHUDWidget::HandleIssued);
		Director->OnMissionCompleted.RemoveDynamic(this, &UQRMissionHUDWidget::HandleCompleted);
		Director->OnMissionProgress.RemoveDynamic(this,  &UQRMissionHUDWidget::HandleProgress);
	}
	Director = InDirector;
	if (Director)
	{
		Director->OnMissionIssued.AddDynamic(this,    &UQRMissionHUDWidget::HandleIssued);
		Director->OnMissionCompleted.AddDynamic(this, &UQRMissionHUDWidget::HandleCompleted);
		Director->OnMissionProgress.AddDynamic(this,  &UQRMissionHUDWidget::HandleProgress);
	}
	Refresh();
}

void UQRMissionHUDWidget::NativeDestruct()
{
	if (Director)
	{
		Director->OnMissionIssued.RemoveDynamic(this,    &UQRMissionHUDWidget::HandleIssued);
		Director->OnMissionCompleted.RemoveDynamic(this, &UQRMissionHUDWidget::HandleCompleted);
		Director->OnMissionProgress.RemoveDynamic(this,  &UQRMissionHUDWidget::HandleProgress);
		Director = nullptr;
	}
	Super::NativeDestruct();
}

void UQRMissionHUDWidget::HandleIssued(FName)            { Refresh(); }
void UQRMissionHUDWidget::HandleCompleted(FName)         { Refresh(); }
void UQRMissionHUDWidget::HandleProgress(FName, int32)   { Refresh(); }

void UQRMissionHUDWidget::Refresh()
{
	if (!MissionList) return;
	MissionList->ClearChildren();
	if (!Director) return;

	// Hide the whole panel when nothing is active.
	SetVisibility(Director->ActiveMissions.Num() > 0
		? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	auto AddLine = [this](const FString& Text, float Size, FLinearColor Color)
	{
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetText(FText::FromString(Text));
		T->SetColorAndOpacity(FSlateColor(Color));
		FSlateFontInfo Font = T->GetFont();
		Font.Size = Size;
		T->SetFont(Font);
		UVerticalBoxSlot* S = MissionList->AddChildToVerticalBox(T);
		if (S) S->SetPadding(FMargin(0, 1));
	};

	AddLine(TEXT("MISSIONS"), 11, FLinearColor(0.8f, 0.75f, 0.5f, 1.0f));

	for (const FQRActiveMission& M : Director->ActiveMissions)
	{
		// Display name from the template row; raw id as fallback.
		FString Name = M.MissionId.ToString();
		if (Director->MissionTemplateTable)
		{
			if (const FQRMissionTemplateRow* Row =
				Director->MissionTemplateTable->FindRow<FQRMissionTemplateRow>(M.MissionId, TEXT("QRMissionHUD"), false))
			{
				if (!Row->DisplayName.IsEmpty()) Name = Row->DisplayName.ToString();
			}
		}
		FString Line = FString::Printf(TEXT("%s   %d/%d"), *Name, M.CurrentProgress, M.TargetQuantity);
		if (M.bHasTargetLocation && M.Family == EQRMissionFamily::ScoutPOI)
		{
			Line += FString::Printf(TEXT("  (%.0fm)"),
				FVector::Dist2D(M.TargetLocation,
					GetOwningPlayerPawn() ? GetOwningPlayerPawn()->GetActorLocation() : FVector::ZeroVector) / 100.0f);
		}
		AddLine(Line, 12, FLinearColor::White);
	}
}
