# Goodmaxton

Jeu de badminton en C avec SDL3, vu de dessus en 2D. Tous les graphismes sont
générés par logiciel : chaque pixel est calculé par le CPU dans un framebuffer,
sans image ni asset externe.

Pour l'instant, la partie oppose un joueur humain (en bas de l'écran) à une IA
(en haut).

## Compilation

Il faut un compilateur C (gcc ou clang), CMake 3.24 ou plus récent, et git.

Ubuntu 24.04 ne fournit pas de paquet SDL3. Au premier build, CMake télécharge
donc SDL3 et le compile en statique. Ce premier build prend quelques minutes ;
les suivants sont rapides.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
./build/goodmaxton
```

Pour un build optimisé, remplacer `Debug` par `Release`.

## Mode headless

Sans fenêtre ni SDL vidéo, pour les tests et la vérification du rendu :

```sh
./build/goodmaxton --headless --script u:120,ur:60,-:30 --state --shot /tmp/out.png
./build/goodmaxton --headless --script u:120,r:120 --shot-every 40 --shot '/tmp/s_%d.png'
```

| Option            | Effet                                                                  |
|-------------------|------------------------------------------------------------------------|
| `--frames N`      | N pas de simulation (1/120 s chacun) sans aucune entrée                |
| `--script S`      | entrées maintenues : segments `touches:pas` séparés par des virgules ; touches = `u` (vers le filet), `d`, `l`, `r`, combinables (`ur`), ou `-` pour aucune. Incompatible avec `--frames` |
| `--seed N`        | graine du générateur aléatoire (défaut 1) ; même graine, même partie   |
| `--shot f`        | capture du framebuffer final ; `.png` ou `.ppm` selon l'extension       |
| `--shot-at L`     | captures aux pas listés (`0,60,120` ; 0 = état initial) ; `f` contient `%d` |
| `--shot-every N`  | une capture tous les N pas, état initial compris ; `f` contient `%d`   |
| `--launch x,y,z,vx,vy,vz` | lance le volant au pas 0 (m et m/s), pour tester la physique |
| `--log`           | journal des événements sur stdout : `launch`, `net`, `landed`, avec le pas et la position |
| `--state`         | affiche le nombre de pas et l'état final de la simulation sur stdout   |

Le `%d` du nom de fichier est remplacé par le numéro de pas.

La simulation est déterministe : mêmes arguments, mêmes résultats, au bit près.
Code de sortie : 0 si tout va bien, 1 en cas d'erreur d'exécution, 2 si les
arguments sont invalides.

## Contrôles

On se déplace au clavier, puis on vise et on tire à la souris.

| Action          | Contrôle                                                              |
|-----------------|-----------------------------------------------------------------------|
| Se déplacer     | ZQSD (AZERTY) / WASD (QWERTY) : mêmes touches physiques               |
| Frappe normale  | Clic gauche **sur son joueur**, glisser vers la cible, relâcher       |
| Smash           | Clic droit, même geste (pour l'instant identique à la frappe normale) |
| Quitter         | Échap                                                                 |

Le tir demande de l'adresse. Si on vise mal, le volant peut retomber dehors ou
dans son propre camp. Si le clic commence ailleurs que sur son joueur, le tir
est raté. Le coup part au relâchement : si le volant n'est pas à portée à ce
moment-là, on le rate.

## Règles

Le jeu applique les règles officielles du simple : 21 points, 2 points d'écart,
plafond à 30, 2 sets gagnants, service en diagonale. Elles sont détaillées dans
[docs/DESIGN.md](docs/DESIGN.md).

## Documentation

- [docs/DESIGN.md](docs/DESIGN.md) : game design, règles, physique du volant, IA,
  rendu.
- [CLAUDE.md](CLAUDE.md) : conventions et consignes pour le développement assisté.
