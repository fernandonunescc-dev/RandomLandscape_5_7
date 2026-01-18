// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class RandomLandscape_5_7 : ModuleRules
{
	public RandomLandscape_5_7(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"ProceduralMeshComponent"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
		
		string ProjectRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", ".."));

		string FastNoise2Dir = Path.Combine(ProjectRoot, "ThirdParty", "FastNoise2");
		string FastSIMDDir   = Path.Combine(ProjectRoot, "ThirdParty", "FastSIMD");

		// FastNoise2 headers: <FastNoise/FastNoise.h>
		PublicIncludePaths.Add(Path.Combine(FastNoise2Dir, "include"));

		// FastSIMD headers: "FastSIMD/DispatchClass.h"
		PublicIncludePaths.Add(Path.Combine(FastSIMDDir, "include"));

		// FastNoise2 often needs RTTI in UE builds (dynamic_cast in the node system)
		bUseRTTI = true;

		PublicIncludePaths.AddRange(new string[] {
			"RandomLandscape_5_7",
			"RandomLandscape_5_7/Variant_Platforming",
			"RandomLandscape_5_7/Variant_Platforming/Animation",
			"RandomLandscape_5_7/Variant_Combat",
			"RandomLandscape_5_7/Variant_Combat/AI",
			"RandomLandscape_5_7/Variant_Combat/Animation",
			"RandomLandscape_5_7/Variant_Combat/Gameplay",
			"RandomLandscape_5_7/Variant_Combat/Interfaces",
			"RandomLandscape_5_7/Variant_Combat/UI",
			"RandomLandscape_5_7/Variant_SideScrolling",
			"RandomLandscape_5_7/Variant_SideScrolling/AI",
			"RandomLandscape_5_7/Variant_SideScrolling/Gameplay",
			"RandomLandscape_5_7/Variant_SideScrolling/Interfaces",
			"RandomLandscape_5_7/Variant_SideScrolling/UI",
		});
		
		bUseRTTI = true;

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
