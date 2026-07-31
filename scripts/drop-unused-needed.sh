#!/bin/sh
#
# Remove a DT_NEEDED entry from a shared library, after proving it is unused.
#
#     drop-unused-needed.sh <elf> <soname> <provider> [staged-provider]
#
#       elf              the library to patch, normally a staged copy
#       soname           the DT_NEEDED entry to drop
#       provider         the library that soname resolves to, read for symbols
#       staged-provider  a copy of it to delete once the entry is gone
#
# READELF and PATCHELF override the tools; both default to $PATH.
#
# The case this exists for is the Miyoo Mini SDL2 runtime. It declares
# libGLESv2.so as NEEDED and references not one symbol from it, so the loader
# maps 21.8 MB of SwiftShader before main() to satisfy a reference that does not
# exist. Evidence and reasoning in planning/2026-07-29-gles-free-runtime/.
#
# The comparison below is exactly what `ld --as-needed` performs at link time;
# the vendor simply did not pass the flag. It is a guard rather than a
# formality — a runtime that turns out to use the library must fail here, not
# produce a bundle that cannot start on a device.

set -eu

# comm(1) requires its two inputs sorted the way it compares them, and both
# sort(1) and comm(1) collate under the locale. They agree when one script runs
# both — but a disagreement does not produce an error, it produces an empty
# intersection, which here reads as "the dependency is unused". Byte order
# throughout removes the class. It also makes the counts below reproducible
# between a host and the toolchain container.
LC_ALL=C
export LC_ALL

if [ $# -lt 3 ] || [ $# -gt 4 ]; then
    echo "usage: $0 <elf> <soname> <provider> [staged-provider]" >&2
    exit 2
fi

elf=$1
soname=$2
provider=$3
staged_provider=${4:-}

READELF=${READELF:-readelf}
PATCHELF=${PATCHELF:-patchelf}

for f in "$elf" "$provider"; do
    [ -f "$f" ] || { echo "$0: $f does not exist" >&2; exit 1; }
done

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT INT TERM

# `patchelf --remove-needed` exits 0 on a soname that is not there, so an entry
# spelled differently than expected would no-op while the caller went on
# believing the library had been dropped. The two mmiyoo SDL2 builds in
# circulation differ in exactly this way — steward-fu's declares libGLESv2.so
# and Onion's parasyte copy declares libGLESv2.so.2.
if ! "$READELF" -d "$elf" \
    | grep -q "(NEEDED).*\[$soname\]"; then
    echo "$0: $elf has no NEEDED entry for $soname." >&2
    echo "  It declares:" >&2
    "$READELF" -d "$elf" | sed -n 's/.*(NEEDED).*\[\(.*\)\]/    \1/p' >&2
    echo "  Nothing was changed. Check which runtime is being staged." >&2
    exit 1
fi

# Symbols $elf references, and symbols $provider supplies.
#
# Every defined symbol on the provider side, not just the functions: an OBJECT
# or a TLS symbol resolved through this dependency counts every bit as much as a
# call does. Versioned names are foo@GLIBC_2.4, and the reference is to foo.
#
# The $1 test anchors on the index column, which keeps the table header out of
# the results — otherwise both sides pick up a symbol called "Name".
"$READELF" --dyn-syms -W "$elf" \
    | awk '$1 ~ /^[0-9]+:$/ && $7 == "UND" && $8 != "" { print $8 }' \
    | sed 's/@.*//' | sort -u >"$work/referenced"

"$READELF" --dyn-syms -W "$provider" \
    | awk '$1 ~ /^[0-9]+:$/ && $7 != "UND" && $8 != "" { print $8 }' \
    | sed 's/@.*//' | sort -u >"$work/supplied"

# An empty side would make the comparison below pass for the wrong reason.
for f in referenced supplied; do
    [ -s "$work/$f" ] || {
        echo "$0: no dynamic symbols parsed for the $f side; refusing to" >&2
        echo "    conclude anything from an empty symbol table" >&2
        exit 1
    }
done

comm -12 "$work/referenced" "$work/supplied" >"$work/common"

if [ -s "$work/common" ]; then
    echo "$0: refusing to drop $soname from" >&2
    echo "    $elf" >&2
    echo "  because it references $(wc -l <"$work/common") symbol(s) that" >&2
    echo "    $provider" >&2
    echo "  supplies:" >&2
    sed 's/^/    /' "$work/common" >&2
    echo >&2
    echo "  This dependency is real for this runtime, so the bundle must carry" >&2
    echo "  the library. Turn the step off with -DWREEL_ONION_DROP_GLES=OFF and" >&2
    echo "  see planning/2026-07-29-gles-free-runtime/." >&2
    exit 1
fi

"$PATCHELF" --remove-needed "$soname" "$elf"

# A bundle staged before this step existed still holds the library.
if [ -n "$staged_provider" ] && [ -f "$staged_provider" ]; then
    rm -f "$staged_provider"
fi

echo "dropped NEEDED $soname from $(basename "$elf"): \
$(wc -l <"$work/referenced") referenced symbols, \
$(wc -l <"$work/supplied") supplied, none in common"
