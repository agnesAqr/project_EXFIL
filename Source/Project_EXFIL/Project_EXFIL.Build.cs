// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Project_EXFIL : ModuleRules
{
	public Project_EXFIL(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			// Day 1
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			// Day 2
			"UMG",
			"SlateCore",
			"Slate",
			"CommonUI",
			"CommonInput",
			"ModelViewViewModel",
			"GameplayTags"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"Project_EXFIL",
			"Project_EXFIL/Core",
			"Project_EXFIL/Inventory",
			"Project_EXFIL/UI",
			"Project_EXFIL/Variant_Platforming",
			"Project_EXFIL/Variant_Platforming/Animation",
			"Project_EXFIL/Variant_Combat",
			"Project_EXFIL/Variant_Combat/AI",
			"Project_EXFIL/Variant_Combat/Animation",
			"Project_EXFIL/Variant_Combat/Gameplay",
			"Project_EXFIL/Variant_Combat/Interfaces",
			"Project_EXFIL/Variant_Combat/UI",
			"Project_EXFIL/Variant_SideScrolling",
			"Project_EXFIL/Variant_SideScrolling/AI",
			"Project_EXFIL/Variant_SideScrolling/Gameplay",
			"Project_EXFIL/Variant_SideScrolling/Interfaces",
			"Project_EXFIL/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
