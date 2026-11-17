#ifndef GOODMAXTON_DRAW_H
#define GOODMAXTON_DRAW_H

#include "game.h"
#include "render.h"

typedef struct {
    float scale;  /* pixels par mètre */
    float ox, oy; /* position du centre du filet à l'écran */
} View;

View view_fit(int w, int h);

/* Convertit un point écran (pixels du framebuffer) en mètres sur le terrain. */
void view_to_court(const View *v, float sx, float sy, float *x, float *y);

/* Dessine la scène complète (terrain, joueurs...) dans le framebuffer. */
void draw_scene(Framebuffer *fb, const Game *g);

#endif
