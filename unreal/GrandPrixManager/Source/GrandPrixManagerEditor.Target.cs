using UnrealBuildTool;

public class GrandPrixManagerEditorTarget : TargetRules
{
	public GrandPrixManagerEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("GrandPrixManager");

		// Xcode 26/27 (clang récent) est plus strict qu'Unreal 5.5 : on coupe les nouveaux avertissements traités comme erreurs.
		if (Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.IOS)
		{
			bOverrideBuildEnvironment = true;
			AdditionalCompilerArguments = "-Wno-unknown-warning-option -Wno-deprecated-literal-operator -Wno-nontrivial-memaccess -Wno-invalid-specialization -Wno-character-conversion -Wno-missing-template-arg-list-after-template-kw -Wno-unused-but-set-variable -Wno-deprecated-declarations";
		}
	}
}
