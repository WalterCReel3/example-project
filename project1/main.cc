#include <SDL.h>
#include <GL/glew.h>
#include <SDL_opengl.h>
#include <iostream>

/*
 * Lesson 0: Test to make sure SDL is setup properly
 */
int main(int, char**) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
      std::cout << "SDL_Init Error: " << SDL_GetError() << std::endl;
      return 1;
  }

  SDL_Window *window = SDL_CreateWindow("My Game Window",
					SDL_WINDOWPOS_UNDEFINED,
					SDL_WINDOWPOS_UNDEFINED,
					640, 480,
					SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);
  SDL_GLContext context = SDL_GL_CreateContext(window);
  if (context == NULL) {
      std::cerr << "Error creating the OpenGL context\n" << std::endl;
  }

  const unsigned char *version = glGetString(GL_VERSION);
  if (version == NULL) {
      std::cerr << "Error getting the OpenGL version\n" << std::endl;
  }

  SDL_GL_MakeCurrent(window, context);

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

  GLenum glew_status = glewInit();
  if (glew_status != 0) {
      std::cerr << "Error initializing GLEW\n" << std::endl;
  }

  SDL_Quit();

  return 0;
}
