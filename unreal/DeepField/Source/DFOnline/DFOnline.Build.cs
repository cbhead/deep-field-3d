using UnrealBuildTool;

// DF module (unreal/Build/modules.json). DF dependencies may only point at lower
// layers; unreal/Build/layering-check.py fails the build otherwise.
public class DFOnline : ModuleRules
{
	public DFOnline(ReadOnlyTargetRules Target) : base(Target)
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
			"CoreOnline",
			"OnlineServicesInterface",
			"OnlineServicesCommon",
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Json",
			"JsonUtilities",
			"AssetRegistry",     // UDFContentHash enumerates the imported DataTables
			"ApplicationCore",   // the primary platform user for the Null / EOS login
			"EngineSettings",    // the front-end map the backend returns to
		});
	}
}
