from __future__ import print_function  # python >= 2.6
from os import rename as file_rename, remove as file_remove
from os.path import join, isfile, isdir, basename, split as path_split
from sys import exit
from glob import glob
from sys import stderr
from argparse import ArgumentParser
from re import match, compile as re_compile, X as re_X
from tempfile import NamedTemporaryFile
from shutil import copyfile

# ROOT_DIR = dirname(dirname(abspath(__file__)))
# CONFIG_DIR = join(ROOT_DIR, './tests/config')

# min. padding for every cell in a test config
MIN_SPACE = 2  # keep in sync with align_testsets.py

# maps option values to abbreviations using in the naming convention
VALUE_MAP = {
    "true": "t",
    "false": "f",
    "ignore": "i",
    "add": "a",
    "remove": "r",
    "force": "f",
    "lf": "lf",
    "crlf": "crlf",
    "cr": "cr",
    "join": "j",
    "lead": "l",
    "lead_break": "lb",
    "lead_force": "lf",
    "trail": "t",
    "trail_break": "tb",
    "trail_force": "tf",
}


def is_comment_line(line):
    """
    checks if a string matches the following pattern: ^ *#


    Parameters
    ----------------------------------------------------------------------------
    :param line: str
        string on which the match is performed


    :return: bool
    ----------------------------------------------------------------------------
        true if line matches, False otherwise
    """
    first_non_sp_idx = next(idx for idx, char in enumerate(line) if char != ' ')
    return True if(line[first_non_sp_idx] == '#') else False


def do_skip_line(line):
    """ returns True if line can be skipped (is empty or a comment line) """
    return True if (not line or is_comment_line(line)) else False


def parse_config_line(line):
    """
    parses a Uncrustify config line to extract option and value


    Parameters
    ----------------------------------------------------------------------------
    :param line: str
        string that is going to be parsed


    :return: str, str
    ----------------------------------------------------------------------------
        config name and config value or "","" if skippable line
        or None, None in case of failure
    """

    if do_skip_line(line):
        return None, None        # return on empty/comment string

    pound_pos = line.find('#')
    if pound_pos != -1:
        line = line[:pound_pos]  # cut away the comment part

    split_pos = line.find('=')
    if split_pos == -1:
        first_non_sp_idx = next(idx for idx, char in enumerate(line)
                                if char != ' ')
        split_pos = line.find(' ', first_non_sp_idx)

    if split_pos == -1:
        return None, None        # no appropriate separator found

    key = line[:split_pos].strip()
    value = line[split_pos + 1:].strip()
    return key, value


def get_change_set_file(file_path):
    """
    checks if a config file is a single option config line and if it needs
    to be renamed to follow the naming convention


    Parameters
    ----------------------------------------------------------------------------
    :param file_path: str
        path to to file that is going to be tested


    :return: dict<str, str>
    ----------------------------------------------------------------------------
        single element dict, key = old file path, value = new file path,
        empty dict if no renaming can or needs to be done
    """

    options = {}
    change_set = {}

    lines = get_lines(file_path)
    for idx, (line) in enumerate(lines):
        if do_skip_line(line):
            continue

        key, value = parse_config_line(line)

        if not key or not value:
            raise ValueError("error while parsing line %d, %s"
                             % (idx, file_path))
        if key in options:
            raise RuntimeError("option defined multiple times: %s" % key)

        options[key] = value
        if len(options) > 1:
            break       # multi option file -> ignore this file

    if not options or len(options) > 1:
        return {}       # no/multi option file -> ignore this file

    key, value = options.popitem()

    if value in VALUE_MAP:
        value = VALUE_MAP[value]
    else:
        try:
            int(value)  # if number use the number string
        except ValueError:
            # else: some other string -> ignore this file
            # => no naming convention for AT_STRING options
            return {}

    expected_name = key + "-" + value + ".cfg"

    f_path, f_name = path_split(file_path)

    if f_name != expected_name:
        change_set[file_path] = join(f_path, expected_name)

    return change_set


def get_change_set(file_paths):
    """
    like get_change_set_file, but for multiple files, returns multi element dict
    """

    change_set = {}

    for file_path in file_paths:
        cs = get_change_set_file(file_path)
        change_set.update(cs)

    return change_set


def dir_glob(dir_path, glob_str):
    """
    like glob.glob but returns paths based on (containing) the provided dir_path
    """

    files = glob(join(dir_path, glob_str))
    return files


def change_set_check(file_paths):
    """
    calls get_change_set, prints messages to stderr if renamings need to be done


    Parameters
    ----------------------------------------------------------------------------
    :param file_paths: list<str>
        path to to file(s) that are going to be tested


    :return: bool
    ----------------------------------------------------------------------------
        True if no renaming of files needs to be done, False otherwise
    """

    change_set = get_change_set(file_paths)

    if change_set:
        print("File(s) that need to be renamed:", file=stderr)
        for k, v in change_set.items():
            print("  '%s' --> '%s'" % (basename(k), basename(v)), file=stderr)

        return False
    return True


def do_check_dir(dir_path):
    """ globs dir_path for cfg files and returns value from change_set_check """
    test_files = dir_glob(dir_path, "*.cfg")
    return change_set_check(test_files)


def do_check_file(file_path):
    """ calls change_set_check with a single file """
    return change_set_check([file_path])


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


def rename_in_test(test_file_path, change_set_names):
    """
    applies renamings inside test files


    Parameters
    ----------------------------------------------------------------------------
    :param test_file_path:
        path to a test file

    :param change_set_names: dict<str, str>
        dict containing old (key) and new (value) config names (not paths!)


    :return: bool
    ----------------------------------------------------------------------------
        True if no errors occurred, False otherwise
    """

    line_regex = re_compile(r"""
        ^\d+!?[ ]+         # test nr + rerun symbol
        (\S+\.cfg)([ ]+)   #1: config file name #2: spaces
        """, re_X)

    lines_i = get_lines(test_file_path)

    with NamedTemporaryFile('w') as f:
        flag_ok = True

        for line in lines_i:
            if do_skip_line(line):
                print(line, file=f)
                continue

            # apply regex to get cfg file name & amount of spaces after it
            reg_obj = match(line_regex, line)
            if not reg_obj:
                print("  Error: regex does not match:\n     " + line,
                      file=stderr)
                return False

            cfg_name = reg_obj.group(1)
            len_spaces = len(reg_obj.group(2))

            # do the replacement, preserve alignment if possible
            if cfg_name in change_set_names:
                replacement = change_set_names[cfg_name]

                len_delta = len(cfg_name) - len(replacement)
                spaces = " " * max(MIN_SPACE, len_spaces + len_delta)

                line = line[0:reg_obj.regs[1][0]] + replacement + spaces \
                       + line[reg_obj.regs[2][1]:]

            print(line, file=f)

        f.flush()

        if flag_ok:
            copyfile(f.name, test_file_path)

    return flag_ok


def rename_in_tests(change_set_names, test_dir):
    """ applies rename_in_test, for all *.test files inside test_dir """
    files = dir_glob(test_dir, "*.test")
    for file in files:
        if not rename_in_test(file, change_set_names):
            return False

    return True


def check_file_rename(old_path, new_path):
    """
    renames/moves config file,

    if new_path file exists:
        - checks if it is a single option file and if it does have the same
          option as the old_path single option file,
        - appends comment lines of old_path into new_path
        - delletes old_path, keeps new_path


    Parameters
    ----------------------------------------------------------------------------
    :param old_path: str
        path to the file that is going to be renamed/moved

    :param new_path: str
        target path where old_path file is going to be moved to


    :return: bool
    ----------------------------------------------------------------------------
        True if no errors occurred, False otherwise
    """

    # if file exists already
    if isfile(new_path):
        # use nameing convention of the file to get name and shortened value
        new_name = basename(new_path)
        o_name, o_val = new_name.rsplit("-", 1)

        # (reverse ambiguous naming: force, false -> f -> force||false)
        # check only the first char, should be good enough
        o_val = o_val.rsplit(".", 1)[0][0]

        # check if it is has only the according (one) option and value
        with open(new_path, 'r') as f:
            l_nr = 0
            for line in f:
                l_nr += 1
                if do_skip_line(line):
                    continue

                k, v = parse_config_line(line)
                if not k or not v:
                    print("error while parsing line %d\n %s" % (l_nr, new_path),
                          file=stderr)
                    return False
                if k != o_name or v[0] != o_val:
                    print("Option/Value combination missing: %s = %s...\n"
                          "or not a single option file:\n %s" % old_path,
                          file=stderr)
                    return False

        # check if the old file has comment lines
        comment_lines = []
        with open(old_path, 'r') as f:
            for line in f:
                if is_comment_line(line):
                    comment_lines.append(line.strip())
        # append them to the existing new_file
        if comment_lines:
            with open(new_path, 'a') as f:
                print("\n", file=f)         # print 2 newlines
                for line in comment_lines:
                    print(line, file=f)

        file_remove(old_path)
        return True

    file_rename(old_path, new_path)
    return True


def change_set_apply(file_paths, test_dir):
    """
    applies renamings if needed (in actual config files and inside test files)


    Parameters
    ----------------------------------------------------------------------------
    :param file_paths:
        paths to the files that are going to be processed

    :param test_dir:
        path to the directory containing test files


    :return: bool
    ----------------------------------------------------------------------------
        True if no errors occurred, False otherwise
    """
    change_set = get_change_set(file_paths)
    if not change_set:
        return True

    # test files only use the file name, remove the path
    change_set_names = {}
    for k, v in change_set.items():
        k = basename(k)
        v = basename(v)
        change_set_names[k] = v
    if not rename_in_tests(change_set_names, test_dir):
        print("Error while renaming option(s) in tests file(s)", file=stderr)

    for k, v in change_set.items():
        if not check_file_rename(k, v):
            return False
    return True


def do_apply_file(file_path, test_dir):
    """ calls change_set_apply with a single config file path """
    return change_set_apply([file_path], test_dir)


def do_apply_dir(dir_path, test_dir):
    """ calls change_set_apply with a all config file paths inside test_dir """
    file_paths = dir_glob(dir_path, "*.cfg")
    return change_set_apply(file_paths, test_dir)


if __name__ == "__main__":
    """
    parses all script arguments and based on that calls needed functions
    """

    arg_parser = ArgumentParser()
    group_mode = arg_parser.add_mutually_exclusive_group(required=True)
    group_mode.add_argument(
        '-c', '--check', action='store_true',
        help="checks if renaming changes are necessary (1) or not (0)"
    )

    group_mode.add_argument('-a', '--apply', action='store_true',
                            help="applies renaming")
    arg_parser.add_argument(
        '-t', '--testsdir', type=str, required='--apply',
        help="directory containing *.test files in which the renaming also "
             "will be done"
    )

    arg_parser.add_argument(
        'input', metavar='INPUT', type=str,
        help='either a single file or all *.cfg files inside a directory'
    )

    FLAGS, unparsed = arg_parser.parse_known_args()

    if FLAGS.apply:
        mode_fcn = do_apply_dir if isdir(FLAGS.input) else do_apply_file
        exit(0 if mode_fcn(FLAGS.input, FLAGS.testsdir) else 1)

    mode_fcn = do_check_dir if isdir(FLAGS.input) else do_check_file
    exit(0 if mode_fcn(FLAGS.input) else 1)
