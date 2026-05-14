#include "QRCreativeBrowserWidget.h"
#include "QRHotbarComponent.h"
#include "QRItemBrowserHelper.h"
#include "QRItemDefinition.h"
#include "QRUISound.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"

// ── Payload buttons ─────────────────────────────────

UQRResultButton::UQRResultButton()
{
	OnClicked.AddDynamic(this, &UQRResultButton::HandleClicked);
}

void UQRResultButton::HandleClicked()
{
	QRUISound::PlayClick(this);
	OnResultRowClicked.Broadcast(CarriedDefinition);
}

UQRSlotAssignButton::UQRSlotAssignButton()
{
	OnClicked.AddDynamic(this, &UQRSlotAssignButton::HandleClicked);
}

void UQRSlotAssignButton::HandleClicked()
{
	QRUISound::PlayConfirm(this);
	OnSlotButtonClicked.Broadcast(SlotIndex);
}

// ── Browser widget ──────────────────────────────────

UQRCreativeBrowserWidget::UQRCreativeBrowserWidget(const FObjectInitializer& OI)
	: Super(OI)
{
}

TSharedRef<SWidget> UQRCreativeBrowserWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Frame"));
		Frame->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.92f));
		Frame->SetPadding(FMargin(18.0f));

		UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VBox"));

		UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
		Title->SetText(FText::FromString(TEXT("Creative Item Browser")));
		VBox->AddChildToVerticalBox(Title);

		SearchBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("Search"));
		SearchBox->SetHintText(FText::FromString(TEXT("Search items…")));
		SearchBox->OnTextChanged.AddDynamic(this, &UQRCreativeBrowserWidget::OnSearchChanged);
		UVerticalBoxSlot* SearchSlot = VBox->AddChildToVerticalBox(SearchBox);
		SearchSlot->SetPadding(FMargin(0.0f, 6.0f));

		ResultsScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("Results"));
		UVerticalBoxSlot* ResultsSlot = VBox->AddChildToVerticalBox(ResultsScroll);
		ResultsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ResultsSlot->SetPadding(FMargin(0.0f, 6.0f));

		StatusLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Status"));
		StatusLabel->SetText(FText::FromString(TEXT("Select an item, then click a slot below.")));
		VBox->AddChildToVerticalBox(StatusLabel);

		UHorizontalBox* SlotRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SlotRow"));
		for (int32 i = 0; i < 9; ++i)
		{
			UQRSlotAssignButton* Btn = WidgetTree->ConstructWidget<UQRSlotAssignButton>(
				UQRSlotAssignButton::StaticClass(),
				*FString::Printf(TEXT("SlotBtn_%d"), i));
			Btn->SlotIndex = i;
			Btn->OnSlotButtonClicked.AddDynamic(this, &UQRCreativeBrowserWidget::HandleSlotButtonClicked);

			UTextBlock* BL = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
				*FString::Printf(TEXT("SlotBtnLabel_%d"), i));
			BL->SetText(FText::AsNumber(i + 1));
			BL->SetJustification(ETextJustify::Center);
			Btn->AddChild(BL);

			UHorizontalBoxSlot* HS = SlotRow->AddChildToHorizontalBox(Btn);
			HS->SetPadding(FMargin(4.0f, 0.0f));
			HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		UVerticalBoxSlot* SlotRowVS = VBox->AddChildToVerticalBox(SlotRow);
		SlotRowVS->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));

		Frame->SetContent(VBox);

		UCanvasPanelSlot* FrameSlot = Root->AddChildToCanvas(Frame);
		FrameSlot->SetAnchors(FAnchors(0.15f, 0.10f, 0.85f, 0.85f));
		FrameSlot->SetOffsets(FMargin(0));
	}

	return Super::RebuildWidget();
}

void UQRCreativeBrowserWidget::Bind(UQRHotbarComponent* InHotbar)
{
	Hotbar = InHotbar;
}

void UQRCreativeBrowserWidget::Toggle()
{
	if (bOpen) Close(); else Open();
}

void UQRCreativeBrowserWidget::Open()
{
	bOpen = true;
	SetVisibility(ESlateVisibility::Visible);

	if (AllDefs.Num() == 0)
	{
		const TArray<UQRItemDefinition*> Loaded = UQRItemBrowserHelper::GetAllItemDefinitions();
		AllDefs.Reset(Loaded.Num());
		for (UQRItemDefinition* D : Loaded) AllDefs.Add(D);
	}
	RebuildResultList();
	ApplyInputMode();
	RefreshStatus();
}

void UQRCreativeBrowserWidget::Close()
{
	bOpen = false;
	SetVisibility(ESlateVisibility::Collapsed);
	ApplyInputMode();
}

void UQRCreativeBrowserWidget::ApplyInputMode()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;
	if (bOpen)
	{
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(Mode);
		PC->bShowMouseCursor = true;
	}
	else
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}
}

void UQRCreativeBrowserWidget::OnSearchChanged(const FText& /*Text*/)
{
	RebuildResultList();
}

void UQRCreativeBrowserWidget::RebuildResultList()
{
	if (!ResultsScroll) return;
	ResultsScroll->ClearChildren();

	const FString Filter = SearchBox ? SearchBox->GetText().ToString() : FString();
	TArray<UQRItemDefinition*> Source;
	Source.Reserve(AllDefs.Num());
	for (UQRItemDefinition* D : AllDefs) Source.Add(D);
	const TArray<UQRItemDefinition*> Filtered = UQRItemBrowserHelper::SearchItems(Source, Filter);

	for (UQRItemDefinition* Def : Filtered)
	{
		if (!Def) continue;

		UQRResultButton* Btn = WidgetTree->ConstructWidget<UQRResultButton>(
			UQRResultButton::StaticClass(),
			*FString::Printf(TEXT("ResBtn_%s"), *Def->ItemId.ToString()));
		Btn->CarriedDefinition = Def;
		Btn->OnResultRowClicked.AddDynamic(this, &UQRCreativeBrowserWidget::HandleResultRowClicked);

		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
			*FString::Printf(TEXT("ResLbl_%s"), *Def->ItemId.ToString()));
		const FString Name = Def->DisplayName.IsEmpty() ? Def->ItemId.ToString()
		                                                : Def->DisplayName.ToString();
		Label->SetText(FText::FromString(FString::Printf(TEXT("%s   [%s]"),
			*Name, *Def->ItemId.ToString())));
		Btn->AddChild(Label);

		ResultsScroll->AddChild(Btn);
	}

	if (Filtered.Num() == 0 && AllDefs.Num() == 0)
	{
		UTextBlock* Empty = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Empty"));
		Empty->SetText(FText::FromString(TEXT(
			"No items found. Make sure Project Settings → Asset Manager has a "
			"'QRItem' primary asset type entry scanning your item folder.")));
		ResultsScroll->AddChild(Empty);
	}
}

void UQRCreativeBrowserWidget::HandleResultRowClicked(UQRItemDefinition* Definition)
{
	Selected = Definition;
	RefreshStatus();
}

void UQRCreativeBrowserWidget::HandleSlotButtonClicked(int32 SlotIndex)
{
	if (!Hotbar || !Selected) return;
	Hotbar->AssignDefinitionToSlot(SlotIndex, Selected, /*Quantity*/ 1);
}

void UQRCreativeBrowserWidget::RefreshStatus()
{
	if (!StatusLabel) return;
	const FString Text = Selected
		? FString::Printf(TEXT("Selected: %s. Click a slot below (1-9) to assign."),
			Selected->DisplayName.IsEmpty()
				? *Selected->ItemId.ToString()
				: *Selected->DisplayName.ToString())
		: FString(TEXT("Select an item, then click a slot below."));
	StatusLabel->SetText(FText::FromString(Text));
}
