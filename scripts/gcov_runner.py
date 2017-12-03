from __future__ import print_function  # python >= 2.6
from sys import exit, argv
from os.path import join, dirname, abspath, split
from os import name as os_name, chdir, remove as os_remove, rename
from subprocess import call
from glob import glob

ROOT_DIR = dirname(dirname(abspath(__file__)))
SRC_DIR = join(ROOT_DIR, "src")
SCRIPTS_DIR = join(ROOT_DIR, "scripts")

NULL_DEV = "/dev/null" if os_name != "nt" else "nul"


def main(argv):
    if not argv or len(argv) < 2:
        return

    build_dir = argv[0]
    target_file = argv[1]

    target_name = split(target_file)[1]
    root_dir_name = split(ROOT_DIR)[1]
    src_dir_name = split(SRC_DIR)[1]

    # remove old gcov files in order to not scew counters
    chdir(build_dir)
    rm_files = glob("*.gcov")
    for rf in rm_files:
        os_remove(rf)

    # src/newlines.cpp -> buid/CMakeFiles/uncrustify.dir/src/
    target_file_path = join(build_dir, "CMakeFiles", root_dir_name+".dir",
                            src_dir_name, target_name+".gcda")

    ret = -1
    with open(NULL_DEV, 'w') as n:
        args = ['gcov', target_file_path]
        ret = call(args, stdout=n)

    if ret == 0:
        args = ['python', join(SCRIPTS_DIR, "code_remover.py"),
                join('.', target_name+".gcov")]
        with open(target_file+".tmp", 'w') as f:
            ret = call(args, stdout=f)

        if ret == 0:
            rename(target_file+".tmp", target_file)
        else:
            os_remove(target_file+".tmp")

    return ret


if __name__ == "__main__":
    exit(main(argv[1:]))
