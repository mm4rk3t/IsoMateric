#include "gameObject.h"
#include <iostream>

GameObject::~GameObject() { } 

void GameObject::draw()
{
	if(this->renderer != nullptr)
		this->renderer->draw(this->position, this->size, 0.0f, this->color);
	else
		std::cout << "No renderer available!" << std::endl;
}	
