"""Build and inspect a local, binary-only Linux TGZ without publishing anything.

The default excludes optional Mixkit WAVs. --with-audio explicitly enables all five
validated effects inside the finished game; SOURCES.md documents author-confirmed
licensing. --provenance remains an optional legacy checksum/category record, not a
requirement to retain original titles or individual asset URLs.

The pipeline configures a portable CMake build, compiles, runs CTest and invokes
CPack installation. It then checks source-file exclusions, the bundled GLFW file,
sound count/names/runtime placement and packaged hashes. Failed validation/build/
tests/package checks abort the command. OpenGL/device tests still need a display
or Xvfb, as described in the project README. No download/upload/release API is used.
"""
import argparse
import hashlib
from pathlib import Path
import subprocess
import tarfile
from validate_audio_release import validate

ROOT = Path(__file__).resolve().parents[1]
if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--with-audio', action='store_true')
    parser.add_argument('--provenance', type=Path, help='Optional legacy checksum/category JSON; titles and asset URLs are not required')
    parser.add_argument('--build-dir', type=Path, default=ROOT/'build/release')
    parser.add_argument('--glad-source', type=Path, help='Reuse an already downloaded GLAD source checkout, optionally offline')
    args = parser.parse_args()
    # Validate before configuring so a requested complete audio release cannot
    # silently degrade to missing or unrecognized effects. Silent builds skip this.
    if args.with_audio:
        try: audio_record = validate(ROOT/'assets', args.provenance)
        except (ValueError, OSError) as error: parser.exit(1,str(error)+'\n')
    build = args.build_dir.resolve()
    command = ['cmake','-S',str(ROOT),'-B',str(build),'-DCMAKE_BUILD_TYPE=Release',
               '-DWOODLAND_PORTABLE=ON','-DBUILD_TESTING=ON',
               '-DWOODLAND_RELEASE_WITH_AUDIO='+('ON' if args.with_audio else 'OFF')]
    # Clear the cached optional record when none is requested. Reusing a build
    # directory must not revive an obsolete path from an earlier packaging run.
    command.append('-DWOODLAND_AUDIO_PROVENANCE='+str(args.provenance.resolve() if args.with_audio and args.provenance else ''))
    if args.glad_source: command.append('-DFETCHCONTENT_SOURCE_DIR_GLAD='+str(args.glad_source.resolve()))
    # check=True stops at the first failed stage; CPack also revalidates audio
    # through the install rules before copying only the named runtime WAV files.
    for cmd in [command, ['cmake','--build',str(build),'-j2'],
                ['ctest','--test-dir',str(build),'--output-on-failure'],
                ['cpack','--config',str(build/'CPackConfig.cmake'),'-B',str(build/'packages')]]:
        subprocess.run(cmd,check=True)
    # Inspect the just-built flavor rather than accidentally choosing the other
    # archive when both explicit audio and sound-free builds share this directory.
    flavor='audio' if args.with_audio else 'silent'
    archives=list((build/'packages').glob(f'little-woodland-*-{flavor}.tar.gz'))
    archive=max(archives,key=lambda path:path.stat().st_mtime)
    with tarfile.open(archive) as package:
        members=package.getmembers()
        if any(item.name.endswith(('.cpp','.h','.py')) for item in members):
            raise RuntimeError('Source files unexpectedly entered the binary package')
        # Installing a renamed symlink can create a self-referencing library link.
        # Require the actual runtime file so extraction does not rely on system GLFW.
        glfw=[item for item in members if item.name.endswith('/lib/libglfw.so.3')]
        if len(glfw)!=1 or not glfw[0].isfile():
            raise RuntimeError('Bundled GLFW must be a real library file, not a dangling symlink')
        waves=[item for item in members if item.name.endswith('.wav')]
        if len(waves)!=(5 if args.with_audio else 0):
            raise RuntimeError('Package sound count does not match its explicit release mode')
        if args.with_audio:
            if {Path(item.name).name for item in waves} != set(audio_record['files']):
                raise RuntimeError('Package does not contain the five required sound names')
            for item in waves:
                if not item.isfile() or '/share/little-woodland/assets/audio/mixkit/' not in item.name:
                    raise RuntimeError('Sound must be a regular runtime file in the finished game')
                # Verify archive bytes against the pre-build inputs, not just names
                # and counts. This guards against stale or replaced staged resources.
                digest=hashlib.sha256(package.extractfile(item).read()).hexdigest()
                if digest != audio_record['files'][Path(item.name).name]['sha256']:
                    raise RuntimeError('Packaged sound differs from validated input: '+item.name)
    print('Verified local binary archive:',archive)
