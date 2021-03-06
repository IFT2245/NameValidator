# NameValidator ![Build status](https://travis-ci.org/IFT2245/NameValidator.svg?branch=master)

Validation de l'entête de votre fichier.

## Installation et utilisation
Ce script requiert `python3`. Vous pouvez installer `python3` sur le [site de python](https://www.python.org/).
Normalement, `python3` est installé si vous utilisez Linux (par défaut, peut-être taper `python3` au lieu de `python`).

Par la suite:

```shell-script
> wget https://raw.githubusercontent.com/IFT2245/NameValidator/master/validate.py # télécharge le script
> chmod u+x # donne les permissions d'exécution au script
> ./validate.py [votre fichier .c] # valide le fichier c
...
```

Si votre installation ne contient pas la librairie `requests`:
```shell script
> wget https://raw.githubusercontent.com/NameValidator/blob/master/requirements.txt # télécharge le script
> pip3 install -r requirements.txt
```

Si vous ne voulez pas donner les permissions au script, vous pouvez aussi faire:

```shell script
> python3 ./validate.py [votre fichier .c]
```

Si le script vous ddit que la version n'est pas à jour, vous devez télécharger la dernière version de celui-ci, sans quoi
vous ne pouvez pas valider des fichiers.

## Problèmes

Si vous avez un problème avec votre fichier (par exemple, votre nom ne semble pas validé), vous pouvez ouvrir un issue et nous essayerons de le régler.

## Format de version

`majeur`.`mineur`
