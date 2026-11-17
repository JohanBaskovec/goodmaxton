#include "shuttle.h"

#include <math.h>

#include "court.h"

typedef struct {
    float x, y, z;
} V3;

static V3 v3_add(V3 a, V3 b) { return (V3){a.x + b.x, a.y + b.y, a.z + b.z}; }
static V3 v3_mul(V3 a, float k) { return (V3){a.x * k, a.y * k, a.z * k}; }

/* Accélération pour une vitesse donnée : gravité + traînée quadratique
 * a = -(g / vt²)·|v|·v, ce qui donne bien la vitesse terminale vt en chute. */
static V3 accel(V3 v)
{
    float k = SHUTTLE_G / (SHUTTLE_VT * SHUTTLE_VT) * sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    return (V3){-k * v.x, -k * v.y, -k * v.z - SHUTTLE_G};
}

/* Un pas de Runge-Kutta d'ordre 4 : précis même quand la traînée est forte
 * (smash à plusieurs dizaines de m/s). */
static void rk4(V3 *p, V3 *v, float dt)
{
    V3 k1v = accel(*v);
    V3 v2 = v3_add(*v, v3_mul(k1v, dt * 0.5f));
    V3 k2v = accel(v2);
    V3 v3 = v3_add(*v, v3_mul(k2v, dt * 0.5f));
    V3 k3v = accel(v3);
    V3 v4 = v3_add(*v, v3_mul(k3v, dt));
    V3 k4v = accel(v4);

    V3 dp = v3_add(v3_add(*v, v3_mul(v2, 2.0f)), v3_add(v3_mul(v3, 2.0f), v4));
    V3 dv = v3_add(v3_add(k1v, v3_mul(k2v, 2.0f)), v3_add(v3_mul(k3v, 2.0f), k4v));
    *p = v3_add(*p, v3_mul(dp, dt / 6.0f));
    *v = v3_add(*v, v3_mul(dv, dt / 6.0f));
}

void shuttle_launch(Shuttle *s, float x, float y, float z, float vx, float vy, float vz)
{
    *s = (Shuttle){SHUTTLE_FLYING, x, y, z, vx, vy, vz, false};
}

ShuttleEvent shuttle_step(Shuttle *s, float dt)
{
    if (s->state != SHUTTLE_FLYING)
        return SHUTTLE_EVENT_NONE;

    V3 p0 = {s->x, s->y, s->z};
    V3 p = p0, v = {s->vx, s->vy, s->vz};
    rk4(&p, &v, dt);

    ShuttleEvent ev = SHUTTLE_EVENT_NONE;

    /* Filet : plan vertical y = 0, de hauteur NET_HEIGHT, de poteau à poteau.
     * Si le volant le traverse en dessous du haut du filet, il s'arrête net
     * et tombe sur place, du côté d'où il vient. */
    if (!s->hit_net && ((p0.y > 0.0f && p.y <= 0.0f) || (p0.y < 0.0f && p.y >= 0.0f))) {
        float t = p0.y / (p0.y - p.y);
        float xc = p0.x + (p.x - p0.x) * t;
        float zc = p0.z + (p.z - p0.z) * t;
        if (fabsf(xc) <= NET_HALF_WIDTH && zc < NET_HEIGHT) {
            s->hit_net = true;
            s->x = xc;
            s->y = p0.y > 0.0f ? 0.02f : -0.02f;
            s->z = zc > 0.0f ? zc : 0.0f;
            s->vx = s->vy = s->vz = 0.0f;
            return SHUTTLE_EVENT_NET;
        }
    }

    /* Sol : on interpole le point d'impact entre les deux positions. */
    if (p.z <= 0.0f) {
        float t = p0.z / (p0.z - p.z);
        s->x = p0.x + (p.x - p0.x) * t;
        s->y = p0.y + (p.y - p0.y) * t;
        s->z = 0.0f;
        s->vx = s->vy = s->vz = 0.0f;
        s->state = SHUTTLE_DEAD;
        return SHUTTLE_EVENT_LANDED;
    }

    s->x = p.x;
    s->y = p.y;
    s->z = p.z;
    s->vx = v.x;
    s->vy = v.y;
    s->vz = v.z;
    return ev;
}
