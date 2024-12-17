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
    error_out(1, f'usage: {sys.argv[0]} compiler_bin tests_dir')


def try_remove(file_paths):
    for p in file_paths:
        try:
            os.remove(p)
            pass
        except FileNotFoundError:
            pass
        except PermissionError:
            print_error(f'Inadequate permissions to delete tempfile {p}')
        except Exception as e:
            print_error(f'Unexpected exception when deleting tempfile(s) {p}: {e}')


class Test:
    def __init__(self, name, program, comp_error, ret):
        self.name = name
        self.program = program
        self.comp_error = comp_error
        self.ret = ret

    def run(self, compiler_bin_path):
        tf = open('tmpfile', 'w')
        bc = f'{tf.name}.bc'
        obj = f'{tf.name}.o'
        bn = f'{tf.name}-bin'
        files = [tf.name, bc, obj, bn]
        def tf_print(msg):
            print(f'{self.name}: {msg}')

        tf.write(self.program)
        tf.close()
        cmd = [compiler_bin_path, tf.name, '-o', bc]
        res = subprocess.run(cmd, capture_output=True, text=True)
        if not res.stdout.startswith('Debug mode enabled'):
            try_remove(files)
            error_out(1, f'{self.name} Debug mode not enabled in compiler binary {compiler_bin_path}')
        if self.comp_error and self.comp_error in res.stderr:
            try_remove(files)
            return 1
        elif res.returncode != 0:
            if self.comp_error:
                tf_print(f'Did not get expected error "{self.comp_error}"')
            tf_print('Got unexpected error during compilation:')
            for line in res.stderr.split('\n'):
                tf_print(f'\t{line}')
            try_remove(files)
            return 0

        llc = ['llc', '--filetype=obj', bc, '-o', obj]
        llc_res = subprocess.run(llc, capture_output=True, text=True)
        if llc_res.stderr != "":
            tf_print('Unexpected failure during call to llc')
            try_remove(files)
            return 0

        link = ['clang', obj, '-o', bn]
        link_res = subprocess.run(link, capture_output=True, text=True)

        #quick and dirty check to see if program has a main function
        if llc_res.stderr != "":
            if 'let main: () -> i32' not in self.program:
                return 1
            tf_print('Unexpected failure while linking')
            try_remove(files)
            return 0
        elif 'let main: () -> i32' not in self.program:
            tf_print('Missing main function but linking didn\'t fail?')
            try_remove(files)
            return 0

        if not os.path.isfile(f'./{bn}'):
            tf_print('Could not compile to binary')
            return 0
        bn_res = subprocess.run(f'./{bn}', capture_output=True, text=True)
        try_remove(files)
        if self.comp_error:
            tf_print(f'Encountered no errors despite expecting an error "{self.comp_error}"')
            return 0
        if self.ret != bn_res.returncode:
            tf_print('Return codes did not match.')
            return 0
        return 1

def test_from_testfile(open_file):
    err = None
    ret = None
    program = ''
    hd = False # 'hd' = 'header done'
    for line in open_file:
        program += line
        if not hd and not line.startswith('//'):
            raise Exception(f'in testfile {open_file.name}: Could not parse command in line {line.strip()}')
        elif not hd:
            splt = line.strip().split()
            match splt[1]:
                case 'comp_err':
                    if len(splt) != 3:
                        raise Exception(f'in testfile {open_file.name}: comp_error directive requires exactly one arg.')
                    err = ' '.join(splt[2:])
                case 'ret':
                    if len(splt) != 3:
                        raise Exception(f'in testfile {open_file.name}: ret directive requires exactly one arg. Got {len(splt)-1}')
                    try:
                        ret = int(splt[2])
                    except ValueError:
                        raise Exception(f'in testfile {open_file.name}: could not parse supplied arg of ret directive "{splt[1]}" as integer')
                case 'END_HEADER':
                    hd = True
                case _:
                    raise Exception(f'in testfile {open_file.name}: bad directive "{splt[0]}" in test {open_file.name}')
    return Test(open_file.name, program, err, ret)


total = 0
passed = 0
for file in os.listdir(sys.argv[2]):
    if not file.endswith('.test'):
        continue
    total += 1
    with open(f'{sys.argv[2]}/{file}', 'r') as f:
        try:
            t = test_from_testfile(f)
            if t.run(sys.argv[1]):
                passed += 1
        except PermissionError:
            print_error('Couldn\'t create necessary tempfile in /tmp directory. Fix permissions')
        except Exception as e:
            print(e)


print(f'{passed}/{total}')
