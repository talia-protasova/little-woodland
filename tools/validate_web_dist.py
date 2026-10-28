"""Check the self-contained sound-free Pages artifact and preloaded byte inventory."""
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import sys
from prepare_web_assets import runtime_paths


def validate(root):
    allowed = {'index.html', 'little_woodland.js', 'little_woodland.wasm', 'little_woodland.data',
               'inventory.json', '.nojekyll'}
    files = [p for p in root.rglob('*') if p.is_file()]
    if not allowed.issubset({p.name for p in files if p.parent == root}):
        raise ValueError('Missing browser distribution files')
    for path in files:
        relative = path.relative_to(root).as_posix()
        if relative not in allowed and relative not in {'notices/' + name for name in
                ('stb.txt', 'GLFW.txt', 'fonts.txt', 'emscripten.txt', 'LLVM.txt', 'musl.txt')}:
            raise ValueError(f'Unexpected distribution file: {relative}')
        if path.suffix.lower() in {'.wav', '.cpp', '.h', '.py', '.gif', '.map'}:
            raise ValueError(f'Forbidden runtime file: {relative}')
    html = (root / 'index.html').read_text()
    for url in re.findall(r'(?:src|href)=["\']([^"\']+)', html):
        if url == 'data:,':
            continue
        if url.startswith(('http:', 'https:', '//', '/')) or not (root / url).is_file():
            raise ValueError(f'Nonlocal or missing HTML resource: {url}')
    script = (root / 'little_woodland.js').read_text()
    # /home/web_user is Emscripten's standard virtual home, not a host path.
    if re.search(r'/home/(?!web_user)', script + html):
        raise ValueError('Unexpected local absolute path')
    for forbidden in ('/tmp/', 'SharedArrayBuffer', 'PThread', 'mixkit/'):
        if forbidden in script or forbidden in html:
            raise ValueError(f'Unexpected path/platform dependency: {forbidden}')
    if 'little_woodland.wasm' not in script or 'little_woodland.data' not in script:
        raise ValueError('Missing generated relative resource references')
    wasm = (root / 'little_woodland.wasm').read_bytes()
    if re.search(rb'/home/(?!web_user)', wasm) or b'/tmp/' in wasm:
        raise ValueError('Unexpected local absolute path in WASM')
    if wasm[:4] != b'\0asm':
        raise ValueError('Invalid WebAssembly header')
    # Emscripten concatenates the preloaded bytes; validate slices against inventory
    # and its generated offset table, not merely the names of output files.
    match = re.search(r'loadPackage\((\{(?:"files"|files):\[.*?\](?:,[^{}]*?)?\})\)', script)
    if not match:
        raise ValueError('Missing Emscripten preload metadata')
    # Release JS minification removes quotes from identifier property names.
    metadata_json = re.sub(r'([{,])(\w+):', r'\1"\2":', match.group(1))
    metadata = json.loads(metadata_json)
    inventory = json.loads((root / 'inventory.json').read_text())
    records = {r['path']: r for r in inventory}
    if len(records) != len(inventory) or not set(runtime_paths()).issubset(records):
        raise ValueError('Incomplete or duplicate asset inventory')
    data = (root / 'little_woodland.data').read_bytes()
    seen = set()
    for entry in metadata['files']:
        name = entry['filename'].removeprefix('/assets/')
        relative = PurePosixPath(name)
        if relative.is_absolute() or '..' in relative.parts or relative.suffix not in {'.png', '.txt'}:
            raise ValueError(f'Invalid preloaded path: {name}')
        if name not in records or name in seen:
            raise ValueError(f'Unexpected or duplicate preloaded asset: {name}')
        seen.add(name)
        start, end = int(entry['start']), int(entry['end'])
        if start != entry['start'] or end != entry['end'] or not 0 <= start < end <= len(data):
            raise ValueError(f'Invalid preload offsets: {name}')
        entry['start'], entry['end'] = start, end
        part = data[start:end]
        if len(part) != records[name]['bytes'] or hashlib.sha256(part).hexdigest() != records[name]['sha256']:
            raise ValueError(f'Preloaded integrity mismatch: {name}')
    if seen != set(records) or sum(r['bytes'] for r in inventory) != len(data):
        raise ValueError('Unaccounted asset bytes')
    for pose in ('happy', 'worried'):
        prefix = f'characters/animations/{pose}/'
        manifest = next(e for e in metadata['files'] if e['filename'] == '/assets/' + prefix + 'frames.txt')
        fields = data[manifest['start']:manifest['end']].decode().split()
        count = int(fields[3])
        expected = {prefix + 'frames.txt'} | {prefix + f'frame_{i:04d}.png' for i in range(count)}
        if expected != {n for n in seen if n.startswith(prefix)}:
            raise ValueError(f'Incomplete {pose} animation')
    print(f'Web distribution validated: {len(records)} runtime assets, {len(data):,} preloaded bytes; no audio')


if __name__ == '__main__':
    try:
        validate(Path(sys.argv[1]))
    except (ValueError, OSError, KeyError, StopIteration) as error:
        sys.exit(f'Web validation failed: {error}')
