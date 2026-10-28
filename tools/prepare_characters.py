"""Prepare authored character GIFs as full-canvas PNG frames and timing manifests.

Pillow is needed only when preparing/checking assets. The game loads committed PNGs
and frames.txt through FrameAnimation, avoiding a runtime GIF decoder. Original GIF
files are read only; this tool writes solely into the prepared destination folders.
Use --check to compare composed pixels, transparency and manifest timing without
rewriting the prepared outputs.
"""
import argparse
import hashlib
import json
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]

def prepare(source, destination, check=False):
    """Export or verify one 256x256 character sequence.

    Args:
        source: Original GIF path, read through Pillow's compositing/disposal state.
        destination: Prepared frame directory read by the runtime FrameAnimation.
        check: Compare existing PNG pixels and manifest instead of writing outputs.

    Frame delays are preserved in seconds in LWAN1, rather than normalized to one
    frame rate. GIF repeat counts become total plays; zero means infinite looping.
    source.json identifies the input bytes but is not required by the renderer.

    Raises:
        ValueError: Canvas/duration requirements or checked outputs do not match.
        OSError: A source or prepared file cannot be read/written.
    """
    with Image.open(source) as gif:
        if gif.size != (256, 256):
            raise ValueError(f'{source}: expected 256x256 canvas')
        # GIF loop metadata counts repeats after the first play. A missing loop
        # extension means one play, unlike an explicit zero meaning repeat forever.
        plays = 0 if gif.info.get('loop') == 0 else gif.info.get('loop', 0) + 1
        durations = []
        if not check:
            destination.mkdir(parents=True, exist_ok=True)
        for i in range(gif.n_frames):
            # Pillow seek composes prior frames and applies GIF disposal before RGBA export.
            gif.seek(i)
            frame = gif.convert('RGBA')
            # Reject absent timing instead of inventing a speed that would change
            # the artist's sequence or make runtime frame selection ambiguous.
            duration = gif.info.get('duration', 0)
            if duration <= 0:
                raise ValueError(f'{source}: frame {i} has no positive duration')
            durations.append(duration)
            path = destination / f'frame_{i:04}.png'
            if check:
                with Image.open(path) as actual:
                    if actual.size != (256,256) or actual.mode != 'RGBA' or actual.tobytes() != frame.tobytes():
                        raise ValueError(f'{path}: pixels/transparency differ from composed GIF')
            else:
                frame.save(path)
        # Runtime needs only the fixed canvas, frame count, total plays and these
        # durations. Numbered full-canvas PNGs already include disposal/composition.
        manifest = 'LWAN1\n256 256 {} {}\n'.format(gif.n_frames, plays)
        manifest += ''.join(f'{ms/1000:.3f}\n' for ms in durations)
        if check:
            if (destination / 'frames.txt').read_text() != manifest:
                raise ValueError(f'{source}: duration/loop manifest differs')
        else:
            (destination / 'frames.txt').write_text(manifest)
            (destination / 'source.json').write_text(json.dumps({
                'source': source.name, 'sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
                'frames': gif.n_frames, 'durations_ms': durations, 'total_plays': plays,
                'composition': 'Pillow seek applies GIF disposal; full-canvas RGBA export',
            }, indent=2) + '\n')
        print(f'{source.name}: {gif.n_frames} RGBA frames, {sum(durations)} ms; ' + ('verified' if check else 'prepared'))

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    for pose in ('happy', 'worried'):
        prepare(ROOT / f'assets/characters/hedgehog_{pose}.gif',
                ROOT / f'assets/characters/animations/{pose}', args.check)
