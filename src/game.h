#ifndef GOODMAXTON_GAME_H
#define GOODMAXTON_GAME_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "rng.h"
#include "shuttle.h"

/* Simulation à pas fixe, indépendante de SDL. */
#define SIM_HZ 120
#define SIM_DT (1.0f / (float)SIM_HZ)

#define PLAYER_RADIUS 0.30f
#define PLAYER_MAX_SPEED 5.0f /* m/s */
#define PLAYER_ACCEL 25.0f    /* m/s² : vitesse max atteinte en 0,2 s */

/* Portée du coup : le volant doit être à moins de HIT_REACH (horizontalement)
 * du joueur, entre HIT_Z_MIN et HIT_Z_MAX de hauteur, et de son côté du filet. */
#define HIT_REACH 1.0f
#define HIT_Z_MIN 0.1f
#define HIT_Z_MAX 3.0f
/* Le clic doit commencer dans ce rayon autour du joueur (disque agrandi). */
#define CLICK_RADIUS (PLAYER_RADIUS * 1.5f)
/* Puissance : maintenir le clic charge de 0 à 1 en CHARGE_TIME secondes ;
 * la vitesse du volant va de SHOT_V_MIN à SHOT_V_MAX (m/s), en courbe carrée
 * pour plus de finesse sur les tirs doux. */
#define CHARGE_TIME 1.0f
#define SHOT_V_MIN 6.0f
#define SHOT_V_MAX 100.0f

/* Entrées du joueur humain pour un pas de simulation. */
typedef struct {
    bool up, down, left, right; /* up = vers le filet */
    float mouse_x, mouse_y;     /* curseur, en mètres dans le repère du terrain */
    bool mouse_left, mouse_right;
} Input;

/* Geste de tir : appui sur le joueur, glissement, relâchement. */
typedef enum {
    AIM_IDLE,
    AIM_ACTIVE, /* appui commencé sur le joueur : le tir partira au relâchement */
    AIM_WHIFF   /* appui commencé ailleurs : le tir sera raté */
} AimState;

typedef struct {
    AimState state;
    bool right;      /* bouton droit (smash) plutôt que gauche */
    long press_step; /* pas de l'appui, pour la durée de charge */
    float cursor_x, cursor_y;
} Aim;

typedef struct {
    float x, y;
    float vx, vy;
} Player;

typedef struct {
    Player human; /* moitié basse du terrain (y > 0) */
    Shuttle shuttle;
    Aim aim;
    bool prev_left, prev_right; /* boutons de souris au pas précédent */
    Rng rng;   /* tout l'aléatoire de la simulation passe par ici */
    long step; /* nombre de pas simulés */
    FILE *log; /* journal d'événements, NULL si désactivé */
} Game;

/* Puissance (0..1) actuellement chargée, ou 0 si aucun tir n'est en préparation. */
float game_aim_power(const Game *g);
/* Vitesse initiale du volant (m/s) pour une puissance 0..1. */
float shot_speed(float power);

void game_init(Game *g, uint64_t seed);
void game_step(Game *g, const Input *in);

/* Met le volant en jeu avec une position et une vitesse données (tests, services). */
void game_launch_shuttle(Game *g, float x, float y, float z, float vx, float vy, float vz);

#endif
