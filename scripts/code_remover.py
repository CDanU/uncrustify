from __future__ import print_function  # python >= 2.6
from sys import exit
import re

TARGET_FILE_IN = "/home/dan/_Files/devel/eclipse/uncrustify/build/combine.cpp.gcov"

STATE = {
    'non_closed_paren': 0,
    'non_closed_brace': 0,
}


def handle_ml_comment(lines, idx):
    for idx in range(idx, len(lines)):
        ret = lines[idx].find("*/")
        if ret != -1:
            lines[idx] = lines[idx][:16] + lines[idx][ret + 2:]
            return

        lines[idx] = lines[idx][:16]


def handle_comments(lines, idx):
    # remove sl-comments
    ret = lines[idx].find("//")
    if ret != -1:
        lines[idx] = lines[idx][:ret]
        return

    # remove ml-comments
    ret = lines[idx].find("/*")
    if ret != -1:
        handle_ml_comment(lines, idx)


def parse_gcov_line_rem(lines, idx):
    init_idx = idx
    handle_comments(lines, idx)

    # rem ml-statements
    if lines[idx].find('(') != -1:
        STATE['non_closed_paren'] = 0
        paren_change = False
        for i in range(idx, len(lines)):
            last_close = -1
            char_bs = False
            in_string = False
            string_quote = '"'

            for i_c, (c) in enumerate(lines[i]):
                if char_bs:
                    char_bs = False
                    continue
                if c == '\\':
                    char_bs = True

                if c == '"':
                    if not in_string:
                        string_quote = '"'
                        in_string = True
                    elif string_quote == '"':
                        in_string = False
                elif c == "'":
                    if not in_string:
                        string_quote = "'"
                        in_string = True
                    elif string_quote == "'":
                        in_string = False

                if in_string:
                    continue

                if c == '(':
                    STATE['non_closed_paren'] += +1
                elif c == ')':
                    STATE['non_closed_paren'] += -1
                    paren_change = True

                if STATE['non_closed_paren'] == 0 and paren_change:
                    paren_change = False
                    last_close = i_c

            if last_close != -1:
                s_idx = last_close + 1

                lstripped_rest = lines[i][last_close+1:].lstrip()
                if lstripped_rest and lstripped_rest[0] == ';':
                    s_idx = lines[i].find(';', last_close+1)

                lines[i] = lines[i][:16] + lines[i][s_idx:]
                idx = i
                break
            else:
                lines[i] = lines[i][:16]

    # handle trailing close paren
    guard_idx = -1
    rem = lines[idx].find(')')
    if rem != -1:
        lines[idx] = lines[idx][:16] + lines[idx][rem:]
        guard_idx = idx

    if guard_idx != init_idx:
        # remove initial rem line
        lines[init_idx] = lines[init_idx][:16]

    # rem-blocks
    if (guard_idx != idx                # trailing ) -> idx not start of a block
            and idx+1 < len(lines)      # overflow check
            and re.match('^\s*{', lines[idx+1][16:])):  # see if block start
        STATE['non_closed_brace'] = 0
        for i in range(idx+1, len(lines)):
            i_cb = 0

            for i_c, (c) in enumerate(lines[i]):
                if c == '{':
                    STATE['non_closed_brace'] += +1
                elif c == '}':
                    STATE['non_closed_brace'] += -1
                    i_cb = i_c

            if STATE['non_closed_brace'] == 0:
                lines[i] = lines[i][:16] + lines[i][i_cb+1:]
                idx = i
                break
            else:
                lines[i] = lines[i][:16]


def parse_gcov_line_ign(lines, idx):
    if lines[idx][13] == ' ' and lines[idx][14] == '0':
        lines[idx] = lines[idx][:16]
        return
    handle_comments(lines, idx)


def parse_gcov_line(lines, idx):
    if lines[idx][8] == '-':
        parse_gcov_line_ign(lines, idx)
    if lines[idx][8] == '#':
        parse_gcov_line_rem(lines, idx)


def main():
    lines = []
    with open(TARGET_FILE_IN, 'r') as f:
        lines = f.read().splitlines()

    for idx in range(0, len(lines)):
        parse_gcov_line(lines, idx)

        line = lines[idx][16:]
        if line:
            print(line)


if __name__ == "__main__":
    exit(main())
