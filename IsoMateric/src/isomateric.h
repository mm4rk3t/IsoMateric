#ifndef ISOMATERIC_H
#define ISOMATERIC_H

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
	void handleInput(float dt);
	void update(float dt);
	void render();

	// members
	unsigned int width, height;
	State state;
	bool keys[1024];

	// TEST ONLY DELETE AFTER!!!
	unsigned int vao;

};

#endif
