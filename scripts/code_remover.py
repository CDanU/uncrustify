from __future__ import print_function  # python >= 2.6
from sys import exit
from sys import argv
import re

STATE = {
    'non_closed_paren': 0,
    'paren_open_line': -1,
    'paren_open_col': -1,
    'paren_close_line': -1,
    'paren_close_col': -1,
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


def process_parens_line(lines, idx):
    char_bs = False
    in_string = False
    string_quote = '"'

    for i_c, (c) in enumerate(lines[idx]):
        handle_comments(lines, idx)
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
        # TODO: C++11 R strings

        if in_string:
            continue

        if c == '(':
            if STATE['non_closed_paren'] == 0:
                STATE['paren_open_line'] = idx
                STATE['paren_open_col'] = i_c
            STATE['non_closed_paren'] += +1

        elif c == ')':
            STATE['non_closed_paren'] += -1
            if STATE['non_closed_paren'] == 0:
                STATE['paren_close_line'] = idx
                STATE['paren_close_col'] = i_c
                return


def parse_gcov_line_rem(lines, idx):
    init_idx = idx
    handle_comments(lines, idx)

    # ignore remove of a single '}'
    stripped_line = lines[idx][16:].strip()
    if stripped_line == "}":
        return

    # rem ml-statements parenthesis, keep preceeding block name (removed later)
    if lines[idx].find('(') != -1:
        STATE['non_closed_paren'] = 0
        for i in range(idx, len(lines)):
            process_parens_line(lines, i)

            if STATE['non_closed_paren'] == 0:
                break

        if STATE['non_closed_paren'] != 0:
            print("Error: STATE['non_closed_paren'] != 0:")
            exit(1)

        po_l = STATE['paren_open_line']
        po_c = STATE['paren_open_col']
        pc_l = STATE['paren_close_line']
        pc_c = STATE['paren_close_col']
        STATE['paren_open_line'] = -1
        STATE['paren_open_col'] = -1
        STATE['paren_close_line'] = -1
        STATE['paren_close_col'] = -1

        # adjust pc_c to remove remove possible ';' after ')'
        lstripped_rest = lines[pc_l][pc_c + 1:].lstrip()
        if lstripped_rest and lstripped_rest[0] == ';':
            pc_c = lines[pc_l].find(';', pc_c + 1)+1

        if po_l == pc_l:                           # paren span 1 line
            lines[po_l] = lines[po_l][:po_c] + lines[pc_l][pc_c+1:]
        else:                                      # multi line paren span
            lines[po_l] = lines[po_l][:po_c]
            lines[pc_l] = lines[pc_l][:16] + lines[pc_l][pc_c+1:]

            if (pc_l - po_l) > 1:                    # delete in between ends
                for i in range(po_l+1, pc_l):        # pc_l -1 + 1
                    lines[i] = lines[i][:16]
        idx = pc_l

    # handle possible trailing ')'
    guard_idx = []
    rem = lines[idx].find(')')
    if rem != -1:
        lines[idx] = lines[idx][:16] + lines[idx][rem:]
        guard_idx.append(idx)

    # rem-blocks
    if (idx not in guard_idx            # trailing ) -> idx not start of a block
            and idx+1 < len(lines)      # overflow check
            and re.match('^\s*{', lines[idx+1][16:])):  # see if block start
        STATE['non_closed_brace'] = 0
        for i in range(idx+1, len(lines)):
            handle_comments(lines, i)
            i_cb = 0
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
                # TODO: C++11 R strings

                if in_string:
                    continue

                if c == '{':
                    STATE['non_closed_brace'] += +1
                elif c == '}':
                    STATE['non_closed_brace'] += -1
                    i_cb = i_c
                    if STATE['non_closed_brace'] == 0:
                        break

            if STATE['non_closed_brace'] == 0:
                lines[i] = lines[i][:16] + lines[i][i_cb+1:]
                idx = i
                break
            else:
                lines[i] = lines[i][:16]

        # check if else on nextline if 'if'-block was removed,
        # if true add dummy back
        if idx+1 < len(lines):
            lstripped_initial = lines[init_idx][16:].lstrip()
            if lstripped_initial.startswith("if"):
                lstripped_pot_else = lines[idx+1][16:].lstrip()
                if lstripped_pot_else.startswith("else"):
                    lines[init_idx] = lines[init_idx][:16] + "if(true){}"
                    guard_idx.append(init_idx)

    if init_idx not in guard_idx:
        # remove initial rem line
        lines[init_idx] = lines[init_idx][:16]

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


def main(argv):
    if not argv:
        return

    target_file = argv[0]
    lines = []

    with open(target_file, 'r') as f:
        lines = f.read().splitlines()

    for idx in range(0, len(lines)):
        parse_gcov_line(lines, idx)

        line = lines[idx][16:]
        if line:
            print(line)


if __name__ == "__main__":
    exit(main(argv[1:]))
