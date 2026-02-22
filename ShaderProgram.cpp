// Simple shader program helper: compile/link GLSL shaders and set uniforms.
// The file contains small helpers that wrap typical GL calls and provide
// basic error reporting on compilation/link failures.

#include <iostream>
#include <fstream>
#include <sstream>

#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include "ShaderProgram.hpp"

// set uniform according to name 
// https://docs.gl/gl4/glUniform

ShaderProgram::ShaderProgram(const std::filesystem::path& VS_file, const std::filesystem::path& FS_file) {
	std::vector<GLuint> shader_ids;
	
    // compile shaders and store IDs for linker
	shader_ids.push_back(compile_shader(VS_file, GL_VERTEX_SHADER));
	shader_ids.push_back(compile_shader(FS_file, GL_FRAGMENT_SHADER));

	// link all compiled shaders into shader_program 
    ID = link_shader(shader_ids);
}

// Set float uniform by name. Prints a warning if uniform is not found.
void ShaderProgram::setUniform(const std::string& name, const float val) {
	auto loc = glGetUniformLocation(ID, name.c_str());
	if (loc == -1) {
		std::cerr << "no uniform with name:" << name << '\n';
		return;
	}
	glUniform1f(loc, val);
}

// Set int uniform by name.
void ShaderProgram::setUniform(const std::string& name, const int val) {
	auto loc = glGetUniformLocation(ID, name.c_str());
	if (loc == -1) {
		std::cerr << "no uniform with name:" << name << '\n';
		return;
	}
	glUniform1i(loc, val);
}

// Set vec2 uniform by name.
// Uses glm::value_ptr to pass contiguous memory to GL.
void ShaderProgram::setUniform(const std::string& name, const glm::vec2 val) {
	auto loc = glGetUniformLocation(ID, name.c_str());
	if (loc == -1) {
		std::cerr << "no uniform with name:" << name << '\n';
		return;
	}
	glUniform2fv(loc, 1, glm::value_ptr(val));
}

// Set vec3 uniform by name.
void ShaderProgram::setUniform(const std::string& name, const glm::vec3 val) {
	auto loc = glGetUniformLocation(ID, name.c_str());
	if (loc == -1) {
		std::cerr << "no uniform with name:" << name << '\n';
		return;
	}
	glUniform3fv(loc, 1, glm::value_ptr(val));
}

// Set vec4 uniform by name.
void ShaderProgram::setUniform(const std::string& name, const glm::vec4 in_vec4) {
	auto loc = glGetUniformLocation(ID, name.c_str());
	if (loc == -1) {
		std::cerr << "no uniform with name:" << name << '\n';
		return;
	}
	glUniform4fv(loc, 1, glm::value_ptr(in_vec4));
}

// Set mat3 uniform by name. GL expects column-major layout; glm::value_ptr provides that.
void ShaderProgram::setUniform(const std::string& name, const glm::mat3 val) {
	auto loc = glGetUniformLocation(ID, name.c_str());
	if (loc == -1) {
		std::cerr << "no uniform with name:" << name << '\n';
		return;
	}
	glUniformMatrix3fv(loc, 1, GL_FALSE, glm::value_ptr(val));
}

// Set mat4 uniform by name.
void ShaderProgram::setUniform(const std::string& name, const glm::mat4 val) {
	auto loc = glGetUniformLocation(ID, name.c_str());
	if (loc == -1) {
		std::cerr << "no uniform with name:" << name << '\n';
		return;
	}
	glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(val));
}

// Retrieve shader compile log as string (empty when no log).
std::string ShaderProgram::getShaderInfoLog(const GLuint obj) {
		int infologLength = 0;
		std::string s;
		glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &infologLength);
		if (infologLength > 0) {
			std::vector<char> v(infologLength);
			glGetShaderInfoLog(obj, infologLength, NULL, v.data());
			s.assign(begin(v), end(v));
		}
		return s;
}

// Retrieve program link log as string (empty when no log).
std::string ShaderProgram::getProgramInfoLog(const GLuint obj) {
	int infologLength = 0;
	std::string s;
	glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &infologLength);
	if (infologLength > 0) {
		std::vector<char> v(infologLength);
		glGetProgramInfoLog(obj, infologLength, NULL, v.data());
		s.assign(begin(v), end(v));
	}
	return s;
}

// Compile a single GLSL shader from file. Throws on compile failure.
GLuint ShaderProgram::compile_shader(const std::filesystem::path& source_file, const GLenum type) {
	GLuint shader_h = glCreateShader(type);

	// Read shader source from disk and attach to shader object.
	std::string shader_src = textFileRead(source_file);
	const char* shader_cstg = shader_src.c_str();
	glShaderSource(shader_h, 1, &shader_cstg, NULL);

	// Compile and verify the result. On failure print the compiler output and throw.
	glCompileShader(shader_h);
	{ // check compile result, display error (if any)
		GLint cmpl_status;
		glGetShaderiv(shader_h, GL_COMPILE_STATUS, &cmpl_status);
		if (cmpl_status == GL_FALSE) {
			std::cerr << getShaderInfoLog(shader_h);
			throw std::runtime_error("Shader compile err.\n");
		}
	}

	return shader_h;
}

// Link a list of compiled shader objects into a program. Throws on link failure.
GLuint ShaderProgram::link_shader(const std::vector<GLuint> shader_ids) {
	GLuint prog_h = glCreateProgram();

	// Attach all compiled shader objects.
	for (const GLuint id : shader_ids)
		glAttachShader(prog_h, id);

	// Link program and check status.
	glLinkProgram(prog_h);
	{ // check link result, print info & throw error (if any)
		GLint status;
		glGetProgramiv(prog_h, GL_LINK_STATUS, &status);
		if (status == GL_FALSE) {
			std::cerr << getProgramInfoLog(prog_h);
 			throw std::runtime_error("Link err.\n");
		}
	}

	// Detach and delete shader objects; program keeps its own copy after linking.
	for (const GLuint id : shader_ids)
		glDetachShader(prog_h, id);

	for (const GLuint id : shader_ids)
		glDeleteShader(id);

	return prog_h;
}

// Read a text file entirely into std::string. Throws if file cannot be opened.
std::string ShaderProgram::textFileRead(const std::filesystem::path& filename) {
	std::ifstream file(filename);
	if (!file.is_open())
		throw std::runtime_error(std::string("Error opening file: ") + filename.string());
	std::stringstream ss;
	ss << file.rdbuf();
	return ss.str();
}
