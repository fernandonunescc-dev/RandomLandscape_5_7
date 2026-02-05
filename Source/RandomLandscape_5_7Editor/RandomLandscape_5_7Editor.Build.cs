// RandomLandscape_5_7Editor.Build.cs
// Build rules for the editor module - handles Details panel customization
// Version: 02.04.2026.23.37

using UnrealBuildTool;

public class RandomLandscape_5_7Editor : ModuleRules
{
	public RandomLandscape_5_7Editor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// ==================== Dependencies ====================
		
		// Core engine modules
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore"
		});
		
		// Editor-specific modules (only available in editor builds)
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// Our runtime module (contains ABiomeDataGenerationActor)
			"RandomLandscape_5_7",
			
			// Editor framework
			"UnrealEd",
			
			// Property/Details panel customization
			"PropertyEditor",
			
			// Slate UI framework
			"Slate",
			"SlateCore",
			
			// Editor styling
			"EditorStyle",
			
			// For EditorWidgets (thumbnails, asset pickers, etc.)
			"EditorWidgets"
		});
		
		// Include paths for the runtime module's headers
		PublicIncludePaths.AddRange(new string[]
		{
			"RandomLandscape_5_7",
			"RandomLandscape_5_7/MapGeneration"
		});
	}
}
