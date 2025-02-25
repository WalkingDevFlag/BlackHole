#include "shader.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <GL/glew.h>
#include <omp.h>

static std::string readFile(const std::string &file) {
  std::string VertexShaderCode;
  std::ifstream ifs(file, std::ios::in);
  if (ifs.is_open()) {
    std::stringstream ss;
    #pragma omp parallel
    {
      #pragma omp single
      ss << ifs.rdbuf();
    }
    return ss.str();
  } else {
    throw "Failed to open file: " + file;
  }
}

static GLuint compileShader(const std::string &shaderSource,
                            GLenum shaderType) {
  // Create shader
  GLuint shader = glCreateShader(shaderType);

  // Compile shader
  char const *pShaderSource = shaderSource.c_str();
  glShaderSource(shader, 1, &pShaderSource, nullptr);
  glCompileShader(shader);

  GLint success = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (success == GL_FALSE) {
    GLint maxLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

    // The maxLength includes the NULL character
    std::vector<GLchar> infoLog(maxLength);
    glGetShaderInfoLog(shader, maxLength, &maxLength, &infoLog[0]);
    #pragma omp parallel
    {
      #pragma omp single
      std::cout << infoLog[0] << std::endl;
    }
    glDeleteShader(shader);
    throw "Failed to compile the shader.";
  }

  return shader;
}

GLuint createShaderProgram(const std::string &vertexShaderFile,
                           const std::string &fragmentShaderFile) {

  GLuint vertexShader, fragmentShader;

  #pragma omp parallel sections
  {
    #pragma omp section
    {
      // Compile vertex shader
      std::cout << "Compiling vertex shader: " << vertexShaderFile << std::endl;
      vertexShader = compileShader(readFile(vertexShaderFile), GL_VERTEX_SHADER);
    }

    #pragma omp section
    {
      // Compile fragment shader
      std::cout << "Compiling fragment shader: " << fragmentShaderFile << std::endl;
      fragmentShader = compileShader(readFile(fragmentShaderFile), GL_FRAGMENT_SHADER);
    }
  }

  // Create shader program.
  GLuint program = glCreateProgram();
  glAttachShader(program, vertexShader);
  glAttachShader(program, fragmentShader);

  // Link the program.
  glLinkProgram(program);
  GLint isLinked = GL_FALSE;
  glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
  if (isLinked == GL_FALSE) {
    int maxLength;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
    if (maxLength > 0) {
      std::vector<GLchar> infoLog(maxLength);
      glGetProgramInfoLog(program, maxLength, NULL, &infoLog[0]);
      #pragma omp parallel
      {
        #pragma omp single
        std::cout << infoLog[0] << std::endl;
      }
      throw "Failed to link the shader.";
    }
  }

  // Detach shaders after a successful link.
  #pragma omp parallel sections
  {
    #pragma omp section
    glDetachShader(program, vertexShader);

    #pragma omp section
    glDetachShader(program, fragmentShader);
  }

  #pragma omp parallel sections
  {
    #pragma omp section
    glDeleteShader(vertexShader);

    #pragma omp section
    glDeleteShader(fragmentShader);
  }

  return program;
}
