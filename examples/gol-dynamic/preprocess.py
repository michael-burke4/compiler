#!/usr/bin/env python3

import argparse
import os
import sys

def preprocess(infile, outfile_name):
    outstring = ""
    for line in infile:
        incstring = '#include '
        if (line.startswith(incstring)):
            incfile = line[len(incstring):].strip()
            if not incfile.startswith('"') or not incfile.endswith('"') or not os.path.isfile(stp := incfile.strip('"')):
                print(f'Could not include file {incfile}', file=sys.stderr)
                exit(1)
            else:
                with open(stp) as f2:
                    for line in f2:
                        outstring += line
        else:
            outstring += line
    with open(outfile_name, 'w') as outf:
        print(outstring, file=outf, end="")

def main():
    parser = argparse.ArgumentParser(description="The world's worst preprocessor")
    parser.add_argument('input', metavar='INPUT', type=str, help='The input file to pre-process.')
    parser.add_argument('-o', '--output', metavar='OUTPUT', type=str, help='The output file (optional).')

    args = parser.parse_args()

    if not args.output:
        args.output = 'a.TXT'

    if not os.path.isfile(args.input):
        print(f'File {args.input} does not exist.', file=sys.stderr)
        exit(1)
    else:
        with open(args.input, 'r') as f:
            preprocess(f, args.output)

if __name__ == '__main__':
    main()
