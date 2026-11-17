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
    bool hit_net; /* a touché le filet pendant ce vol */
} Shuttle;

void shuttle_launch(Shuttle *s, float x, float y, float z, float vx, float vy, float vz);

/* Avance d'un pas dt. Retourne l'événement survenu pendant ce pas, s'il y en a un. */
ShuttleEvent shuttle_step(Shuttle *s, float dt);

#endif
