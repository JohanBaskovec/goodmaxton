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

static float charge_power(const Game *g)
{
    float p = (float)(g->step - g->aim.press_step) * SIM_DT / CHARGE_TIME;
    return p < 0.0f ? 0.0f : (p > 1.0f ? 1.0f : p);
}

float game_aim_power(const Game *g)
{
    return g->aim.state == AIM_ACTIVE ? charge_power(g) : 0.0f;
}

float shot_speed(float power)
{
    return SHOT_V_MIN + (SHOT_V_MAX - SHOT_V_MIN) * power * power;
}

static void log_miss(const Game *g, const char *reason)
{
    if (g->log)
        fprintf(g->log, "step=%ld t=%.3f miss who=human reason=%s\n", g->step,
                (double)g->step * (double)SIM_DT, reason);
}

/* Relâchement du clic : tente de frapper le volant vers le curseur. */
static void release_shot(Game *g, float tx, float ty)
{
    const Player *p = &g->human;
    Shuttle *s = &g->shuttle;
    float power = charge_power(g);

    if (s->state != SHUTTLE_FLYING) {
        log_miss(g, "no_shuttle");
        return;
    }
    if (s->y <= 0.0f) {
        log_miss(g, "other_side");
        return;
    }
    float dx = s->x - p->x, dy = s->y - p->y;
    if (sqrtf(dx * dx + dy * dy) > HIT_REACH || s->z < HIT_Z_MIN || s->z > HIT_Z_MAX) {
        log_miss(g, "out_of_reach");
        return;
    }
    if (s->last_hitter == 1) {
        log_miss(g, "double_hit");
        return;
    }

    float speed = shot_speed(power);
    float vx, vy, vz;
    bool reaches = shuttle_solve_shot(s->x, s->y, s->z, tx, ty, speed, &vx, &vy, &vz);
    s->vx = vx;
    s->vy = vy;
    s->vz = vz;
    s->hit_net = false;
    s->last_hitter = 1;
    if (g->log)
        fprintf(g->log,
                "step=%ld t=%.3f hit who=human %s power=%.2f speed=%.1f target=(%.2f,%.2f) "
                "from=(%.2f,%.2f,%.2f) reaches=%d\n",
                g->step, (double)g->step * (double)SIM_DT, g->aim.right ? "smash" : "normal",
                (double)power, (double)speed, (double)tx, (double)ty, (double)s->x, (double)s->y,
                (double)s->z, (int)reaches);
}

/* Geste souris : appui sur le joueur, glissement, relâchement. */
static void step_aim(Game *g, const Input *in)
{
    bool press_l = in->mouse_left && !g->prev_left;
    bool press_r = in->mouse_right && !g->prev_right;
    g->prev_left = in->mouse_left;
    g->prev_right = in->mouse_right;

    if (g->aim.state == AIM_IDLE) {
        if (!press_l && !press_r)
            return;
        float dx = in->mouse_x - g->human.x, dy = in->mouse_y - g->human.y;
        bool on_player = dx * dx + dy * dy <= CLICK_RADIUS * CLICK_RADIUS;
        g->aim.state = on_player ? AIM_ACTIVE : AIM_WHIFF;
        g->aim.right = !press_l; /* le gauche l'emporte si les deux arrivent ensemble */
        g->aim.press_step = g->step;
    }

    g->aim.cursor_x = in->mouse_x;
    g->aim.cursor_y = in->mouse_y;

    bool held = g->aim.right ? in->mouse_right : in->mouse_left;
    if (held)
        return;

    /* Relâchement. */
    AimState st = g->aim.state;
    g->aim.state = AIM_IDLE;
    if (st == AIM_ACTIVE)
        release_shot(g, in->mouse_x, in->mouse_y);
    else
        log_miss(g, "off_player");
}

void game_step(Game *g, const Input *in)
{
    step_player(&g->human, in);
    step_aim(g, in);
    ShuttleEvent ev = shuttle_step(&g->shuttle, SIM_DT);
    g->step++;
    if (ev == SHUTTLE_EVENT_NET)
        log_event(g, "net", &g->shuttle);
    else if (ev == SHUTTLE_EVENT_LANDED)
        log_event(g, "landed", &g->shuttle);
}
