#pragma once
#include <string>
#include "UnigmaMaterial.h"
#include "UnigmaModel.h"

struct UnigmaRenderingStruct
{

};

class UnigmaRenderingObject {
	public:
		UnigmaRenderingObject()
			: _model(), _material(), _renderer()
		{
		}
		UnigmaRenderingObject(UnigmaModel model, UnigmaMaterial mat = UnigmaMaterial())
			: _model(), _renderer(), _material()
		{
			_model = model;
			_material = mat;
		}
		UnigmaModel _model;
		UnigmaRenderingStruct _renderer;
		UnigmaMaterial _material;
		void Initialization(UnigmaMaterial material);
	private:
};