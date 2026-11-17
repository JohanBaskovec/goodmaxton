#ifndef GOODMAXTON_RENDER_H
#define GOODMAXTON_RENDER_H

#include <stdint.h>

/* Framebuffer logiciel XRGB8888. Toutes les primitives sont antialiasées. */
typedef struct {
    uint32_t *px;
    int w, h;
} Framebuffer;

/* Couleur en floats 0..1, alpha pour le mélange. */
typedef struct {
    float r, g, b, a;
} Color;

#define RGB(r, g, b) ((Color){(r) / 255.0f, (g) / 255.0f, (b) / 255.0f, 1.0f})
#define RGBA(r, g, b, a) ((Color){(r) / 255.0f, (g) / 255.0f, (b) / 255.0f, (a)})

/* Alloue ou redimensionne le framebuffer. Retourne 0 si OK. */
int fb_resize(Framebuffer *fb, int w, int h);
void fb_free(Framebuffer *fb);

void fb_clear(Framebuffer *fb, Color c);

/* Rectangle plein, bords antialiasés (coordonnées en pixels, flottantes). */
void fb_fill_rect(Framebuffer *fb, float x0, float y0, float x1, float y1, Color c);

/* Segment épais à bouts arrondis. */
void fb_line(Framebuffer *fb, float x0, float y0, float x1, float y1, float width, Color c);

/* Disque plein. */
void fb_disc(Framebuffer *fb, float cx, float cy, float r, Color c);

/* Ellipse pleine (demi-axes rx, ry). */
void fb_ellipse(Framebuffer *fb, float cx, float cy, float rx, float ry, Color c);

#endif
