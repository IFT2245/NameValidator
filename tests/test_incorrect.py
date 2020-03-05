import unittest

from validate import extract_students


class IncorrectTests(unittest.TestCase):
    def test_nothing_found(self):
        names = extract_students("incorrect_file.c")
        self.assertEqual(0, len(names))


if __name__ == '__main__':
    unittest.main()
