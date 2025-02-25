#include "render.h"
#include "shader.h"

#include <iostream>
#include <omp.h>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

GLuint createColorTexture(int width, int height, bool hdr) {
  GLuint colorTexture;
  glGenTextures(1, &colorTexture);

  glBindTexture(GL_TEXTURE_2D, colorTexture);
  glTexImage2D(GL_TEXTURE_2D, 0, hdr ? GL_RGB16F : GL_RGB, width, height, 0,
               GL_RGB, hdr ? GL_FLOAT : GL_UNSIGNED_BYTE, NULL);
  
  #pragma omp parallel sections
  {
    #pragma omp section
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    
    #pragma omp section
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  }

  return colorTexture;
}

GLuint createFramebuffer(const FramebufferCreateInfo &info) {
  GLuint framebuffer;

  glGenFramebuffers(1, &framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         info.colorTexture, 0);

  if (info.createDepthBuffer) {
    GLuint rbo;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, info.width,
                          info.height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, rbo);
  }

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    std::cout << "ERROR: Framebuffer is not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return 0;
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  return framebuffer;
}

GLuint createQuadVAO() {
  std::vector<glm::vec3> vertices;

  #pragma omp parallel sections
  {
    #pragma omp section
    {
      vertices.push_back(glm::vec3(-1, -1, 0));
      vertices.push_back(glm::vec3(-1, 1, 0));
      vertices.push_back(glm::vec3(1, 1, 0));
    }

    #pragma omp section
    {
      vertices.push_back(glm::vec3(1, 1, 0));
      vertices.push_back(glm::vec3(1, -1, 0));
      vertices.push_back(glm::vec3(-1, -1, 0));
    }
  }

  GLuint vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  GLuint vbo;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3),
               &vertices[0], GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);

  glBindVertexArray(0);

  return vao;
}

static bool bindToTextureUnit(GLuint program, const std::string &name,
                              GLenum textureType, GLuint texture,
                              int textureUnitIndex) {
  GLint loc = glGetUniformLocation(program, name.c_str());
  if (loc != -1) {
    glUniform1i(loc, textureUnitIndex);

    glActiveTexture(GL_TEXTURE0 + textureUnitIndex);
    glBindTexture(textureType, texture);
    return true;
  } else {
    std::cout << "WARNING: uniform " << name << " is not found in shader"
              << std::endl;
    return false;
  }
}

void renderToTexture(const RenderToTextureInfo &rtti) {
  static std::map<GLuint, GLuint> textureFramebufferMap;
  GLuint targetFramebuffer;
  if (!textureFramebufferMap.count(rtti.targetTexture)) {
    FramebufferCreateInfo createInfo;
    createInfo.colorTexture = rtti.targetTexture;
    targetFramebuffer = createFramebuffer(createInfo);
    textureFramebufferMap[rtti.targetTexture] = targetFramebuffer;
  } else {
    targetFramebuffer = textureFramebufferMap[rtti.targetTexture];
  }

  static std::map<std::string, GLuint> shaderProgramMap;
  GLuint program;
  if (!shaderProgramMap.count(rtti.fragShader)) {
    program = createShaderProgram(rtti.vertexShader, rtti.fragShader);
    shaderProgramMap[rtti.fragShader] = program;
  } else {
    program = shaderProgramMap[rtti.fragShader];
  }

  glBindFramebuffer(GL_FRAMEBUFFER, targetFramebuffer);

  glViewport(0, 0, rtti.width, rtti.height);

  glDisable(GL_DEPTH_TEST);

  glClearColor(0.0f, 1.0f, 1.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  glUseProgram(program);

  #pragma omp parallel
  {
    #pragma omp single
    {
      glUniform2f(glGetUniformLocation(program, "resolution"),
                  (float)rtti.width, (float)rtti.height);

      glUniform1f(glGetUniformLocation(program, "time"), (float)glfwGetTime());
    }

    #pragma omp for
    for (auto const &[name, val] : rtti.floatUniforms) {
      GLint loc = glGetUniformLocation(program, name.c_str());
      if (loc != -1) {
        glUniform1f(loc, val);
      } else {
        std::cout << "WARNING: uniform " << name << " is not found"
                  << std::endl;
      }
    }

    #pragma omp single
    {
      int textureUnit = 0;
      for (auto const &[name, texture] : rtti.textureUniforms) {
        bindToTextureUnit(program, name, GL_TEXTURE_2D, texture, textureUnit++);
      }
      for (auto const &[name, texture] : rtti.cubemapUniforms) {
        bindToTextureUnit(program, name, GL_TEXTURE_CUBE_MAP, texture,
                          textureUnit++);
      }
    }
  }

  glDrawArrays(GL_TRIANGLES, 0, 6);

  glUseProgram(0);
}
