using UnrealBuildTool;

// DF module (unreal/Build/modules.json). DF dependencies may only point at lower
// layers; unreal/Build/layering-check.py fails the build otherwise.
public class DFPlayer : ModuleRules
{
	public DFPlayer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		PublicDependencyModuleNames.AddRange(new string[] {
			"DFCore",
			"DFGameplay",
			"DFWorld",
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"GameplayAbilities",
			"EnhancedInput",
			"InputCore",
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"ModularGameplay",
		});

		if (Target.bBuildEditor)
		{
			// DF.Func.Weapon.RifleKillsTestlaneEnemy opens a map and runs it in PIE: FEndPlayMapCommand (Tests/AutomationEditorCommon.h).
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
