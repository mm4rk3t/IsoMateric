#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

// Owns a GL program built from a vertex + fragment file. A failed (re)load keeps the
// previous working program, so a typo while hot-reloading does not kill the game.
// Sources may use  #include "file.glsl"  (relative to the including file); included
// files are watched for hot reload too.
class Shader
{
public:
	Shader() = default;
	Shader(std::string vertexPath, std::string fragmentPath);
	~Shader();

	Shader(const Shader&) = delete;
	Shader& operator=(const Shader&) = delete;
	Shader(Shader&& other) noexcept;
	Shader& operator=(Shader&& other) noexcept;

	bool load();
	bool reloadIfChanged(); // true if it recompiled successfully
	bool valid() const { return program != 0; }
	void use() const { glUseProgram(program); }

	void set(const std::string& name, int value);
	void set(const std::string& name, float value);
	void set(const std::string& name, const glm::vec3& value);
	void set(const std::string& name, const glm::vec4& value);
	void set(const std::string& name, const glm::mat4& value);
	void set(const std::string& name, const std::vector<glm::mat4>& values);

	// binds a named uniform block to a binding point, if the shader uses it
	void bindBlock(const char* name, GLuint binding);

private:
	GLint location(const std::string& name);
	void release();

	std::string vertexPath, fragmentPath;
	GLuint program = 0;
	std::unordered_map<std::string, GLint> locations;
	struct Dependency
	{
		std::string path;
		std::filesystem::file_time_type time;
	};
	std::vector<Dependency> dependencies;
};

#endif
