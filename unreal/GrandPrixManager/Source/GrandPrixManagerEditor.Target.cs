using UnrealBuildTool;

public class GrandPrixManagerEditorTarget : TargetRules
{
	public GrandPrixManagerEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("GrandPrixManager");
	}
}
