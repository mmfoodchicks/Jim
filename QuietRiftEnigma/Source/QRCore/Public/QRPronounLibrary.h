#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "QRTypes.h"
#include "QRPronounLibrary.generated.h"

/**
 * The fully expanded pronoun forms for one EQRPronouns value, used by
 * UQRPronounLibrary::Substitute and exposed to Blueprint authors who want
 * to do their own substitution.
 *
 * Form        He         She        They (singular)
 * ----        --         ---        ---
 * Subjective  he         she        they
 * Objective   him        her        them
 * PossDet     his hat    her hat    their hat       (used before a noun)
 * PossPro     it's his   it's hers  it's theirs     (standalone)
 * Reflexive   himself    herself    themself
 * bPlural     false      false      true            (verb agreement)
 *
 * Note: themself (singular) is preferred in modern usage over themselves —
 * Cyberpunk 2077, Baldur's Gate 3, Saints Row, and the Merriam-Webster
 * dictionary all use themself for the singular they.
 */
USTRUCT(BlueprintType)
struct QRCORE_API FQRPronounForms
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Pronouns")
	FString Subjective;

	UPROPERTY(BlueprintReadOnly, Category = "Pronouns")
	FString Objective;

	UPROPERTY(BlueprintReadOnly, Category = "Pronouns")
	FString PossessiveDet;

	UPROPERTY(BlueprintReadOnly, Category = "Pronouns")
	FString PossessivePro;

	UPROPERTY(BlueprintReadOnly, Category = "Pronouns")
	FString Reflexive;

	UPROPERTY(BlueprintReadOnly, Category = "Pronouns")
	bool bPlural = false;
};

/**
 * Static helper that does dialogue token substitution. Every NPC bark,
 * codex entry, and quest line passes through Substitute() so the player's
 * chosen pronouns + name appear in third-person text correctly.
 *
 * Token grammar — case is preserved through the substitution:
 *   {name}                       -> Identity.DisplayName
 *   {he}    {they}               -> "he"     / "she"    / "they"     (subjective)
 *   {him}   {them}               -> "him"    / "her"    / "them"     (objective)
 *   {his}   {their}              -> "his"    / "her"    / "their"    (possessive determiner)
 *   {hers}  {theirs}             -> "his"    / "hers"   / "theirs"   (possessive pronoun)
 *   {himself} {themself}         -> "himself"/ "herself"/ "themself" (reflexive)
 *
 * Verb agreement (singular they uses plural conjugation):
 *   {is}                         -> "is"     / "is"     / "are"
 *   {was}                        -> "was"    / "was"    / "were"
 *   {has}                        -> "has"    / "has"    / "have"
 *   {does}                       -> "does"   / "does"   / "do"
 *   {s}                          -> "s"      / "s"      / ""         (third-person -s drop)
 *
 * Capitalization rules (driven by the input token, not the output form):
 *   {he}     -> lowercase output:  he / she / they
 *   {He}     -> first letter cap:  He / She / They
 *   {HE}     -> all caps:          HE / SHE / THEY
 *
 * Unknown tokens are left in the output as-is so {literal text} survives
 * unchanged. This makes it safe to pass arbitrary user-authored content
 * through Substitute without worrying about token escaping.
 *
 * Examples:
 *   "{Name} took {his} hat."       (he)   -> "Jim took his hat."
 *   "{Name} took {his} hat."       (she)  -> "Jim took her hat."
 *   "{Name} took {his} hat."       (they) -> "Jim took their hat."
 *   "{He} {is} ready."             (they) -> "They are ready."
 *   "{He} walk{s} fast."           (they) -> "They walk fast."
 *   "The hat is {hers}."           (they) -> "The hat is theirs."
 *   "{Name} hurt {himself}."       (they) -> "Jim hurt themself."
 */
UCLASS()
class QRCORE_API UQRPronounLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Look up the canonical pronoun forms for an EQRPronouns value.
	UFUNCTION(BlueprintPure, Category = "Pronouns")
	static FQRPronounForms GetForms(EQRPronouns Pronouns);

	// Replace every {token} in InText with the matching form for Identity.
	// Pure text in/text out — no FText localization here. NPCs that need
	// localization should localize their templates separately and pass the
	// localized FString through Substitute.
	UFUNCTION(BlueprintPure, Category = "Pronouns")
	static FString Substitute(const FString& InText, const FQRPlayerIdentity& Identity);

	// Convenience overload that takes a raw EQRPronouns + name. Does not
	// support {name} substitution (pass DisplayName="" to skip it).
	UFUNCTION(BlueprintPure, Category = "Pronouns")
	static FString SubstituteRaw(const FString& InText, EQRPronouns Pronouns, const FString& DisplayName);

	// Default-fill an identity for a new save / new character. Used by the
	// character creator before the player has filled in any fields, and by
	// UQRSaveGameSystem when migrating a save without an identity record.
	UFUNCTION(BlueprintPure, Category = "Pronouns")
	static FQRPlayerIdentity MakeDefaultIdentity();
};
