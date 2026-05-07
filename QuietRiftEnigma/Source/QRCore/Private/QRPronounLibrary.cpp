#include "QRPronounLibrary.h"

namespace
{
	// Build the three canonical forms once, return by const ref. Static const
	// locals are constructed thread-safely on first call (C++11 magic statics).
	const FQRPronounForms& GetFormsCached(EQRPronouns P)
	{
		static const FQRPronounForms HeForms = []
		{
			FQRPronounForms F;
			F.Subjective    = TEXT("he");
			F.Objective     = TEXT("him");
			F.PossessiveDet = TEXT("his");
			F.PossessivePro = TEXT("his");
			F.Reflexive     = TEXT("himself");
			F.bPlural       = false;
			return F;
		}();
		static const FQRPronounForms SheForms = []
		{
			FQRPronounForms F;
			F.Subjective    = TEXT("she");
			F.Objective     = TEXT("her");
			F.PossessiveDet = TEXT("her");
			F.PossessivePro = TEXT("hers");
			F.Reflexive     = TEXT("herself");
			F.bPlural       = false;
			return F;
		}();
		static const FQRPronounForms TheyForms = []
		{
			FQRPronounForms F;
			F.Subjective    = TEXT("they");
			F.Objective     = TEXT("them");
			F.PossessiveDet = TEXT("their");
			F.PossessivePro = TEXT("theirs");
			F.Reflexive     = TEXT("themself");
			F.bPlural       = true;
			return F;
		}();

		switch (P)
		{
			case EQRPronouns::He:   return HeForms;
			case EQRPronouns::She:  return SheForms;
			case EQRPronouns::They: return TheyForms;
			default:                return TheyForms; // safest neutral fallback
		}
	}

	// Determine the casing intent of an input token name like "He" / "HE" / "he"
	// so we can match it on the output. Returns 0 = lowercase, 1 = first-cap,
	// 2 = all-caps.
	enum class ECase : uint8 { Lower, FirstCap, AllCaps };

	ECase DetectCase(const FString& Token)
	{
		if (Token.IsEmpty())                 return ECase::Lower;
		if (!FChar::IsUpper(Token[0]))       return ECase::Lower;
		if (Token.Len() == 1)                return ECase::FirstCap;
		// First char upper. If any *letter* in the rest is lower, it's first-cap.
		for (int32 i = 1; i < Token.Len(); ++i)
		{
			TCHAR c = Token[i];
			if (FChar::IsAlpha(c) && FChar::IsLower(c))
				return ECase::FirstCap;
		}
		return ECase::AllCaps;
	}

	void ApplyCase(FString& S, ECase Casing)
	{
		if (S.IsEmpty()) return;
		switch (Casing)
		{
			case ECase::Lower:
				// Forms are stored lowercase already; nothing to do.
				return;
			case ECase::FirstCap:
				S[0] = FChar::ToUpper(S[0]);
				return;
			case ECase::AllCaps:
				S = S.ToUpper();
				return;
		}
	}

	// Build the lowercase token → replacement table for one Identity. Cached
	// per call (cheap — 16 entries, all FString-on-stack).
	TMap<FString, FString> BuildTokenTable(const FQRPlayerIdentity& Identity)
	{
		const FQRPronounForms& F = GetFormsCached(Identity.Pronouns);

		TMap<FString, FString> M;
		M.Reserve(20);

		M.Add(TEXT("name"), Identity.DisplayName.IsEmpty() ? FString(TEXT("Survivor")) : Identity.DisplayName);

		// Subjective / objective / possessive / reflexive — both {he}-family
		// and {they}-family token names map to the same forms so writers can
		// use whichever convention they prefer.
		M.Add(TEXT("he"),       F.Subjective);
		M.Add(TEXT("they"),     F.Subjective);
		M.Add(TEXT("him"),      F.Objective);
		M.Add(TEXT("them"),     F.Objective);
		M.Add(TEXT("his"),      F.PossessiveDet);
		M.Add(TEXT("their"),    F.PossessiveDet);
		M.Add(TEXT("hers"),     F.PossessivePro);
		M.Add(TEXT("theirs"),   F.PossessivePro);
		M.Add(TEXT("himself"),  F.Reflexive);
		M.Add(TEXT("themself"), F.Reflexive);

		// Verb agreement — "they" uses plural forms.
		M.Add(TEXT("is"),   F.bPlural ? TEXT("are")  : TEXT("is"));
		M.Add(TEXT("was"),  F.bPlural ? TEXT("were") : TEXT("was"));
		M.Add(TEXT("has"),  F.bPlural ? TEXT("have") : TEXT("has"));
		M.Add(TEXT("does"), F.bPlural ? TEXT("do")   : TEXT("does"));
		M.Add(TEXT("s"),    F.bPlural ? TEXT("")     : TEXT("s"));

		return M;
	}

	FString SubstituteCore(const FString& InText, const FQRPlayerIdentity& Identity)
	{
		if (InText.IsEmpty()) return InText;
		const TMap<FString, FString> Tokens = BuildTokenTable(Identity);

		FString Out;
		Out.Reserve(InText.Len() + 16);

		const int32 N = InText.Len();
		int32 i = 0;
		while (i < N)
		{
			const TCHAR c = InText[i];
			if (c != TEXT('{'))
			{
				Out.AppendChar(c);
				++i;
				continue;
			}

			// Try to find the matching closing brace. If none, treat the '{'
			// literally and continue.
			const int32 EndIdx = InText.Find(TEXT("}"), ESearchCase::CaseSensitive,
			                                  ESearchDir::FromStart, i + 1);
			if (EndIdx == INDEX_NONE)
			{
				Out.AppendChar(c);
				++i;
				continue;
			}

			const FString Token       = InText.Mid(i + 1, EndIdx - i - 1);
			const FString TokenLower  = Token.ToLower();
			const FString* Replacement = Tokens.Find(TokenLower);

			if (Replacement)
			{
				FString Output = *Replacement;
				ApplyCase(Output, DetectCase(Token));
				Out.Append(Output);
				i = EndIdx + 1;
			}
			else
			{
				// Unknown token — leave it alone so user-authored {literal}
				// content survives untouched. Just emit the '{' and keep
				// scanning; we'll re-encounter '}' and emit it normally.
				Out.AppendChar(c);
				++i;
			}
		}

		return Out;
	}
}

FQRPronounForms UQRPronounLibrary::GetForms(EQRPronouns Pronouns)
{
	return GetFormsCached(Pronouns);
}

FString UQRPronounLibrary::Substitute(const FString& InText, const FQRPlayerIdentity& Identity)
{
	return SubstituteCore(InText, Identity);
}

FString UQRPronounLibrary::SubstituteRaw(const FString& InText, EQRPronouns Pronouns, const FString& DisplayName)
{
	FQRPlayerIdentity Identity;
	Identity.Pronouns    = Pronouns;
	Identity.DisplayName = DisplayName;
	return SubstituteCore(InText, Identity);
}

FQRPlayerIdentity UQRPronounLibrary::MakeDefaultIdentity()
{
	FQRPlayerIdentity Identity;
	Identity.DisplayName = FString(TEXT("Survivor"));
	Identity.Pronouns    = EQRPronouns::They;
	return Identity;
}
