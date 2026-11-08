#!/bin/bash
# Sestavi DusiX DeVerb a nainstaluje ho do Logicu.
# Pouziti: v Terminalu napis "bash " (s mezerou), pretahni tento soubor do okna a dej Enter.
set -e
cd "$(dirname "$0")"

# --- najdi CMake
CMAKE="$(command -v cmake || true)"
[ -z "$CMAKE" ] && [ -x /Applications/CMake.app/Contents/bin/cmake ] && CMAKE=/Applications/CMake.app/Contents/bin/cmake
if [ -z "$CMAKE" ]; then
    echo "CHYBA: Nenasel jsem CMake. Stahni ho z https://cmake.org/download (macOS .dmg) a pretahni do Applications."
    exit 1
fi

# --- najdi prekladac
if xcodebuild -version >/dev/null 2>&1; then
    GEN=(-G Xcode)
    echo "Pouzivam Xcode."
elif xcode-select -p >/dev/null 2>&1; then
    GEN=(-DCMAKE_BUILD_TYPE=Release)
    echo "Pouzivam Command Line Tools."
else
    echo "CHYBA: Chybi Xcode. Nainstaluj ho z App Store, jednou ho spust a pak zadej:"
    echo "  sudo xcode-select -s /Applications/Xcode.app"
    exit 1
fi

# smaz starou verzi pod puvodnim nazvem DusiVerb, at v Logicu nezustanou dva pluginy
rm -rf "$HOME/Library/Audio/Plug-Ins/Components/DusiVerb.component" "$HOME/Library/Audio/Plug-Ins/VST3/DusiVerb.vst3"

echo "Konfiguruju (pri prvnim spusteni se stahuje JUCE, chvili to trva)..."
"$CMAKE" -B build "${GEN[@]}"

echo "Kompiluju..."
"$CMAKE" --build build --config Release --parallel

# at si Logic plugin nacte znovu
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo
echo "Kontrola pluginu (auval)..."
if auval -v aufx Dxdv Dusx 2>&1 | grep -q "AU VALIDATION SUCCEEDED"; then
    echo "HOTOVO: plugin prosel validaci. Restartuj Logic a hledej: Audio FX > Audio Units > DusiX > DusiX DeVerb"
else
    echo "Plugin je sestaveny, ale validace neprosla. Posli Claudovi vystup prikazu: auval -v aufx Dxdv Dusx"
fi
