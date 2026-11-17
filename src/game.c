#include "game.h"

#include <math.h>

#include "court.h"

/* Marge autorisée autour du terrain pour les déplacements. */
#define MARGIN_SIDE 1.0f
#define MARGIN_BACK 1.5f

void game_init(Game *g, uint64_t seed)
{
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

void game_step(Game *g, const Input *in)
{
    step_player(&g->human, in);
}
