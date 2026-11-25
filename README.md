# OCSH - mini-shell en C

OCSH est un petit shell écrit en C avec les appels système vus en cours. Il lit une ligne, sépare les mots, puis lance les programmes externes avec `fork`, `execvp` et `waitpid`.

## Compilation et lancement

Sous Linux, depuis ce dossier :

```sh
make
./ocsh
```

Pour lancer un fichier de commandes : `./ocsh fichier.ocsh`. Le fichier `.ocshrc` du répertoire courant est lu au démarrage interactif.

## Commandes

Commandes intégrées : `go [dossier]`, `path`, `say [-n] texte`, `info`, `past`, `osinfo`, `export NOM=valeur` et `quit`.

Alias externes : `list` (ls), `show` (cat), `copy` (cp), `move` (mv), `del` (rm), `newdir` (mkdir), `newfile` (touch).

Le shell accepte les redirections `<`, `>` et `>>`, le pipe `->` (ainsi que `|`) et `&` à la fin d'une commande. Les variables `$NOM`, `${NOM}`, `$?` et `$!` sont développées sans découpage en plusieurs arguments.

## Démonstration à vérifier sous Linux

```sh
list
go ..
path
say hello > f.txt
show f.txt
show f.txt -> wc -l
sleep 10 &
ps
osinfo
```

L'historique `past` dure le temps de la session. `SIGCHLD` récupère les processus d'arrière-plan. Le shell est volontairement petit : les substitutions `$(...)` et backticks, la complétion, la sauvegarde de l'historique entre sessions et les constructions de script `if`, `for` et `while` ne sont pas prises en charge. `jobs`, `fg` et `bg` ne sont pas implémentées. Le texte exact demandé par la commande `osinfo` doit être ajusté si le PDF du sujet le précise.

## Vidéo de démonstration

Enregistrer la démonstration réelle sous Linux, avec la caméra ouverte comme demandé par le professeur. Afficher la compilation, le lancement du shell et les commandes de la liste ci-dessus. Ne pas fabriquer de vidéo. Le fichier final peut être nommé `demo.mp4`.
