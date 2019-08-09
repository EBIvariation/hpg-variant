#!/usr/bin/env python
import argparse
import subprocess
from collections import defaultdict
from toposort import toposort_flatten


def parse_cmdline():
    parser = argparse.ArgumentParser()
    parser.add_argument("files", type=str, nargs='+', help="Files to merge")
    return parser.parse_args()


def read_vcf(path):
    # less can handle both compressed and uncompressed files.
    cmd = "less \"{path}\" | grep -v '#' | cut -f1 | uniq".format(path=path)
    return subprocess.check_output(cmd, shell=True)


def read_contigs_file(state, f):
    last_value = None

    for value in f:
        value = value.strip()
        if last_value is not None:
            state[value].add(last_value)

        last_value = value

if __name__ == '__main__':
    state = defaultdict(set)
    args = parse_cmdline()

    for path in args.files:
        contigs = read_vcf(path)
        read_contigs_file(state, contigs.split('\n'))

    for item in toposort_flatten(state):
        print item

