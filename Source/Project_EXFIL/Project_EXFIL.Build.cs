// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Project_EXFIL : ModuleRules
{
	public Project_EXFIL(ReadOnlyTargetRules Target) : base(Target)
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
			"SlateCore",
			"Slate",
			"CommonUI",
			"CommonInput",
			"ModelViewViewModel",
			"GameplayTags",
			"GameplayAbilities",
			"GameplayTasks",
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
	}
}
