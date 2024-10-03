#!/usr/bin/env python3
import tempfile
import os
import subprocess
import sys

def error_out(code, msg):
    print(f'{sys.argv[0]}: {msg}', file=sys.stderr)
    exit(code)

def print_error(msg):
    print(msg, file=sys.stderr)

if len(sys.argv) != 3:
    error(1, f'usage: {sys.argv[0]} compiler_bin tests_dir')


def try_remove(file_paths):
    for p in file_paths:
        try:
            os.remove(p)
            pass
        except FileNotFoundError:
            pass
        except PermissionError:
            print('Inadequate permissions to delete tempfile {p}')
        except Exception as e:
            print(f'Unexpected exception when deleting tempfile(s) {p}: {e}')


class Test:
    def __init__(self, program, comp_error, ret):
        self.program = program
        self.comp_error = comp_error
        self.ret = ret

    def run(self, compiler_bin_path):
        tf = open("tmpfile", 'w')
        bc = f'{tf.name}.bc'
        obj = f'{tf.name}.o'
        bn = f'{tf.name}-bin'

        tf.write(self.program)
        tf.close()
        cmd = [compiler_bin_path, tf.name, '-o', bc]
        res = subprocess.run(cmd, capture_output=True, text=True)
        if not res.stdout.startswith('Debug mode enabled'):
            error_out(1, f'debug mode not enabled in compiler binary {compiler_bin_path}')
        llc = ['llc', '--filetype=obj', bc, '-o', obj]
        llc_res = subprocess.run(llc, capture_output=True, text=True)

        link = ['clang', obj, '-o', bn]
        link_res = subprocess.run(link, capture_output=True, text=True)

        bn_res = subprocess.run(f'./{bn}', capture_output=True, text=True)
        try_remove([tf.name, bc, obj, bn])
        return 1
        

def test_from_testfile(open_file):
    err = None
    ret = None
    program = ''
    for line in open_file:
        splt = line.split()
        match splt[0]:
            case 'comp_err':
                if len(splt) != 2:
                    print(f'in testfile {f.name}: comp_error directive requires exactly one arg. Got {len(splt)-1}', file=sys.stderr)
                err = splt[1]
            case 'ret':
                if len(splt) != 2:
                    print_error(f'in testfile {f.name}: ret directive requires exactly one arg. Got {len(splt)-1}')
                try:
                    ret = int(splt[1])
                except ValueError:
                    print_error(f'in testfile {f.name}: could not parse supplied arg of ret directive "{splt[1]}" as integer')
            case '--TEXT--':
                for remaining in open_file:
                    program += remaining
                return Test(program, err, ret)
            case _:
                print_error(f'in testfile {f.name}: bad directive "{splt[0]}" in test {f.name}')
    print_error(f'in tesfile {f.name}: missing text section')


for file in os.listdir(sys.argv[2]):
    total = 0
    passed = 0
    if not file.endswith('.test'):
        continue
    with open(f'{sys.argv[2]}/{file}', 'r') as f:
        t = test_from_testfile(f)
        try:
            t.run(sys.argv[1])
        except PermissionError:
            print('Couldn\'t create necessary tempfile in /tmp directory. Fix permissions')
