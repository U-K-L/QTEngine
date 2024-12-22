#pragma once
#include <string>
struct UnigmaShader {

	std::string shaderName;
	std::string frag_path;
	std::string vert_path;

	UnigmaShader(const std::string& shaderName) :
		shaderName(shaderName), frag_path("src/shaders/" + shaderName + "frag.spv"), vert_path("src/shaders/" + shaderName + "vert.spv")
	{}

	UnigmaShader() :
		shaderName("default"), frag_path("src/shaders/frag.spv"), vert_path("src/shaders/vert.spv")
	{}

};