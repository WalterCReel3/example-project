#pragma once

#include <string>

//============================================================================
//
// Where assets are read from
//
//     TTF_OpenFontIndex(rig::asset_path("Speedy.fon").c_str(), 10, 0);
//
// Assets were opened by path relative to the *working directory* — literally
// "data/Speedy.fon". That works when a developer runs the binary from the
// repository root and breaks the moment anything else launches it, which is the
// normal case on a handheld: a firmware's launcher runs from its own directory,
// or from /, and the demo then fails to find a font it is sitting next to.
//
// Note that `name` is relative to the asset root and does NOT include "data/".
// The directory name is part of the layout this resolves, not part of the asset
// name.
//
// Resolution order, first hit wins:
//
//   1. $WREEL_DATA_DIR, if set and non-empty. cmake/Testing.cmake already
//      exports this for every test, and it is the override for running a build
//      tree's binary against a source tree's assets.
//   2. <directory containing the executable>/data/, if it exists. This is the
//      shipped layout — cmake/Packaging.cmake installs data/ to bin/data/
//      precisely so this resolves on a device.
//   3. "data/", relative to the working directory. The 2016 behaviour, kept as
//      the last resort so running from the repository root keeps working.
//
// None of this needs configuring to work. Rule 1 is an override and nothing
// sets it outside ctest; a shipped bundle resolves by rule 2 with no
// environment at all, which is what cmake/Packaging.cmake's install of data/ to
// bin/data/ is for.
//
// Which rule won is logged. A device reading the wrong assets, or falling back
// to (3) because its bundle is laid out wrong, is otherwise hard to tell apart
// from one whose assets are simply missing.
//
// So a build tree resolves by rule 3 and a device by rule 2. Making them match
// — by symlinking data/ beside the built binaries — was considered and declined
// on 2026-07-27: it would put a build artefact pointing into the source tree,
// and running from the repository root is the established habit here. The cost
// is that rule 2 is exercised by installed bundles rather than by daily use,
// which is why it has an explicit check in docs/DEVELOPMENT.md § Status rather
// than being assumed to work.
//
//============================================================================
namespace rig
{

// Directory assets are read from, with a trailing '/'. Resolved on first call
// and cached; the result cannot change during a run.
const std::string& asset_root();

// asset_root() + name. No validation: a missing asset is the caller's error to
// report, and the caller can say what it was looking for.
std::string asset_path(const std::string& name);

// Forces re-resolution on the next call. Exists for tests, which exercise the
// resolution order without spawning a process per case; nothing on a shipped
// path should call it.
void reset_asset_root();

// Per-user writable directory for logs, saves and settings, with a trailing
// '/', or an empty string if the platform cannot offer one.
//
// Distinct from asset_root() and not interchangeable with it: a handheld's
// assets typically sit on a read-only mount or an SD card the firmware owns,
// so writing beside them either fails or litters the card. skratch's log
// already goes here.
std::string pref_path(const std::string& app_name);

} // namespace rig
