using UnrealBuildTool;
using System.Collections.Generic;

public class DeepFieldEditorTarget : TargetRules
{
	public DeepFieldEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] {
			"DFCore", "DFGameplay", "DFPlayer", "DFTowers", "DFEnemies", "DFWorld",
			"DFVehicles", "DFMatch", "DFUI", "DFOnline", "DFAudio", "DFVfx",
			"DFEditor", "DFTests", "DFContentPipeline"
		});
	}
}
