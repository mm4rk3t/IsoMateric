#ifndef INVENTORY_H
#define INVENTORY_H

#include <glm/glm.hpp>

#include <string>
#include <vector>

struct ImDrawList;
struct ImFont;

// One box of an item's look, in item units (an item is about 1 unit long). Used both
// for the pickup in the world (scaled down) and for the rotating icon in the menu.
struct ItemBox
{
	glm::vec3 offset;
	glm::vec3 size;
	glm::vec3 color;
};

struct ItemDef
{
	std::string id;
	std::string name;
	std::string description; // one line, under the name
	std::string examine;     // longer text for Examine / Read
	bool equippable = false;
	bool readable = false;   // "Read" instead of "Examine"
	std::vector<ItemBox> boxes;
};

const ItemDef* findItem(const std::string& id);

// Draws an item as flat-shaded boxes, rotated by yaw, with a painter's sort.
void drawItemIcon(ImDrawList* dl, const ItemDef& item, glm::vec2 center, float size, float yaw, float alpha);

// Silent Hill style item screen: the game pauses, items sit on a carousel, the selected
// one turns slowly, with its name, description and a small command menu.
class Inventory
{
public:
	struct Keys
	{
		bool left = false, right = false, up = false, down = false, confirm = false, close = false;
	};

	bool has(const std::string& id) const;
	void add(const std::string& id);
	void toggle();

	// navigation while open; returns true when something should make a sound
	bool update(const Keys& keys, float dt);
	void draw(glm::vec2 display, ImFont* title, ImFont* body, float time) const;

	bool open = false;
	std::vector<std::string> items;
	std::string equipped;
	bool flashlightOn = false;

private:
	std::vector<std::string> commands() const;
	void run(const std::string& command);

	int selected = 0;
	int command = 0;
	float scroll = 0.0f; // eases towards selected, for the carousel slide
	bool examining = false;
	std::string message;
	float messageTime = 0.0f;
};

#endif
