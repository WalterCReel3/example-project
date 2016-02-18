#include <cmath>
#include <stdexcept>
#include <SDL.h>
#include <SDL_ttf.h>
#include <GL/glew.h>
#include <util/logging.hpp>
#include <gfx/utils.hpp>

namespace gfx
{

using namespace std;
using util::logging;

inline int make_texture_size(int x)
{
    double lb2 = log(x) / log(2);
    return (int)((pow(2, ceil(lb2))) + 0.5);
}

void render_surface(SDL_Surface *surface, SDL_Rect *rect)
{
    int w = surface->w;
    int h = surface->h;
    GLuint texture;

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // Load the texture.  NOTE: Could release surfaces at this point
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
                 /// glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_ABGR_EXT,
                 GL_UNSIGNED_BYTE, surface->pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);

    // glEnable(GL_BLEND);
    // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glBegin(GL_QUADS);

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(rect->x, rect->y);

    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(rect->x + w, rect->y);

    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(rect->x + w, rect->y + h);

    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(rect->x, rect->y + h);

    glEnd();

    glFinish();

    rect->w = surface->w;
    rect->h = surface->h;

    // glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glDeleteTextures(1, &texture);
}

void render_text(char *str, TTF_Font *font, SDL_Color color, SDL_Rect *rect)
{
    SDL_Surface *initial;
    SDL_Surface *intermediary;
    int w;
    int h;
    GLuint texture;

    // Create the text surface
    initial = TTF_RenderUTF8_Blended(font, str, color);

    // Calculate texture size for opengl
    w = make_texture_size(initial->w);
    h = make_texture_size(initial->h);

    // Create the texture for opengl
    intermediary = SDL_CreateRGBSurface(
                       0, w, h, 32, R_MASK, G_MASK, B_MASK, A_MASK);

    // Blit to the proper texture
    SDL_BlitSurface(initial, 0, intermediary, 0);
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // Load the texture.  NOTE: Could release surfaces at this point
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 intermediary->pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glColor3f(1.0f, 1.0f, 1.0f);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBegin(GL_QUADS);
    glColor4f(1.0f, 1.0f, 1.0f, 0.5f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(rect->x, rect->y);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(rect->x + w, rect->y);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(rect->x + w, rect->y + h);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(rect->x, rect->y + h);
    glEnd();

    glFinish();

    rect->w = initial->w;
    rect->h = initial->h;

    glDisable(GL_BLEND);
    SDL_FreeSurface(initial);
    SDL_FreeSurface(intermediary);
    glDeleteTextures(1, &texture);
}

} // namespace wogl

// vim: set sts=2 sw=2 expandtab:
