# Goodmaxton : document de conception

## 1. Vue d'ensemble

- C'est un jeu de badminton en simple : un humain contre une IA.
- La vue est de dessus en 2D, avec le terrain vertical : l'humain occupe la
  moitié basse, l'IA la moitié haute.
- Le volant est simulé en 3D. Sa hauteur est rendue lisible grâce à son ombre
  au sol.
- Les graphismes sont générés par logiciel, avec des formes lisses et
  antialiasées.
- La fenêtre fait 720×1080 en portrait et est redimensionnable. Le terrain est
  mis à l'échelle en gardant ses proportions.

## 2. Terrain

Le terrain a les dimensions officielles, en mètres :

| Élément                                  | Valeur           |
|------------------------------------------|------------------|
| Longueur totale                          | 13,40            |
| Largeur du double (lignes extérieures)   | 6,10             |
| Largeur du simple                        | 5,18             |
| Ligne de service court (depuis le filet) | 1,98             |
| Ligne de fond (depuis le filet)          | 6,70             |
| Hauteur du filet                         | 1,524 au centre  |

En simple, le volant est **bon** s'il retombe dans les couloirs du simple et
avant la ligne de fond. Les lignes comptent comme dedans.

Les lignes du double sont dessinées, mais elles ne servent pas aux règles.

## 3. Volant (physique 3D)

L'état du volant comprend sa position `(x, y, z)` et sa vitesse
`(vx, vy, vz)`.

Il subit deux forces :
- la gravité, `g = 9,81 m/s²` ;
- une **traînée quadratique**, `a_drag = -g · |v| · v / v_t²`, avec une vitesse
  terminale `v_t ≈ 6,8 m/s`.

La traînée explique le comportement caractéristique du volant : il part très
vite, ralentit brutalement, puis retombe presque à la verticale.

Les collisions sont gérées ainsi :
- **Sol** (`z ≤ 0`) : le volant s'arrête et l'échange se termine. On regarde
  alors s'il est dedans ou dehors.
- **Filet** : il est modélisé comme un plan vertical en `y = 0`, de hauteur
  1,524 m. Si le volant le traverse en dessous de cette hauteur, il tombe du
  côté du frappeur et la faute lui revient. Le cas du volant qui touche la
  bande puis passe sera traité plus tard.

Le pas d'intégration est fixe (1/120 s). Le schéma (Euler semi-implicite ou
RK2) sera choisi au moment de l'implémentation.

L'affichage combine deux éléments :
- l'**ombre** au sol en `(x, y)`, une ellipse sombre semi-transparente ;
- le **volant**, dessiné en `(x, y − k·z)`, c'est-à-dire décalé vers le haut de
  l'écran proportionnellement à sa hauteur. On obtient ainsi une perspective
  oblique légère, avec un volant légèrement plus gros quand il est haut.

L'écart entre le volant et son ombre donne la lecture de la hauteur.

## 4. Joueurs

- Chaque joueur est un disque, éventuellement accompagné de sa raquette. Il a
  une position au sol, une vitesse maximale et une accélération, qui lui donnent
  un peu d'inertie.
- Il a une **zone de frappe** : un rayon autour de lui, plus une plage de
  hauteur où il peut atteindre le volant.
- Il ne peut pas franchir le filet.

## 5. Contrôles et coups

### Déplacement

Le joueur se déplace avec les touches **ZQSD sur AZERTY**, c'est-à-dire
**WASD sur QWERTY**. Ce sont les mêmes touches physiques. On utilise donc les
*scancodes* SDL (`SDL_SCANCODE_W/A/S/D`), qui désignent la position de la
touche, et non les *keycodes*, qui dépendent de la disposition. Le contrôle
marche ainsi sur toutes les dispositions de clavier.

| Position (QWERTY) | AZERTY | Direction       |
|-------------------|--------|-----------------|
| W                 | Z      | Vers le filet   |
| S                 | S      | Vers le fond    |
| A                 | Q      | Gauche          |
| D                 | D      | Droite          |

### Visée et tir à la souris (« drag & release »)

1. Le joueur **appuie** sur un bouton de la souris **sur son propre
   personnage**.
2. Il **glisse** la souris vers le point du terrain qu'il vise.
3. Il **relâche** le bouton pour tirer vers ce point.

Le geste n'est pas assisté et demande de l'adresse :
- Le point de relâchement devient le point visé, sans correction. Si on vise
  mal, le volant peut retomber dehors ou dans son propre camp.
- Si le clic commence **ailleurs que sur son personnage**, le tir est **raté**
  et le joueur ne touche pas le volant.

Deux boutons, deux coups :

| Bouton      | Coup           | Trajectoire                                    |
|-------------|----------------|------------------------------------------------|
| Clic gauche | Frappe normale | Haute et longue, vers le point visé            |
| Clic droit  | Smash          | Même geste ; fonctionnement détaillé à définir |

La vitesse initiale du volant se calcule à partir du point visé et du type de
coup. Un solveur de tir vise ce point en tenant compte de la traînée.

### Règles du geste

- **Moment de la frappe** : le coup part au relâchement. Si le volant n'est pas
  dans la zone de frappe à cet instant, le joueur le rate. Il faut donc
  anticiper le geste.
- **Tolérance du clic** : le clic doit commencer dans le disque du personnage,
  légèrement agrandi pour rester jouable.
- **Pendant la visée** : le joueur peut continuer à se déplacer au clavier, et
  le temps n'est pas ralenti. Le point de départ du geste suit le joueur.
- **Retour visuel** : une simple ligne relie le joueur au curseur. Le point de
  chute prévu n'est pas affiché.
- **Service** : il utilise le même geste. Le serveur est immobile, et le volant
  part de sa main.
- **Smash** : son fonctionnement sera défini plus tard. D'ici là, le clic droit
  fait une frappe normale.

## 6. Règles (simple, BWF)

**Score**
- Chaque échange gagné rapporte un point, quel que soit le serveur.
- Un set se gagne à 21 points avec 2 points d'écart. Si le score atteint 29-29,
  le premier à 30 l'emporte.
- Le match se joue en 2 sets gagnants.

**Service**
- Le gagnant de l'échange sert l'échange suivant.
- Le serveur sert depuis sa zone de droite quand son score est pair, depuis sa
  zone de gauche quand il est impair. Le receveur se place en diagonale.
- Le volant doit être frappé à moins de 1,15 m de hauteur.
- Pour être bon, le service doit retomber dans la zone diagonale du receveur :
  au-delà de la ligne de service court, avant la ligne de fond, et à l'intérieur
  de la ligne médiane et de la ligne latérale du simple.
- Le gagnant d'un set sert en premier au set suivant. L'humain sert en premier
  au premier set.

**Fautes**
- Le volant tombe dehors, ou tombe dans son propre camp.
- Le volant ne passe pas le filet.
- Un service mauvais.

Plusieurs points sont simplifiés pour l'instant : pas de pause à 11 points, pas
de faute de contact avec le filet, et pas de changement de côté visuel.
L'humain reste toujours en bas.

## 7. IA (un seul niveau)

- **Anticipation** : elle simule la trajectoire du volant en avance, avec la
  même physique, pour trouver le point où il sera frappable, puis s'y déplace.
- **Placement** : entre deux coups, elle revient vers le centre de son camp.
- **Choix du coup** : elle smashe quand le volant est assez haut et qu'elle est
  bien placée, sinon elle fait une frappe normale.
- **Cible** : elle vise une zone du camp adverse éloignée du joueur humain.
- **Imperfections**, pour qu'elle reste battable : un temps de réaction, une
  erreur aléatoire sur la cible et une vitesse de déplacement limitée.

## 8. Rendu logiciel

- Le framebuffer est un tableau `uint32_t` en XRGB8888, à la résolution de la
  fenêtre. Chaque frame, il est envoyé dans une `SDL_Texture` en streaming.
- Les primitives sont antialiasées, avec une couverture calculée par pixel :
  - disques et ellipses, dont l'ombre ;
  - lignes épaisses, pour les lignes du terrain ;
  - rectangles ;
  - mélange alpha.
- Le texte (score) passe par une petite police vectorielle ou matricielle
  générée dans le code, sans fichier externe.
- La scène est dessinée dans cet ordre : sol, lignes, ombres, joueurs côté IA,
  filet, joueurs côté humain, volant, interface.

## 9. Architecture prévue

```
src/
  main.c        boucle principale, SDL, pas de temps fixe
  platform.c/h  fenêtre, texture, entrées souris/clavier (seul module SDL)
  render.c/h    framebuffer et primitives antialiasées
  draw.c/h      dessin de la scène (terrain, joueurs, volant, interface)
  shuttle.c/h   physique du volant, solveur de tir
  player.c/h    déplacement, zone de frappe, coups
  rules.c/h     score, service, fautes, sets
  ai.c/h        comportement de l'IA
  font.c/h      police générée par code
  vec.h         petits utilitaires vectoriels
```

## 10. Hors périmètre pour l'instant

Ces éléments sont exclus pour l'instant : le double, les coups supplémentaires
(amorti, lob, rush au filet), plusieurs niveaux d'IA, le son, la manette, les
menus, et le changement de côté.
