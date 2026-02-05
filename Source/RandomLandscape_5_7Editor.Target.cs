// Copyright Epic Games, Inc. All Rights Reserved.
// Version: 02.04.2026.23.37

using UnrealBuildTool;
using System.Collections.Generic;

public class RandomLandscape_5_7EditorTarget : TargetRules
{
	public RandomLandscape_5_7EditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
		
		// Include both runtime and editor modules
		ExtraModuleNames.Add("RandomLandscape_5_7");
		ExtraModuleNames.Add("RandomLandscape_5_7Editor");
	}
}
