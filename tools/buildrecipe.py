"""Read configured CMake compile recipes without changing compiler defaults."""
from pathlib import Path
import shlex
import re

DEFAULT_F = ('-fstack-protector-strong', '-ffreestanding', '-fno-builtin',
             '-fno-optimize-sibling-calls', '-fno-delete-null-pointer-checks',
             '-fno-section-anchors', '-fno-shrink-wrap')


def read_flags(path):
    return dict(line.split(' = ', 1) for line in path.read_text().splitlines()
                if ' = ' in line and not line.startswith('#'))


def compile_recipes(text, flags, cwd):
    """Return object -> argv, expanding flags while retaining option order."""
    out = {}
    for line in text.splitlines():
        if not line.startswith('\t') or ' -c ' not in line:
            continue
        command = line.strip()
        for key, value in flags.items():
            command = command.replace('$(' + key + ')', value)
        argv = shlex.split(command)
        if '&&' in argv:
            argv = argv[argv.index('&&') + 1:]
        if '-c' not in argv or '-o' not in argv:
            continue
        out[(cwd / argv[argv.index('-o') + 1]).resolve()] = argv
    return out


def instruction_mode(argv):
    """Honor explicit GCC mode options; absent options retain our Thumb default."""
    modes = [word[2:] for word in argv if word in ('-marm', '-mthumb')]
    return modes[-1] if modes else 'thumb'


def compile_options(argv):
    """Options consumed by the Sony compiler wrapper, in configured order."""
    opts = [x for x in argv if re.fullmatch(r'-O(?:[0-3sg]|fast)', x)]
    return dict(defines=[x for x in argv if x.startswith('-D')],
                includes=[x for x in argv if x.startswith('-I')],
                opt=opts[-1] if opts else '-O2',
                extra=[x for x in argv if x.startswith('-f') and x not in DEFAULT_F],
                mode=instruction_mode(argv))


def source_options(cmake, cwd, source):
    recipes = compile_recipes((cmake / 'build.make').read_text(),
                              read_flags(cmake / 'flags.make'), cwd)
    matches = [compile_options(argv) for argv in recipes.values()
               if (cwd / argv[argv.index('-c') + 1]).resolve() == source.resolve()]
    if matches and any(value != matches[0] for value in matches[1:]):
        raise ValueError(f'conflicting configured compile options for {source}')
    if not matches:
        raise ValueError(f'no configured compile recipe for {source}')
    return matches[0]


def source_mode(cmake, cwd, source):
    return source_options(cmake, cwd, source)['mode']
