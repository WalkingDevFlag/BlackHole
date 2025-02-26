#include "texture.h"
#include <iostream>
#include <vector>
#include <stb_image.h>
#include <omp.h>

GLuint loadTexture2D(const std::string &file, bool repeat) {
  GLuint textureID;
  glGenTextures(1, &textureID);

  int width, height, comp;
  unsigned char *data = stbi_load(file.c_str(), &width, &height, &comp, 0);
  if (data) {
    GLenum format;
    GLenum internalFormat;
    #pragma omp parallel sections
    {
      #pragma omp section
      {
        if (comp == 1) {
          format = GL_RED;
          internalFormat = GL_RED;
        } else if (comp == 3) {
          format = GL_RGB;
          internalFormat = GL_SRGB;
        } else if (comp == 4) {
          format = GL_RGBA;
          internalFormat = GL_SRGB_ALPHA;
        }
      }
      #pragma omp section
      {
        glBindTexture(GL_TEXTURE_2D, textureID);
      }
    }

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format,
                 GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    #pragma omp parallel sections
    {
      #pragma omp section
      {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                        repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                        repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
      }
      #pragma omp section
      {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      }
    }

    stbi_image_free(data);
  } else {
    std::cout << "ERROR: Failed to load texture at: " << file << std::endl;
    stbi_image_free(data);
  }

  return textureID;
}

GLuint loadCubemap(const std::string &cubemapDir) {
  const std::vector<std::string> faces = {"right", "left", "top",
                                          "bottom", "front", "back"};

  GLuint textureID;
  glGenTextures(1, &textureID);
  glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

  int width, height, comp;
  #pragma omp parallel for
  for (GLuint i = 0; i < faces.size(); i++) {
    unsigned char *data =
        stbi_load((cubemapDir + "/" + faces[i] + ".png").c_str(), &width,
                  &height, &comp, 0);
    if (data) {
      glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_SRGB, width,
                   height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
      stbi_image_free(data);
    } else {
      #pragma omp critical
      {
        std::cout << "Cubemap texture failed to load at path: "
                  << (cubemapDir + "/" + faces[i] + ".png").c_str() << std::endl;
      }
      stbi_image_free(data);
    }
  }

  #pragma omp parallel sections
  {
    #pragma omp section
    {
      glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    #pragma omp section
    {
      glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    }
  }

  return textureID;
}
