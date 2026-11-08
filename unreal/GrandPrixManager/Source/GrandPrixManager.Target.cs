using UnrealBuildTool;

public class GrandPrixManagerTarget : TargetRules
{
	public GrandPrixManagerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("GrandPrixManager");
	}
}
