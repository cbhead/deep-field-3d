using UnrealBuildTool;

// DF module (unreal/Build/modules.json). DF dependencies may only point at lower
// layers; unreal/Build/layering-check.py fails the build otherwise.
public class DFMatch : ModuleRules
{
	public DFMatch(ReadOnlyTargetRules Target) : base(Target)
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
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"GameplayAbilities",
			"NetCore",
			"EnhancedInput",
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			// (none)
		});

		if (Target.bBuildEditor)
		{
			// DF.Func.Tower.KillsTestlaneEnemies opens a map and runs it in PIE: FEndPlayMapCommand (Tests/AutomationEditorCommon.h).
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
