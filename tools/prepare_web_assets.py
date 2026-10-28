"""Stage only runtime PNGs/manifests, never optional audio or development inputs."""
import hashlib
import json
from pathlib import Path
import shutil
import sys


def runtime_paths():
    paths = [f'tiles/{name}.png' for name in ('berry', 'acorn', 'leaf', 'mushroom', 'star', 'letter')]
    paths += [f'fonts/{name}.png' for name in ('serif', 'sans')]
    paths += [f'characters/hedgehog_{pose}.png' for pose in ('neutral', 'happy', 'worried', 'celebrating', 'letter')]
    paths += ['backgrounds/post_office.png']
    return paths


def stage(source, output):
    paths = runtime_paths()
    for pose in ('happy', 'worried'):
        directory = f'characters/animations/{pose}'
        fields = (source / directory / 'frames.txt').read_text().split()
        if fields[:3] != ['LWAN1', '256', '256']:
            raise ValueError(f'Invalid {pose} animation manifest')
        count = int(fields[3])
        if not 0 < count <= 1000 or len(fields) != 5 + count or any(float(t) <= 0 for t in fields[5:]):
            raise ValueError(f'Invalid {pose} animation timing')
        paths += [f'{directory}/frames.txt'] + [f'{directory}/frame_{i:04d}.png' for i in range(count)]
    # A private build staging directory: replace its assets so stale files cannot leak.
    root = output / 'assets'
    if root.exists():
        shutil.rmtree(root)
    inventory = []
    for name in sorted(paths):
        data = (source / name).read_bytes()
        if name.endswith('.png') and not data.startswith(b'\x89PNG\r\n\x1a\n'):
            raise ValueError(f'Invalid PNG: {name}')
        destination = root / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(data)
        inventory.append({'path': name, 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()})
    output.mkdir(parents=True, exist_ok=True)
    (output / 'inventory.json').write_text(json.dumps(inventory, indent=2) + '\n')


if __name__ == '__main__':
    stage(Path(sys.argv[1]), Path(sys.argv[2]))
