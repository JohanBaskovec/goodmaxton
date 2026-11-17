#include "render.h"

#include <math.h>
#include <stdlib.h>

int fb_resize(Framebuffer *fb, int w, int h)
{
    if (w <= 0 || h <= 0)
        return -1;
    uint32_t *p = realloc(fb->px, (size_t)w * (size_t)h * sizeof *p);
    if (!p)
        return -1;
    fb->px = p;
    fb->w = w;
    fb->h = h;
    return 0;
}

void fb_free(Framebuffer *fb)
{
    free(fb->px);
    fb->px = NULL;
    fb->w = fb->h = 0;
}

static uint32_t pack(float r, float g, float b)
{
    uint32_t ri = (uint32_t)(r * 255.0f + 0.5f);
    uint32_t gi = (uint32_t)(g * 255.0f + 0.5f);
    uint32_t bi = (uint32_t)(b * 255.0f + 0.5f);
    return (ri << 16) | (gi << 8) | bi;
}

void fb_clear(Framebuffer *fb, Color c)
{
    uint32_t v = pack(c.r, c.g, c.b);
    size_t n = (size_t)fb->w * (size_t)fb->h;
    for (size_t i = 0; i < n; i++)
        fb->px[i] = v;
}

/* Mélange la couleur c avec une couverture cov (0..1) sur le pixel (x, y). */
static void blend(Framebuffer *fb, int x, int y, Color c, float cov)
{
    float a = c.a * cov;
    if (a <= 0.0f)
        return;
    uint32_t *p = &fb->px[(size_t)y * (size_t)fb->w + (size_t)x];
    if (a >= 1.0f) {
        *p = pack(c.r, c.g, c.b);
        return;
    }
    float dr = (float)((*p >> 16) & 0xFF) / 255.0f;
    float dg = (float)((*p >> 8) & 0xFF) / 255.0f;
    float db = (float)(*p & 0xFF) / 255.0f;
    *p = pack(dr + (c.r - dr) * a, dg + (c.g - dg) * a, db + (c.b - db) * a);
}

static float clamp01(float v)
{
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

void fb_fill_rect(Framebuffer *fb, float x0, float y0, float x1, float y1, Color c)
{
    if (x0 > x1) { float t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { float t = y0; y0 = y1; y1 = t; }
    int ix0 = (int)floorf(x0), ix1 = (int)ceilf(x1);
    int iy0 = (int)floorf(y0), iy1 = (int)ceilf(y1);
    if (ix0 < 0) ix0 = 0;
    if (iy0 < 0) iy0 = 0;
    if (ix1 > fb->w) ix1 = fb->w;
    if (iy1 > fb->h) iy1 = fb->h;
    for (int y = iy0; y < iy1; y++) {
        float cy = clamp01(fminf((float)y + 1.0f, y1) - fmaxf((float)y, y0));
        for (int x = ix0; x < ix1; x++) {
            float cx = clamp01(fminf((float)x + 1.0f, x1) - fmaxf((float)x, x0));
            blend(fb, x, y, c, cx * cy);
        }
    }
}

void fb_line(Framebuffer *fb, float x0, float y0, float x1, float y1, float width, Color c)
{
    float r = width * 0.5f;
    float pad = r + 1.0f;
    int ix0 = (int)floorf(fminf(x0, x1) - pad), ix1 = (int)ceilf(fmaxf(x0, x1) + pad);
    int iy0 = (int)floorf(fminf(y0, y1) - pad), iy1 = (int)ceilf(fmaxf(y0, y1) + pad);
    if (ix0 < 0) ix0 = 0;
    if (iy0 < 0) iy0 = 0;
    if (ix1 > fb->w) ix1 = fb->w;
    if (iy1 > fb->h) iy1 = fb->h;

    float dx = x1 - x0, dy = y1 - y0;
    float len2 = dx * dx + dy * dy;
    for (int y = iy0; y < iy1; y++) {
        for (int x = ix0; x < ix1; x++) {
            float px = (float)x + 0.5f - x0, py = (float)y + 0.5f - y0;
            float t = len2 > 0.0f ? clamp01((px * dx + py * dy) / len2) : 0.0f;
            float ex = px - t * dx, ey = py - t * dy;
            float d = sqrtf(ex * ex + ey * ey);
            blend(fb, x, y, c, clamp01(r - d + 0.5f));
        }
    }
}

void fb_disc(Framebuffer *fb, float cx, float cy, float r, Color c)
{
    fb_ellipse(fb, cx, cy, r, r, c);
}

void fb_ellipse(Framebuffer *fb, float cx, float cy, float rx, float ry, Color c)
{
    if (rx <= 0.0f || ry <= 0.0f)
        return;
    int ix0 = (int)floorf(cx - rx - 1.0f), ix1 = (int)ceilf(cx + rx + 1.0f);
    int iy0 = (int)floorf(cy - ry - 1.0f), iy1 = (int)ceilf(cy + ry + 1.0f);
    if (ix0 < 0) ix0 = 0;
    if (iy0 < 0) iy0 = 0;
    if (ix1 > fb->w) ix1 = fb->w;
    if (iy1 > fb->h) iy1 = fb->h;

    float rmin = fminf(rx, ry);
    for (int y = iy0; y < iy1; y++) {
        for (int x = ix0; x < ix1; x++) {
            /* Distance normalisée au bord, ramenée en pixels (approximation
             * du premier ordre, exacte pour un cercle). */
            float nx = ((float)x + 0.5f - cx) / rx;
            float ny = ((float)y + 0.5f - cy) / ry;
            float d = (sqrtf(nx * nx + ny * ny) - 1.0f) * rmin;
            blend(fb, x, y, c, clamp01(0.5f - d));
        }
    }
}
