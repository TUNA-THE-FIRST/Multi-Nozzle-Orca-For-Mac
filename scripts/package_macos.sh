#!/usr/bin/env bash
# Sign Snapmaker Orca Mac.app and pack it into a drag-to-Applications DMG.
#
#   ./scripts/package_macos.sh "<build>/Snapmaker Orca Mac.app" <output.dmg> [identity]
#
# Without an Apple Developer ID certificate the app is signed ad hoc (identity "-"). Apple Silicon
# runs only signed code, and Gatekeeper reports a downloaded app whose bundle is not sealed as one
# signature as "damaged and can't be opened". So every Mach-O file inside the bundle is signed
# first (the bundled Python runtime included) and the bundle is sealed last; a final strict
# verification fails the build if anything was left out.
#
# The bundle must be complete before this runs: anything changed inside it afterwards (the preset
# caches, say) breaks the seal.
set -euo pipefail

if [ $# -lt 2 ]; then
    echo "Usage: $0 <app bundle> <output.dmg> [signing identity, default '-' (ad hoc)]" >&2
    exit 2
fi

app="${1%/}"
dmg="$2"
identity="${3:--}"
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
entitlements="$script_dir/disable_validation.entitlements"

if [ ! -d "$app/Contents/MacOS" ]; then
    echo "error: $app is not an app bundle (no Contents/MacOS)." >&2
    echo "       Build it first with ./build_release_macos.sh -s, which leaves it in build/arm64/Snapmaker_Orca/." >&2
    exit 1
fi

# codesign refuses files carrying Finder info or resource forks ("detritus not allowed").
xattr -cr "$app"

# Signing a Developer ID build needs a secure timestamp; an ad hoc signature must not ask for one.
sign_args=(--force --sign "$identity")
[ "$identity" != "-" ] && sign_args+=(--timestamp --options runtime)

machos=()
while IFS= read -r f; do machos+=("$f"); done < <(
    find "$app" -type f -print0 |
        xargs -0 file --no-pad --mime-type -- |
        grep -v ' (for architecture ' |
        grep ': application/x-mach-binary$' |
        sed 's|: application/x-mach-binary$||'
)
if [ "${#machos[@]}" -eq 0 ]; then
    echo "error: found no executables in $app, so nothing could be signed." >&2
    exit 1
fi
echo "Signing ${#machos[@]} executables and libraries in $(basename "$app") (identity: $identity)..."
codesign "${sign_args[@]}" "${machos[@]}"
codesign "${sign_args[@]}" --entitlements "$entitlements" "$app"
codesign --verify --deep --strict --verbose=2 "$app"

echo "Packing $(basename "$dmg")..."
staging="$(mktemp -d)"
trap 'rm -rf "$staging"' EXIT
cp -R "$app" "$staging/"
ln -s /Applications "$staging/Applications"
volname="$(basename "$app" .app)"
rm -f "$dmg"
# hdiutil create fails now and then with "Resource busy"; retry it.
# shellcheck disable=SC1091 # retry.sh sits next to this script
source "$script_dir/retry.sh"
retry hdiutil create -volname "$volname" -srcfolder "$staging" -ov -format UDZO "$dmg"
echo "Created $dmg"
