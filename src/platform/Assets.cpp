#include "Assets.h"
#include <stdexcept>
namespace woodland {
/**
 * Chooses a directory only; this layer does not decode images, open audio devices
 * or own textures/animation frames. Callers append their resource subpaths and
 * apply required-resource errors or optional-resource fallbacks at load time.
 */
std::filesystem::path assetRoot() {
#ifdef __EMSCRIPTEN__
    // Preloaded VFS path, independent of the HTTP project-site base URL.
    if (std::filesystem::is_regular_file("/assets/tiles/berry.png")) return "/assets";
#else
    std::error_code error;
    // Resolve the real executable rather than argv[0] or the shell's working
    // directory. This also supports launching the extracted package via a symlink.
    const auto executable=std::filesystem::read_symlink("/proc/self/exe",error);
    if(!error) {
        const auto installed=executable.parent_path()/"../share/little-woodland/assets";
        // Presence of the known tile identifies an installed asset tree without
        // scanning every optional file. Corrupted image bytes are diagnosed by
        // Texture later; finding this root does not certify the whole inventory.
        if(std::filesystem::is_regular_file(installed/"tiles/berry.png"))return installed.lexically_normal();
    }
#ifdef WOODLAND_ASSET_DIR
    // This fallback exists only in development builds; portable release binaries
    // must work without the original checkout and therefore omit the definition.
    const auto source=std::filesystem::path(WOODLAND_ASSET_DIR);
    if(std::filesystem::is_regular_file(source/"tiles/berry.png"))return source;
#endif
#endif
    // No current-directory fallback is attempted: changing the launch directory
    // must not select another project's art. Renderer uses tiles/fonts/characters
    // below the returned root; Audio independently uses audio/mixkit below it.
#ifdef __EMSCRIPTEN__
    throw std::runtime_error("Cannot locate preloaded game assets in /assets");
#else
    throw std::runtime_error("Cannot locate game assets beside the executable (../share/little-woodland/assets)");
#endif
}
}
