using UnrealBuildTool;
using System.Collections.Generic;

public class QuietRiftEnigmaTarget : TargetRules
{
	public QuietRiftEnigmaTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion  = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[]
		{
			"QuietRiftEnigma",
			"QRCore",
			"QRItems",
			"QRSurvival",
			"QRLogistics",
			"QRCraftingResearch",
			"QRColonyAI",
			"QRCombatThreat",
			"QRSaveNet",
			"QRUI"
		});
	}
}
