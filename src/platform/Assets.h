#pragma once
#include <filesystem>
namespace woodland {
/**
 * Resolves the runtime asset root without depending on the current working directory.
 *
 * On Linux, /proc/self/exe locates the installed ../share/little-woodland/assets
 * first. Development builds may then use a compiled-in checkout fallback; portable
 * builds omit that fallback. A berry image is the directory sentinel, not a full
 * asset inventory check; individual loaders report other missing required files.
 * @throws std::runtime_error If neither supported asset location can be found.
 */
std::filesystem::path assetRoot();
}
