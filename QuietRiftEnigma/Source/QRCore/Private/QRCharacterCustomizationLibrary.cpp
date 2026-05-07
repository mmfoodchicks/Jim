#include "QRCharacterCustomizationLibrary.h"

namespace
{
	// ── Realism range constants ─────────────────
	// These mirror the ClampMin/ClampMax from the UPROPERTY meta tags and
	// are duplicated here as a defense-in-depth so saves loaded with
	// out-of-range values get snapped on the first read regardless of
	// whether the property reflection clamps fired.
	constexpr float kHeightMinCm = 145.0f;
	constexpr float kHeightMaxCm = 205.0f;
	constexpr float kWeightMinKg = 40.0f;
	constexpr float kWeightMaxKg = 130.0f;

	// Skin-tone realism envelope. Wider than the curated palette so modder
	// edits inside the range survive; outside this range we snap.
	constexpr float kSkinRMin = 0.30f, kSkinRMax = 0.95f;
	constexpr float kSkinGMin = 0.20f, kSkinGMax = 0.85f;
	constexpr float kSkinBMin = 0.15f, kSkinBMax = 0.80f;

	// Hair color: black through blonde + greys. Excludes saturated reds /
	// blues / greens. The realistic red end (auburn) lands inside this box.
	constexpr float kHairRMin = 0.04f, kHairRMax = 0.95f;
	constexpr float kHairGMin = 0.04f, kHairGMax = 0.85f;
	constexpr float kHairBMin = 0.04f, kHairBMax = 0.75f;

	// Eye color: brown / blue / green / hazel / grey. Excludes saturated
	// red / yellow.
	constexpr float kEyeRMin = 0.10f, kEyeRMax = 0.65f;
	constexpr float kEyeGMin = 0.15f, kEyeGMax = 0.70f;
	constexpr float kEyeBMin = 0.10f, kEyeBMax = 0.85f;

	void ClampUnit(float& V) { V = FMath::Clamp(V, 0.0f, 1.0f); }

	void ClampColor(FLinearColor& C,
	                float RMin, float RMax,
	                float GMin, float GMax,
	                float BMin, float BMax)
	{
		C.R = FMath::Clamp(C.R, RMin, RMax);
		C.G = FMath::Clamp(C.G, GMin, GMax);
		C.B = FMath::Clamp(C.B, BMin, BMax);
		// Alpha kept at 1 so VFX never see a transparent skin/hair/eye.
		C.A = 1.0f;
	}
}

// ── Defaults ─────────────────────────────────────────────────────────────────

FQRBodyCustomization UQRCharacterCustomizationLibrary::MakeDefaultBody(EQRBodyType BodyType)
{
	FQRBodyCustomization B;
	B.BodyType = BodyType;
	if (BodyType == EQRBodyType::Feminine)
	{
		B.HeightCm       = 162.0f;
		B.WeightKg       = 60.0f;
		B.Muscularity    = 0.30f;
		B.BodyFat        = 0.45f;
		B.ShoulderWidth  = 0.40f;
		B.WaistSize      = 0.45f;
		B.HipWidth       = 0.60f;
		B.BustFullness   = 0.45f;
		B.GluteFullness  = 0.55f;
	}
	else // Masculine
	{
		B.HeightCm       = 175.0f;
		B.WeightKg       = 75.0f;
		B.Muscularity    = 0.45f;
		B.BodyFat        = 0.35f;
		B.ShoulderWidth  = 0.60f;
		B.WaistSize      = 0.50f;
		B.HipWidth       = 0.45f;
		B.BustFullness   = 0.00f;
		B.GluteFullness  = 0.45f;
	}
	return B;
}

FQRFaceCustomization UQRCharacterCustomizationLibrary::MakeDefaultFace()
{
	// Neutral baseline — every slider at 0.5. The character creator UI
	// will offer presets that bias these on top of the baseline.
	return FQRFaceCustomization();
}

FQRCharacterAppearance UQRCharacterCustomizationLibrary::MakeDefaultAppearance(EQRBodyType BodyType)
{
	FQRCharacterAppearance A;
	A.Body = MakeDefaultBody(BodyType);
	A.Face = MakeDefaultFace();
	// Skin / hair / eye colors keep their struct defaults (warm tan, dark
	// brown hair, grey-blue eyes). Character creator UI lets the player
	// retune everything.
	return A;
}

// ── Realism clamps ───────────────────────────────────────────────────────────

void UQRCharacterCustomizationLibrary::ClampBodyToRealism(FQRBodyCustomization& B)
{
	B.HeightCm       = FMath::Clamp(B.HeightCm, kHeightMinCm, kHeightMaxCm);
	B.WeightKg       = FMath::Clamp(B.WeightKg, kWeightMinKg, kWeightMaxKg);
	ClampUnit(B.Muscularity);
	ClampUnit(B.BodyFat);
	ClampUnit(B.ShoulderWidth);
	ClampUnit(B.WaistSize);
	ClampUnit(B.HipWidth);
	ClampUnit(B.BustFullness);
	ClampUnit(B.GluteFullness);
}

void UQRCharacterCustomizationLibrary::ClampFaceToRealism(FQRFaceCustomization& F)
{
	ClampUnit(F.JawWidth);
	ClampUnit(F.JawLength);
	ClampUnit(F.ChinProminence);
	ClampUnit(F.CheekboneHeight);
	ClampUnit(F.CheekboneWidth);
	ClampUnit(F.BrowRidge);
	ClampUnit(F.EyeSpacing);
	ClampUnit(F.EyeSize);
	ClampUnit(F.NoseLength);
	ClampUnit(F.NoseWidth);
	ClampUnit(F.NoseTipShape);
	ClampUnit(F.LipFullness);
	ClampUnit(F.MouthWidth);
	ClampUnit(F.ForeheadHeight);
}

void UQRCharacterCustomizationLibrary::ClampSkinToneToRealism(FLinearColor& SkinTone)
{
	ClampColor(SkinTone, kSkinRMin, kSkinRMax, kSkinGMin, kSkinGMax, kSkinBMin, kSkinBMax);
}

void UQRCharacterCustomizationLibrary::ClampHairColorToRealism(FLinearColor& HairColor)
{
	ClampColor(HairColor, kHairRMin, kHairRMax, kHairGMin, kHairGMax, kHairBMin, kHairBMax);
}

void UQRCharacterCustomizationLibrary::ClampEyeColorToRealism(FLinearColor& EyeColor)
{
	ClampColor(EyeColor, kEyeRMin, kEyeRMax, kEyeGMin, kEyeGMax, kEyeBMin, kEyeBMax);
}

void UQRCharacterCustomizationLibrary::ClampAppearanceToRealism(FQRCharacterAppearance& A)
{
	ClampBodyToRealism(A.Body);
	ClampFaceToRealism(A.Face);
	ClampSkinToneToRealism(A.SkinTone);
	ClampHairColorToRealism(A.HairColor);
	ClampEyeColorToRealism(A.EyeColor);
	A.HairStyleIndex   = FMath::Max(0, A.HairStyleIndex);
	A.FacialHairIndex  = FMath::Max(0, A.FacialHairIndex);
}

// ── Derived metrics ──────────────────────────────────────────────────────────

float UQRCharacterCustomizationLibrary::ComputeBMI(const FQRBodyCustomization& B)
{
	const float HeightM = FMath::Max(B.HeightCm * 0.01f, 0.01f);
	return B.WeightKg / (HeightM * HeightM);
}

FString UQRCharacterCustomizationLibrary::DescribeBodyMass(const FQRBodyCustomization& B)
{
	const float BMI = ComputeBMI(B);
	// WHO BMI buckets, relabeled in plain game-UI tone:
	//   <18.5   Slim       (medical: underweight — softened label)
	//   <25.0   Average    (medical: normal)
	//   <30.0   Heavy      (medical: overweight)
	//   <35.0   Overweight (medical: obese class I)
	//   >=35.0  Heavyset   (medical: obese class II+ — softened label)
	if (BMI < 18.5f) return TEXT("Slim");
	if (BMI < 25.0f) return TEXT("Average");
	if (BMI < 30.0f) return TEXT("Heavy");
	if (BMI < 35.0f) return TEXT("Overweight");
	return TEXT("Heavyset");
}
