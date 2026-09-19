using UnrealBuildTool;

// DF module (unreal/Build/modules.json). DF dependencies may only point at lower
// layers; unreal/Build/layering-check.py fails the build otherwise.
public class DFEditor : ModuleRules
{
	public DFEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		PublicDependencyModuleNames.AddRange(new string[] {
			"DFCore",
			"DFGameplay",
			"DFWorld",
			"DFTowers",
			"DFEnemies",
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"UnrealEd",
			"AssetRegistry",
			"EditorSubsystem",
			"Json",
			"JsonUtilities",
			"DataValidation",
		});
	}
}
