"""Release audio checks use synthetic WAVs; no downloaded sounds are needed for tests."""
import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from validate_audio_release import validate, NAMES


def wav(encoding=1, bits=16, extensible=False):
    """Build two zero-valued stereo frames with an intentional format descriptor.

    Payload amplitudes are irrelevant to structural release validation. Tests can
    vary the descriptor to cover supported and unsupported encodings without
    copying any licensed WAV bytes into the public test tree.
    """
    width = bits // 8
    samples = b'\0' * (4 * width)
    fmt = struct.pack('<HHIIHH', 65534 if extensible else encoding, 2, 44100,
                      44100 * width * 2, width * 2, bits)
    if extensible:
        fmt += struct.pack('<HHI', 22, bits, 3)
        fmt += struct.pack('<I', encoding) + bytes.fromhex('00001000800000aa00389b71')
    body = b'WAVEfmt ' + struct.pack('<I', len(fmt)) + fmt
    body += b'data' + struct.pack('<I', len(samples)) + samples
    return b'RIFF' + struct.pack('<I', len(body)) + body


class ReleaseAudio(unittest.TestCase):
    """Keep packaging checks technical without inventing per-asset licensing metadata.

    Every test owns a temporary synthetic audio tree and documentation fixture.
    The real author's SOURCES.md and optional downloaded sounds remain untouched.
    """
    def setUp(self):
        """Isolate each case in five synthetic WAVs and a licensing-document fixture.

        No hashes are recorded initially, so format tests fail for their intended
        structural reason rather than an unrelated identity mismatch.
        """
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.folder = self.root / 'audio/mixkit'
        self.folder.mkdir(parents=True)
        for name in NAMES:
            (self.folder / name).write_bytes(wav())
        self.sources = self.root / 'audio/SOURCES.md'
        self.sources.write_text('Author confirms Mixkit Sound Effects Free License.\n')

    def record_hashes(self):
        """Record fixture identities in the same Markdown-table form as SOURCES.md."""
        self.sources.write_text('Author confirmation remains here.\n' + ''.join(
            f'| `{name}` | `{hashlib.sha256((self.folder / name).read_bytes()).hexdigest()}` |\n'
            for name in NAMES))

    def test_no_titles_urls_or_extra_review_required(self):
        """Accept documented, hash-matching files without inventing missing metadata.

        Validation is read-only: even the fixture confirmation text must survive.
        """
        self.record_hashes()
        before = self.sources.read_bytes()
        result = validate(self.root)
        self.assertEqual(set(result['files']), set(NAMES))
        self.assertEqual(self.sources.read_bytes(), before)

    def test_supported_formats(self):
        """Cover the accepted PCM/float widths and extensible subformat descriptors."""
        for encoding, bits, extensible in [(1, 8, False), (1, 16, False), (1, 24, False),
                                           (1, 32, False), (3, 32, False), (3, 64, False),
                                           (1, 24, True), (3, 32, True)]:
            with self.subTest(encoding=encoding, bits=bits, extensible=extensible):
                (self.folder / 'letter_delivered.wav').write_bytes(wav(encoding, bits, extensible))
                validate(self.root)

    def test_missing_each_required_sound(self):
        """Remove and restore each name independently to catch incomplete releases."""
        for name in NAMES:
            with self.subTest(name=name):
                path = self.folder / name
                data = path.read_bytes()
                path.unlink()
                with self.assertRaisesRegex(ValueError, 'missing'):
                    validate(self.root)
                path.write_bytes(data)

    def test_hash_mismatch(self):
        """Reject changed sample bytes even when the WAV structure remains supported."""
        self.record_hashes()
        data = bytearray((self.folder / 'victory.wav').read_bytes())
        # Preserve a structurally valid container so the failure specifically
        # demonstrates identity checking, not an earlier malformed-header rejection.
        data[-1] = 1  # Keep the WAV valid; change only sample data.
        (self.folder / 'victory.wav').write_bytes(data)
        with self.assertRaisesRegex(ValueError, 'SHA-256'):
            validate(self.root)

    def test_invalid_or_unsupported_wav(self):
        """Distinguish container corruption, rejected encodings and incomplete frames.

        Bad alignment and empty data retain coherent RIFF sizes so those checks
        are exercised instead of being masked by the outer-length rejection.
        """
        valid = wav()
        bad_alignment = bytearray(valid)
        struct.pack_into('<H', bad_alignment, 32, 2)
        empty = valid[:40] + struct.pack('<I', 0)
        empty = empty[:4] + struct.pack('<I', len(empty)-8) + empty[8:]
        for data in (b'not a WAV', valid[:-1], wav(6, 8), wav(1, 64), wav(3, 16),
                     bytes(bad_alignment), empty):
            with self.subTest(data=data[:20]):
                (self.folder / 'victory.wav').write_bytes(data)
                with self.assertRaisesRegex(ValueError, 'invalid WAV'):
                    validate(self.root)

    def test_optional_legacy_record_has_no_title_url_requirement(self):
        """Accept a checksum-only legacy record, while rejecting contradictory
        categories and hashes that disagree with the actual fixture bytes.
        """
        path = self.root / 'checksums.json'
        digest = hashlib.sha256((self.folder / 'victory.wav').read_bytes()).hexdigest()
        record = {'files': {'victory.wav': {'sha256': digest}}}
        path.write_text(json.dumps(record))
        validate(self.root, path)
        record['files']['victory.wav']['license_category'] = 'Music Free License'
        path.write_text(json.dumps(record))
        with self.assertRaisesRegex(ValueError, 'contradicts'):
            validate(self.root, path)
        path.write_text(json.dumps({'files': {'victory.wav': {'sha256': '0'*64}}}))
        with self.assertRaisesRegex(ValueError, 'SHA-256'):
            validate(self.root, path)

    def test_licensing_document_required(self):
        """Keep documentation presence separate from optional per-asset metadata."""
        self.sources.unlink()
        with self.assertRaisesRegex(ValueError, 'licensing documentation'):
            validate(self.root)


if __name__ == '__main__':
    unittest.main()
