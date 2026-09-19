using UnrealBuildTool;

// DF module (unreal/Build/modules.json). DF dependencies may only point at lower
// layers; unreal/Build/layering-check.py fails the build otherwise.
public class DFTests : ModuleRules
{
	public DFTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		PublicDependencyModuleNames.AddRange(new string[] {
			"DFCore",
			"DFGameplay",
			"DFPlayer",
			"DFTowers",
			"DFEnemies",
			"DFWorld",
			"DFVehicles",
			"DFMatch",
			"DFUI",
			"DFOnline",
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"FunctionalTesting",
			"AutomationController",
			"GameplayAbilities",
		});
	}
}
