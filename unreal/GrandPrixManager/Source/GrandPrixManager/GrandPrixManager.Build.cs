using UnrealBuildTool;

public class GrandPrixManager : ModuleRules
{
	public GrandPrixManager(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			"ChaosVehicles", "PhysicsCore", "ProceduralMeshComponent",
			"UMG", "Slate", "SlateCore", "CommonUI", "Json", "JsonUtilities"
		});
	}
}
