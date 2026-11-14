#include "input.h"

Input::Input()
{
	bind(Action::MoveForward, {GLFW_KEY_W, GLFW_KEY_UP});
	bind(Action::MoveBack,    {GLFW_KEY_S, GLFW_KEY_DOWN});
	bind(Action::MoveLeft,    {GLFW_KEY_A, GLFW_KEY_LEFT});
	bind(Action::MoveRight,   {GLFW_KEY_D, GLFW_KEY_RIGHT});
	bind(Action::Jump,        {GLFW_KEY_SPACE});
	bind(Action::RotateLeft,  {GLFW_KEY_Q});
	bind(Action::RotateRight, {GLFW_KEY_E});
	// GLFW key codes are physical US positions. '-' / '+' live on SLASH / RIGHT_BRACKET
	// on a Spanish layout, so both sets are bound, plus the keypad.
	bind(Action::ZoomIn,  {GLFW_KEY_EQUAL, GLFW_KEY_RIGHT_BRACKET, GLFW_KEY_KP_ADD});
	bind(Action::ZoomOut, {GLFW_KEY_MINUS, GLFW_KEY_SLASH, GLFW_KEY_KP_SUBTRACT});
	bind(Action::ToggleFollow, {GLFW_KEY_T});
	bind(Action::ToggleDebug,  {GLFW_KEY_F2});
	bind(Action::ToggleEditor, {GLFW_KEY_TAB});
}

void Input::bind(Action action, std::initializer_list<int> keyList)
{
	bindings[static_cast<std::size_t>(action)] = keyList;
}

bool Input::down(Action action) const
{
	if (keyboardBlocked)
		return false;
	for (int key : bindings[static_cast<std::size_t>(action)])
		if (keys[key])
			return true;
	return false;
}

bool Input::pressed(Action action) const
{
	if (keyboardBlocked)
		return false;
	for (int key : bindings[static_cast<std::size_t>(action)])
		if (keysPressed[key])
			return true;
	return false;
}

bool Input::mouseDown(int button) const
{
	return !mouseBlocked && buttons[button];
}

bool Input::mousePressed(int button) const
{
	return !mouseBlocked && buttonsPressed[button];
}

void Input::onKey(int key, int action)
{
	if (key < 0 || key > GLFW_KEY_LAST)
		return;
	if (action == GLFW_PRESS)
	{
		keys[key] = true;
		keysPressed[key] = true;
	}
	else if (action == GLFW_RELEASE)
	{
		keys[key] = false;
	}
}

void Input::onMouseButton(int button, int action)
{
	if (button < 0 || button > GLFW_MOUSE_BUTTON_LAST)
		return;
	if (action == GLFW_PRESS)
	{
		buttons[button] = true;
		buttonsPressed[button] = true;
	}
	else if (action == GLFW_RELEASE)
	{
		buttons[button] = false;
	}
}

void Input::onCursor(double x, double y)
{
	if (haveCursor)
	{
		mouseDeltaX += x - mouseX;
		mouseDeltaY += y - mouseY;
	}
	mouseX = x;
	mouseY = y;
	haveCursor = true;
}

void Input::onScroll(double dy)
{
	scroll += static_cast<float>(dy);
}

void Input::endFrame()
{
	keysPressed.fill(false);
	buttonsPressed.fill(false);
	scroll = 0.0f;
	mouseDeltaX = mouseDeltaY = 0.0;
}
