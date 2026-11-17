#ifndef GOODMAXTON_GAME_H
#define GOODMAXTON_GAME_H

#include <stdbool.h>
#include <stdint.h>

#include "rng.h"

/* Simulation à pas fixe, indépendante de SDL. */
#define SIM_HZ 120
#define SIM_DT (1.0f / (float)SIM_HZ)

#define PLAYER_RADIUS 0.30f
#define PLAYER_MAX_SPEED 5.0f /* m/s */
#define PLAYER_ACCEL 25.0f    /* m/s² : vitesse max atteinte en 0,2 s */

/* Entrées du joueur humain pour un pas de simulation. */
typedef struct {
    bool up, down, left, right; /* up = vers le filet */
} Input;

typedef struct {
    float x, y;
    float vx, vy;
} Player;

typedef struct {
    Player human; /* moitié basse du terrain (y > 0) */
    Rng rng;      /* tout l'aléatoire de la simulation passe par ici */
} Game;

void game_init(Game *g, uint64_t seed);
void game_step(Game *g, const Input *in);

#endif
