#pragma once
#include <iostream>
#include "tiny_gltf.h"

class AssetLoader
{
	public:
		AssetLoader();

		int LoadGLTF(std::string filePath, tinygltf::Model& model);
		int LoadGLB(std::string filePath, tinygltf::Model& model);
};