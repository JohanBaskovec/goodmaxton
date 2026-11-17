#include "game.h"

#include <math.h>

#include "court.h"

/* Marge autorisée autour du terrain pour les déplacements. */
#define MARGIN_SIDE 1.0f
#define MARGIN_BACK 1.5f

void game_init(Game *g, uint64_t seed)
{
    *g = (Game){0};
    rng_seed(&g->rng, seed);
    g->human = (Player){0.0f, COURT_HALF_LENGTH - 1.0f, 0.0f, 0.0f};
}

/* Fait tendre la vitesse (vx, vy) vers la cible, avec une variation maximale. */
static void approach(Player *p, float tx, float ty, float max_dv)
{
    float dx = tx - p->vx, dy = ty - p->vy;
    float d = sqrtf(dx * dx + dy * dy);
    if (d <= max_dv) {
        p->vx = tx;
        p->vy = ty;
    } else {
        p->vx += dx / d * max_dv;
        p->vy += dy / d * max_dv;
    }
}

static void step_player(Player *p, const Input *in)
{
    float dx = (float)in->right - (float)in->left;
    float dy = (float)in->down - (float)in->up;
    float n = sqrtf(dx * dx + dy * dy);
    float tx = 0.0f, ty = 0.0f;
    if (n > 0.0f) { /* diagonales normalisées */
        tx = dx / n * PLAYER_MAX_SPEED;
        ty = dy / n * PLAYER_MAX_SPEED;
    }
    approach(p, tx, ty, PLAYER_ACCEL * SIM_DT);

    p->x += p->vx * SIM_DT;
    p->y += p->vy * SIM_DT;

    /* Limites : filet (le joueur ne le franchit pas), côtés et fond. */
    const float xmax = COURT_HALF_WIDTH_DOUBLES + MARGIN_SIDE;
    const float ymin = PLAYER_RADIUS;
    const float ymax = COURT_HALF_LENGTH + MARGIN_BACK;
    if (p->x < -xmax) { p->x = -xmax; p->vx = 0.0f; }
    if (p->x > xmax)  { p->x = xmax;  p->vx = 0.0f; }
    if (p->y < ymin)  { p->y = ymin;  p->vy = 0.0f; }
    if (p->y > ymax)  { p->y = ymax;  p->vy = 0.0f; }
}

static void log_event(const Game *g, const char *name, const Shuttle *s)
{
    if (!g->log)
        return;
    fprintf(g->log, "step=%ld t=%.3f %s x=%.3f y=%.3f z=%.3f\n", g->step,
            (double)g->step * (double)SIM_DT, name, (double)s->x, (double)s->y, (double)s->z);
}

void game_launch_shuttle(Game *g, float x, float y, float z, float vx, float vy, float vz)
{
    shuttle_launch(&g->shuttle, x, y, z, vx, vy, vz);
    if (g->log)
        fprintf(g->log, "step=%ld t=%.3f launch x=%.3f y=%.3f z=%.3f vx=%.3f vy=%.3f vz=%.3f\n",
                g->step, (double)g->step * (double)SIM_DT, (double)x, (double)y, (double)z,
                (double)vx, (double)vy, (double)vz);
}

void game_step(Game *g, const Input *in)
{
    step_player(&g->human, in);
    ShuttleEvent ev = shuttle_step(&g->shuttle, SIM_DT);
    g->step++;
    if (ev == SHUTTLE_EVENT_NET)
        log_event(g, "net", &g->shuttle);
    else if (ev == SHUTTLE_EVENT_LANDED)
        log_event(g, "landed", &g->shuttle);
}
