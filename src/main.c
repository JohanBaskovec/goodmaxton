#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "render.h"

/* Dimensions officielles du terrain, en mètres. Repère centré sur le filet :
 * x vers la droite de l'écran, y vers le bas (côté humain). */
#define COURT_HALF_LENGTH 6.70f
#define COURT_HALF_WIDTH_DOUBLES 3.05f
#define COURT_HALF_WIDTH_SINGLES 2.59f
#define SHORT_SERVICE_LINE 1.98f
#define LONG_SERVICE_LINE_DOUBLES 5.94f
#define LINE_WIDTH 0.04f
#define VIEW_MARGIN 1.2f

typedef struct {
    float scale; /* pixels par mètre */
    float ox, oy; /* position du centre du filet à l'écran */
} View;

static View view_fit(int w, int h)
{
    float sw = (float)w / (2.0f * (COURT_HALF_WIDTH_DOUBLES + VIEW_MARGIN));
    float sh = (float)h / (2.0f * (COURT_HALF_LENGTH + VIEW_MARGIN));
    View v;
    v.scale = sw < sh ? sw : sh;
    v.ox = (float)w * 0.5f;
    v.oy = (float)h * 0.5f;
    return v;
}

static float vx(const View *v, float x) { return v->ox + x * v->scale; }
static float vy(const View *v, float y) { return v->oy + y * v->scale; }

static void draw_line_m(Framebuffer *fb, const View *v, float x0, float y0, float x1, float y1,
                        Color c)
{
    fb_line(fb, vx(v, x0), vy(v, y0), vx(v, x1), vy(v, y1), LINE_WIDTH * v->scale, c);
}

static void draw_court(Framebuffer *fb, const View *v)
{
    const Color floor_out = RGB(34, 52, 78);
    const Color floor_in = RGB(46, 120, 92);
    const Color line = RGB(240, 244, 240);

    fb_clear(fb, floor_out);

    fb_fill_rect(fb, vx(v, -COURT_HALF_WIDTH_DOUBLES), vy(v, -COURT_HALF_LENGTH),
                 vx(v, COURT_HALF_WIDTH_DOUBLES), vy(v, COURT_HALF_LENGTH), floor_in);

    const float dw = COURT_HALF_WIDTH_DOUBLES, sw = COURT_HALF_WIDTH_SINGLES;
    const float hl = COURT_HALF_LENGTH, ss = SHORT_SERVICE_LINE;

    /* Contour du double et lignes de fond. */
    draw_line_m(fb, v, -dw, -hl, dw, -hl, line);
    draw_line_m(fb, v, -dw, hl, dw, hl, line);
    draw_line_m(fb, v, -dw, -hl, -dw, hl, line);
    draw_line_m(fb, v, dw, -hl, dw, hl, line);
    /* Lignes latérales du simple. */
    draw_line_m(fb, v, -sw, -hl, -sw, hl, line);
    draw_line_m(fb, v, sw, -hl, sw, hl, line);
    /* Lignes de service court. */
    draw_line_m(fb, v, -dw, -ss, dw, -ss, line);
    draw_line_m(fb, v, -dw, ss, dw, ss, line);
    /* Lignes de service long du double. */
    draw_line_m(fb, v, -dw, -LONG_SERVICE_LINE_DOUBLES, dw, -LONG_SERVICE_LINE_DOUBLES, line);
    draw_line_m(fb, v, -dw, LONG_SERVICE_LINE_DOUBLES, dw, LONG_SERVICE_LINE_DOUBLES, line);
    /* Ligne médiane. */
    draw_line_m(fb, v, 0.0f, -hl, 0.0f, -ss, line);
    draw_line_m(fb, v, 0.0f, ss, 0.0f, hl, line);

    /* Filet et poteaux. */
    const float post_x = dw + 0.1f;
    fb_line(fb, vx(v, -post_x), vy(v, 0.0f), vx(v, post_x), vy(v, 0.0f), 0.05f * v->scale,
            RGB(250, 250, 250));
    fb_disc(fb, vx(v, -post_x), vy(v, 0.0f), 0.09f * v->scale, RGB(60, 60, 66));
    fb_disc(fb, vx(v, post_x), vy(v, 0.0f), 0.09f * v->scale, RGB(60, 60, 66));
}

#define DEFAULT_WIDTH 720
#define DEFAULT_HEIGHT 1080

typedef struct {
    bool headless;
    long frames;           /* nombre de pas de simulation en mode headless */
    const char *dump_path; /* image PPM du framebuffer final (headless) */
} Options;

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s [--headless [--frames N] [--dump fichier.ppm]]\n"
            "  --headless      aucune fenêtre ni SDL vidéo ; simule puis quitte\n"
            "  --frames N      nombre de pas de simulation (défaut 0)\n"
            "  --dump fichier  écrit le framebuffer final au format PPM\n",
            prog);
}

static int parse_args(int argc, char **argv, Options *o)
{
    memset(o, 0, sizeof *o);
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--headless") == 0) {
            o->headless = true;
        } else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            char *end;
            o->frames = strtol(argv[++i], &end, 10);
            if (*end != '\0' || o->frames < 0)
                return -1;
        } else if (strcmp(argv[i], "--dump") == 0 && i + 1 < argc) {
            o->dump_path = argv[++i];
        } else {
            return -1;
        }
    }
    if (!o->headless && (o->frames != 0 || o->dump_path))
        return -1;
    return 0;
}

/* Dessine une frame complète dans le framebuffer. */
static void render_frame(Framebuffer *fb)
{
    View view = view_fit(fb->w, fb->h);
    draw_court(fb, &view);
}

static int dump_ppm(const Framebuffer *fb, const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) {
        perror(path);
        return -1;
    }
    fprintf(f, "P6\n%d %d\n255\n", fb->w, fb->h);
    for (size_t i = 0; i < (size_t)fb->w * (size_t)fb->h; i++) {
        uint32_t p = fb->px[i];
        unsigned char rgb[3] = {(unsigned char)(p >> 16), (unsigned char)(p >> 8),
                                (unsigned char)p};
        fwrite(rgb, 1, 3, f);
    }
    if (fclose(f) != 0) {
        perror(path);
        return -1;
    }
    return 0;
}

/* Mode headless : pas de SDL, simulation déterministe à pas fixe, image finale. */
static int run_headless(const Options *o)
{
    Framebuffer fb = {0};
    if (fb_resize(&fb, DEFAULT_WIDTH, DEFAULT_HEIGHT) != 0) {
        fprintf(stderr, "Allocation du framebuffer impossible\n");
        return 1;
    }
    for (long step = 0; step < o->frames; step++) {
        /* La simulation viendra ici (un appel par pas de 1/120 s). */
    }
    render_frame(&fb);
    int rc = 0;
    if (o->dump_path && dump_ppm(&fb, o->dump_path) != 0)
        rc = 1;
    fb_free(&fb);
    return rc;
}

static int run_windowed(void)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window =
        SDL_CreateWindow("Goodmaxton", DEFAULT_WIDTH, DEFAULT_HEIGHT, SDL_WINDOW_RESIZABLE);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderVSync(renderer, 1);

    Framebuffer fb = {0};
    SDL_Texture *tex = NULL;
    int tex_w = 0, tex_h = 0;

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT)
                running = false;
            else if (e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == SDL_SCANCODE_ESCAPE)
                running = false;
        }

        int w, h;
        SDL_GetWindowSizeInPixels(window, &w, &h);
        if (w <= 0 || h <= 0) {
            SDL_Delay(16);
            continue;
        }
        if (w != tex_w || h != tex_h) {
            if (tex)
                SDL_DestroyTexture(tex);
            tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888,
                                    SDL_TEXTUREACCESS_STREAMING, w, h);
            if (!tex || fb_resize(&fb, w, h) != 0) {
                fprintf(stderr, "Allocation du framebuffer impossible: %s\n", SDL_GetError());
                break;
            }
            tex_w = w;
            tex_h = h;
        }

        render_frame(&fb);

        SDL_UpdateTexture(tex, NULL, fb.px, fb.w * (int)sizeof(uint32_t));
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, tex, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    if (tex)
        SDL_DestroyTexture(tex);
    fb_free(&fb);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

int main(int argc, char **argv)
{
    Options opts;
    if (parse_args(argc, argv, &opts) != 0) {
        usage(argv[0]);
        return 2;
    }
    return opts.headless ? run_headless(&opts) : run_windowed();
}
