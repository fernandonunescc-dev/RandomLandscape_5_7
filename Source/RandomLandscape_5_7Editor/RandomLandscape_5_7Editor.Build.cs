// RandomLandscape_5_7Editor.Build.cs
// Build rules for the editor module - handles Details panel customization

using UnrealBuildTool;

public class RandomLandscape_5_7Editor : ModuleRules
{
	public RandomLandscape_5_7Editor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Core engine modules
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore"
		});

		// Editor-specific modules
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// Our runtime module (contains AWorldGenerationActor)
			"RandomLandscape_5_7",

			// Editor framework
			"UnrealEd",

			// Property/Details panel customization
			"PropertyEditor",

			// Slate UI framework
			"Slate",
			"SlateCore"
		});

		// Include paths for the runtime module's headers
		PublicIncludePaths.AddRange(new string[]
		{
			"RandomLandscape_5_7",
			"RandomLandscape_5_7/MapGeneration"
		});
	}
}
