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
		
		// Project root: .../Source/RandomLandscape_5_7/  ->  .../RandomLandscape_5_7/
		string ProjectRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", ".."));

		string FastNoise2Dir = Path.Combine(ProjectRoot, "ThirdParty", "FastNoise2");
		string FastSIMDDir   = Path.Combine(ProjectRoot, "ThirdParty", "FastSIMD");

// Includes
		PublicIncludePaths.Add(Path.Combine(FastNoise2Dir, "include"));
		PublicIncludePaths.Add(Path.Combine(FastSIMDDir, "include"));

// Tell FastNoise headers we are linking a static lib (prevents dllimport issues)
		PublicDefinitions.Add("FASTNOISE_STATIC_LIB");

// Link libs (NOTE the /lib subfolder!)
		PublicAdditionalLibraries.Add(Path.Combine(FastNoise2Dir, "build", "Release", "lib", "FastNoise.lib"));

// These two are commonly needed as well for the node/dispatch system:
		PublicAdditionalLibraries.Add(Path.Combine(FastNoise2Dir, "build", "src", "FastSIMD_FastNoise.dir", "Release", "FastSIMD_FastNoise.lib"));
		PublicAdditionalLibraries.Add(Path.Combine(FastNoise2Dir, "build", "_deps", "fastsimd-build", "FastSIMD.dir", "Release", "FastSIMD.lib"));

// Keep RTTI enabled for this module
		bUseRTTI = true;

		PublicIncludePaths.AddRange(new string[] {
			"RandomLandscape_5_7",
			"RandomLandscape_5_7/MapGeneration",
			"RandomLandscape_5_7/ProceduralLandscape",
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
			"RandomLandscape_5_7/UI",
		});
		
		bUseRTTI = true;

		// Slate UI support for UMG widgets
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
