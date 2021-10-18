#ifndef ISOMATERIC_H
#define ISOMATERIC_H

#include "camera.h"
#include <vector>

enum State
{
	ACTIVE,
	PAUSE
};

class IsoMateric
{
public:
	// constructor and destructor
	IsoMateric(unsigned int width, unsigned int height);
	~IsoMateric();

	// game loop
	void init();
	void handleInput();
	void update(float dt);
	void render();

	// members
	unsigned int width, height;
	State state;
	bool keys[1024];
	bool keysProcessed[1024];
};

#endif
