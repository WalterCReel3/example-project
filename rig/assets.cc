#include <rig/assets.hpp>

#include <SDL.h>

#include <sys/stat.h>

#include <cstdlib>

#include <util/logging.hpp>

namespace rig
{

namespace
{

// Cached because SDL_GetBasePath() allocates, and because the log line below
// should appear once per run rather than once per asset.
std::string resolved_root;
bool resolved = false;

bool is_directory(const std::string& path)
{
    // Plain ::stat rather than posix::wrap(): a missing directory is the
    // expected outcome of probing for one, not an exceptional condition, and
    // the typed-exception machinery would turn ordinary control flow into a
    // throw. That is the distinction include/posix/errors.hpp draws.
    struct stat info;
    if (::stat(path.c_str(), &info) != 0) {
        return false;
    }
    return S_ISDIR(info.st_mode);
}

// Guarantees exactly one trailing separator. SDL's paths carry one already; an
// environment variable may or may not.
std::string with_separator(std::string path)
{
    if (path.empty() || path.back() != '/') {
        path += '/';
    }
    return path;
}

std::string resolve()
{
    const char* env = std::getenv("WREEL_DATA_DIR");
    if (env && *env) {
        const std::string path = with_separator(env);
        util::log_info("assets: %s (WREEL_DATA_DIR)", path.c_str());
        return path;
    }

    // May be null: SDL cannot answer this on every platform. It allocates, so
    // it needs freeing either way.
    char* base = SDL_GetBasePath();
    if (base) {
        const std::string candidate = with_separator(base) + "data/";
        SDL_free(base);
        if (is_directory(candidate)) {
            util::log_info("assets: %s (beside the executable)",
                           candidate.c_str());
            return candidate;
        }
    }

    // A warning rather than info, deliberately. Reaching this on a device means
    // the bundle is laid out wrong, and whatever assets do load are coming from
    // wherever the firmware's launcher happened to be.
    util::log_warning(
        "assets: falling back to ./data/ relative to the working directory; "
        "no data/ beside the executable and WREEL_DATA_DIR is unset");
    return "data/";
}

} // namespace

const std::string& asset_root()
{
    if (!resolved) {
        resolved_root = resolve();
        resolved = true;
    }
    return resolved_root;
}

std::string asset_path(const std::string& name)
{
    return asset_root() + name;
}

void reset_asset_root()
{
    resolved = false;
    resolved_root.clear();
}

std::string pref_path(const std::string& app_name)
{
    // SDL creates the directory as a side effect, which is why this is not
    // simply a string join.
    char* path = SDL_GetPrefPath("wreel", app_name.c_str());
    if (!path) {
        util::log_warning("no writable preferences directory: %s",
                          SDL_GetError());
        return std::string();
    }
    const std::string result(path);
    SDL_free(path);
    return result;
}

} // namespace rig
