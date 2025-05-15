#pragma once
#include <string>
struct UnigmaShader {

	std::string shaderName;
	std::string frag_path;
	std::string vert_path;
	std::string comp_path;

	UnigmaShader(const std::string& shaderName, const std::string& shaderType) :
		shaderName(shaderName), frag_path("src/shaders/" + shaderName + "frag.spv"), vert_path("src/shaders/" + shaderName + "vert.spv")
	{
		if (shaderType == "compute") {
			comp_path = "src/shaders/" + shaderName + "comp.spv";
		}
		else if (shaderType == "geometry") {
			frag_path = "src/shaders/" + shaderName + "geom.spv";
			vert_path = "src/shaders/" + shaderName + "geom.spv";
		}
		else if (shaderType == "tessellation") {
			frag_path = "src/shaders/" + shaderName + "tesc.spv";
			vert_path = "src/shaders/" + shaderName + "tese.spv";
		}
		else if (shaderType == "raytracing") {
			frag_path = "src/shaders/" + shaderName + "rgen.spv";
			vert_path = "src/shaders/" + shaderName + "rahit.spv";
		}
		else if (shaderType == "mesh") {
			frag_path = "src/shaders/" + shaderName + "rmiss.spv";
			vert_path = "src/shaders/" + shaderName + "rchit.spv";
		}
		else if (shaderType == "fragment") {
			frag_path = "src/shaders/" + shaderName + "frag.spv";
			vert_path = "src/shaders/" + shaderName + "vert.spv";
		}
		else if (shaderType == "vertex") {
			frag_path = "src/shaders/" + shaderName + "vert.spv";
			vert_path = "src/shaders/" + shaderName + "vert.spv";
		}
	}

	UnigmaShader(const std::string& shaderName) :
		shaderName(shaderName), frag_path("src/shaders/" + shaderName + "frag.spv"), vert_path("src/shaders/" + shaderName + "vert.spv")
	{}

	UnigmaShader() :
		shaderName("default"), frag_path("src/shaders/frag.spv"), vert_path("src/shaders/vert.spv")
	{}

};