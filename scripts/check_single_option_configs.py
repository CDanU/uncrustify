
from __future__ import print_function  # python >= 2.6
from os.path import dirname, abspath, join, split
from sys import exit
from os import chdir
from glob import glob
from sys import stderr

ROOT_DIR = dirname(dirname(abspath(__file__)))
CONFIG_DIR = join(ROOT_DIR, './tests/config')

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


def parse_config_line(line):
    if not line:
        return None, None        # return on empty string

    pound_pos = line.find('#')
    if pound_pos != -1:
        if pound_pos == 0:
            return None, None    # comment line -> empty string
        line = line[:pound_pos]  # cut away the  comment part

    split_pos = line.find('=')
    if split_pos == -1:
        split_pos = line.find(' ')
    if split_pos == -1:
        return None, None        # no appropriate separator found

    key = line[:split_pos].strip()
    value = line[split_pos + 1:].strip()
    return key, value


def get_change_set(file_paths):
    lines = []
    options = {}

    change_set = {}

    for file_path in file_paths:
        options.clear()

        with open(file_path, 'r') as f:
            tmp_lines = f.read()
            lines = tmp_lines.splitlines()

        for idx, (line) in enumerate(lines):
            if not line or line[0] == '#':
                continue  # skip empty lines
            key, value = parse_config_line(line)

            if key and value:
                if key in options:
                    raise RuntimeError("option '%s' defined multiple times"
                                       % key)
                options[key] = value

                if len(options) > 1:
                    break  # multi option file -> ignore this file
            else:
                raise ValueError("error while parsing line %d, %s"
                                 % (idx, file_path))

        if not options or len(options) > 1:
            continue  # multi option file -> ignore

        key, value = options.popitem()

        if value in VALUE_MAP:
            value = VALUE_MAP[value]
        else:
            try:
                int(value)  # if number use the number string
            except ValueError:
                continue    # else: some other string -> ignore this file

        expected_name = key+"-"+value+".cfg"

        f_path, f_name = split(file_path)

        if f_name != expected_name:
            change_set[file_path] =  join(f_path, expected_name)
            pass

    return change_set


def main():
    chdir(CONFIG_DIR)
    test_files = glob("*.cfg")

    change_set = get_change_set(test_files)

    if change_set:
        for k, v in change_set.items():
            print("Error: file '%s' needs to be renamed to '%s'"
                  % (k, v), file=stderr)
        return 1


if __name__ == "__main__":
    exit(main())
