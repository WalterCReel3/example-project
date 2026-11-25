#!/usr/bin/env bash
# Make a git worktree that builds and tests with no network, and prove it.
#
#   scripts/new-worktree.sh [options] PATH BRANCH
#
# 1. git worktree add PATH — creating BRANCH from --base if it does not exist.
# 2. Carry across the gitignored files a working tree needs: local.env,
#    .claude/settings.local.json, tools/.venv and, with --sunny,
#    data/Sunny-land-files/. They are copied or linked FROM the main checkout
#    and never written TO it; the only change outside PATH is git's own
#    worktree metadata.
# 3. Configure, build and ctest desktop-software against the offline cache
#    (scripts/fetch-deps-cache.sh), so the tree is shown to work before anyone
#    relies on it. --no-verify stops after step 2.
#
# See docs/DEVELOPMENT.md § Worktrees.

set -euo pipefail

usage()
{
    cat <<EOF
usage: ${0##*/} [options] PATH BRANCH

  --base REF      start a new BRANCH from REF (default: HEAD)
  --from DIR      checkout to take the gitignored files from (default: the
                  main worktree)
  --cache DIR     offline dependency cache. Default: \$WREEL_DEP_CACHE, else
                  \${XDG_CACHE_HOME:-\$HOME/.cache}/wreel-deps
  --venv MODE     tools/.venv: 'link' (default) symlinks the --from one, which
                  works because the venv's interpreter and site-packages are
                  found through the path it is invoked by; 'create' builds a
                  fresh one and pip-installs tools/requirements.txt, which
                  needs a PyPI mirror or network; 'skip'
  --sunny         also copy data/Sunny-land-files/ (5.9 MB; only
                  tools/pack_atlas.py's inputs, nothing in the build reads it)
  --no-verify     do not configure, build and test
EOF
}

base=HEAD from='' venv=link sunny=0 verify=1
cache=${WREEL_DEP_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/wreel-deps}
args=()
while (($#)); do
    case $1 in
        --base)      base=${2:?--base needs a ref}; shift 2 ;;
        --from)      from=${2:?--from needs a directory}; shift 2 ;;
        --cache)     cache=${2:?--cache needs a directory}; shift 2 ;;
        --venv)      venv=${2:?--venv needs a mode}; shift 2 ;;
        --sunny)     sunny=1; shift ;;
        --no-verify) verify=0; shift ;;
        -h|--help)   usage; exit 0 ;;
        -*)          echo "error: unknown option $1" >&2; usage >&2; exit 2 ;;
        *)           args+=("$1"); shift ;;
    esac
done
((${#args[@]} == 2)) || { usage >&2; exit 2; }
[[ $venv == link || $venv == create || $venv == skip ]] || {
    echo "error: --venv takes link, create or skip" >&2; exit 2; }

mkdir -p "$(dirname "${args[0]}")"
path=$(cd "$(dirname "${args[0]}")" && pwd)/$(basename "${args[0]}")
branch=${args[1]}

# The first entry of the porcelain list is the main worktree.
[[ -n $from ]] || from=$(git worktree list --porcelain | awk 'NR == 1 { print $2 }')
from=$(cd "$from" && pwd)

if ((verify)); then
    # Before anything is created: a missing cache is a one-line fix, a
    # half-made worktree is not.
    [[ -f $cache/PINS ]] || {
        echo "error: no dependency cache at $cache" >&2
        echo "  populate it once, with network: scripts/fetch-deps-cache.sh --cache $cache" >&2
        echo "  or pass --no-verify" >&2
        exit 1
    }
fi

# ---------------------------------------------------------------------------
# 1. The worktree
# ---------------------------------------------------------------------------

if git show-ref --verify --quiet "refs/heads/$branch"; then
    git worktree add "$path" "$branch"
else
    git worktree add -b "$branch" "$path" "$base"
fi

# ---------------------------------------------------------------------------
# 2. Gitignored files
# ---------------------------------------------------------------------------

# carry SRC-relative-path: copy a file the new tree lacks, if the source has it.
carry()
{
    local rel=$1
    if [[ -e $from/$rel ]]; then
        mkdir -p "$path/$(dirname "$rel")"
        cp -an "$from/$rel" "$path/$rel"
        echo "carried $rel"
    else
        echo "absent  $rel (not in $from)"
    fi
}

carry local.env
carry .claude/settings.local.json
((sunny)) && carry data/Sunny-land-files

case $venv in
    link)
        if [[ -d $from/tools/.venv ]]; then
            ln -s "$from/tools/.venv" "$path/tools/.venv"
            echo "linked  tools/.venv -> $from/tools/.venv"
        else
            echo "absent  tools/.venv (not in $from); use --venv create"
        fi ;;
    create)
        python3 -m venv "$path/tools/.venv"
        "$path/tools/.venv/bin/pip" install --quiet -r "$path/tools/requirements.txt" ;;
esac

((verify)) || { echo "worktree ready: $path"; exit 0; }

# ---------------------------------------------------------------------------
# 3. Offline configure, build, test
# ---------------------------------------------------------------------------

cd "$path"
export WREEL_DEP_CACHE=$cache

# ccache aborts the compile, rather than falling back, when its directory is not
# writable — which is the case in a sandbox that can write only the worktree.
# Share hits across worktrees when it works (hash_dir off, or -g builds embed the
# cwd and never hit; base_dir so the worktree's own path is not hashed), and
# build uncached when it does not.
if command -v ccache >/dev/null; then
    ccache_dir=$(ccache -k cache_dir 2>/dev/null || true)
    if [[ -n $ccache_dir ]] && mkdir -p "$ccache_dir" 2>/dev/null &&
       [[ -w $ccache_dir ]]; then
        export CCACHE_BASEDIR=$path CCACHE_NOHASHDIR=1
    else
        export CCACHE_DISABLE=1
        echo "ccache directory '${ccache_dir:-?}' is not writable; building uncached"
    fi
fi

cmake --preset desktop-software
cmake --build --preset desktop-software
ctest --preset desktop-software
echo "worktree ready and verified: $path"
