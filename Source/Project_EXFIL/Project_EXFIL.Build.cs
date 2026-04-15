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
			"GameplayTags",
			// Day 4
			"GameplayAbilities",
			"GameplayTasks",
			// Replication
			"NetCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"Project_EXFIL",
			"Project_EXFIL/Core",
			"Project_EXFIL/Data",
			"Project_EXFIL/Inventory",
			"Project_EXFIL/UI",
			"Project_EXFIL/GAS",
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
