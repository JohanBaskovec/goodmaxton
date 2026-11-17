# CLAUDE.md

Consignes pour Claude Code sur le projet Goodmaxton.

## Le projet

Goodmaxton est un jeu de badminton en C17 avec SDL3, vu de dessus en 2D, avec
un rendu entièrement logiciel. Le design de référence est dans
`docs/DESIGN.md` : le lire avant toute modification du gameplay, de la physique
ou du rendu.

## Façon de travailler

- **Discuter avant de coder.** Avant chaque nouvelle fonctionnalité ou chaque
  changement de conception, proposer l'approche et attendre l'accord de
  l'utilisateur.
- La communication avec l'utilisateur se fait en français.
- Ne pas reprendre la configuration d'autres projets (CMake, presets, etc.) :
  ce projet a sa propre configuration.
- Garder `docs/DESIGN.md` à jour quand une décision de conception change.
- **Contrôles** : on se déplace au clavier, puis on vise et on tire à la
  souris en cliquant sur son joueur, en glissant et en relâchant (détail dans
  `docs/DESIGN.md` §5). Ne rien inventer au-delà de ce qui y est spécifié :
  poser la question à l'utilisateur.

## Compilation

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
./build/goodmaxton
```

SDL3 est récupéré via FetchContent, puis compilé en statique, avec un tag de
release épinglé dans `CMakeLists.txt`.

## Tests en headless

Pour vérifier le rendu ou la simulation sans fenêtre, utiliser le mode
headless (voir le README pour toutes les options) et lire le PNG produit :

```sh
./build/goodmaxton --headless --script u:240,ur:120 --state --shot /tmp/out.png
./build/goodmaxton --headless --script u:120,r:120 --shot-at 0,60,240 --shot '/tmp/s_%d.png'
```

Pour comparer plusieurs étapes, assembler les captures avec
`convert a.png b.png c.png -resize 33% +append montage.png`.

Toute nouvelle fonctionnalité de simulation doit rester testable ainsi :
- le mode headless ne doit jamais initialiser SDL vidéo ;
- tout l'aléatoire passe par `Game.rng` (graine `--seed`), jamais par `rand()`
  ni par l'horloge, pour que les parties restent reproductibles ;
- la simulation n'utilise que `game_step` et des `Input` : l'IA et l'humain
  passent par les mêmes actions.

Pour la physique : `--launch x,y,z,vx,vy,vz` lance le volant au pas 0 et `--log`
affiche les événements (`launch`, `hit`, `miss`, `net`, `landed`). `--gesture`
scripte un tir (appui, maintien, relâchement) sans souris. Les nouveaux événements
(frappes, fautes, points) doivent passer par le même journal (`Game.log`).

Prévu : entrées souris scriptées, vérification d'invariants à chaque pas,
parties IA contre IA (`--matches`) et scénarios `ctest`.

## Captures de la vraie fenêtre

La session est sous Wayland (GNOME) : `import -window root` ne marche pas. Pour
capturer la fenêtre du jeu et lui envoyer des touches, forcer le pilote X11
(XWayland) :

```sh
SDL_VIDEODRIVER=x11 ./build/goodmaxton &
WID=$(xwininfo -root -tree | grep -i goodmaxton | head -1 | awk '{print $1}')
xdotool windowfocus --sync $WID
xdotool keydown w; sleep 0.8; xdotool keyup w
import -window $WID /tmp/win.png   # puis lire le PNG
```

Le timing de `xdotool` est approximatif : pour des mesures exactes, utiliser le
mode headless.

## Règles techniques

- **Rendu** : tout est dessiné par le CPU dans un framebuffer `uint32_t`
  (XRGB8888). Ce framebuffer est envoyé chaque frame dans une texture SDL en
  streaming, puis affiché. N'utiliser ni les primitives de dessin du
  `SDL_Renderer` (`SDL_RenderLine`, `SDL_RenderFillRect`, etc.), ni le GPU, ni
  aucune image externe.
- **Formes lisses** : toutes les primitives sont antialiasées, avec une
  couverture calculée par pixel.
- **Simulation** : pas de temps fixe (120 Hz), découplé du rendu. La physique
  et les règles ne dépendent pas de SDL.
- **Unités** : mètres et secondes. Le repère est centré au milieu du filet :
  `x` va vers la droite de l'écran, `y` vers le bas de l'écran (côté humain) et
  `z` vers le haut (hauteur).
- **Style C** : C17, warnings stricts (`-Wall -Wextra -Wpedantic`), pas de
  variables globales mutables hors de `main.c`, et aucune allocation dans la
  boucle de jeu. Les identifiants sont en anglais et en `snake_case`.
