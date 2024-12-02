C:/VulkanSDK/1.3.290.0/Bin/dxc.exe dxc -T ps_6_0 -E main -spirv tests/basicFragHLSL.hlsl -Fo frag.spv
C:/VulkanSDK/1.3.290.0/Bin/dxc.exe dxc -T vs_6_0 -E main -spirv tests/basicVertHLSL.hlsl -Fo vert.spv
C:/VulkanSDK/1.3.290.0/Bin/dxc.exe dxc -T ps_6_0 -E main -spirv Background/background_frag.hlsl -Fo backgroundFrag.spv
C:/VulkanSDK/1.3.290.0/Bin/dxc.exe dxc -T vs_6_0 -E main -spirv Background/background_vert.hlsl -Fo backgroundVert.spv
C:/VulkanSDK/1.3.290.0/Bin/dxc.exe dxc -T ps_6_0 -E main -spirv Background/albedo_frag.hlsl -Fo backgroundFrag.spv
C:/VulkanSDK/1.3.290.0/Bin/dxc.exe dxc -T vs_6_0 -E main -spirv Background/albedo_vert.hlsl -Fo backgroundVert.spv
C:/VulkanSDK/1.3.290.0/Bin/dxc.exe dxc -T ps_6_0 -E main -spirv Composition/composition_frag.hlsl -Fo albedofrag.spv
C:/VulkanSDK/1.3.290.0/Bin/dxc.exe dxc -T vs_6_0 -E main -spirv Composition/composition_vert.hlsl -Fo albedovert.spv
pause