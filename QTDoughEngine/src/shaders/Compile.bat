@echo off
cd /d %~dp0
C:/VulkanSDK/1.3.290.0/Bin/dxc.exe dxc -T ps_6_0 -E main -spirv tests/basicFragHLSL.hlsl -Fo frag.spv
C:/VulkanSDK/1.3.290.0/Bin/dxc.exe dxc -T vs_6_0 -E main -spirv tests/basicVertHLSL.hlsl -Fo vert.spv
C:/VulkanSDK/1.3.290.0/Bin/dxc.exe dxc -T cs_6_0 -E main -spirv tests/particleTest_compute.hlsl -Fo particletestcompute.spv
C:/VulkanSDK/1.3.290.0/Bin/dxc.exe dxc -T cs_6_0 -E main -spirv RaymarchSDF/raymarchsdf_compute.hlsl -Fo raymarchsdf.spv
C:/VulkanSDK/1.3.290.0/Bin/dxc.exe dxc -T cs_6_0 -E main -spirv Voxelizer/voxelizer_compute.hlsl -Fo voxelizer.spv
C:/VulkanSDK/1.3.290.0/Bin/dxc.exe dxc -T cs_6_0 -E main -spirv TileSDF/tilesdf_compute.hlsl -Fo tilesdf.spv
C:/VulkanSDK/1.3.290.0/Bin/dxc.exe dxc -T ps_6_0 -E main -spirv Background/background_frag.hlsl -Fo backgroundFrag.spv
C:/VulkanSDK/1.3.290.0/Bin/dxc.exe dxc -T vs_6_0 -E main -spirv Background/background_vert.hlsl -Fo backgroundVert.spv
C:/VulkanSDK/1.3.290.0/Bin/dxc.exe dxc -T ps_6_0 -E main -spirv Albedo/albedo_frag.hlsl -Fo albedoFrag.spv
C:/VulkanSDK/1.3.290.0/Bin/dxc.exe dxc -T vs_6_0 -E main -spirv Albedo/albedo_vert.hlsl -Fo albedoVert.spv
C:/VulkanSDK/1.3.290.0/Bin/dxc.exe dxc -T ps_6_0 -E main -spirv Normal/normal_frag.hlsl -Fo normalFrag.spv
C:/VulkanSDK/1.3.290.0/Bin/dxc.exe dxc -T vs_6_0 -E main -spirv Normal/normal_vert.hlsl -Fo normalVert.spv
C:/VulkanSDK/1.3.290.0/Bin/dxc.exe dxc -T ps_6_0 -E main -spirv Position/position_frag.hlsl -Fo positionFrag.spv
C:/VulkanSDK/1.3.290.0/Bin/dxc.exe dxc -T vs_6_0 -E main -spirv Position/position_vert.hlsl -Fo positionVert.spv
C:/VulkanSDK/1.3.290.0/Bin/dxc.exe dxc -T ps_6_0 -E main -spirv CombineSDFRaster/combineSDFRaster_frag.hlsl -Fo combineSDFRasterFrag.spv
C:/VulkanSDK/1.3.290.0/Bin/dxc.exe dxc -T vs_6_0 -E main -spirv CombineSDFRaster/combineSDFRaster_vert.hlsl -Fo combineSDFRasterVert.spv
C:/VulkanSDK/1.3.290.0/Bin/dxc.exe dxc -T ps_6_0 -E main -spirv Outline/outline_frag.hlsl -Fo outlinefrag.spv
C:/VulkanSDK/1.3.290.0/Bin/dxc.exe dxc -T vs_6_0 -E main -spirv Outline/outline_vert.hlsl -Fo outlinevert.spv
C:/VulkanSDK/1.3.290.0/Bin/dxc.exe dxc -T ps_6_0 -E main -spirv Composition/composition_frag.hlsl -Fo compositionfrag.spv
C:/VulkanSDK/1.3.290.0/Bin/dxc.exe dxc -T vs_6_0 -E main -spirv Composition/composition_vert.hlsl -Fo compositionvert.spv