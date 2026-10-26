// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LastCheckOut : ModuleRules
{
	public LastCheckOut(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] 
			{ "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "AIModule", 
				"NavigationSystem", "GameplayTasks", "UMG", "Slate", "SlateCore", "MoviePlayer", 
				"GameplayTags","PhysicsCore", "Slate", "SlateCore" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });

		PublicIncludePaths.AddRange(new string[]
		{
			"LastCheckOut", // 이게 없으면 헤더파일 인클루드시 경로를 찾지 못함.
		});
		
		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
