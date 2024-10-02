#!/usr/bin/env python3
import os
import sys

def error(code, msg):
    print(f'{sys.argv[0]}: {msg}', file=sys.stderr)
    exit(code)

if len(sys.argv) != 3:
    error(1, f'Usage: {sys.argv[0]} compiler_bin tests_dir')


class Test:
    def __init__(self, program, comp_error, ret):
        self.program = program
        self.comp_error = error
        self.ret = ret

    def __str__(self):
        return f''

    def run(self, compiler_bin_path):
        print(compiler_bin_path)
        

def test_from_testfile(open_file):
    err = None
    ret = None
    program = ''
    for line in open_file:
        splt = line.split()
        match splt[0]:
            case 'comp_err':
                if len(splt) != 2:
                    error(1, f'in testfile {f.name}: comp_error directive requires exactly one arg. Got {len(splt)-1}')
                err = splt[1]
            case 'ret':
                if len(splt) != 2:
                    error(1, f'in testfile {f.name}: ret directive requires exactly one arg. Got {len(splt)-1}')
                try:
                    ret = int(splt[1])
                except ValueError:
                    error(1, f'in testfile {f.name}: could not parse supplied arg of ret directive "{splt[1]}" as integer')
            case '--TEXT--':
                for remaining in open_file:
                    program += remaining
                return Test(program, err, ret)
            case _:
                error(1, f'in testfile {f.name}: bad directive "{splt[0]}" in test {f.name}')
    error(1, f'in tesfile {f.name}: missing text section')


for file in os.listdir(sys.argv[2]):
    if file.endswith('.swp'):
        continue
    with open(f'{sys.argv[2]}/{file}', 'r') as f:
        t = test_from_testfile(f)
        t.run(sys.argv[1])
