#pragma once
#pragma once
#include <string>

struct UnigmaModel {
	std::string MODEL_PATH;

	UnigmaModel(const std::string& modelPath)
		: MODEL_PATH(modelPath) {}

	//Default
	UnigmaModel() : MODEL_PATH("") {}
};