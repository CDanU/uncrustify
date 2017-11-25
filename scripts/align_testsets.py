from __future__ import print_function  # python >= 2.6
from os.path import dirname, abspath, join
from sys import exit
from os import chdir
from glob import glob
from sys import stderr
from re import match, compile as re_compile, X as re_X

ROOT_DIR = dirname(dirname(abspath(__file__)))
TEST_DIR = join(ROOT_DIR, 'tests')
COLS = [1, 8, 45, 77]

LINE_REG = re_compile(r"""
    ^
    (\d+)          #1: test nr
    \s*
    (!?)           #2: rerun symbol
    \s+
    (\S+\.cfg)     #3: config file name
    \s+
    ((\w+)/(\S+))  #4: partial source file path, #5: test dir name, #6: source file name
    \s*
    (\S+)?         #7: language (optional)
    (.*)           #8: remaining stuff (comments, etc.)
    $
    """, re_X)


def min_ljust(str, width, min=0, fillchar=None):
    o_len = len(str)
    if o_len > width:
        return str + ' ' * min
    else:
        return str.ljust(width)


def process_line(line):
    line_s = line.strip()

    if not line_s or line_s[0] == '#':
        return line  # ignore blank/comment lines

    reg_obj = match(LINE_REG, line_s)
    if not reg_obj:
        print("Warning: line does not match the regex:\n   "+line, file=stderr)
        return line  # regex did not match

    test_nr = reg_obj.group(1)
    rerun_symbol = reg_obj.group(2) or " "
    cfg_name = reg_obj.group(3)
    src_path = reg_obj.group(4)
    lang = reg_obj.group(7) or ""
    rest = reg_obj.group(8) or ""

    col0 = min_ljust(test_nr + rerun_symbol, COLS[1]-COLS[0], min=1)
    col1 = min_ljust(cfg_name, COLS[2]-COLS[1], min=2)
    col2 = min_ljust(src_path, COLS[3]-COLS[2], min=2)
    col3 = lang

    line_s = (col0+col1+col2+col3+rest).strip()
    return line_s


def main():
    chdir(TEST_DIR)

    test_files = glob("*.test")
    lines = []

    for file in test_files:
        with open(file, 'r') as f:
            tmp_lines = f.read()
            lines = tmp_lines.splitlines()

        with open(file, 'w') as f:
            for line in lines:
                line = process_line(line)
                print(line, file=f)


if __name__ == "__main__":
    exit(main())
