#include "AssetLoader.h"

// Define these only in *one* .cc file.
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// #define TINYGLTF_NOEXCEPTION // optional. disable exception handling.
#include "tiny_gltf.h"

using namespace tinygltf;
TinyGLTF loader;
std::string err;
std::string warn;

AssetLoader::AssetLoader()
{
}

int AssetLoader::LoadGLTF(std::string filePath, Model &model)
{
	bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, filePath);

	if (!warn.empty()) {
		printf("Warn: %s\n", warn.c_str());
	}

	if (!err.empty()) {
		printf("Err: %s\n", err.c_str());
	}

	if (!ret) {
		printf("Failed to parse glTF\n");
		return -1;
	}

	return 0;
}

int AssetLoader::LoadGLB(std::string filePath, Model& model)
{
	// Load binary blob.
	std::ifstream file(filePath, std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		printf("Failed to open file: %s\n", filePath.c_str());
		return -1;
	}

	//Get the beginning and the end of the file.
	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);

	//Get the size of the file.
	std::vector<char> buffer(size);
	if (!file.read(buffer.data(), size)) {
		printf("Failed to read file: %s\n", filePath.c_str());
		return -1;
	}

	file.close();

	bool ret = loader.LoadBinaryFromMemory(&model, &err, &warn, reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size());

	if (!warn.empty()) {
		printf("Warn: %s\n", warn.c_str());
	}

	if (!err.empty()) {
		printf("Err: %s\n", err.c_str());
	}

	if (!ret) {
		printf("Failed to parse glTF\n");
		return -1;
	}

	return 0;
}

