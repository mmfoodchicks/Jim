using UnrealBuildTool;
using System.Collections.Generic;

public class QuietRiftEnigmaEditorTarget : TargetRules
{
	public QuietRiftEnigmaEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V4;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_4;
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
