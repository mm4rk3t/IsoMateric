#include "model.h"
#include "log.h"

#include "cgltf.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <limits>

namespace
{
	constexpr int kFloatsPerVertex = 16; // pos3 normal3 uv2 joints4 weights4

	glm::mat4 localMatrix(const glm::vec3& t, const glm::quat& r, const glm::vec3& s)
	{
		return glm::translate(glm::mat4(1.0f), t) * glm::mat4_cast(r) * glm::scale(glm::mat4(1.0f), s);
	}

	glm::vec4 keyValue(const Model::Channel& ch, size_t key)
	{
		// cubic spline stores (in-tangent, value, out-tangent) per key
		return ch.interpolation == Model::Channel::Interpolation::CubicSpline ? ch.values[key * 3 + 1] : ch.values[key];
	}

	glm::vec4 sample(const Model::Channel& ch, float t)
	{
		if (ch.times.empty())
			return glm::vec4(0.0f);
		if (t <= ch.times.front())
			return keyValue(ch, 0);
		if (t >= ch.times.back())
			return keyValue(ch, ch.times.size() - 1);

		size_t k = static_cast<size_t>(std::upper_bound(ch.times.begin(), ch.times.end(), t) - ch.times.begin()) - 1;
		glm::vec4 a = keyValue(ch, k), b = keyValue(ch, k + 1);
		if (ch.interpolation == Model::Channel::Interpolation::Step)
			return a;
		float f = (t - ch.times[k]) / std::max(ch.times[k + 1] - ch.times[k], 1e-6f);
		if (ch.path == Model::Channel::Path::Rotation)
		{
			glm::quat qa(a.w, a.x, a.y, a.z), qb(b.w, b.x, b.y, b.z);
			glm::quat q = glm::slerp(qa, qb, f);
			return glm::vec4(q.x, q.y, q.z, q.w);
		}
		return glm::mix(a, b, f);
	}

	const cgltf_accessor* attribute(const cgltf_primitive& prim, cgltf_attribute_type type)
	{
		for (cgltf_size i = 0; i < prim.attributes_count; i++)
			if (prim.attributes[i].type == type && prim.attributes[i].index == 0)
				return prim.attributes[i].data;
		return nullptr;
	}
}

Model::~Model()
{
	for (Primitive& p : primitives)
	{
		glDeleteVertexArrays(1, &p.vao);
		glDeleteBuffers(1, &p.vbo);
		glDeleteBuffers(1, &p.ebo);
	}
}

bool Model::load(const std::string& file)
{
	path = file;
	cgltf_options options{};
	cgltf_data* data = nullptr;
	if (cgltf_parse_file(&options, file.c_str(), &data) != cgltf_result_success)
	{
		LOG_ERROR("model: cannot parse '%s'", file.c_str());
		return false;
	}
	if (cgltf_load_buffers(&options, data, file.c_str()) != cgltf_result_success)
	{
		LOG_ERROR("model: cannot load buffers of '%s'", file.c_str());
		cgltf_free(data);
		return false;
	}
	auto nodeIndex = [&](const cgltf_node* n) { return n ? static_cast<int>(n - data->nodes) : -1; };

	// nodes
	nodes.resize(data->nodes_count);
	for (cgltf_size i = 0; i < data->nodes_count; i++)
	{
		const cgltf_node& src = data->nodes[i];
		Node& n = nodes[i];
		n.parent = nodeIndex(src.parent);
		if (src.has_matrix)
		{
			// decompose by hand: the bundled glm's decompose() returns a conjugated rotation
			glm::mat4 m = glm::make_mat4(src.matrix);
			n.translation = glm::vec3(m[3]);
			n.scale = glm::vec3(glm::length(glm::vec3(m[0])), glm::length(glm::vec3(m[1])), glm::length(glm::vec3(m[2])));
			const glm::vec3 c0 = glm::vec3(m[0]) / n.scale.x, c1 = glm::vec3(m[1]) / n.scale.y, c2 = glm::vec3(m[2]) / n.scale.z;
			const glm::mat3 r{c0, c1, c2};
			n.rotation = glm::normalize(glm::quat_cast(r));
		}
		else
		{
			if (src.has_translation)
				n.translation = glm::make_vec3(src.translation);
			if (src.has_rotation)
				n.rotation = glm::quat(src.rotation[3], src.rotation[0], src.rotation[1], src.rotation[2]);
			if (src.has_scale)
				n.scale = glm::make_vec3(src.scale);
		}
	}
	for (size_t i = 0; i < nodes.size(); i++)
		if (nodes[i].parent < 0)
			nodeOrder.push_back(static_cast<int>(i));
	for (size_t i = 0; i < nodeOrder.size(); i++) // breadth first: parents before children
		for (cgltf_size c = 0; c < data->nodes[nodeOrder[i]].children_count; c++)
			nodeOrder.push_back(nodeIndex(data->nodes[nodeOrder[i]].children[c]));

	// skin (first one only)
	if (data->skins_count > 0)
	{
		const cgltf_skin& skin = data->skins[0];
		if (skin.joints_count > static_cast<cgltf_size>(kMaxJoints))
			LOG_WARN("model: '%s' has %d joints, only %d are supported", file.c_str(), static_cast<int>(skin.joints_count), kMaxJoints);
		for (cgltf_size j = 0; j < skin.joints_count; j++)
		{
			skinJoints.push_back(nodeIndex(skin.joints[j]));
			glm::mat4 ib(1.0f);
			if (skin.inverse_bind_matrices)
				cgltf_accessor_read_float(skin.inverse_bind_matrices, j, glm::value_ptr(ib), 16);
			inverseBind.push_back(ib);
		}
	}

	// materials and their base color textures
	const std::string dir = std::filesystem::path(file).parent_path().string();
	for (cgltf_size i = 0; i < data->materials_count; i++)
	{
		const cgltf_material& src = data->materials[i];
		Material m;
		if (src.has_pbr_metallic_roughness)
		{
			m.baseColor = glm::make_vec4(src.pbr_metallic_roughness.base_color_factor);
			const cgltf_texture* tex = src.pbr_metallic_roughness.base_color_texture.texture;
			if (tex && tex->image)
			{
				Texture t;
				bool ok = false;
				if (tex->image->buffer_view)
				{
					const cgltf_buffer_view* bv = tex->image->buffer_view;
					const unsigned char* bytes = static_cast<const unsigned char*>(bv->buffer->data) + bv->offset;
					ok = t.loadFromMemory(bytes, static_cast<int>(bv->size));
				}
				else if (tex->image->uri)
				{
					ok = t.loadFromFile(dir + "/" + tex->image->uri);
				}
				if (ok)
				{
					m.texture = static_cast<int>(textures.size());
					textures.push_back(std::move(t));
				}
			}
		}
		materials.push_back(m);
	}

	// meshes -> GPU buffers (kept on the CPU too until the bounds are known)
	struct CpuMesh
	{
		std::vector<float> vertices;
		bool skinned;
		int node;
	};
	std::vector<CpuMesh> cpu;
	for (cgltf_size ni = 0; ni < data->nodes_count; ni++)
	{
		const cgltf_node& node = data->nodes[ni];
		if (!node.mesh)
			continue;
		for (cgltf_size pi = 0; pi < node.mesh->primitives_count; pi++)
		{
			const cgltf_primitive& prim = node.mesh->primitives[pi];
			const cgltf_accessor* pos = attribute(prim, cgltf_attribute_type_position);
			if (prim.type != cgltf_primitive_type_triangles || !pos)
				continue;
			const cgltf_accessor* nrm = attribute(prim, cgltf_attribute_type_normal);
			const cgltf_accessor* uv = attribute(prim, cgltf_attribute_type_texcoord);
			const cgltf_accessor* jnt = attribute(prim, cgltf_attribute_type_joints);
			const cgltf_accessor* wgt = attribute(prim, cgltf_attribute_type_weights);
			bool skinned = node.skin && jnt && wgt && !skinJoints.empty();

			std::vector<float> v(pos->count * kFloatsPerVertex, 0.0f);
			for (cgltf_size i = 0; i < pos->count; i++)
			{
				float* o = &v[i * kFloatsPerVertex];
				cgltf_accessor_read_float(pos, i, o, 3);
				if (nrm)
					cgltf_accessor_read_float(nrm, i, o + 3, 3);
				else
					o[4] = 1.0f;
				if (uv)
					cgltf_accessor_read_float(uv, i, o + 6, 2);
				if (skinned)
				{
					cgltf_uint j[4] = {0, 0, 0, 0};
					cgltf_accessor_read_uint(jnt, i, j, 4);
					for (int k = 0; k < 4; k++)
						o[8 + k] = static_cast<float>(std::min<cgltf_uint>(j[k], kMaxJoints - 1));
					cgltf_accessor_read_float(wgt, i, o + 12, 4);
				}
			}

			std::vector<unsigned int> indices;
			if (prim.indices)
			{
				indices.resize(prim.indices->count);
				for (cgltf_size i = 0; i < prim.indices->count; i++)
					indices[i] = static_cast<unsigned int>(cgltf_accessor_read_index(prim.indices, i));
			}
			else
			{
				for (cgltf_size i = 0; i < pos->count; i++)
					indices.push_back(static_cast<unsigned int>(i));
			}

			Primitive p;
			p.node = static_cast<int>(ni);
			p.skinned = skinned;
			p.material = prim.material ? static_cast<int>(prim.material - data->materials) : -1;
			p.indexCount = static_cast<GLsizei>(indices.size());
			glGenVertexArrays(1, &p.vao);
			glGenBuffers(1, &p.vbo);
			glGenBuffers(1, &p.ebo);
			glBindVertexArray(p.vao);
			glBindBuffer(GL_ARRAY_BUFFER, p.vbo);
			glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(v.size() * sizeof(float)), v.data(), GL_STATIC_DRAW);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, p.ebo);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)), indices.data(), GL_STATIC_DRAW);
			const GLsizei stride = kFloatsPerVertex * sizeof(float);
			const int sizes[5] = {3, 3, 2, 4, 4};
			size_t offset = 0;
			for (GLuint a = 0; a < 5; a++)
			{
				glEnableVertexAttribArray(a);
				glVertexAttribPointer(a, sizes[a], GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offset * sizeof(float)));
				offset += static_cast<size_t>(sizes[a]);
			}
			glBindVertexArray(0);
			primitives.push_back(p);
			cpu.push_back({std::move(v), skinned, static_cast<int>(ni)});
		}
	}

	// animations
	for (cgltf_size ai = 0; ai < data->animations_count; ai++)
	{
		const cgltf_animation& src = data->animations[ai];
		Clip clip;
		clip.name = src.name ? src.name : ("clip" + std::to_string(ai));
		for (cgltf_size ci = 0; ci < src.channels_count; ci++)
		{
			const cgltf_animation_channel& c = src.channels[ci];
			Channel ch;
			ch.node = nodeIndex(c.target_node);
			if (ch.node < 0)
				continue;
			int width;
			switch (c.target_path)
			{
				case cgltf_animation_path_type_translation: ch.path = Channel::Path::Translation; width = 3; break;
				case cgltf_animation_path_type_rotation:    ch.path = Channel::Path::Rotation;    width = 4; break;
				case cgltf_animation_path_type_scale:       ch.path = Channel::Path::Scale;       width = 3; break;
				default: continue; // morph target weights are not supported
			}
			ch.interpolation = c.sampler->interpolation == cgltf_interpolation_type_step          ? Channel::Interpolation::Step
			                 : c.sampler->interpolation == cgltf_interpolation_type_cubic_spline ? Channel::Interpolation::CubicSpline
			                                                                                     : Channel::Interpolation::Linear;
			ch.times.resize(c.sampler->input->count);
			for (cgltf_size i = 0; i < ch.times.size(); i++)
				cgltf_accessor_read_float(c.sampler->input, i, &ch.times[i], 1);
			ch.values.resize(c.sampler->output->count, glm::vec4(0.0f));
			for (cgltf_size i = 0; i < ch.values.size(); i++)
				cgltf_accessor_read_float(c.sampler->output, i, glm::value_ptr(ch.values[i]), width);
			if (!ch.times.empty())
				clip.duration = std::max(clip.duration, ch.times.back());
			clip.channels.push_back(std::move(ch));
		}
		clips.push_back(std::move(clip));
	}
	cgltf_free(data);

	// rest-pose bounds, skinned on the CPU so they match what is drawn
	std::vector<glm::mat4> globals, joints;
	pose(-1, 0.0f, globals, joints);
	boundsMin = glm::vec3(std::numeric_limits<float>::max());
	boundsMax = glm::vec3(std::numeric_limits<float>::lowest());
	for (const CpuMesh& m : cpu)
	{
		for (size_t i = 0; i < m.vertices.size(); i += kFloatsPerVertex)
		{
			const float* o = &m.vertices[i];
			glm::vec4 p(o[0], o[1], o[2], 1.0f);
			glm::mat4 xf = globals[m.node];
			if (m.skinned)
			{
				xf = glm::mat4(0.0f);
				for (int k = 0; k < 4; k++)
					xf += joints[static_cast<size_t>(o[8 + k])] * o[12 + k];
			}
			glm::vec3 w = glm::vec3(xf * p);
			boundsMin = glm::min(boundsMin, w);
			boundsMax = glm::max(boundsMax, w);
		}
	}
	if (cpu.empty())
		boundsMin = boundsMax = glm::vec3(0.0f);

	LOG_INFO("model: loaded '%s' (%d primitives, %d joints, %d clips)", file.c_str(), static_cast<int>(primitives.size()),
	         static_cast<int>(skinJoints.size()), static_cast<int>(clips.size()));
	return !primitives.empty();
}

void Model::pose(int clip, float time, std::vector<glm::mat4>& nodeGlobals, std::vector<glm::mat4>& joints) const
{
	std::vector<glm::vec3> t(nodes.size()), s(nodes.size());
	std::vector<glm::quat> r(nodes.size());
	for (size_t i = 0; i < nodes.size(); i++)
	{
		t[i] = nodes[i].translation;
		r[i] = nodes[i].rotation;
		s[i] = nodes[i].scale;
	}

	if (clip >= 0 && clip < static_cast<int>(clips.size()))
	{
		for (const Channel& ch : clips[clip].channels)
		{
			glm::vec4 v = sample(ch, time);
			switch (ch.path)
			{
				case Channel::Path::Translation: t[ch.node] = glm::vec3(v); break;
				case Channel::Path::Rotation:    r[ch.node] = glm::normalize(glm::quat(v.w, v.x, v.y, v.z)); break;
				case Channel::Path::Scale:       s[ch.node] = glm::vec3(v); break;
			}
		}
	}

	nodeGlobals.resize(nodes.size());
	for (int n : nodeOrder)
	{
		glm::mat4 local = localMatrix(t[n], r[n], s[n]);
		nodeGlobals[n] = nodes[n].parent >= 0 ? nodeGlobals[nodes[n].parent] * local : local;
	}

	size_t count = std::min(skinJoints.size(), static_cast<size_t>(kMaxJoints));
	joints.resize(count);
	for (size_t j = 0; j < count; j++)
		joints[j] = nodeGlobals[skinJoints[j]] * inverseBind[j];
}

std::shared_ptr<Model> ModelCache::get(const std::string& file)
{
	auto it = models.find(file);
	if (it != models.end())
		return it->second;
	auto model = std::make_shared<Model>();
	if (!model->load(file))
		model = nullptr;
	models.emplace(file, model); // failures are cached too, so a bad path is reported once
	return model;
}
