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
./build/goodmaxton --headless --frames 600 --dump /tmp/out.ppm
```

- `--frames N` : nombre de pas de simulation (1/120 s chacun), déterministe.
- `--dump fichier.ppm` : écrit le framebuffer final au format PPM.
- Code de sortie : 0 si tout va bien, 1 en cas d'erreur d'exécution, 2 si les
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
