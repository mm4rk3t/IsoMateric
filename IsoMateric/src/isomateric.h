#ifndef ISOMATERIC_H
#define ISOMATERIC_H

#include "camera.h"
#include "gameObject.h"

#include <vector>
#include <string>

enum State
{
	ACTIVE,
	PAUSE
};

class Light :public GameObject
{
public:
	float intensity;
	inline Light(float intensity = 1.0f) : intensity(intensity){ }
};

class Player :public GameObject
{
public:
	float speed;
	inline Player(float speed = 2.5f) : speed(speed){ }
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
	void collisions();

	// members
	unsigned int width, height;
	State state;
	bool keys[1024];
	bool keysProcessed[1024];

	// arrays
	std::vector<Light*> lights;
	std::vector<GameObject*> floor;
	std::vector<GameObject*> obstacles;
};

#endif
