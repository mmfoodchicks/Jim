using UnrealBuildTool;

public class QRCraftingResearch : ModuleRules
{
	public QRCraftingResearch(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"QRCore",
			"QRItems",
			"QRLogistics"
		});
	}
}
