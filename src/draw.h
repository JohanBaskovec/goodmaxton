#ifndef GOODMAXTON_DRAW_H
#define GOODMAXTON_DRAW_H

#include "game.h"
#include "render.h"

typedef struct {
    float scale;  /* pixels par mètre */
    float ox, oy; /* position du centre du filet à l'écran */
} View;

View view_fit(int w, int h);

/* Dessine la scène complète (terrain, joueurs...) dans le framebuffer. */
void draw_scene(Framebuffer *fb, const Game *g);

#endif
