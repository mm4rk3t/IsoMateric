#include "shader.h"
#include "log.h"

#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

namespace
{
	bool readFile(const std::string& path, std::string& out)
	{
		std::ifstream file(path, std::ios::binary);
		if (!file)
		{
			LOG_ERROR("shader: cannot open '%s'", path.c_str());
			return false;
		}
		std::stringstream ss;
		ss << file.rdbuf();
		out = ss.str();
		return true;
	}

	// expands #include "x" lines recursively, recording every file read
	bool preprocess(const std::string& path, std::string& out, std::vector<std::string>& files, int depth = 0)
	{
		if (depth > 8)
		{
			LOG_ERROR("shader: include depth exceeded at '%s'", path.c_str());
			return false;
		}
		files.push_back(path);
		std::string source;
		if (!readFile(path, source))
			return false;

		std::istringstream in(source);
		std::string line;
		const std::string dir = std::filesystem::path(path).parent_path().string();
		while (std::getline(in, line))
		{
			auto pos = line.find("#include");
			auto q1 = line.find('"');
			auto q2 = q1 == std::string::npos ? q1 : line.find('"', q1 + 1);
			if (pos != std::string::npos && line.find_first_not_of(" \t") == pos && q2 != std::string::npos)
			{
				std::string inc = dir + "/" + line.substr(q1 + 1, q2 - q1 - 1);
				if (!preprocess(inc, out, files, depth + 1))
					return false;
			}
			else
			{
				out += line;
				out += '\n';
			}
		}
		return true;
	}

	std::filesystem::file_time_type modTime(const std::string& path)
	{
		std::error_code ec;
		auto t = std::filesystem::last_write_time(path, ec);
		return ec ? std::filesystem::file_time_type{} : t;
	}

	GLuint compileStage(GLenum stage, const std::string& source, const std::string& path)
	{
		GLuint id = glCreateShader(stage);
		const char* src = source.c_str();
		glShaderSource(id, 1, &src, nullptr);
		glCompileShader(id);

		GLint ok = 0;
		glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
		if (!ok)
		{
			char log[2048];
			glGetShaderInfoLog(id, sizeof(log), nullptr, log);
			LOG_ERROR("shader: compiling '%s' failed:\n%s", path.c_str(), log);
			glDeleteShader(id);
			return 0;
		}
		return id;
	}
}

Shader::Shader(std::string vertexPath, std::string fragmentPath)
	: vertexPath(std::move(vertexPath)), fragmentPath(std::move(fragmentPath)) { }

Shader::~Shader()
{
	release();
}

Shader::Shader(Shader&& other) noexcept
{
	*this = std::move(other);
}

Shader& Shader::operator=(Shader&& other) noexcept
{
	if (this != &other)
	{
		release();
		vertexPath = std::move(other.vertexPath);
		fragmentPath = std::move(other.fragmentPath);
		program = std::exchange(other.program, 0);
		locations = std::move(other.locations);
		dependencies = std::move(other.dependencies);
	}
	return *this;
}

void Shader::release()
{
	if (program)
		glDeleteProgram(program);
	program = 0;
	locations.clear();
}

bool Shader::load()
{
	std::string vs, fs;
	std::vector<std::string> files;
	bool read = preprocess(vertexPath, vs, files) && preprocess(fragmentPath, fs, files);

	// remember the timestamps even on failure so a broken file is not retried every tick
	dependencies.clear();
	if (files.empty())
		files = {vertexPath, fragmentPath};
	for (const std::string& f : files)
		dependencies.push_back({f, modTime(f)});
	if (!read)
		return false;

	GLuint v = compileStage(GL_VERTEX_SHADER, vs, vertexPath);
	GLuint f = compileStage(GL_FRAGMENT_SHADER, fs, fragmentPath);
	if (!v || !f)
	{
		if (v) glDeleteShader(v);
		if (f) glDeleteShader(f);
		return false;
	}

	GLuint p = glCreateProgram();
	glAttachShader(p, v);
	glAttachShader(p, f);
	glLinkProgram(p);
	glDeleteShader(v);
	glDeleteShader(f);

	GLint ok = 0;
	glGetProgramiv(p, GL_LINK_STATUS, &ok);
	if (!ok)
	{
		char log[2048];
		glGetProgramInfoLog(p, sizeof(log), nullptr, log);
		LOG_ERROR("shader: linking '%s' + '%s' failed:\n%s", vertexPath.c_str(), fragmentPath.c_str(), log);
		glDeleteProgram(p);
		return false;
	}

	release();
	program = p;
	return true;
}

bool Shader::reloadIfChanged()
{
	bool changed = dependencies.empty();
	for (const Dependency& d : dependencies)
		changed = changed || modTime(d.path) != d.time;
	if (!changed)
		return false;
	bool ok = load();
	if (ok)
		LOG_INFO("shader: reloaded '%s'", fragmentPath.c_str());
	return ok;
}

GLint Shader::location(const std::string& name)
{
	auto it = locations.find(name);
	if (it != locations.end())
		return it->second;
	GLint loc = glGetUniformLocation(program, name.c_str());
	locations.emplace(name, loc);
	return loc;
}

void Shader::set(const std::string& name, int value)              { glUniform1i(location(name), value); }
void Shader::set(const std::string& name, float value)            { glUniform1f(location(name), value); }
void Shader::set(const std::string& name, const glm::vec3& value) { glUniform3fv(location(name), 1, glm::value_ptr(value)); }
void Shader::set(const std::string& name, const glm::vec4& value) { glUniform4fv(location(name), 1, glm::value_ptr(value)); }
void Shader::set(const std::string& name, const glm::mat4& value) { glUniformMatrix4fv(location(name), 1, GL_FALSE, glm::value_ptr(value)); }

void Shader::set(const std::string& name, const std::vector<glm::mat4>& values)
{
	if (!values.empty())
		glUniformMatrix4fv(location(name), static_cast<GLsizei>(values.size()), GL_FALSE, glm::value_ptr(values[0]));
}

void Shader::bindBlock(const char* name, GLuint binding)
{
	GLuint index = glGetUniformBlockIndex(program, name);
	if (index != GL_INVALID_INDEX)
		glUniformBlockBinding(program, index, binding);
}
