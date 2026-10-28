#!/bin/bash
# Dump trainer front + back sprites from any HGSS / hg-engine ROM to PNG.
# Safe version: does NOT touch base/ or data/graphics/.
#
# Run from the hg-engine folder (needs tools/ndstool, tools/nitrogfx, tools/narcpy.py, .venv):
#   bash dump_trainer_sprites_safe.sh path/to/rom.nds [output_folder]
#
# Output (default: dumped_trainer_gfx/):
#   front/NNN.png       trainer front sprite (same layout as data/graphics/trainer_gfx)
#   front/NNN_enc.png   encounter-intro sprite
#   back/NN.png         player back sprite (same layout as data/graphics/trainer_back_gfx)
#   back/NN_enc.png
#   plus _cell.json / _anim.json for each, so they can be dropped back into hg-engine as-is.
set -e

ROM="$1"
OUT="${2:-dumped_trainer_gfx}"

if [ -z "$ROM" ] || [ ! -f "$ROM" ]; then
    echo "Usage: bash $0 path/to/rom.nds [output_folder]" >&2
    exit 1
fi
for t in tools/ndstool tools/nitrogfx tools/narcpy.py .venv/bin/activate; do
    if [ ! -e "$t" ]; then
        echo "Missing $t - run this from the hg-engine folder after building once." >&2
        exit 1
    fi
done

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "Extracting ROM to a temp folder..."
./tools/ndstool -x "$ROM" -d "$TMP/root" >/dev/null

. .venv/bin/activate
python3 tools/narcpy.py extract "$TMP/root/a/0/5/8" -o "$TMP/a058"
python3 tools/narcpy.py extract "$TMP/root/a/0/0/6" -o "$TMP/a006"

mkdir -p "$OUT/front" "$OUT/back"

# Each trainer = 5 files: NCGR, NCLR, NCER, NANR, NCGR(encounter)
dump_set () {
    local dir="$1" prefix="$2" outdir="$3" width="$4"
    local count total i base name pad
    total=$(ls "$dir" | wc -l)
    count=$((total / 5))
    pad=${#total}; [ $(( total - 1 )) -lt $(( 10 ** (pad - 1) )) ] && pad=$((pad - 1))  # narcpy pads to digits of (total-1)
    for ((i = 0; i < count; i++)); do
        base=$((i * 5))
        name=$(printf "%0${width}d" "$i")
        f() { printf "%s/%s_%0${pad}d" "$dir" "$prefix" "$1"; }
        cp "$(f $base)"       "$TMP/x.NCGR"
        cp "$(f $((base+1)))" "$TMP/x.NCLR"
        cp "$(f $((base+2)))" "$TMP/x.NCER"
        cp "$(f $((base+3)))" "$TMP/x.NANR"
        cp "$(f $((base+4)))" "$TMP/xe.NCGR"
        ./tools/nitrogfx "$TMP/x.NCGR"  "$outdir/$name.png"     -width 10 -palette "$TMP/x.NCLR"
        ./tools/nitrogfx "$TMP/xe.NCGR" "$outdir/${name}_enc.png" -width 20 -encodebacktofront -palette "$TMP/x.NCLR"
        ./tools/nitrogfx "$TMP/x.NCER"  "$outdir/${name}_cell.json"
        ./tools/nitrogfx "$TMP/x.NANR"  "$outdir/${name}_anim.json"
    done
    echo "  $count sprites -> $outdir"
}

echo "Converting trainer front sprites..."
dump_set "$TMP/a058" 8 "$OUT/front" 3
echo "Converting trainer back sprites..."
dump_set "$TMP/a006" 6 "$OUT/back" 2

echo "Done. Sprites are in $OUT/"
