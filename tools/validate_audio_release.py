"""Check optional release audio presence, supported WAV formats and recorded integrity.

Licensing is documented by the project author in assets/audio/SOURCES.md. Original
asset titles, individual URLs and a separate provenance record are not required.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct

NAMES = ('tile_click.wav', 'success_tile_match.wav', 'tiles_falling.wav', 'letter_delivered.wav', 'victory.wav')
LICENSE = 'Sound Effects Free License'


def wav_format(data, name):
    """Inspect the supported WAV container/encoding subset without decoding samples.

    Args:
        data: Complete file bytes, including RIFF chunk padding.
        name: Filename used to identify failures in release diagnostics.

    Returns:
        A dictionary describing encoding, sample bits, channel count and sample rate.

    Raises:
        ValueError: Headers, chunk bounds, encoding or complete-frame checks fail.

    Release input is restricted to little-endian RIFF/WAVE PCM8/16/24/32 or IEEE
    float32/64, including their extensible GUID forms, with 1-32 channels. These
    checks establish structure and format support, not audible quality or licensing.
    """
    def fail(reason):
        raise ValueError(f'{name}: unsupported or invalid WAV ({reason})')

    if len(data) < 12 or data[:4] != b'RIFF' or data[8:12] != b'WAVE':
        fail('expected RIFF/WAVE header')
    end = struct.unpack_from('<I', data, 4)[0] + 8
    if end != len(data):
        fail('RIFF size does not match file length')
    offset, fmt, audio_bytes = 12, None, 0
    # RIFF chunks can include metadata before/between audio chunks. Walk declared
    # sizes with even-byte padding, rather than assuming a fixed 44-byte header.
    while offset < end:
        if offset + 8 > end:
            fail('truncated chunk header')
        tag, length = struct.unpack_from('<4sI', data, offset)
        offset += 8
        if offset + length + (length % 2) > end:
            fail('truncated chunk or missing padding')
        if tag == b'fmt ':
            if fmt is not None:
                fail('duplicate format chunk')
            fmt = data[offset:offset + length]
        elif tag == b'data':
            audio_bytes += length
        offset += length + (length % 2)
    if fmt is None or len(fmt) < 16:
        fail('missing or incomplete format chunk')
    encoding, channels, rate, byte_rate, alignment, bits = struct.unpack_from('<HHIIHH', fmt)
    # The extensible outer tag delegates the actual PCM/float encoding to its GUID.
    # Check that GUID family and valid-bit count before treating the payload as samples.
    if encoding == 0xFFFE:
        if len(fmt) < 40 or struct.unpack_from('<H', fmt, 16)[0] < 22:
            fail('incomplete extensible format')
        valid_bits = struct.unpack_from('<H', fmt, 18)[0]
        if not 0 < valid_bits <= bits:
            fail('invalid extensible valid-bit count')
        if fmt[28:40] != bytes.fromhex('00001000800000aa00389b71'):
            fail('unknown extensible subformat GUID')
        encoding = struct.unpack_from('<I', fmt, 24)[0]
    if encoding == 1:
        if bits not in (8, 16, 24, 32):
            fail('unsupported PCM sample width')
    elif encoding == 3:
        if bits not in (32, 64):
            fail('unsupported IEEE float sample width')
    else:
        fail('only PCM and IEEE float are accepted for release')
    if not 1 <= channels <= 32 or rate == 0:
        fail('invalid channel count or sample rate')
    # PCM/float bytes must describe complete interleaved frames; inconsistent
    # rates/alignment often indicate a truncated file or misidentified format.
    if alignment != channels * (bits // 8) or byte_rate != rate * alignment:
        fail('inconsistent block alignment or byte rate')
    if not audio_bytes or audio_bytes % alignment:
        fail('empty or incomplete audio frames')
    return {'encoding': 'PCM' if encoding == 1 else 'IEEE float',
            'bits': bits, 'channels': channels, 'sample_rate': rate}


def validate(assets, provenance=None):
    """Validate all five explicitly packaged effects without modifying their sources.

    Args:
        assets: Root containing audio/SOURCES.md and the local audio/mixkit WAVs.
        provenance: Optional legacy JSON path with extra per-file hashes/category.
            Original titles, asset URLs and reviewer fields are never required.

    Returns:
        A files mapping keyed by required WAV name, with each computed SHA-256 and
        format description. Packaging uses these hashes to check its archive bytes.

    Raises:
        ValueError: Required documentation/files, supported formats, recorded hashes
            or optional category consistency checks fail. Invalid JSON also fails.
        OSError: Documentation, optional JSON or WAV bytes cannot be read.

    SOURCES.md remains the author's licensing record. Its matching hash-table entries
    are enforced when present; hashes identify bytes, not their legal provenance.
    Validation reads files only and never writes approvals or substitutes audio.
    """
    assets = Path(assets)
    sources = assets / 'audio/SOURCES.md'
    if not sources.is_file():
        raise ValueError(f'Required licensing documentation missing: {sources}')
    # The author's existing Markdown table remains the canonical integrity record.
    hashes = dict(re.findall(r'\|\s*`([^`]+\.wav)`\s*\|\s*`([0-9a-fA-F]{64})`\s*\|', sources.read_text()))
    # Backward-compatible optional checksum/category record, never required metadata.
    extra = json.loads(Path(provenance).read_text()) if provenance else {}
    result = {'files': {}}
    for name in NAMES:
        path = assets / 'audio/mixkit' / name
        if not path.is_file():
            raise ValueError(f'Required release sound missing: {path}')
        # Read once for both structural checks and hashing. The returned digest
        # later catches replacement between validation and finished-archive inspection.
        data = path.read_bytes()
        format_info = wav_format(data, name)
        digest = hashlib.sha256(data).hexdigest()
        expected = hashes.get(name)
        if expected and digest != expected.lower():
            raise ValueError(f'{name}: SHA-256 does not match assets/audio/SOURCES.md')
        info = extra.get('files', {}).get(name, {})
        if info.get('license_category', LICENSE) != LICENSE:
            raise ValueError(f'{name}: optional record contradicts the documented Sound Effects license category')
        if info.get('sha256') and digest != info['sha256'].lower():
            raise ValueError(f'{name}: SHA-256 does not match optional integrity record')
        result['files'][name] = {'sha256': digest, 'format': format_info}
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('assets', type=Path)
    parser.add_argument('provenance', nargs='?', help='Optional legacy JSON checksum/category record')
    args = parser.parse_args()
    try:
        record = validate(args.assets, args.provenance)
    except (ValueError, OSError, struct.error) as error:
        parser.exit(1, str(error) + '\n')
    print(f'Validated {len(record["files"])} release WAVs: supported formats and recorded integrity')
