#pragma once
#include "../../Application/QTDoughApplication.h"
#include "../Renderer/UnigmaMaterial.h"

#define QUANTA_COUNT 1048576 //Only changes per official build. 

//The particle that emerges from the field.
//Compact, w values may store arbitrary different results.
struct Quanta {
	glm::vec4 position; //The position this quanta is currently in.
	glm::vec4 resonance; //Harmonic, waveform, fourier. Dot(sum(qset(i1), qset(i2)) = resonating.
	glm::ivec4 information; //Hashed ledger, maps to a lookup, a larger ledger.
	glm::vec4 mana; //Potential energy.
};

struct UnigmaField
{
	Unigma3DTexture SignedDistanceField; //3D texture holding the signed distance field. Resolutions changes based on settings.
	glm::ivec3 FieldSize; //invariant holding the size of the field. This can be non-cubic, ie 64x64x16...
	Quanta* Quantas; //The quantas emerging from this field.
};


class MaterialSimulation
{

	public:
		MaterialSimulation();
		~MaterialSimulation();
		static MaterialSimulation* instance;

		void SetInstance(MaterialSimulation* matSim)
		{
			instance = matSim;
		}

		void InitMaterialSim();
		void InitQuanta();
		void InitComputeWorkload(); //eg descriptors, layouts, etc.
		void Simulate(); //Dispatch that happens in main application compute physics.
		void CleanUp();
		void InitQuantaPositions();
		void SerializeQuantaBlob(const std::string& path);
		void SerializeQuantaText(const std::string& path);
		void DeserializeQuantaBlob(const std::string& path);
		UnigmaField Field; //Underlying field of everything.
};