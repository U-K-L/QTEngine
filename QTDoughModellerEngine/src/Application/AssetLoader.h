#pragma once
#include <iostream>

class AssetLoader
{
	public:
		AssetLoader();

		int LoadGLTF(std::string filePath);
		int LoadGLB(std::string filePath);
};