#ifndef ISOMATERIC_H
#define ISOMATERIC_H

#include "camera.h"

#include <vector>
#include <string>

enum State
{
	ACTIVE,
	PAUSE
};

struct Light
{
	glm::vec3 position;
	glm::vec3 color;
	float intensity;

	inline Light(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f), float intensity = 1.0f) 
		: position(position), color(color), intensity(intensity){ }
};

class IsoMateric
{
public:
	// constructor and destructor
	IsoMateric(unsigned int width, unsigned int height);
	~IsoMateric();

	// game loop
	void init();
	void handleInput(float dt);
	void update(float dt);
	void render();
	void ui();

	// members
	unsigned int width, height;
	State state;
	bool keys[1024];
	bool keysProcessed[1024];

	// arrays
	std::vector<Light*> lights;
};

#endif
