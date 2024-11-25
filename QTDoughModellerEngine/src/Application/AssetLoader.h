#pragma once
#include <iostream>

class AssetLoader
{
	public:
		AssetLoader();

		int LoadGLTF(std::string filePath);
};