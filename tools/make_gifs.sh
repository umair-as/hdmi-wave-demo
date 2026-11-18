#!/bin/sh
# Render the README GIFs as docs/gifs/sceneN.gif, short loops of the scenes
# listed in SCENES below.
#
#   tools/make_gifs.sh [BINARY] [OUTDIR]
#
# BINARY defaults to build/hdmi-wave-demo, OUTDIR to docs/gifs. Each scene is
# recorded headless (--record, 960x540 at 20 fps) from an interesting moment,
# resampled to FPS, scaled to WIDTH px and encoded with a COLORS-entry
# ffmpeg palette (defaults 15 fps, 640 px, 64 colours, no dithering: it
# costs several times the file size on the gradient scenes). Needs ffmpeg and a
# display for the hidden GL context (e.g. WAYLAND_DISPLAY with
# SDL_VIDEODRIVER=wayland for an SDL build).
set -eu

BIN=${1:-build/hdmi-wave-demo}
OUT=${2:-docs/gifs}
WIDTH=${WIDTH:-640}
FPS=${FPS:-15}
COLORS=${COLORS:-64}
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

# scene  start(s)  length(s): what the loop shows. Only the README's GIFs
# are listed; any scene can be added the same way.
SCENES="
3  4   6   the mark being redrawn by its circles
9  24  6   the trajectories splitting
"

mkdir -p "$OUT"
echo "$SCENES" | while read -r n start len _; do
    [ -n "$n" ] || continue
    rm -f "$TMP"/*.png
    "$BIN" --record "$TMP" --scene "$n" --start "$start" --duration "$len" --mute </dev/null >/dev/null
    ffmpeg -nostdin -loglevel error -y -framerate 20 -i "$TMP/frame_%04d.png" \
        -vf "fps=$FPS,scale=$WIDTH:-1:flags=lanczos,split[a][b];[a]palettegen=max_colors=$COLORS:stats_mode=full[p];[b][p]paletteuse=dither=none:diff_mode=rectangle" \
        "$OUT/scene$n.gif"
    printf 'scene %s: %s (%s KiB)\n' "$n" "$OUT/scene$n.gif" "$(( $(wc -c < "$OUT/scene$n.gif") / 1024 ))"
done
