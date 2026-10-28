# Audio Assets — Sources and Licensing

Little Woodland uses five sound effects downloaded from Mixkit.

All five files were downloaded by the project author from the **Mixkit Sound Effects** section and are used under the **Mixkit Sound Effects Free License**.

The files were renamed to reflect their gameplay purposes.

Original asset titles and individual download URLs were not retained. The applicable license does not require preserving original filenames, documenting individual asset pages, or providing attribution.

The license category has been confirmed by the project author. This confirmation is not an independent verification of each file's download history.

## License

**Provider:** Mixkit  
**License:** Mixkit Sound Effects Free License  
**License URL:** https://mixkit.co/license/modal/sfxFree/

The license permits incorporating sound effects into commercial and non-commercial projects, including video games.

The following restrictions apply:

- Sound effects must not be redistributed as standalone stock assets, in stock libraries, tools, or templates.
- Sound effects must not be distributed together with source files.
- The original sound effects must not be claimed as original creations of this project.
- Sound effects must not be registered with rights-management services.

Attribution is not required under the Sound Effects Free License.

The Mixkit User Terms also apply:

https://mixkit.co/terms/

## Audio Files

All audio files are stored locally in `assets/audio/mixkit/`.

| Filename | Gameplay event |
|---|---|
| `tile_click.wav` | Tile selection |
| `success_tile_match.wav` | Successful match, once per match wave |
| `tiles_falling.wav` | Gravity animation |
| `letter_delivered.wav` | Successful letter delivery |
| `victory.wav` | Victory screen |

All five files are confirmed by the project author as Mixkit Sound Effects, not Mixkit Stock Music.

## Technical Information

The audio files were inspected during development.

- Four files use stereo PCM16 at 44,100 Hz.
- `letter_delivered.wav` uses stereo PCM24 at 44,100 Hz in WAVE_FORMAT_EXTENSIBLE.
- The audio implementation uses miniaudio to decode the files to interleaved float32 stereo at 44,100 Hz for mixing.

### Recorded SHA-256 Hashes

These hashes were recorded during the initial audio integration.

| Filename | SHA-256 |
|---|---|
| `tile_click.wav` | `fe535fd085fe27f4397e2e6ab2e5798f66636f1d7244b559018aed85fa99348b` |
| `success_tile_match.wav` | `6ce399cf01e7f1b36531b231fcd30de54c8c3d9a498129d6cab742d74db5be33` |
| `tiles_falling.wav` | `6494913a149204c0ae7a1b9dbac3427af8ffb7ae0d1bc9d4a65854c53ba7b2bb` |
| `letter_delivered.wav` | `ec083b56d66a7f6f2c58beaddd54167b143f0bd99782f6929976dcfc198d651e` |
| `victory.wav` | `0cccec87e7e0f03b0be4b35df7fdc851f648a5ae9345af3c68748ec1a269f56e` |

Hashes are recorded for file identification and integrity checks. They do not independently establish licensing or provenance.

## Repository and Distribution

Little Woodland distinguishes between its public source repository and its packaged game distribution.

### Public Source Repository

The original downloaded Mixkit WAV files are excluded from Git through:

```gitignore
/assets/audio/mixkit/
```

The public repository contains the audio implementation, build configuration, and licensing documentation, but not the downloaded sound effects.

The project must remain buildable and playable without optional audio files.

### Packaged Game Release

Licensed sound effects may be incorporated into the completed Little Woodland game under the Mixkit Sound Effects Free License.

The packaged release:

- Includes audio as part of the finished playable game.
- Does not distribute audio as a standalone asset library.
- Does not include the application's C++ source files alongside the licensed WAV assets.
- Includes the relevant licensing information.
- Must work independently of the development directory.

The packaging process must check that all required audio files exist.

A release containing sound effects must not be silently generated with missing audio files.

The applicable license and Mixkit User Terms should be reviewed again if the distribution model changes.

## Release Validation

The project author has confirmed that all five WAV files originate from Mixkit Sound Effects.

Individual original asset titles and download-page URLs are not mandatory release metadata under the applicable license.

The packaging workflow may validate:

- Explicit selection of audio-enabled packaging.
- Presence of all five required files.
- Valid supported audio formats.
- File integrity when hashes are used.
- Correct inclusion of audio in the finished game package.
- Exclusion of third-party WAV files from Git.

Release validation must not require original asset titles or individual download-page URLs solely for compliance with the Mixkit Sound Effects Free License.

This documentation records the project's licensing decisions and does not replace the applicable Mixkit terms.

## Attribution

Sound effects provided by Mixkit.

https://mixkit.co/

Attribution is included voluntarily and is not required by the Mixkit Sound Effects Free License.