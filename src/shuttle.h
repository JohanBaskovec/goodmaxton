#ifndef GOODMAXTON_SHUTTLE_H
#define GOODMAXTON_SHUTTLE_H

#include <stdbool.h>

/* Physique du volant : gravité + traînée quadratique, vitesse terminale
 * SHUTTLE_VT. Repère du terrain : x droite, y vers le bas (côté humain),
 * z vers le haut. Unités SI. */
#define SHUTTLE_G 9.81f
#define SHUTTLE_VT 6.8f /* vitesse terminale en chute libre (m/s) */

typedef enum {
    SHUTTLE_INACTIVE, /* pas de volant en jeu */
    SHUTTLE_FLYING,
    SHUTTLE_DEAD /* posé au sol : l'échange est terminé */
} ShuttleState;

typedef enum {
    SHUTTLE_EVENT_NONE,
    SHUTTLE_EVENT_NET,   /* a touché le filet (puis tombe sur place) */
    SHUTTLE_EVENT_LANDED /* a touché le sol */
} ShuttleEvent;

typedef struct {
    ShuttleState state;
    float x, y, z;
    float vx, vy, vz;
    bool hit_net;    /* a touché le filet pendant ce vol */
    int last_hitter; /* 0 = personne, 1 = humain, 2 = IA */
} Shuttle;

void shuttle_launch(Shuttle *s, float x, float y, float z, float vx, float vy, float vz);

/* Solveur de tir. Depuis (x, y, z), cherche la vitesse initiale qui, avec la
 * vitesse `speed` (m/s) et en tenant compte de la traînée, fait retomber le
 * volant au sol en (tx, ty). Parmi les angles possibles, il prend le plus bas
 * (tir tendu). Retourne false si `speed` est trop faible pour atteindre la
 * cible : le tir est alors donné à l'angle de portée maximale et retombera
 * trop court. */
bool shuttle_solve_shot(float x, float y, float z, float tx, float ty, float speed, float *vx,
                        float *vy, float *vz);

/* Avance d'un pas dt. Retourne l'événement survenu pendant ce pas, s'il y en a un. */
ShuttleEvent shuttle_step(Shuttle *s, float dt);

#endif
