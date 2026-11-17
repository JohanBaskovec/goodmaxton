#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "draw.h"
#include "game.h"
#include "render.h"

#define DEFAULT_WIDTH 720
#define DEFAULT_HEIGHT 1080
#define MAX_SCRIPT_SEGMENTS 256
#define MAX_SHOTS 256
#define MAX_FRAME_TIME 0.25 /* secondes : évite la spirale de la mort */

/* Un segment de script : des entrées maintenues pendant `frames` pas. */
typedef struct {
    Input in;
    long frames;
} ScriptSegment;

typedef struct {
    bool headless;
    long frames;           /* nombre de pas de simulation (headless) */
    const char *script;    /* entrées scriptées, ex. "u:120,ur:60,-:30" */
    const char *shot_path; /* capture (.png ou .ppm) ; contient %d avec --shot-at/-every */
    const char *shot_at;   /* pas des captures, ex. "0,60,120" */
    long shot_every;       /* une capture tous les N pas */
    uint64_t seed;         /* graine du générateur aléatoire de la simulation */
    bool print_state;      /* affiche l'état final sur stdout (headless) */
} Options;

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s [--headless [options]]\n"
            "  --headless        aucune fenêtre ni SDL vidéo ; simule puis quitte\n"
            "  --frames N        nombre de pas de simulation, sans entrées (défaut 0)\n"
            "  --script S        entrées maintenues, segments \"touches:pas\" séparés par des\n"
            "                    virgules ; touches = u d l r combinés, ou - pour aucune.\n"
            "                    Exemple : u:120,ur:60,-:30\n"
            "  --seed N          graine aléatoire de la simulation (défaut 1)\n"
            "  --shot f          capture du framebuffer final, au format .png ou .ppm\n"
            "  --shot-at L       captures aux pas listés (ex. 0,60,120) ; f doit contenir %%d\n"
            "  --shot-every N    une capture tous les N pas ; f doit contenir %%d\n"
            "  --state           affiche l'état final de la simulation sur stdout\n",
            prog);
}

static bool parse_long(const char *s, long *out)
{
    char *end;
    *out = strtol(s, &end, 10);
    return *s != '\0' && *end == '\0' && *out >= 0;
}

/* Vrai si le gabarit contient exactement un %d et aucun autre %. */
static bool path_has_step_placeholder(const char *path)
{
    int n = 0;
    for (const char *c = path; *c; c++) {
        if (*c != '%')
            continue;
        if (c[1] != 'd')
            return false;
        n++;
        c++;
    }
    return n == 1;
}

static int parse_args(int argc, char **argv, Options *o)
{
    memset(o, 0, sizeof *o);
    o->seed = 1;
    for (int i = 1; i < argc; i++) {
        long v;
        if (strcmp(argv[i], "--headless") == 0) {
            o->headless = true;
        } else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            if (!parse_long(argv[++i], &o->frames))
                return -1;
        } else if (strcmp(argv[i], "--script") == 0 && i + 1 < argc) {
            o->script = argv[++i];
        } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            if (!parse_long(argv[++i], &v))
                return -1;
            o->seed = (uint64_t)v;
        } else if (strcmp(argv[i], "--shot") == 0 && i + 1 < argc) {
            o->shot_path = argv[++i];
        } else if (strcmp(argv[i], "--shot-at") == 0 && i + 1 < argc) {
            o->shot_at = argv[++i];
        } else if (strcmp(argv[i], "--shot-every") == 0 && i + 1 < argc) {
            if (!parse_long(argv[++i], &o->shot_every) || o->shot_every == 0)
                return -1;
        } else if (strcmp(argv[i], "--state") == 0) {
            o->print_state = true;
        } else {
            return -1;
        }
    }
    bool headless_only = o->frames != 0 || o->script || o->shot_path || o->shot_at ||
                         o->shot_every != 0 || o->print_state || o->seed != 1;
    if (!o->headless && headless_only)
        return -1;
    if (o->frames != 0 && o->script)
        return -1;
    if ((o->shot_at || o->shot_every != 0) &&
        (!o->shot_path || !path_has_step_placeholder(o->shot_path)))
        return -1;
    if (o->shot_at && o->shot_every != 0)
        return -1;
    if (o->shot_path && !o->shot_at && o->shot_every == 0 && strchr(o->shot_path, '%'))
        return -1;
    return 0;
}

/* Analyse un script. Retourne le nombre de segments, ou -1 si invalide. */
static int script_parse(const char *s, ScriptSegment *out, int max)
{
    int n = 0;
    while (*s) {
        if (n >= max)
            return -1;
        ScriptSegment seg = {0};
        if (*s == '-') {
            s++;
        } else {
            if (*s == ':')
                return -1;
            for (; *s && *s != ':'; s++) {
                switch (*s) {
                case 'u': seg.in.up = true; break;
                case 'd': seg.in.down = true; break;
                case 'l': seg.in.left = true; break;
                case 'r': seg.in.right = true; break;
                default: return -1;
                }
            }
        }
        if (*s != ':')
            return -1;
        char *end;
        seg.frames = strtol(s + 1, &end, 10);
        if (end == s + 1 || seg.frames < 0)
            return -1;
        s = end;
        if (*s == ',')
            s++;
        else if (*s != '\0')
            return -1;
        out[n++] = seg;
    }
    return n;
}

static int save_ppm(const Framebuffer *fb, const char *path)
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

static int save_png(const Framebuffer *fb, const char *path)
{
    SDL_Surface *surf = SDL_CreateSurfaceFrom(fb->w, fb->h, SDL_PIXELFORMAT_XRGB8888, fb->px,
                                              fb->w * (int)sizeof(uint32_t));
    if (!surf) {
        fprintf(stderr, "%s: %s\n", path, SDL_GetError());
        return -1;
    }
    bool ok = SDL_SavePNG(surf, path);
    if (!ok)
        fprintf(stderr, "%s: %s\n", path, SDL_GetError());
    SDL_DestroySurface(surf);
    return ok ? 0 : -1;
}

/* Écrit le framebuffer ; le format dépend de l'extension (.png, sinon PPM). */
static int save_image(const Framebuffer *fb, const char *path)
{
    size_t n = strlen(path);
    if (n >= 4 && strcmp(path + n - 4, ".png") == 0)
        return save_png(fb, path);
    return save_ppm(fb, path);
}

/* Contexte d'une simulation headless : avance pas à pas et prend les captures. */
typedef struct {
    Game game;
    Framebuffer fb;
    const Options *opts;
    long shots_at[MAX_SHOTS];
    int n_shots_at;
    long step; /* nombre de pas déjà simulés */
    int rc;
} Headless;

static bool shot_due(const Headless *h)
{
    if (h->opts->shot_every != 0)
        return h->step % h->opts->shot_every == 0;
    for (int i = 0; i < h->n_shots_at; i++)
        if (h->shots_at[i] == h->step)
            return true;
    return false;
}

static void take_shot(Headless *h, const char *path)
{
    draw_scene(&h->fb, &h->game);
    if (save_image(&h->fb, path) != 0)
        h->rc = 1;
}

/* Capture éventuelle de l'état courant (avant tout pas, puis après chacun). */
static void headless_check_shot(Headless *h)
{
    if (!h->opts->shot_at && h->opts->shot_every == 0)
        return;
    if (!shot_due(h))
        return;
    char path[1024];
    snprintf(path, sizeof path, h->opts->shot_path, (int)h->step);
    take_shot(h, path);
}

static void headless_advance(Headless *h, const Input *in, long frames)
{
    for (long i = 0; i < frames; i++) {
        game_step(&h->game, in);
        h->step++;
        headless_check_shot(h);
    }
}

static bool parse_shot_list(const char *s, long *out, int *n)
{
    *n = 0;
    while (*s) {
        if (*n >= MAX_SHOTS)
            return false;
        char *end;
        long v = strtol(s, &end, 10);
        if (end == s || v < 0)
            return false;
        out[(*n)++] = v;
        s = end;
        if (*s == ',')
            s++;
        else if (*s != '\0')
            return false;
    }
    return *n > 0;
}

/* Mode headless : pas de SDL vidéo, simulation déterministe à pas fixe. */
static int run_headless(const Options *o)
{
    static ScriptSegment segs[MAX_SCRIPT_SEGMENTS];
    int nsegs = 0;
    if (o->script) {
        nsegs = script_parse(o->script, segs, MAX_SCRIPT_SEGMENTS);
        if (nsegs < 0) {
            fprintf(stderr, "Script invalide : %s\n", o->script);
            return 2;
        }
    }

    static Headless h;
    memset(&h, 0, sizeof h);
    h.opts = o;
    if (o->shot_at && !parse_shot_list(o->shot_at, h.shots_at, &h.n_shots_at)) {
        fprintf(stderr, "Liste de captures invalide : %s\n", o->shot_at);
        return 2;
    }
    if (fb_resize(&h.fb, DEFAULT_WIDTH, DEFAULT_HEIGHT) != 0) {
        fprintf(stderr, "Allocation du framebuffer impossible\n");
        return 1;
    }

    game_init(&h.game, o->seed);
    headless_check_shot(&h); /* état initial (pas 0) */
    const Input none = {0};
    if (o->script) {
        for (int i = 0; i < nsegs; i++)
            headless_advance(&h, &segs[i].in, segs[i].frames);
    } else {
        headless_advance(&h, &none, o->frames);
    }

    if (o->shot_path && !o->shot_at && o->shot_every == 0)
        take_shot(&h, o->shot_path);
    if (o->print_state)
        printf("step=%ld human x=%.4f y=%.4f vx=%.4f vy=%.4f\n", h.step,
               (double)h.game.human.x, (double)h.game.human.y, (double)h.game.human.vx,
               (double)h.game.human.vy);
    fb_free(&h.fb);
    return h.rc;
}

/* Lit le clavier par scancode : ZQSD (AZERTY) = WASD (QWERTY), même position
 * physique, quelle que soit la disposition. */
static Input read_input(void)
{
    const bool *keys = SDL_GetKeyboardState(NULL);
    Input in;
    in.up = keys[SDL_SCANCODE_W];
    in.down = keys[SDL_SCANCODE_S];
    in.left = keys[SDL_SCANCODE_A];
    in.right = keys[SDL_SCANCODE_D];
    return in;
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

    Game game;
    game_init(&game, (uint64_t)SDL_GetTicks());
    double accumulator = 0.0;
    Uint64 last = SDL_GetPerformanceCounter();
    const double freq = (double)SDL_GetPerformanceFrequency();

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT)
                running = false;
            else if (e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == SDL_SCANCODE_ESCAPE)
                running = false;
        }

        /* Pas de temps fixe, découplé du rendu. */
        Uint64 now = SDL_GetPerformanceCounter();
        double frame_time = (double)(now - last) / freq;
        last = now;
        if (frame_time > MAX_FRAME_TIME)
            frame_time = MAX_FRAME_TIME;
        accumulator += frame_time;
        Input in = read_input();
        while (accumulator >= (double)SIM_DT) {
            game_step(&game, &in);
            accumulator -= (double)SIM_DT;
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

        draw_scene(&fb, &game);

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
