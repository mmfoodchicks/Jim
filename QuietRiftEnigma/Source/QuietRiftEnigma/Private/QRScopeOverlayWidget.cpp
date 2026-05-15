#include "QRScopeOverlayWidget.h"
#include "QRFPViewComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/Image.h"


UQRScopeOverlayWidget::UQRScopeOverlayWidget(const FObjectInitializer& OI)
	: Super(OI)
{
}


TSharedRef<SWidget> UQRScopeOverlayWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		WidgetTree->RootWidget = Canvas;

		auto MakeLetterbox = [&](FAnchors A, FVector2D Align, FMargin Offsets)
			-> UBorder*
		{
			UBorder* B = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			B->SetBrushColor(FLinearColor(0, 0, 0, 1.0f));
			UCanvasPanelSlot* S = Canvas->AddChildToCanvas(B);
			if (S)
			{
				S->SetAnchors(A);
				S->SetAlignment(Align);
				S->SetOffsets(Offsets);
			}
			return B;
		};

		// Outer black frames: top/bottom (full width × 30% height each) and
		// left/right (20% × full middle band). Leaves a roughly square
		// porthole in the center — the "scope view" area.
		LetterboxTop    = MakeLetterbox(FAnchors(0.0f, 0.0f, 1.0f, 0.30f), FVector2D(0, 0), FMargin(0));
		LetterboxBottom = MakeLetterbox(FAnchors(0.0f, 0.70f, 1.0f, 1.0f), FVector2D(0, 0), FMargin(0));
		LetterboxLeft   = MakeLetterbox(FAnchors(0.0f, 0.30f, 0.20f, 0.70f), FVector2D(0, 0), FMargin(0));
		LetterboxRight  = MakeLetterbox(FAnchors(0.80f, 0.30f, 1.0f, 0.70f), FVector2D(0, 0), FMargin(0));

		// Reticle — a thin cross drawn via two centered Borders. Image
		// would need a Texture2D asset; Borders work without authoring.
		auto MakeReticleLine = [&](float W, float H) -> UBorder*
		{
			UBorder* L = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			L->SetBrushColor(FLinearColor(0.0f, 0.95f, 0.30f, 0.95f));
			UCanvasPanelSlot* S = Canvas->AddChildToCanvas(L);
			if (S)
			{
				S->SetAnchors(FAnchors(0.5f, 0.5f));
				S->SetAlignment(FVector2D(0.5f, 0.5f));
				S->SetSize(FVector2D(W, H));
			}
			return L;
		};
		MakeReticleLine(2.0f, 24.0f);   // vertical
		MakeReticleLine(24.0f, 2.0f);   // horizontal
	}
	return Super::RebuildWidget();
}


void UQRScopeOverlayWidget::Bind(UQRFPViewComponent* InView)
{
	View = InView;
	SetActiveVisuals(false);
}


void UQRScopeOverlayWidget::NativeTick(const FGeometry& InGeometry, float DeltaTime)
{
	Super::NativeTick(InGeometry, DeltaTime);
	const bool bActive = View && View->bScopeAvailable && View->IsADS();
	if (bActive != bWasActive)
	{
		bWasActive = bActive;
		SetActiveVisuals(bActive);
	}
}


void UQRScopeOverlayWidget::SetActiveVisuals(bool bActive)
{
	const ESlateVisibility Vis = bActive
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed;
	if (LetterboxTop)    LetterboxTop->SetVisibility(Vis);
	if (LetterboxBottom) LetterboxBottom->SetVisibility(Vis);
	if (LetterboxLeft)   LetterboxLeft->SetVisibility(Vis);
	if (LetterboxRight)  LetterboxRight->SetVisibility(Vis);
}
