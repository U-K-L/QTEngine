#pragma once
#pragma once
#include <string>

struct UnigmaMesh {
	std::string MODEL_PATH;

	UnigmaMesh(const std::string& modelPath)
		: MODEL_PATH(modelPath) {}

	//Default
	UnigmaMesh() : MODEL_PATH("") {}
};