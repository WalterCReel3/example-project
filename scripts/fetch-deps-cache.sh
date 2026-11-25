#!/usr/bin/env bash
# Populate (or refresh) the offline dependency cache.
#
# Run once, WITH network. Afterwards any number of worktrees configure without
# it, by setting WREEL_DEP_CACHE to the directory this script fills — see
# docs/DEVELOPMENT.md § Worktrees and cmake/Dependencies.cmake.
#
# What it writes: one shallow bare git repository per upstream, at the path the
# upstream URL has under its host, holding exactly the pinned tag:
#
#   <cache>/github.com/libsdl-org/SDL.git        release-2.32.10
#   <cache>/github.com/libsdl-org/freetype.git   the commit SDL2_ttf's gitlink names
#   ...
#   <cache>/PINS                                 the pins it was populated for
#
# The pins and repositories are read from cmake/Dependencies.cmake, which stays
# the only place they are written down. Submodules named by GIT_SUBMODULES are
# resolved from the tag's own .gitmodules and gitlink.
#
# Re-running is the refresh: a repository that already has its tag is left
# alone, a missing one is fetched, and PINS is rewritten last, so an
# interrupted run leaves a cache that configure refuses rather than half-trusts.
# Tags from a superseded pin stay in the mirror, harmlessly; delete the cache
# directory to reclaim the space.
#
# The finished cache is made read-only (chmod a-w). Nothing in configure writes
# to it, and a mirror that a build could edit would not be a shared one.

set -euo pipefail

usage()
{
    cat <<EOF
usage: ${0##*/} [--cache DIR] [--deps-file FILE]

  --cache DIR       cache directory. Default: \$WREEL_DEP_CACHE, else
                    \${XDG_CACHE_HOME:-\$HOME/.cache}/wreel-deps
  --deps-file FILE  the file holding the pins. Default: cmake/Dependencies.cmake
                    beside this script
EOF
}

here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
deps_file=$here/../cmake/Dependencies.cmake
cache=${WREEL_DEP_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/wreel-deps}

while (($#)); do
    case $1 in
        --cache)     cache=${2:?--cache needs a directory}; shift 2 ;;
        --deps-file) deps_file=${2:?--deps-file needs a file}; shift 2 ;;
        -h|--help)   usage; exit 0 ;;
        *)           usage >&2; exit 2 ;;
    esac
done

[[ -r $deps_file ]] || { echo "error: cannot read $deps_file" >&2; exit 1; }

# Pin lines, exactly as configure will compare them: NAME=value.
pins=$(sed -n 's/^set(\(WREEL_PIN_[A-Z0-9_]*\)[[:space:]]*"\([^"]*\)").*/\1=\2/p' \
    "$deps_file")

# One "url<TAB>tag<TAB>submodule-paths" line per distinct FetchContent_Declare.
# Submodule paths are space separated; "-" when there are none.
declared=$(awk '
    /^set\(WREEL_PIN_/ {
        name = $1; sub(/^set\(/, "", name)
        val = $0; sub(/^[^"]*"/, "", val); sub(/".*$/, "", val)
        pin[name] = val
    }
    function flush() {
        if (url != "") print url "\t" tag "\t" (subs == "" ? "-" : subs)
        url = ""; tag = ""; subs = ""
    }
    /^[[:space:]]*FetchContent_Declare\(/ { flush() }
    /^[[:space:]]*GIT_REPOSITORY[[:space:]]/ { url = $2 }
    /^[[:space:]]*GIT_TAG[[:space:]]/ {
        tag = $2; gsub(/[${}]/, "", tag); tag = (tag in pin) ? pin[tag] : tag
    }
    /^[[:space:]]*GIT_SUBMODULES[[:space:]]/ {
        subs = $0; sub(/^[^"]*"/, "", subs); sub(/".*$/, "", subs)
    }
    END { flush() }
' "$deps_file" | sort -u)

[[ -n $declared && -n $pins ]] || {
    echo "error: found no pins or no FetchContent_Declare in $deps_file" >&2
    exit 1
}

# An earlier run left the tree read-only.
[[ -d $cache ]] && chmod -R u+w "$cache"
mkdir -p "$cache"
cache=$(cd "$cache" && pwd)

# mirror URL REF — make <cache>/<host>/<path> hold REF (a tag or a commit) and
# have HEAD point at it, so a clone of the mirror has something to check out.
mirror()
{
    local url=$1 ref=$2
    local path=${url#https://}
    local repo=$cache/$path

    if [[ ! -d $repo ]]; then
        mkdir -p "$(dirname "$repo")"
        git init --bare --quiet "$repo"
        # Submodule commits are named by id, not by tag.
        git -C "$repo" config uploadpack.allowAnySHA1InWant true
    fi

    if git -C "$repo" cat-file -e "$ref^{commit}" 2>/dev/null; then
        echo "have   $path @ $ref"
    else
        echo "fetch  $path @ $ref"
        if [[ $ref =~ ^[0-9a-f]{40}$ ]]; then
            git -C "$repo" fetch --quiet --depth 1 "$url" "$ref"
            git -C "$repo" update-ref "refs/heads/pinned-$ref" "$ref"
        else
            git -C "$repo" fetch --quiet --depth 1 "$url" \
                "refs/tags/$ref:refs/tags/$ref"
        fi
    fi
    git -C "$repo" update-ref --no-deref HEAD "$(git -C "$repo" rev-parse "$ref^{commit}")"
}

# mirror_submodules URL REF [PATH...] — mirror the submodules REF records, and
# theirs in turn: FetchContent updates them with --recursive, and FreeType has
# one of its own. .gitmodules and the gitlinks are read from the mirror, not
# the network, so the URL and commit are the ones that ref actually names.
# With PATHs, only those submodules; without, all of them.
mirror_submodules()
{
    local url=$1 ref=$2; shift 2
    local repo=$cache/${url#https://}
    local key path suburl subsha

    git -C "$repo" cat-file -e "$ref:.gitmodules" 2>/dev/null || return 0
    while read -r key path; do
        if (($#)) && [[ " $* " != *" $path "* ]]; then continue; fi
        suburl=$(git -C "$repo" config --blob "$ref:.gitmodules" "${key%.path}.url")
        subsha=$(git -C "$repo" ls-tree "$ref" -- "$path" | awk '{print $3}')
        [[ $suburl == https://* && -n $subsha ]] || {
            echo "error: cannot resolve submodule '$path' of $url $ref" >&2; exit 1; }
        mirror "$suburl" "$subsha"
        mirror_submodules "$suburl" "$subsha"
    done < <(git -C "$repo" config --blob "$ref:.gitmodules" \
        --get-regexp '^submodule\..*\.path$')
}

while IFS=$'\t' read -r url tag subs; do
    mirror "$url" "$tag"
    # shellcheck disable=SC2086 # subs is a space-separated list of paths
    [[ $subs == - ]] || mirror_submodules "$url" "$tag" $subs
done <<<"$declared"

printf '%s\n' "$pins" >"$cache/PINS"
chmod -R a-w "$cache"
echo "cache ready: $cache  (read-only; WREEL_DEP_CACHE=$cache)"
