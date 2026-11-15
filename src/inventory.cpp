#include "inventory.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
	std::vector<ItemDef> buildItems()
	{
		std::vector<ItemDef> items;

		ItemDef flashlight;
		flashlight.id = "flashlight";
		flashlight.name = "Flashlight";
		flashlight.description = "A small flashlight. The switch sticks a little.";
		flashlight.examine =
			"An old metal flashlight with a dented head. Someone scratched initials into the grip, "
			"but they have worn away. The batteries rattle when you shake it.";
		flashlight.equippable = true;
		const glm::vec3 metal(0.18f, 0.18f, 0.2f);
		flashlight.boxes = {
			{{-0.1f, 0.0f, 0.0f}, {0.8f, 0.2f, 0.2f}, metal},
			{{-0.1f, 0.0f, 0.0f}, {0.5f, 0.22f, 0.22f}, metal * 0.7f},   // grip
			{{0.4f, 0.0f, 0.0f}, {0.24f, 0.32f, 0.32f}, metal * 1.3f},  // head
			{{0.53f, 0.0f, 0.0f}, {0.03f, 0.25f, 0.25f}, {1.0f, 0.93f, 0.6f}}, // lens
			{{0.08f, 0.12f, 0.0f}, {0.12f, 0.05f, 0.08f}, {0.7f, 0.25f, 0.12f}}, // switch
			{{-0.52f, 0.0f, 0.0f}, {0.05f, 0.18f, 0.18f}, metal * 0.5f},  // end cap
		};
		items.push_back(flashlight);

		ItemDef key;
		key.id = "rusty_key";
		key.name = "Rusty Key";
		key.description = "A key, red with rust. The paper tag reads CHAPEL.";
		key.examine = "The teeth are worn almost smooth. The tag is stained, and the word CHAPEL has been written over twice, "
		              "as if someone was afraid of forgetting it.";
		const glm::vec3 rust(0.52f, 0.3f, 0.18f);
		key.boxes = {
			{{0.1f, 0.0f, 0.0f}, {0.6f, 0.07f, 0.07f}, rust},             // shaft
			{{-0.3f, 0.12f, 0.0f}, {0.26f, 0.05f, 0.06f}, rust * 0.9f},    // bow ring
			{{-0.3f, -0.12f, 0.0f}, {0.26f, 0.05f, 0.06f}, rust * 0.9f},
			{{-0.42f, 0.0f, 0.0f}, {0.05f, 0.29f, 0.06f}, rust * 0.9f},
			{{-0.18f, 0.0f, 0.0f}, {0.05f, 0.29f, 0.06f}, rust * 0.9f},
			{{0.3f, -0.08f, 0.0f}, {0.06f, 0.12f, 0.06f}, rust * 0.8f},    // teeth
			{{0.38f, -0.06f, 0.0f}, {0.05f, 0.09f, 0.06f}, rust * 0.8f},
			{{-0.3f, -0.3f, 0.02f}, {0.18f, 0.22f, 0.01f}, {0.75f, 0.68f, 0.52f}}, // tag
		};
		items.push_back(key);

		ItemDef note;
		note.id = "torn_note";
		note.name = "Torn Note";
		note.description = "A page torn from a diary. The ink has run.";
		note.examine = "...the glass remembers what it sees. Don't stand in front of the mirrors after dark.\n\n"
		               "If you hear knocking from the other side, do not knock back.\n\n"
		               "The greenhouse was hers. Nothing in it has died since she left, and nothing in it is alive.";
		note.readable = true;
		const glm::vec3 paper(0.82f, 0.78f, 0.66f);
		note.boxes = {{{0.0f, 0.0f, 0.0f}, {0.62f, 0.8f, 0.02f}, paper}};
		for (int i = 0; i < 6; i++)
			note.boxes.push_back({{-0.04f + (i % 2) * 0.03f, 0.28f - i * 0.1f, 0.012f}, {0.44f - (i % 3) * 0.06f, 0.025f, 0.005f}, {0.25f, 0.22f, 0.24f}});
		note.boxes.push_back({{0.24f, -0.33f, 0.012f}, {0.1f, 0.12f, 0.005f}, {0.45f, 0.15f, 0.12f}}); // a stain
		items.push_back(note);

		return items;
	}

	const std::vector<ItemDef>& allItems()
	{
		static const std::vector<ItemDef> items = buildItems();
		return items;
	}

	ImU32 rgba(glm::vec3 c, float a)
	{
		c = glm::clamp(c, 0.0f, 1.0f);
		return IM_COL32(static_cast<int>(c.r * 255.0f), static_cast<int>(c.g * 255.0f), static_cast<int>(c.b * 255.0f),
		                static_cast<int>(std::clamp(a, 0.0f, 1.0f) * 255.0f));
	}
}

const ItemDef* findItem(const std::string& id)
{
	for (const ItemDef& d : allItems())
		if (d.id == id)
			return &d;
	return nullptr;
}

void drawItemIcon(ImDrawList* dl, const ItemDef& item, glm::vec2 center, float size, float yaw, float alpha)
{
	struct Face
	{
		std::array<ImVec2, 4> p;
		float depth;
		ImU32 fill, edge;
	};
	std::vector<Face> faces;
	const float pitch = 0.45f;
	const float cy = std::cos(yaw), sy = std::sin(yaw), cp = std::cos(pitch), sp = std::sin(pitch);
	auto rotate = [&](glm::vec3 v) {
		glm::vec3 a(v.x * cy + v.z * sy, v.y, -v.x * sy + v.z * cy); // yaw
		return glm::vec3(a.x, a.y * cp - a.z * sp, a.y * sp + a.z * cp); // pitch towards the viewer
	};
	const glm::vec3 lightDir = glm::normalize(glm::vec3(-0.4f, 0.7f, 0.6f));
	static const glm::vec3 normals[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
	for (const ItemBox& b : item.boxes)
	{
		for (const glm::vec3& n : normals)
		{
			glm::vec3 rn = rotate(n);
			if (rn.z <= 0.0f)
				continue; // facing away (viewer looks down -z)
			// the face's two in-plane axes
			glm::vec3 u = std::abs(n.x) > 0.5f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
			glm::vec3 v = glm::cross(n, u);
			glm::vec3 h = b.size * 0.5f;
			glm::vec3 c = b.offset + n * h;
			glm::vec3 du = u * h, dv = v * h;
			Face f;
			glm::vec3 corners[4] = {c - du - dv, c + du - dv, c + du + dv, c - du + dv};
			float depth = 0.0f;
			for (int i = 0; i < 4; i++)
			{
				glm::vec3 r = rotate(corners[i]);
				f.p[i] = ImVec2(center.x + r.x * size, center.y - r.y * size);
				depth += r.z;
			}
			f.depth = depth * 0.25f;
			float light = 0.35f + 0.75f * std::max(glm::dot(rn, lightDir), 0.0f);
			f.fill = rgba(b.color * light, alpha);
			f.edge = rgba(b.color * light * 0.55f, alpha);
			faces.push_back(f);
		}
	}
	std::sort(faces.begin(), faces.end(), [](const Face& a, const Face& b) { return a.depth < b.depth; });
	for (const Face& f : faces)
	{
		dl->AddQuadFilled(f.p[0], f.p[1], f.p[2], f.p[3], f.fill);
		dl->AddQuad(f.p[0], f.p[1], f.p[2], f.p[3], f.edge, 1.0f);
	}
}

bool Inventory::has(const std::string& id) const
{
	return std::find(items.begin(), items.end(), id) != items.end();
}

void Inventory::add(const std::string& id)
{
	if (!has(id))
		items.push_back(id);
}

void Inventory::toggle()
{
	open = !open;
	examining = false;
	command = 0;
	message.clear();
	selected = std::clamp(selected, 0, std::max(0, static_cast<int>(items.size()) - 1));
	scroll = static_cast<float>(selected);
}

std::vector<std::string> Inventory::commands() const
{
	if (items.empty())
		return {"Close"};
	const ItemDef* d = findItem(items[selected]);
	std::vector<std::string> c;
	if (d && d->equippable)
		c.push_back(equipped == d->id ? "Unequip" : "Equip");
	else if (d && !d->readable)
		c.push_back("Use");
	c.push_back(d && d->readable ? "Read" : "Examine");
	c.push_back("Close");
	return c;
}

void Inventory::run(const std::string& cmd)
{
	const ItemDef* d = items.empty() ? nullptr : findItem(items[selected]);
	if (cmd == "Close")
		toggle();
	else if (cmd == "Examine" || cmd == "Read")
		examining = true;
	else if (cmd == "Equip" && d)
	{
		equipped = d->id;
		flashlightOn = d->id == "flashlight";
		message = "Equipped the " + d->name + ".";
		messageTime = 2.5f;
	}
	else if (cmd == "Unequip" && d)
	{
		equipped.clear();
		flashlightOn = false;
		message = "Put the " + d->name + " away.";
		messageTime = 2.5f;
	}
	else if (cmd == "Use")
	{
		message = "There is nothing to use it on here.";
		messageTime = 2.5f;
	}
}

bool Inventory::update(const Keys& keys, float dt)
{
	if (!open)
		return false;
	messageTime = std::max(0.0f, messageTime - dt);
	scroll += (static_cast<float>(selected) - scroll) * (1.0f - std::exp(-dt * 12.0f));
	if (examining)
	{
		if (keys.confirm || keys.close)
		{
			examining = false;
			return true;
		}
		return false;
	}
	if (keys.close)
	{
		toggle();
		return true;
	}
	const int count = static_cast<int>(items.size());
	bool moved = false;
	if (count > 0 && (keys.left || keys.right))
	{
		selected = (selected + (keys.right ? 1 : -1) + count) % count;
		command = 0;
		moved = true;
	}
	const int cmds = static_cast<int>(commands().size());
	if (keys.up || keys.down)
	{
		command = (command + (keys.down ? 1 : -1) + cmds) % cmds;
		moved = true;
	}
	if (keys.confirm)
	{
		run(commands()[std::clamp(command, 0, cmds - 1)]);
		moved = true;
	}
	return moved;
}

void Inventory::draw(glm::vec2 display, ImFont* title, ImFont* body, float time) const
{
	if (!open)
		return;
	ImDrawList* dl = ImGui::GetForegroundDrawList();
	ImFont* tf = title ? title : ImGui::GetFont();
	ImFont* bf = body ? body : ImGui::GetFont();
	const float ts = title ? title->FontSize : 26.0f, bs = body ? body->FontSize : 16.0f;
	auto textCentered = [&](ImFont* f, float s, float y, const std::string& t, ImU32 col) {
		ImVec2 size = f->CalcTextSizeA(s, FLT_MAX, 0.0f, t.c_str());
		dl->AddText(f, s, ImVec2((display.x - size.x) * 0.5f, y), col, t.c_str());
	};

	// darkened, slightly red screen with a vignette, like an old TV in a dark room
	dl->AddRectFilled(ImVec2(0, 0), ImVec2(display.x, display.y), IM_COL32(6, 3, 4, 225));
	const float band = display.y * 0.18f;
	dl->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(display.x, band), IM_COL32(0, 0, 0, 200), IM_COL32(0, 0, 0, 200), IM_COL32(0, 0, 0, 0),
	                            IM_COL32(0, 0, 0, 0));
	dl->AddRectFilledMultiColor(ImVec2(0, display.y - band), ImVec2(display.x, display.y), IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0),
	                            IM_COL32(0, 0, 0, 200), IM_COL32(0, 0, 0, 200));
	for (float y = 0.0f; y < display.y; y += 3.0f) // scanlines
		dl->AddLine(ImVec2(0, y), ImVec2(display.x, y), IM_COL32(0, 0, 0, 40));

	dl->AddText(tf, ts, ImVec2(display.x * 0.06f, display.y * 0.06f), IM_COL32(150, 140, 125, 255), "ITEMS");
	const ImU32 text = IM_COL32(205, 196, 178, 255), dim = IM_COL32(120, 112, 100, 255), accent = IM_COL32(170, 40, 30, 255);

	if (items.empty())
	{
		textCentered(bf, bs, display.y * 0.45f, "You are not carrying anything.", dim);
		textCentered(bf, bs * 0.85f, display.y * 0.9f, "I  close", dim);
		return;
	}

	const ItemDef* sel = findItem(items[selected]);
	if (examining && sel)
	{
		drawItemIcon(dl, *sel, glm::vec2(display.x * 0.5f, display.y * 0.3f), display.y * 0.28f, time * 0.5f, 1.0f);
		const float w = std::min(display.x * 0.7f, 760.0f);
		const float x = (display.x - w) * 0.5f;
		textCentered(tf, ts, display.y * 0.5f, sel->name, text);
		dl->AddText(bf, bs, ImVec2(x, display.y * 0.5f + ts * 1.6f), text, sel->examine.c_str(), nullptr, w);
		textCentered(bf, bs * 0.85f, display.y * 0.92f, "F  back", dim);
		return;
	}

	// carousel
	const float cy = display.y * 0.36f;
	const float spacing = display.y * 0.26f;
	for (int i = 0; i < static_cast<int>(items.size()); i++)
	{
		const ItemDef* d = findItem(items[i]);
		if (!d)
			continue;
		float offset = static_cast<float>(i) - scroll;
		float focus = std::max(0.0f, 1.0f - std::abs(offset));
		float x = display.x * 0.5f + offset * spacing;
		float size = display.y * (0.11f + 0.09f * focus);
		float yaw = i == selected ? time * 0.8f : 0.6f;
		drawItemIcon(dl, *d, glm::vec2(x, cy), size, yaw, 0.35f + 0.65f * focus);
	}
	// brackets around the selected item
	const float b = display.y * 0.14f, arm = b * 0.25f;
	const ImVec2 c(display.x * 0.5f, cy);
	for (int sx = -1; sx <= 1; sx += 2)
		for (int sy = -1; sy <= 1; sy += 2)
		{
			ImVec2 p(c.x + sx * b, c.y + sy * b);
			dl->AddLine(p, ImVec2(p.x - sx * arm, p.y), dim, 2.0f);
			dl->AddLine(p, ImVec2(p.x, p.y - sy * arm), dim, 2.0f);
		}

	if (sel)
	{
		float y = display.y * 0.58f;
		textCentered(tf, ts, y, sel->name, text);
		y += ts * 1.3f;
		if (equipped == sel->id)
		{
			textCentered(bf, bs * 0.9f, y, sel->id == "flashlight" ? (flashlightOn ? "EQUIPPED  -  light on (L)" : "EQUIPPED  -  light off (L)") : "EQUIPPED",
			             accent);
			y += bs * 1.4f;
		}
		textCentered(bf, bs, y, sel->description, dim);
		y += bs * 2.2f;
		std::vector<std::string> cmds = commands();
		for (int i = 0; i < static_cast<int>(cmds.size()); i++)
		{
			bool on = i == command;
			textCentered(bf, bs * 1.05f, y, on ? "> " + cmds[i] + " <" : cmds[i], on ? text : dim);
			y += bs * 1.5f;
		}
	}
	if (messageTime > 0.0f && !message.empty())
		textCentered(bf, bs, display.y * 0.86f, message, IM_COL32(205, 196, 178, static_cast<int>(255 * std::min(1.0f, messageTime))));
	textCentered(bf, bs * 0.85f, display.y * 0.92f, "A / D  item     W / S  command     F  confirm     I  close", dim);
}
