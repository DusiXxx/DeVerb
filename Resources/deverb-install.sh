#!/bin/bash
# DusiX DeVerb – instalace aktualizace (spousti ji plugin, bezi samostatne na pozadi)
#   $1 = zip z GitHubu, $2 = verze, $3 = soubor se stavem (JSON pro plugin)
# Pocka, az se Logic zavre, zazalohuje stary plugin, nainstaluje novy a obnovi seznam pluginu.

ZIP="$1"; VERSION="$2"; STATUS="$3"
DIR="$(dirname "$ZIP")"
STAGE="$DIR/staged"
BACKUP="$DIR/Zaloha"
COMP_DIR="$HOME/Library/Audio/Plug-Ins/Components"
VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
NAME="DusiX DeVerb"

status() { printf '{"state":"%s","version":"%s","message":"%s","time":"%s"}\n' "$1" "$VERSION" "$2" "$(date '+%Y-%m-%d %H:%M:%S')" > "$STATUS"; }
notify() { /usr/bin/osascript -e "display notification \"$1\" with title \"DusiX DeVerb\"" >/dev/null 2>&1 || true; }
logic_running() { /usr/bin/pgrep -x "Logic Pro X" >/dev/null || /usr/bin/pgrep -x "Logic Pro" >/dev/null || /usr/bin/pgrep -x "MainStage" >/dev/null || /usr/bin/pgrep -x "GarageBand" >/dev/null; }

echo "$(date) instalace $VERSION z $ZIP"
status waiting "Čeká na zavření Logicu"

# 1) pockat na zavreni Logicu (max. 7 dni)
for _ in $(seq 1 120960); do
    logic_running || break
    sleep 5
done
if logic_running; then status failed "Logic se nezavřel"; exit 1; fi
status installing ""

# 2) rozbalit a zkontrolovat
rm -rf "$STAGE"; mkdir -p "$STAGE"
if ! /usr/bin/ditto -x -k "$ZIP" "$STAGE"; then status failed "Zip nejde rozbalit"; notify "Aktualizace selhala (zip)"; exit 1; fi
NEW_COMP="$(/usr/bin/find "$STAGE" -maxdepth 2 -name "$NAME.component" -type d | head -1)"
NEW_VST3="$(/usr/bin/find "$STAGE" -maxdepth 2 -name "$NAME.vst3" -type d | head -1)"
if [ -z "$NEW_COMP" ] || [ ! -f "$NEW_COMP/Contents/Info.plist" ]; then status failed "V zipu chybí plugin"; notify "Aktualizace selhala"; exit 1; fi
GOT=$(/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" "$NEW_COMP/Contents/Info.plist" 2>/dev/null)
if [ "$GOT" != "$VERSION" ]; then status failed "V zipu je verze $GOT místo $VERSION"; notify "Aktualizace selhala"; exit 1; fi

# 3) zaloha a vymena (pri chybe se vrati puvodni)
mkdir -p "$BACKUP" "$COMP_DIR" "$VST3_DIR"
rm -rf "$BACKUP/$NAME.component" "$BACKUP/$NAME.vst3"
[ -d "$COMP_DIR/$NAME.component" ] && /usr/bin/ditto "$COMP_DIR/$NAME.component" "$BACKUP/$NAME.component"
[ -d "$VST3_DIR/$NAME.vst3" ] && /usr/bin/ditto "$VST3_DIR/$NAME.vst3" "$BACKUP/$NAME.vst3"

restore() {
    echo "chyba: $1 - vracim puvodni verzi"
    if [ -d "$BACKUP/$NAME.component" ]; then rm -rf "$COMP_DIR/$NAME.component"; /usr/bin/ditto "$BACKUP/$NAME.component" "$COMP_DIR/$NAME.component"; fi
    if [ -d "$BACKUP/$NAME.vst3" ]; then rm -rf "$VST3_DIR/$NAME.vst3"; /usr/bin/ditto "$BACKUP/$NAME.vst3" "$VST3_DIR/$NAME.vst3"; fi
    status failed "$1"
    notify "Aktualizace selhala – zůstala původní verze"
    exit 1
}

rm -rf "$COMP_DIR/$NAME.component" || restore "Starý plugin nejde smazat"
/usr/bin/ditto "$NEW_COMP" "$COMP_DIR/$NAME.component" || restore "Kopírování selhalo"
if [ -n "$NEW_VST3" ]; then
    rm -rf "$VST3_DIR/$NAME.vst3"
    /usr/bin/ditto "$NEW_VST3" "$VST3_DIR/$NAME.vst3" || restore "Kopírování VST3 selhalo"
fi
/usr/bin/xattr -dr com.apple.quarantine "$COMP_DIR/$NAME.component" 2>/dev/null
[ -n "$NEW_VST3" ] && /usr/bin/xattr -dr com.apple.quarantine "$VST3_DIR/$NAME.vst3" 2>/dev/null

# 4) obnovit seznam Audio Units a overit
/usr/bin/killall -9 AudioComponentRegistrar >/dev/null 2>&1
sleep 2
if /usr/bin/auval -v aufx Dxdv Dusx >/dev/null 2>&1; then
    echo "auval OK"
else
    echo "auval hlasi problem (plugin necham, Logic si ho overi sam)"
fi

rm -rf "$STAGE"
rm -f "$ZIP"
status done ""
notify "Nainstalována verze $VERSION – otevři Logic"
echo "$(date) hotovo"
