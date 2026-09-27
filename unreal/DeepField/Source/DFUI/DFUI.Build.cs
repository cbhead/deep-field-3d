using UnrealBuildTool;

// DF module (unreal/Build/modules.json). DF dependencies may only point at lower
// layers; unreal/Build/layering-check.py fails the build otherwise.
public class DFUI : ModuleRules
{
	public DFUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		PublicDependencyModuleNames.AddRange(new string[] {
			"DFCore",
			"DFMatch",
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"UMG",
			"Slate",
			"SlateCore",
			"CommonUI",
			"CommonInput",
			"ModelViewViewModel",
			"EnhancedInput",
			"InputCore",
		});

		// The real feed (ViewModels/DFMatchStateFeed) reads WS-06's economy component and the hero's
		// attribute set (DFGameplay, GAS) and the hero pawn (DFPlayer); its tests drive a real wave
		// director (DFEnemies), as DFMatch's own do. All are lower layers.
		PrivateDependencyModuleNames.AddRange(new string[] {
			"DFGameplay",
			"DFPlayer",
			"DFEnemies",
			"GameplayAbilities",
		});
	}
}
