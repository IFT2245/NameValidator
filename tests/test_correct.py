import unittest
from validate import extract_students


class TestCorrect(unittest.TestCase):

    def test_correct_len(self):
        names = extract_students("./tests/correct_file.c")
        self.assertEqual(2, len(names))

    def test_correct_names(self):
        names = extract_students("./tests/correct_file.c")

        name_one = names[0]
        name_two = names[1]

        self.assertEqual(name_one[0], "firstname")
        self.assertEqual(name_one[1], "lastname")
        self.assertEqual(name_one[2], "00001111")

        self.assertEqual(name_two[0], "anothername")
        self.assertEqual(name_two[1], "anotherlastname")
        self.assertEqual(name_two[2], "11110000")

    def test_long_comment(self):
        names = extract_students("./tests/long_comment_style_correct.c")

        self.assertEqual(len(names), 2)

        name_one = names[0]
        name_two = names[1]

        # *author
        # one
        # 11112222
        # *author
        # two
        # 33331111
        self.assertEqual(name_one[0], "author")
        self.assertEqual(name_one[1], "one")
        self.assertEqual(name_one[2], "11112222")

        self.assertEqual(name_two[0], "author")
        self.assertEqual(name_two[1], "two")
        self.assertEqual(name_two[2], "33331111")

    def test_long_names(self):
        # *author
        # one
        # uno
        # dos
        # tres
        # 11112222
        # *author
        # two
        # quatro
        # cinco
        # seis
        # 33331111
        names = extract_students("./tests/long_names_correct.c")

        self.assertEqual(len(names), 2)

        name_one = names[0]
        name_two = names[1]

        # the re library doesn't allow repeated group
        self.assertEqual(name_one[0], "author")
        self.assertEqual(name_one[1], "tres")
        self.assertEqual(name_one[2], "11112222")

        self.assertEqual(name_two[0], "author")
        self.assertEqual(name_two[1], "seis")
        self.assertEqual(name_two[2], "33331111")

    def test_accents(self):
        # *author
        # ÉéÀà
        # 11112222
        # *author
        # çÇaa
        # 33331111
        names = extract_students("./tests/accents_name_correct.c")

        self.assertEqual(len(names), 2)

        name_one = names[0]
        name_two = names[1]

        self.assertEqual(name_one[0], "author")
        self.assertEqual(name_one[1], "ÉéÀà")
        self.assertEqual(name_one[2], "11112222")

        self.assertEqual(name_two[0], "author")
        self.assertEqual(name_two[1], "çÇaa")
        self.assertEqual(name_two[2], "33331111")


if __name__ == '__main__':
    unittest.main()
