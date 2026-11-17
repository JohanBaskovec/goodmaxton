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
    *s = (Shuttle){SHUTTLE_FLYING, x, y, z, vx, vy, vz, false, 0};
}

typedef struct {
    float range;    /* distance horizontale à l'impact au sol */
    float z_at_net; /* hauteur quand le volant atteint la distance `net_dist` */
} ShotSim;

/* Simule un tir de vitesse `speed` et d'élévation `elev` (rad) depuis la hauteur
 * z0, sans filet. Si net_dist > 0, relève aussi la hauteur à cette distance
 * horizontale (là où le tir croise le plan du filet). */
static ShotSim sim_shot(float speed, float elev, float z0, float dt, float net_dist)
{
    ShotSim out = {0.0f, 1e9f};
    V3 p = {0.0f, 0.0f, z0};
    V3 v = {speed * cosf(elev), 0.0f, speed * sinf(elev)};
    for (int i = 0; i < 4000; i++) {
        V3 p0 = p;
        rk4(&p, &v, dt);
        if (net_dist > 0.0f && p0.x < net_dist && p.x >= net_dist) {
            float t = (net_dist - p0.x) / (p.x - p0.x);
            out.z_at_net = p0.z + (p.z - p0.z) * t;
        }
        if (p.z <= 0.0f) {
            float t = p0.z / (p0.z - p.z);
            out.range = p0.x + (p.x - p0.x) * t;
            return out;
        }
    }
    out.range = p.x;
    return out;
}

/* Marge au-dessus du haut du filet pour qu'un tir soit jugé « passant ». */
#define NET_CLEARANCE 0.10f

/* Affine par dichotomie la racine de range(e) = dist dans [lo, hi]. */
static float refine_root(float speed, float z0, float dist, float lo, float hi, bool rising)
{
    for (int i = 0; i < 30; i++) {
        float m = 0.5f * (lo + hi);
        bool below = sim_shot(speed, m, z0, 1.0f / 240.0f, 0.0f).range < dist;
        if (below == rising)
            lo = m;
        else
            hi = m;
    }
    return 0.5f * (lo + hi);
}

bool shuttle_solve_shot(float x, float y, float z, float tx, float ty, float speed, float *vx,
                        float *vy, float *vz)
{
    const float deg = 3.14159265f / 180.0f;
    const float elev_min = -80.0f * deg, elev_max = 85.0f * deg, coarse = 1.5f * deg;
    enum { MAX_ROOTS = 4 };

    float dx = tx - x, dy = ty - y;
    float dist = sqrtf(dx * dx + dy * dy);
    if (dist < 1e-3f) { /* cible sous le volant : on tire vers le camp adverse */
        dx = 0.0f;
        dy = -1.0f;
        dist = 0.0f;
    } else {
        dx /= dist;
        dy /= dist;
    }
    float zz = z > 0.05f ? z : 0.05f;

    /* Distance horizontale, le long du tir, jusqu'au plan du filet (y = 0). */
    float net_dist = -1.0f;
    if ((y > 0.0f && dy < -1e-4f) || (y < 0.0f && dy > 1e-4f))
        net_dist = -y / dy;
    bool crosses_net = net_dist > 0.0f && net_dist < dist;

    /* Balayage grossier : intervalles d'élévation où la portée passe par `dist`. */
    float lo[MAX_ROOTS], hi[MAX_ROOTS];
    bool rising[MAX_ROOTS];
    int n_roots = 0;
    float best_elev = elev_min, best_range = -1.0f;
    float prev_e = elev_min;
    float prev_r = sim_shot(speed, elev_min, zz, 1.0f / 60.0f, 0.0f).range;
    best_range = prev_r;
    if (prev_r >= dist) { /* même le tir le plus plongeant dépasse la cible */
        n_roots = 1;
        lo[0] = hi[0] = elev_min;
        rising[0] = true;
    }
    for (float e = elev_min + coarse; e <= elev_max; e += coarse) {
        float r = sim_shot(speed, e, zz, 1.0f / 60.0f, 0.0f).range;
        if (r > best_range) {
            best_range = r;
            best_elev = e;
        }
        if (n_roots < MAX_ROOTS && (prev_r < dist) != (r < dist)) {
            lo[n_roots] = prev_e;
            hi[n_roots] = e;
            rising[n_roots] = r >= dist;
            n_roots++;
        }
        prev_e = e;
        prev_r = r;
    }

    float elev;
    bool found = n_roots > 0;
    if (!found) {
        elev = best_elev; /* vitesse insuffisante : portée maximale, trop court */
    } else {
        /* Racine la plus basse qui passe au-dessus du filet ; à défaut, la plus
         * basse (le tir finira dans le filet : c'est une faute du joueur). */
        float first = 0.0f;
        elev = 0.0f;
        bool chosen = false;
        for (int i = 0; i < n_roots; i++) {
            float e = lo[i] == hi[i] ? lo[i] : refine_root(speed, zz, dist, lo[i], hi[i], rising[i]);
            if (i == 0)
                first = e;
            if (!crosses_net ||
                sim_shot(speed, e, zz, 1.0f / 240.0f, net_dist).z_at_net >=
                    NET_HEIGHT + NET_CLEARANCE) {
                elev = e;
                chosen = true;
                break;
            }
        }
        if (!chosen)
            elev = first;
    }

    *vx = speed * cosf(elev) * dx;
    *vy = speed * cosf(elev) * dy;
    *vz = speed * sinf(elev);
    return found;
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
