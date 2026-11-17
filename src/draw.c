#include "draw.h"

#include "court.h"

#define LINE_WIDTH 0.04f
#define VIEW_MARGIN 1.2f

View view_fit(int w, int h)
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

static void line_m(Framebuffer *fb, const View *v, float x0, float y0, float x1, float y1,
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
    line_m(fb, v, -dw, -hl, dw, -hl, line);
    line_m(fb, v, -dw, hl, dw, hl, line);
    line_m(fb, v, -dw, -hl, -dw, hl, line);
    line_m(fb, v, dw, -hl, dw, hl, line);
    /* Lignes latérales du simple. */
    line_m(fb, v, -sw, -hl, -sw, hl, line);
    line_m(fb, v, sw, -hl, sw, hl, line);
    /* Lignes de service court. */
    line_m(fb, v, -dw, -ss, dw, -ss, line);
    line_m(fb, v, -dw, ss, dw, ss, line);
    /* Lignes de service long du double. */
    line_m(fb, v, -dw, -LONG_SERVICE_LINE_DOUBLES, dw, -LONG_SERVICE_LINE_DOUBLES, line);
    line_m(fb, v, -dw, LONG_SERVICE_LINE_DOUBLES, dw, LONG_SERVICE_LINE_DOUBLES, line);
    /* Ligne médiane. */
    line_m(fb, v, 0.0f, -hl, 0.0f, -ss, line);
    line_m(fb, v, 0.0f, ss, 0.0f, hl, line);

    /* Filet et poteaux. */
    const float post_x = NET_HALF_WIDTH;
    fb_line(fb, vx(v, -post_x), vy(v, 0.0f), vx(v, post_x), vy(v, 0.0f), 0.05f * v->scale,
            RGB(250, 250, 250));
    fb_disc(fb, vx(v, -post_x), vy(v, 0.0f), 0.09f * v->scale, RGB(60, 60, 66));
    fb_disc(fb, vx(v, post_x), vy(v, 0.0f), 0.09f * v->scale, RGB(60, 60, 66));
}

static void draw_player(Framebuffer *fb, const View *v, const Player *p, Color body)
{
    float cx = vx(v, p->x), cy = vy(v, p->y), r = PLAYER_RADIUS * v->scale;
    /* Ombre portée légèrement décalée, puis corps avec un liseré sombre. */
    fb_ellipse(fb, cx + 0.06f * v->scale, cy + 0.06f * v->scale, r * 1.05f, r * 0.9f,
               RGBA(0, 0, 0, 0.30f));
    fb_disc(fb, cx, cy, r, RGB(20, 24, 32));
    fb_disc(fb, cx, cy, r - 2.0f, body);
}

/* Hauteur → décalage vertical à l'écran (en mètres d'écran par mètre de hauteur)
 * et grossissement léger : perspective oblique qui rend la hauteur lisible. */
#define HEIGHT_SHIFT 0.30f
#define HEIGHT_GROW 0.03f
#define SHUTTLE_DRAW_RADIUS 0.11f

static void draw_shuttle(Framebuffer *fb, const View *v, const Shuttle *s)
{
    if (s->state == SHUTTLE_INACTIVE)
        return;
    float z = s->z > 0.0f ? s->z : 0.0f;
    float gx = vx(v, s->x), gy = vy(v, s->y);
    float r = SHUTTLE_DRAW_RADIUS * v->scale;

    /* Ombre au sol : plus petite et plus pâle quand le volant est haut. */
    float shade = 0.45f / (1.0f + 0.15f * z);
    fb_ellipse(fb, gx, gy, r * 1.1f, r * 0.7f, RGBA(0, 0, 0, shade));

    /* Volant : décalé vers le haut de l'écran selon sa hauteur. */
    float sy = gy - z * HEIGHT_SHIFT * v->scale;
    float sr = r * (1.0f + HEIGHT_GROW * z);
    fb_disc(fb, gx, sy, sr, RGB(60, 60, 70));
    fb_disc(fb, gx, sy, sr - 1.5f, RGB(250, 250, 245));
}

void draw_scene(Framebuffer *fb, const Game *g)
{
    View v = view_fit(fb->w, fb->h);
    draw_court(fb, &v);
    draw_player(fb, &v, &g->human, RGB(235, 120, 60));
    draw_shuttle(fb, &v, &g->shuttle);
}
