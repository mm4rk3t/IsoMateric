#ifndef INPUT_H
#define INPUT_H

#include <GLFW/glfw3.h>

#include <array>
#include <cstddef>
#include <initializer_list>
#include <vector>

// Game-level actions. Code asks "is MoveForward down?" instead of checking raw key codes,
// so bindings can change (or be remapped per keyboard layout) in one place.
enum class Action
{
	MoveForward,
	MoveBack,
	MoveLeft,
	MoveRight,
	Jump,
	RotateLeft,
	RotateRight,
	ZoomIn,
	ZoomOut,
	ToggleFollow,
	ToggleDebug,
	ToggleEditor,
	Count
};

class Input
{
public:
	Input();

	void bind(Action action, std::initializer_list<int> keys);

	bool down(Action action) const;     // held this frame
	bool pressed(Action action) const;  // went down this frame
	bool mouseDown(int button) const;
	bool mousePressed(int button) const;

	// fed by GLFW callbacks
	void onKey(int key, int action);
	void onMouseButton(int button, int action);
	void onCursor(double x, double y);
	void onScroll(double dy);

	void endFrame();

	double mouseX = 0.0, mouseY = 0.0;
	double mouseDeltaX = 0.0, mouseDeltaY = 0.0;
	float scroll = 0.0f;

	// set every frame from ImGui so typing in a text box does not move the player
	bool keyboardBlocked = false;
	bool mouseBlocked = false;

private:
	std::array<bool, GLFW_KEY_LAST + 1> keys{};
	std::array<bool, GLFW_KEY_LAST + 1> keysPressed{};
	std::array<bool, GLFW_MOUSE_BUTTON_LAST + 1> buttons{};
	std::array<bool, GLFW_MOUSE_BUTTON_LAST + 1> buttonsPressed{};
	std::array<std::vector<int>, static_cast<std::size_t>(Action::Count)> bindings;
	bool haveCursor = false;
};

#endif
