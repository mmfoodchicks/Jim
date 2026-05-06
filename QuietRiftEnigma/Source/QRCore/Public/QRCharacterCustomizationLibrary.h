#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "QRTypes.h"
#include "QRCharacterCustomizationLibrary.generated.h"

/**
 * Static helper for the character creator. Provides:
 *   - Realism clamps (defense-in-depth alongside the UPROPERTY meta clamps —
 *     useful when loading saves authored before a clamp was tightened, or
 *     when a modder pokes raw values past the UI bounds)
 *   - Body-type-aware defaults (Masculine vs Feminine starts at slightly
 *     different baselines for BustFullness, HipWidth, ShoulderWidth, etc.)
 *   - Skin / hair / eye color clamps that snap unrealistic colors back
 *     into a plausible human range
 *   - A simple BMI computation helper for UI feedback ("Your character
 *     reads as: lean / average / heavy / overweight")
 *
 * Realism rationale for body sliders: every 0..1 slider is hard-clamped at
 * both ends in code, AND the morph target authoring rule is that the 0.0
 * and 1.0 endpoints stay inside real human variance. A maxed BustFullness
 * morph target reads as "clearly busty" not "comic-book monstrous." The
 * data layer alone can't enforce that — it has to be respected in the
 * Blender authoring pass too.
 */
UCLASS()
class QRCORE_API UQRCharacterCustomizationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ── Defaults ─────────────────────────────────

	// New-character defaults that scale gently with body type. Pronouns are
	// NOT set here — that's a separate FQRPlayerIdentity field the player
	// chooses independently.
	UFUNCTION(BlueprintPure, Category = "Customization")
	static FQRCharacterAppearance MakeDefaultAppearance(EQRBodyType BodyType);

	UFUNCTION(BlueprintPure, Category = "Customization")
	static FQRBodyCustomization MakeDefaultBody(EQRBodyType BodyType);

	UFUNCTION(BlueprintPure, Category = "Customization")
	static FQRFaceCustomization MakeDefaultFace();

	// ── Realism clamps ───────────────────────────

	// Snap every body slider into its UPROPERTY-declared realism range.
	// Idempotent — clamps a second time produce the same result.
	UFUNCTION(BlueprintCallable, Category = "Customization")
	static void ClampBodyToRealism(UPARAM(ref) FQRBodyCustomization& Body);

	UFUNCTION(BlueprintCallable, Category = "Customization")
	static void ClampFaceToRealism(UPARAM(ref) FQRFaceCustomization& Face);

	// Clamp skin tone to a plausible human range (R 0.30–0.95, G 0.20–0.85,
	// B 0.15–0.80) — rejects green / blue / purple skin a modder might set.
	UFUNCTION(BlueprintCallable, Category = "Customization")
	static void ClampSkinToneToRealism(UPARAM(ref) FLinearColor& SkinTone);

	// Clamp hair color to the realistic eumelanin / pheomelanin range plus
	// natural-grey. Bleached white and dyed unnatural colors will snap back.
	UFUNCTION(BlueprintCallable, Category = "Customization")
	static void ClampHairColorToRealism(UPARAM(ref) FLinearColor& HairColor);

	// Clamp eye color to a plausible human range (covers brown / blue /
	// green / hazel / grey). Excludes red / yellow / white sclera tints.
	UFUNCTION(BlueprintCallable, Category = "Customization")
	static void ClampEyeColorToRealism(UPARAM(ref) FLinearColor& EyeColor);

	// One-call clamp for the whole appearance bundle. Idempotent.
	UFUNCTION(BlueprintCallable, Category = "Customization")
	static void ClampAppearanceToRealism(UPARAM(ref) FQRCharacterAppearance& Appearance);

	// ── Derived metrics ──────────────────────────

	// BMI (kg / m^2). Useful for UI feedback and for downstream gameplay
	// hooks that care about body mass (carry weight bonus / penalty later).
	UFUNCTION(BlueprintPure, Category = "Customization")
	static float ComputeBMI(const FQRBodyCustomization& Body);

	// Returns a short editor-friendly category for UI display:
	// "Underweight" / "Lean" / "Average" / "Heavy" / "Overweight".
	UFUNCTION(BlueprintPure, Category = "Customization")
	static FString DescribeBodyMass(const FQRBodyCustomization& Body);
};
