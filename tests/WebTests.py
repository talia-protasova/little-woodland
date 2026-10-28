"""Asset staging exclusions and artifact tamper detection, without an SDK dependency."""
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from prepare_web_assets import stage
from validate_web_dist import validate


class WebAssets(unittest.TestCase):
    def test_public_runtime_only_and_integrity_gate(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            stage(ROOT / 'assets', root / 'staging')
            records = json.loads((root / 'staging/inventory.json').read_text())
            self.assertEqual(len(records), 318)
            self.assertFalse(any('audio' in r['path'] or r['path'].endswith(('.gif', '.json')) for r in records))
            dist = root / 'dist'
            dist.mkdir()
            metadata = {'files': []}
            data = bytearray()
            for record in records:
                part = (root / 'staging/assets' / record['path']).read_bytes()
                metadata['files'].append({'filename': '/assets/' + record['path'], 'start': len(data), 'end': len(data) + len(part)})
                data.extend(part)
            (dist / 'index.html').write_text('<script src="little_woodland.js"></script>')
            script = 'little_woodland.wasm; little_woodland.data; loadPackage(' + json.dumps(metadata, separators=(',', ':')) + ');'
            (dist / 'little_woodland.js').write_text(script)
            (dist / 'little_woodland.wasm').write_bytes(b'\0asm')
            (dist / 'little_woodland.data').write_bytes(data)
            (dist / 'inventory.json').write_text(json.dumps(records))
            (dist / '.nojekyll').touch()
            validate(dist)
            # The pinned Release compiler removes key quotes and may shorten
            # numeric offsets to exponential notation; both forms must validate.
            import re
            minified = re.sub(r'"(files|filename|start|end)":', r'\1:', script)
            first_end = metadata['files'][0]['end']
            minified = minified.replace(f'end:{first_end}', f'end:{first_end}e0', 1)
            (dist / 'little_woodland.js').write_text(minified)
            validate(dist)
            data[20] ^= 1
            (dist / 'little_woodland.data').write_bytes(data)
            with self.assertRaisesRegex(ValueError, 'integrity mismatch'):
                validate(dist)
            (dist / 'rogue.wav').write_bytes(b'not a public asset')
            with self.assertRaisesRegex(ValueError, 'Unexpected distribution file'):
                validate(dist)
            (dist / 'rogue.wav').unlink()
            (dist / 'index.html').write_text('<script src="/little_woodland.js"></script>')
            with self.assertRaisesRegex(ValueError, 'Nonlocal'):
                validate(dist)


if __name__ == '__main__':
    unittest.main()
