from __future__ import print_function  # python >= 2.6
from os.path import dirname, abspath, join, isfile, isdir
from sys import exit
from os import chdir
from glob import glob
from sys import stderr, exit
from tempfile import NamedTemporaryFile
from re import match, compile as re_compile, X as re_X
from shutil import copyfile
from argparse import ArgumentParser


ROOT_DIR = dirname(dirname(abspath(__file__)))
TEST_DIR = join(ROOT_DIR, 'tests')
CELL_COLS = [1, 8, 45, 77]  # cell col positions
MIN_SPACE = 2               # min. padding for every cell
# ------------------------------------------------------------------------------

# stores the distance between cells the cells, one extra dummy elem for loops
CELL_WIDTHS = [(CELL_COLS[idx] - CELL_COLS[idx-1])
               for idx in range(1, len(CELL_COLS))] + [0]

# regex used to extract information in an Uncrustify test line
LINE_REG = re_compile(r"""
    ^
    (\d+)              #1: test nr
    (!?)[ ]+           #2: rerun symbol
    (\S+\.cfg)[ ]+     #3: config file name
    ((\w+)/(\S+))[ ]*  #4: source file path, #5: test dir name, #6: source file name
    (\S+)?             #7: language (optional)
    (.*)               #8: remaining stuff (comments, etc.)
    $
    """, re_X)


def min_ljust(str, width, min=MIN_SPACE):
    """
    like ljust() but with a guaranteed minimum amount of padding added


    Parameters
    ----------------------------------------------------------------------------
    :param str: str
        string that is going to be adjusted
    :param width: int
        string length in total after padding
    :param min: int
        minimal amount of padding that will be applied


    :return: str
    ----------------------------------------------------------------------------
        left padded string
    """

    o_len = len(str)
    if o_len > width - min:
        return str + ' ' * min
    else:
        return str.ljust(width)


def process_line(line, check_fcn=None):
    """
    applies parsing regex on a line, extracts needed regex groups and combines
    them to the desired format

    accesses global var(s): MIN_SPACE, LINE_REG, CELL_WIDTHS
    depends upon a specific format of the LINE_REG


    Parameters
    ----------------------------------------------------------------------------
    :param line: str
        a single line that is going to be processed

    :param check_fcn: function
        an optional check function that is executed after the regex extraction
        was performed


    :return: str
    ----------------------------------------------------------------------------
        the formatted line
    """

    line_s = line.strip()

    if not line_s or line_s[0] == '#':
        return line_s  # ignore blank/comment lines

    reg_obj = match(LINE_REG, line_s)
    if not reg_obj:
        print("  Error: regex does not match:\n     "+line, file=stderr)
        return None
    if check_fcn and not check_fcn(reg_obj.regs):
        print("  Error: checks failed:\n     "+line, file=stderr)
        return None

    reg_groups = [
        reg_obj.group(1) + (reg_obj.group(2) or " "),  # testNr + rerunSymbol
        reg_obj.group(3),  # cfgName
        reg_obj.group(4),  # src_path
        ((reg_obj.group(7) or "") + (" " * MIN_SPACE)
         + (reg_obj.group(8) or "").strip()).strip(),  # lang + rest
    ]

    sub = 0
    cell_txts = []
    min = 1

    for idx, (grp_str) in enumerate(reg_groups):
        cell_txts.append(min_ljust(grp_str, CELL_WIDTHS[idx] - sub, min))
        min = 2
        sub = len(cell_txts[-1]) - CELL_WIDTHS[idx]

    line_s = "".join(cell_txts).strip()
    return line_s


def col_pos_check(reg_regs_obj):
    """
    checks the correctness of a positions list of groups of a regex object

    accesses global var(s): CELL_COLS
    depends upon a specific format of the LINE_REG


    Parameters
    ----------------------------------------------------------------------------
    :param reg_regs_obj: list<pair<uint, uint>>
        regex.regs object containing start and end pos of groups


    :return: bool
    ----------------------------------------------------------------------------
        True - all checks pass, False otherwise
    """

    test_nr_start = reg_regs_obj[1][0] + 1
    cfg_name_start = reg_regs_obj[3][0] + 1
    src_path_start = reg_regs_obj[4][0] + 1
    lang_start = reg_regs_obj[7][0] + 1

    test_nr_end = reg_regs_obj[1][1] + 1
    cfg_name_end = reg_regs_obj[3][1] + 1
    src_path_end = reg_regs_obj[4][1] + 1

    if test_nr_start != CELL_COLS[0]:
        return False

    if (cfg_name_start != CELL_COLS[1]
        and test_nr_end < CELL_COLS[1] - MIN_SPACE)                            \
       or abs(cfg_name_start - test_nr_end) < MIN_SPACE:
            return False

    if (src_path_start != CELL_COLS[2]
        and cfg_name_end < CELL_COLS[2] - MIN_SPACE)                           \
       or abs(src_path_start - cfg_name_end) < MIN_SPACE:
            return False

    if lang_start != 0:  # -1 + 1
        if (lang_start != CELL_COLS[3]
            and src_path_end < CELL_COLS[3] - MIN_SPACE)                       \
           or abs(lang_start - src_path_end) < MIN_SPACE:
            return False

    return True


def get_lines(file):
    """
    returns a list with all lines of a file (without newline character)


    Parameters
    ----------------------------------------------------------------------------
    :param file: str
        file that is going to be read


    :return: list<str>
    ----------------------------------------------------------------------------
        list with all lines of the file
    """

    with open(file, 'r') as f:
        tmp_lines = f.read()
        return tmp_lines.splitlines()


def do_check(file_path):
    """
    checks if formatting is necessary


    Parameters
    ----------------------------------------------------------------------------
    :param file_path: str
        file that is going  to be checked


    :return: bool
    ----------------------------------------------------------------------------
        True if no formatting is needed, False otherwise
    """

    flag_ok = True

    lines = get_lines(file_path)
    for line in lines:
        if process_line(line, col_pos_check) is None:
            flag_ok = False

    if not flag_ok:
        print("Formatting error(s) in file: " + file_path, file=stderr)

    return flag_ok


def do_format(file_path):
    """
    applies the formatting


    Parameters
    ----------------------------------------------------------------------------
    :param file_path: str
        file that is going to be formatted


    :return: bool
    ----------------------------------------------------------------------------
        True if no errors occurred, False otherwise
    """

    flag_ok = True
    lines = get_lines(file_path)

    with NamedTemporaryFile('w') as f:
        for line in lines:
            line = process_line(line)

            if line is None:
                flag_ok = False
                break

            print(line, file=f)
        f.flush()

        if flag_ok:
            copyfile(f.name, file_path)

    return flag_ok


def process_tests(dir, mode_fcn):
    """
    globs *.test files in a directory and applies the provided function on them


    Parameters
    ----------------------------------------------------------------------------
    :param dir: str
        path to a directory that includes *.test files that are going to be
        processed

    :param mode_fcn: function
        function that is going to be applied on every file


    :return: bool
    ----------------------------------------------------------------------------
        True if no errors occurred, False otherwise
    """

    chdir(dir)

    flag_ok = True
    test_files = glob("*.test")
    lines = []

    for file in test_files:
        if not mode_fcn(file):
            flag_ok = False

    return flag_ok


if __name__ == "__main__":
    """
    parses all script arguments and based on that calls needed functions
    """

    arg_parser = ArgumentParser()
    group_mode = arg_parser.add_mutually_exclusive_group(required=True)
    group_mode.add_argument(
        '-c', '--check', action='store_true',
        help="checks if formatting changes are necessary (1) or not (0)"
    )
    group_mode.add_argument('-f', '--format', action='store_true',
                            help="applies formatting")
    arg_parser.add_argument(
        'input', metavar='INPUT', type=str,
        help='either a single file or all *.test files inside a directory'
    )

    FLAGS, unparsed = arg_parser.parse_known_args()

    mode_fcn = do_check if FLAGS.check else do_format

    if isfile(FLAGS.input):
        if not mode_fcn(FLAGS.input):
            exit(1)
    elif isdir(FLAGS.input):
        if not process_tests(FLAGS.input, mode_fcn):
            exit(1)
    else:
        print("Error: input is neither a file nor a directory\n  %s"
              % FLAGS.input, file=stderr)
        exit(1)

    exit(0)

