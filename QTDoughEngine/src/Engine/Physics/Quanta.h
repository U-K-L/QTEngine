#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define QUANTA_COUNT 2097152 //Only changes per official build. 

//The particle that emerges from the field.
//Compact, w values may store arbitrary different results.
struct Quanta {
	glm::vec4 canonicalPosition;
	glm::vec4 position; //The position this quanta is currently in. w is mass.
	glm::vec4 resonance; //Harmonic, waveform, fourier. Dot(sum(qset(i1), qset(i2)) = resonating. w is distance from the observer.
	glm::ivec4 information; //X = body ID. Y = Periodic ID.
	glm::vec4 mana; //Potential energy. xyz is velocity, w energy.
};


//Carries information, is the "hit" that interacts with the materialField.
//A type of boson.
struct Photon
{
	glm::vec4 position;
	glm::vec4 direction;
	glm::vec4 normal;
	glm::vec4 force;
	glm::ivec4 information;
};

//Control points... used for cage deformation AND creating spacetime metric.
//A type of boson.
struct Graviton
{
	glm::vec4 position;
	glm::vec4 direction; //xyz normalize = direction, unormalized = direction and speed, w = time direction (dt * w)
};

struct Lepton
{
	glm::vec4 position; //Persistent position while claimed. w is ID of claimer.
	glm::vec4 direction; //Direction of movement through the field. w is radius of influence which falls off with distance.
	glm::vec4 mana; //Potential energy, or "charge". each xyz different type. w is lifespan
	glm::vec4 velocity; //Speed at which it moves through the field.
};

//The type of Quark, this determines material property, this is part of a lookup table.
struct QuarkElemental
{
	float charge;
	float mass;
	float _pad;
	float _pad2;
	glm::vec4 bond; //XYZ the direction of bonding, W the strength. The stiffness of a patch K is sum(dot(M))

};