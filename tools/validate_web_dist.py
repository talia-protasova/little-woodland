"""Check the self-contained sound-free Pages artifact and preloaded byte inventory."""
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import sys
from prepare_web_assets import runtime_paths


def diagnostic(file, kind, detail):
    """Bound and escape untrusted generated content; never print a whole script."""
    excerpt = str(detail)[:160]
    return ValueError(f'{file}: {kind}: {json.dumps(excerpt, ensure_ascii=True)}')


def check_paths(file, text, package_metadata=False):
    """Reject host paths, except the pinned SDK's non-fetch package identifier.

    file_packager.py sets PACKAGE_NAME from the link output path but fetches
    REMOTE_PACKAGE_BASE via locateFile. In this uncached build PACKAGE_NAME is
    used only as Module.preloadResults' bookkeeping key, never as a resource URL.
    Mask only that declaration, and only when all its uses have that known role.
    """
    if package_metadata:
        declaration = re.search(r"var\s+PACKAGE_NAME\s*=\s*([\"'])([^\"'\\\r\n]*)\1\s*;", text)
        remote = re.search(r"var\s+REMOTE_PACKAGE_BASE\s*=\s*([\"'])little_woodland\.data\1\s*;", text)
        if declaration and remote and PurePosixPath(declaration[2]).name == 'little_woodland.data':
            other_uses = text[:declaration.start()] + text[declaration.end():]
            other_uses = re.sub(r"""Module(?:\.preloadResults|\["preloadResults"\]|\['preloadResults'\])\[PACKAGE_NAME\]""", '', other_uses)
            if not re.search(r'\bPACKAGE_NAME\b', other_uses):
                text = text[:declaration.start()] + text[declaration.end():]
                # The same path also labels add/removeRunDependency's paired
                # completion tokens. These are identifiers, not fetched URLs.
                token = re.escape('datafile_' + declaration[2])
                text = re.sub(r"""Module(?:\[["'](?:add|remove)RunDependency["']\]|\.(?:add|remove)RunDependency)\(["']"""
                              + token + r"""["']\)""", '', text)
    resource = re.search(r"""["']((?:/|https?://)[^"'\s]*\.(?:js|wasm|data)(?:[?#][^"'\s]*)?)["']""", text)
    if resource:
        raise diagnostic(file, 'nonrelative runtime resource', resource[1])
    for pattern in (r"""/home/(?!web_user(?:["'\x00]|$))""", r'/tmp/'):
        match = re.search(pattern, text)
        if match:
            raise diagnostic(file, 'unexpected local absolute path',
                             text[max(0, match.start() - 32):match.start() + 128])
    for dependency in ('SharedArrayBuffer', 'PThread', 'mixkit/'):
        if dependency in text:
            raise diagnostic(file, 'unexpected runtime dependency', dependency)


def validate(root):
    allowed = {'index.html', 'little_woodland.js', 'little_woodland.wasm', 'little_woodland.data',
               'inventory.json', '.nojekyll'}
    files = [p for p in root.rglob('*') if p.is_file()]
    if not allowed.issubset({p.name for p in files if p.parent == root}):
        raise diagnostic('distribution', 'missing files', ', '.join(sorted(allowed - {p.name for p in files if p.parent == root})))
    for path in files:
        relative = path.relative_to(root).as_posix()
        if relative not in allowed and relative not in {'notices/' + name for name in
                ('stb.txt', 'GLFW.txt', 'fonts.txt', 'emscripten.txt', 'LLVM.txt', 'musl.txt')}:
            raise diagnostic(relative, 'Unexpected distribution file', relative)
        if path.suffix.lower() in {'.wav', '.cpp', '.h', '.py', '.gif', '.map'}:
            raise diagnostic(relative, 'Forbidden runtime file', relative)
    html = (root / 'index.html').read_text()
    for url in re.findall(r'(?:src|href)=["\']([^"\']+)', html):
        if url == 'data:,':
            continue
        if url.startswith(('http:', 'https:', '//', '/')) or not (root / url).is_file():
            raise diagnostic('index.html', 'Nonlocal or missing HTML resource', url)
    script = (root / 'little_woodland.js').read_text()
    check_paths('little_woodland.js', script, package_metadata=True)
    check_paths('index.html', html)
    if 'little_woodland.wasm' not in script or 'little_woodland.data' not in script:
        raise diagnostic('little_woodland.js', 'missing resource references', 'little_woodland.wasm / little_woodland.data')
    wasm = (root / 'little_woodland.wasm').read_bytes()
    check_paths('little_woodland.wasm', wasm.decode('latin1'))
    if wasm[:4] != b'\0asm':
        raise diagnostic('little_woodland.wasm', 'invalid header', wasm[:4])
    # Emscripten concatenates the preloaded bytes; validate slices against inventory
    # and its generated offset table, not merely the names of output files.
    match = re.search(r'loadPackage\((\{(?:"files"|files):\[.*?\](?:,[^{}]*?)?\})\)', script)
    if not match:
        raise diagnostic('little_woodland.js', 'missing Emscripten preload metadata', 'loadPackage(...)')
    # Release JS minification removes quotes from identifier property names.
    metadata_json = re.sub(r'([{,])(\w+):', r'\1"\2":', match.group(1))
    metadata = json.loads(metadata_json)
    inventory = json.loads((root / 'inventory.json').read_text())
    records = {r['path']: r for r in inventory}
    if len(records) != len(inventory) or not set(runtime_paths()).issubset(records):
        raise diagnostic('inventory.json', 'incomplete or duplicate asset inventory', 'runtime asset records')
    data = (root / 'little_woodland.data').read_bytes()
    seen = set()
    for entry in metadata['files']:
        name = entry['filename'].removeprefix('/assets/')
        relative = PurePosixPath(name)
        if relative.is_absolute() or '..' in relative.parts or relative.suffix not in {'.png', '.txt'}:
            raise diagnostic('little_woodland.js', 'invalid preloaded path', name)
        if name not in records or name in seen:
            raise diagnostic('little_woodland.js', 'unexpected or duplicate preloaded asset', name)
        seen.add(name)
        start, end = int(entry['start']), int(entry['end'])
        if start != entry['start'] or end != entry['end'] or not 0 <= start < end <= len(data):
            raise diagnostic('little_woodland.js', 'invalid preload offsets', name)
        entry['start'], entry['end'] = start, end
        part = data[start:end]
        if len(part) != records[name]['bytes'] or hashlib.sha256(part).hexdigest() != records[name]['sha256']:
            raise diagnostic('little_woodland.data', 'Preloaded integrity mismatch', name)
    if seen != set(records) or sum(r['bytes'] for r in inventory) != len(data):
        raise diagnostic('little_woodland.data', 'unaccounted asset bytes', 'preload inventory/offsets do not cover the data')
    for pose in ('happy', 'worried'):
        prefix = f'characters/animations/{pose}/'
        manifest = next(e for e in metadata['files'] if e['filename'] == '/assets/' + prefix + 'frames.txt')
        fields = data[manifest['start']:manifest['end']].decode().split()
        count = int(fields[3])
        expected = {prefix + 'frames.txt'} | {prefix + f'frame_{i:04d}.png' for i in range(count)}
        if expected != {n for n in seen if n.startswith(prefix)}:
            raise diagnostic('little_woodland.data', 'incomplete animation', pose)
    print(f'Web distribution validated: {len(records)} runtime assets, {len(data):,} preloaded bytes; no audio')


if __name__ == '__main__':
    try:
        validate(Path(sys.argv[1]))
    except (ValueError, OSError, KeyError, StopIteration) as error:
        sys.exit(f'Web validation failed: {error}')
