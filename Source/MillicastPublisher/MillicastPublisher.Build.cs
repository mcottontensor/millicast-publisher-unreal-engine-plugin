// Copyright Millicast 2022. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	using System.IO;

	public class MillicastPublisher: ModuleRules
	{
		public MillicastPublisher(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
			PrivatePCHHeaderFile = "Private/PCH.h";


			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"Media",
				});

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"MediaAssets",
					"OpenSSL",
					"TimeManagement",
					"WebRTC",
					"RenderCore",
					"AudioCaptureCore"
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Engine",
					"MediaUtils",
					"MediaIOCore",
					"Projects",
					"Slate",
					"SlateCore",
					"AudioMixer",
					"WebSockets",
					"HTTP",
					"Json",
					"SSL",
					"RHI",
					"HeadMountedDisplay",
					"CinematicCamera",
					"InputCore",
					"AudioPlatformConfiguration",
					"AVEncoder",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Media",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"MillicastPublisher/Private",
				});

			var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);
			PrivateDependencyModuleNames.AddRange(new string[] { "CUDA", "VulkanRHI", "nvEncode" });
			//PrivateIncludePathModuleNames.Add("VulkanRHI");
			PrivateIncludePaths.Add(Path.Combine(EngineDir, "Source/Runtime/VulkanRHI/Private"));
			AddEngineThirdPartyPrivateStaticDependencies(Target, "Vulkan");
			if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
			{
				PrivateIncludePaths.Add(Path.Combine(EngineDir, "Source/Runtime/VulkanRHI/Private/Windows"));
			}
			else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Linux))
			{
				PrivateIncludePaths.Add(Path.Combine(EngineDir, "Source/Runtime/VulkanRHI/Private/Linux"));
			}
		}
	}
}
