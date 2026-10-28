# Browser demo

Little Woodland has a single-threaded C++17/WebAssembly build using Emscripten's
GLFW 3 port and WebGL 2. The native Linux application uses the same game rules,
presentation, artwork and frame routine. No GitHub settings or deployments are
changed by a local build. The expected project-site URL after publication is
`https://talia-protasova.github.io/little-woodland/`; it is not a verified live demo.

## Toolchain and local builds

Use the pinned **Emscripten 3.1.64** SDK (official emsdk commit
`0b3bcbc3b005cbb811d48e497eabcc6846d43001`), Python 3.9+, CMake 3.16+ and Git.
Install the SDK outside the game checkout:

```sh
git clone https://github.com/emscripten-core/emsdk.git ../emsdk
git -C ../emsdk checkout 0b3bcbc3b005cbb811d48e497eabcc6846d43001
../emsdk/emsdk install 3.1.64
../emsdk/emsdk activate 3.1.64
source ../emsdk/emsdk_env.sh
emcc --version
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build-web --parallel 2
python3 tools/validate_web_dist.py dist/web
```

The build stages an explicit runtime asset inventory in `build-web/web-assets`,
then preloads it into `/assets` in Emscripten's virtual filesystem. This absolute
VFS path is internal to WASM, not an HTTP URL. All network resources use relative
URLs. `dist/web` contains `index.html`, JS, WASM, data, integrity inventory, notices
and `.nojekyll`. Only upload this directory. Build directories and distributions
are Git-ignored. The browser build never downloads artwork or WAVs.

Serve the generated distribution over HTTP, including a project-site subpath:

```sh
python3 -m http.server 8000 --directory dist
```

Open `http://localhost:8000/web/`. To reproduce the intended repository subpath
on Linux exactly, without changing generated files:

```sh
mkdir -p build-web/preview
ln -sfn ../../dist/web build-web/preview/little-woodland
python3 -m http.server 8000 --directory build-web/preview
```

Open `http://localhost:8000/little-woodland/`. Do not open `index.html` via `file://`.
No special cross-origin isolation headers, external CDN or SharedArrayBuffer is
required. Click the canvas to focus; R restarts, M toggles session mute and Escape
stops the application and releases its resources. The page then offers Open again.
Mouse and GLFW's touch-to-mouse path use the fixed 480×800 framebuffer scaled by
CSS, preserving portrait proportions on narrow and short viewports.

Native configuration remains separate (run without `emcmake`):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
# On a headless Linux machine, include graphical texture tests:
xvfb-run -a ctest --test-dir build --output-on-failure
python3 tools/package_linux.py
```

See README/RELEASE.md for native dependencies and `--with-audio` packaging.
The native Python CTest also checks browser asset staging, rejects extra audio/
source files and absolute resource URLs, and detects corrupted preload bytes.

## GitHub Pages setup (author action after review)

1. Review `feature/web-demo` and merge the implementation into **master**.
2. In repository **Settings → Pages → Build and deployment → Source**, select
   **GitHub Actions**. Do not choose a generated-files branch.
3. Ensure repository **Settings → Actions → General** permits the official actions
   and deployment token permissions. In **Settings → Environments → github-pages**,
   restrict deployment branches to **master**; add reviewers if desired.
4. A push to master runs `.github/workflows/deploy-web.yml`, or select **Actions →
   Build and deploy browser demo → Run workflow → master**. Manual dispatches on
   other refs are explicitly skipped. No feature-branch push deploys anything.
5. Check the build validation, Pages upload and deployment jobs, then open the URL
   reported by the deployment. Verify the actual public site before adding a Play
   Online link or changing the repository Website field.

The workflow uses Ubuntu 22.04 and the exact SDK above. Official actions are pinned
to immutable commits: checkout v6 (`d23441a…`), configure-pages v5 (`983d773…`),
upload-pages-artifact v4 (`7b1f4a7…`), deploy-pages v4 (`d6db901…`). Update these
pins deliberately after testing. SDK installation occurs in CI, not on a developer
machine. Permissions are scoped by job: contents read, Pages write for configuration/
deployment, and id-token write for deployment. The `github-pages` environment and
concurrency group prevent overlapping publication. No generated files are committed.

Official references: [Emscripten runtime/main loop](https://emscripten.org/docs/porting/emscripten-runtime-environment.html),
[WebGL support](https://emscripten.org/docs/porting/multimedia_and_graphics/OpenGL-support.html),
and [GitHub Pages custom workflows](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages).

## Audio and limits

The first browser build is deliberately sound-free. `AudioSilent.cpp` retains the
cue/mute facade and reports **No audio**; R/M and the control remain safe when audio
is unavailable. Native `Audio.cpp` still uses miniaudio with PulseAudio/ALSA and
supports existing explicit `--with-audio` release packaging. Browser configuration
rejects `WOODLAND_RELEASE_WITH_AUDIO=ON` rather than claiming to enable sound.
Mixkit WAVs stay ignored under `/assets/audio/mixkit/`; CI neither needs nor embeds
them. `assets/audio/SOURCES.md` remains the author's licensing documentation.

A future sound-enabled port can use miniaudio's Emscripten Web Audio backend or a
small Web Audio implementation behind the same facade. It must resume/unlock the
AudioContext from a user gesture, decode clips once, retain bounded voice mixing
and stop/mute semantics, and remain playable when audio is blocked. Do not select
Linux backends in that port. For future automated builds, a private release asset
store fetched with a narrowly scoped Actions secret into an ephemeral workspace
can provision the five WAVs before license/format/hash validation. Do not expose
secret URLs/tokens in logs, commit sounds, or publish a standalone sound bundle.
This private provisioning and browser sound backend are not implemented or required.

Both 151-frame authored hedgehog sequences are preloaded, retaining their 30/40ms
frame timing, infinite looping, alpha and static PNG fallback. Their GPU texture
storage with mipmaps is roughly 101 MiB; the compressed preload is about 20 MiB,
plus fonts, VFS memory and browser overhead. WASM reserves a 4 MiB stack for the existing renderer scratch array, starts with
128 MiB and can grow
to 512 MiB. Low-memory devices may struggle. WebGL 2/hardware acceleration is
required; initialization/context-loss errors are shown on the page. Background-tab
throttling can advance animation clocks on resume, as elapsed-time playback intends.
The canvas has a keyboard label but the illustrated board is not a screen-reader
accessible alternative interface. Touch is a GLFW fallback; desktop browsers are
the primary target. No public deployment has been performed by this change.

## Browser verification

The local Release artifact was tested through `/little-woodland/` with Google
Chrome 156.0.8078.47 and Firefox 142.0.1 using software WebGL in this development
environment. Both completed selection, mouse swaps, gravity/cascades, letter
completion, the 20-move defeat path, happy/worried frame changes and looping,
result-button restart, R/M, touch on a scaled narrow canvas, resizing, Escape
cleanup and the no-WebGL2 error page, without console/network errors. Screenshots
were reviewed for both result pages. A Chrome run with deliberately invalidated
animation manifests also passed, retaining stable PNG characters on both results. Each run stayed at the initial 128 MiB WASM
heap; this excludes GPU/browser-process memory. Real mobile GPUs were not tested.
GitHub-hosted CI and the public Pages deployment still need author execution.

The optional smoke script uses Playwright and Node 18+ outside the distribution:

```sh
npm install --prefix /tmp/woodland-web-test playwright@1.56.1
PLAYWRIGHT_BROWSERS_PATH=/tmp/woodland-web-test/browsers \
  node /tmp/woodland-web-test/node_modules/playwright/cli.js install chromium firefox
# Start the HTTP preview above in another terminal, then:
WOODLAND_PLAYWRIGHT=/tmp/woodland-web-test/node_modules/playwright \
PLAYWRIGHT_BROWSERS_PATH=/tmp/woodland-web-test/browsers \
  node tools/smoke_web.cjs
WOODLAND_PLAYWRIGHT=/tmp/woodland-web-test/node_modules/playwright \
PLAYWRIGHT_BROWSERS_PATH=/tmp/woodland-web-test/browsers \
WOODLAND_BROWSER=firefox node tools/smoke_web.cjs
```

Set `WOODLAND_CHROME` to an installed Chrome executable if desired. Override
`WOODLAND_SMOKE_URL` for another local port and `WOODLAND_SMOKE_OUTPUT` for
screenshots (default `/tmp/woodland-web-smoke`). `WOODLAND_TEST_PNG_FALLBACK=1`
intercepts only the local data response to invalidate both animation manifests;
the same route then verifies stable static PNG characters instead of moving
frames. It does not change the built artifact. These optional development tools
are not browser runtime or CI build dependencies.

## Implementation files

New browser-specific files: `cmake/WebBuild.cmake`, `web/index.html`,
`web/notices/*.txt`, `src/rendering/GL.h`, `src/audio/AudioSilent.cpp`,
`tools/prepare_web_assets.py`, `tools/validate_web_dist.py`, `tools/smoke_web.cjs`,
`tests/WebTests.py`, `.github/workflows/deploy-web.yml` and this guide.

Adapted shared files: `CMakeLists.txt`, `src/main.cpp`,
`src/platform/Assets.{h,cpp}`, `src/rendering/BoardRenderer.cpp`,
`src/rendering/ShaderProgram.h`, `src/rendering/Texture.h`, `src/audio/Audio.h`,
`.gitignore` and the short README reference. Puzzle rules, playback timing,
illustrations, font metrics and audio licensing confirmation are unchanged.

Native CMake build and all ten CTest suites passed with Xvfb, including PNG/GL
resources and the new web-distribution gate. Both sound-free and explicit
`--with-audio` Linux TGZ packaging validated; a native graphical smoke test also
checked mouse/M mute, held-key handling, mute persistence, animated victory,
restart and Escape using an ALSA null sink (no audible listening test).
