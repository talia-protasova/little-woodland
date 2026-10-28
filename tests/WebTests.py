"""Asset staging exclusions and artifact tamper detection, without an SDK dependency."""
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from prepare_web_assets import stage
from validate_web_dist import check_paths, validate


class GeneratedPaths(unittest.TestCase):
    def package_script(self, path):
        return (f'var PACKAGE_NAME="{path}";var REMOTE_PACKAGE_BASE="little_woodland.data";'
                f'Module["addRunDependency"]("datafile_{path}");'
                'Module.preloadResults[PACKAGE_NAME]={fromCache:false};'
                f'Module["removeRunDependency"]("datafile_{path}");')

    def test_absolute_output_metadata_is_not_a_fetch_url(self):
        for directory in ('/home/runner/work/little-woodland/little-woodland/dist/web',
                          '/home/developer/game/dist/web', '/tmp/web-build'):
            with self.subTest(directory=directory):
                check_paths('little_woodland.js', self.package_script(directory + '/little_woodland.data'), True)
        check_paths('little_woodland.js', 'FS.mkdir("/home/web_user");HOME:"/home/web_user";')

    def test_same_paths_remain_forbidden_outside_bookkeeping(self):
        path = '/home/runner/work/little-woodland/little-woodland/dist/web/little_woodland.data'
        for extra in (f'fetch("{path}");', 'fetch(PACKAGE_NAME);',
                      f'var unrelated="{path}";', 'var source="/home/runner/work/game/main.cpp";'):
            with self.subTest(extra=extra):
                with self.assertRaisesRegex(ValueError, 'little_woodland.js:'):
                    check_paths('little_woodland.js', self.package_script(path) + extra, True)
        for resource in ('/little_woodland.data', '/home/runner/work/game/little_woodland.data',
                         'https://example.test/little_woodland.data'):
            with self.subTest(resource=resource):
                with self.assertRaises(ValueError):
                    check_paths('little_woodland.js', self.package_script(path).replace(
                        'REMOTE_PACKAGE_BASE="little_woodland.data"', f'REMOTE_PACKAGE_BASE="{resource}"'), True)
        for file in ('index.html', 'little_woodland.wasm'):
            with self.assertRaisesRegex(ValueError, file):
                check_paths(file, self.package_script(path))

    def test_diagnostics_are_bounded_and_name_file_and_failure(self):
        with self.assertRaises(ValueError) as caught:
            check_paths('little_woodland.js', 'x' * 1000 + '"/home/developer/' + 'y' * 10000)
        message = str(caught.exception)
        self.assertIn('little_woodland.js: unexpected local absolute path:', message)
        self.assertIn('/home/developer/', message)
        self.assertLess(len(message), 250)



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
            bookkeeping = GeneratedPaths().package_script('/home/runner/work/little-woodland/little-woodland/dist/web/little_woodland.data')
            script = bookkeeping + 'little_woodland.wasm; little_woodland.data; loadPackage(' + json.dumps(metadata, separators=(',', ':')) + ');'
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
