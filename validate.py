import argparse
import os
import re


def extract_students(file: str):
    """
    Extract the student information from the file
    Will throw FileNotFoundError if the file is not found
    :param file: the file path
    :return:
    """
    with open(file, "r") as f:
        code = f.read()

        # Clean
        code = re.sub(r"\(.*?\)\s*?\{(\n|.)*?\n\}", "", code)
        code = re.sub(r"#include <[A-Za-z]+.h>\n", "", code)

        code = code.replace(":", " ") \
            .replace("-", "") \
            .replace("|", "") \
            .replace(">", "") \
            .replace("<", "") \
            .replace(".", "")

        code = code.replace("Auteurs", "") \
            .replace("Auteur", "")

        code = re.sub(r"\*+", "*", code)

        name_and_id_regex = r"(\S*) (\S*) ([0-9]{6,8})"

        matches = re.findall(
            name_and_id_regex,
            code)

        return matches


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Valide les noms d''étudiants dans un fichier c.')
    parser.add_argument('path', metavar='p', type=str, nargs=1,
                        help='Le chemin du fichier')

    args = parser.parse_args()

    dirname = os.path.dirname(__file__)
    filename = os.path.join(dirname, args.path[0])

    try:
        results = extract_students(filename)
        print("---------------- Résultat ------------------")
        if len(results) > 0:
            for firstname, lastname, student_id in results:
                print(f"Étudiant trouvé: '{firstname} {lastname}' avec le matricule {student_id}.")
            exit(0)
        else:
            print("Aucun étudiant trouvé. Veuillez changer votre entête de fichier.")
            exit(1)
    except FileNotFoundError as e:
        print(f"Le fichier {e} n'a pas été trouvé. Erreur.")
        exit(1)
