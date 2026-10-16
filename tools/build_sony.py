"""Recompile a configured module with Sony's compiler and relink it.

build_sony.py DEVPATH --build-dir DIR --output-dir DIR --compiler gcc48|snc
    [--source-compiler SOURCE=gcc48|snc ...] [--link-map FILE]

Reads CMake's actual compile/link recipes (the build configured with
cmake -S src -B DIR -DFW=<FW>), including per-source options. C and C++ objects are
compiled by sonycc.py with Sony's compiler; assembler objects and import stub
archives come from the configured build. The SDK linker and the configured packaging
steps produce a separate raw ELF/VELF. Does not change the input build.
"""
from pathlib import Path
import argparse
import re
import shlex
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import elfsections
import sonycc
from buildrecipe import compile_recipes, compile_options, read_flags

ROOT = sonycc.ROOT


def rel_path(devpath):
    """Source folder below src/ of a device path (os0:kd/sysmem.skprx -> os0/kd/sysmem)."""
    part, path = devpath.split(':', 1)
    return part + '/' + re.sub(r'\.(skprx|suprx|self|elf)$', '', path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('devpath')
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--compiler', choices=('gcc48', 'snc'), required=True)
    parser.add_argument('--source-compiler', action='append', default=[],
                        metavar='SOURCE=COMPILER',
                        help='compiler for a source in an original mixed-compiler module')
    parser.add_argument('--link-map', type=Path,
                        help='write diagnostic linker map (does not change compiler options)')
    args = parser.parse_args()
    source_compilers = {}
    for item in args.source_compiler:
        source, separator, compiler = item.rpartition('=')
        if not separator or compiler not in ('gcc48', 'snc'):
            parser.error('--source-compiler requires SOURCE=gcc48 or SOURCE=snc')
        source = Path(source).resolve()
        if not source.is_file() or source in source_compilers:
            parser.error('source compiler requires a unique existing source')
        source_compilers[source] = compiler
    rel = rel_path(args.devpath)
    target = rel.replace('/', '_')
    cwd = args.build_dir.resolve() / rel
    cmake = cwd / 'CMakeFiles' / (target + '.dir')
    out = args.output_dir.resolve()
    if out == cwd or cwd in out.parents:
        parser.error('output must be separate from the input module build')
    out.mkdir(parents=True, exist_ok=True)
    flags = read_flags(cmake / 'flags.make')
    recipes = compile_recipes((cmake / 'build.make').read_text(), flags, cwd)
    kind = args.compiler
    link = shlex.split((cmake / 'link.txt').read_text())
    for index, token in enumerate(link):
        if not token.endswith(('.o', '.obj')):
            continue
        obj = (cwd / token).resolve()
        recipe = recipes.get(obj)
        if recipe is None:
            raise ValueError(f'missing compile recipe for {obj}')
        source = (cwd / recipe[recipe.index('-c') + 1]).resolve()
        if source.suffix not in ('.c', '.cpp', '.cc', '.cxx'):
            continue
        # Keep object-relative paths: modules can have duplicate source basenames.
        candidate = out / obj.relative_to(cwd)
        options = compile_options(recipe)
        defines, includes = options['defines'], options['includes']
        opt, extra = options['opt'], options['extra']
        source_kind = source_compilers.pop(source, kind)
        print(f'{source_kind} {opt} {source.relative_to(ROOT)}', flush=True)
        if not sonycc.compile_one(source, candidate, source_kind, defines, includes, opt,
                                 extra=extra, mode=options['mode']):
            raise SystemExit(1)
        if any(h['type'] == 4 for h in elfsections.Sections(candidate.read_bytes()).headers):
            converted = candidate.with_suffix('.rel.o')
            converted.write_bytes(elfsections.object_rela_to_rel(candidate.read_bytes()))
            candidate = converted
        link[index] = str(candidate)
    if source_compilers:
        parser.error('source compiler selection was not present in the link recipe')
    raw = out / target
    velf = raw.with_suffix('.velf')
    link[link.index('-o') + 1] = str(raw)
    if args.link_map is not None:
        map_path = args.link_map.resolve()
        map_path.parent.mkdir(parents=True, exist_ok=True)
        link.append('-Wl,-Map,' + str(map_path))
    subprocess.run(link, cwd=cwd, check=True)
    subprocess.run([sys.executable, str(ROOT / 'tools/dbginfo.py'), str(raw)], check=True)
    packaged_raw = raw
    if any(h['type'] == 4 for h in elfsections.Sections(raw.read_bytes()).headers):
        # SNC emits RELA; vita-elf-create only consumes REL. Linked bytes
        # already contain the applied relocations, which its reader decodes.
        packaged_raw = raw.with_suffix('.rel.elf')
        packaged_raw.write_bytes(elfsections.linked_rela_to_rel(raw.read_bytes()))
    # Use the configured export file and all firmware-specific fixup arguments.
    packaging = (cwd / 'CMakeFiles' / (target + '_velf.dir') / 'build.make').read_text()
    sce_layout = '/mkprx.py elf ' in packaging
    markers = ('/mkprx.py elf ',) if sce_layout else ('vita-elf-create ', '/fixup_velf.py ', '/fixup_unwind.py ')
    for marker in markers:
        lines = [line.strip() for line in packaging.splitlines()
                 if '&& ' in line and marker in line]
        if not lines and marker == '/fixup_unwind.py ':
            continue
        if len(lines) != 1:
            raise ValueError(f'expected one configured packaging command for {marker}')
        line = lines[0]
        command = shlex.split(line.split('&&', 1)[1])
        command = [str(packaged_raw) if word in (target, str(cwd / target)) else
                   str(velf) if word in (target + '.velf', str(cwd / (target + '.velf'))) else word
                   for word in command]
        subprocess.run(command, cwd=cwd, check=True)
    print(velf)


if __name__ == '__main__':
    main()
