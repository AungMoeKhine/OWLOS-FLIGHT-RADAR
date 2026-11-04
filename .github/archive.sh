#!/usr/bin/env bash
# every version's firmware kept: a GitHub release a version (tag v0.4), in it a zip a board
# (fw-<board>.zip: its three binaries and flash_args) and version.txt (the version as its manifests
# say it). the site puts the older ones next to the newest, so a board can go back to one
#   archive.sh seed <list>               the versions from before the archive, from the runs that built them
#   archive.sh this <fw> <ver> <text>    this run's build: the release of <ver>, made or made again
#   archive.sh site <fw> <ver> <text>    every older release into <fw>/<version>/, and <fw>/versions.json
# <fw> holds a folder a board, as the firmware job leaves them. needs gh (GH_TOKEN, GH_REPO), zip, unzip
set -euo pipefail

# the versions in the archive, the newest first
releases() {
  gh release list --limit 200 --json tagName -q '.[].tagName' | sed -n 's/^v\([0-9][0-9.]*\)$/\1/p' | sort -V -r
}

# a release from a folder of board folders: <dir>/<board>/flash_args and the binaries next to it
publish() {   # <ver> <text> <dir> <commit> <--latest or --latest=false>
  local ver="$1" text="$2" dir="$3" sha="$4" latest="$5" out fa
  out=$(mktemp -d)
  for fa in "$dir"/*/flash_args; do
    (cd "$(dirname "$fa")" && zip -q -j "$out/fw-$(basename "$(dirname "$fa")").zip" ./*)
  done
  ls "$out"/fw-*.zip >/dev/null || { echo "no boards in $dir" >&2; exit 1; }
  printf '%s\n' "$text" > "$out/version.txt"
  # the release's text: that version's points in CHANGES.md
  awk -v v="$ver" '/^## /{ p = ($2 == v); next } p && (NF || s) { s = 1; print }' CHANGES.md > "$out/notes.md"
  gh release create "v$ver" "$out"/fw-*.zip "$out/version.txt" --target "$sha" --title "$ver" \
    --notes-file "$out/notes.md" "$latest"
  echo "v$ver: $(cd "$out" && ls fw-*.zip | tr '\n' ' ')"
}

seed() {   # <list>: lines "version run-id version text", # for a comment
  local ver run text tmp dir fa
  while read -r ver run text <&3; do
    case "$ver" in ""|\#*) continue ;; esac
    if gh release view "v$ver" >/dev/null 2>&1; then
      echo "v$ver is in the archive already"
      continue
    fi
    tmp=$(mktemp -d)
    dir=$(mktemp -d)
    gh run download "$run" -p 'fw-*' -D "$tmp"
    # each board's files to a folder of its own, wherever the download put them
    while read -r fa; do
      cp -r "$(dirname "$fa")" "$dir/"
    done < <(find "$tmp" -name flash_args)
    publish "$ver" "$text" "$dir" "$(gh run view "$run" --json headSha -q .headSha)" --latest=false
  done 3< "$1"
}

this() {   # <fw> <ver> <text>
  local fw="$1" ver="$2" text="$3" newest
  # a lower number than the archive's newest would make an older version again over the one kept
  newest=$(releases | sed -n 1p)                      # sed reads it all: no broken pipe under pipefail
  if [ -n "$newest" ] && [ "$(printf '%s\n%s\n' "$newest" "$ver" | sort -V | tail -n 1)" != "$ver" ]; then
    echo "CHANGES.md says $ver, the archive has $newest: a new version needs a higher number" >&2
    exit 1
  fi
  # the same number again: this build is that version now, its release and tag go with it
  if gh release view "v$ver" >/dev/null 2>&1; then
    echo "v$ver is in the archive from an earlier build of $ver: made again from this one"
    gh release delete "v$ver" --cleanup-tag --yes
  fi
  publish "$ver" "$text" "$fw" "$GITHUB_SHA" --latest
}

# one entry of versions.json: the version, its text, its folder from the page, the boards in it.
# all of them are ours (CHANGES.md, the git log, make-manifests.sh's names): nothing to escape
entry() {   # <ver> <text> <dir as the page has it> <dir here>
  local boards="" m
  for m in "$4"/*/manifest.json; do
    if [ -f "$m" ]; then
      boards="$boards${boards:+, }\"$(basename "$(dirname "$m")")\""
    fi
  done
  printf '    { "version": "%s", "text": "%s", "dir": "%s", "boards": [ %s ] }' "$1" "$2" "$3" "$boards"
}

site() {   # <fw> <ver> <text>
  local fw="$1" ver="$2" text="$3" v t tmp z b list
  list=$(entry "$ver" "$text" "fw" "$fw")             # the newest is <fw> itself, built by this run
  for v in $(releases); do
    [ "$v" = "$ver" ] && continue
    tmp=$(mktemp -d)
    gh release download "v$v" -D "$tmp" -p 'fw-*.zip' -p version.txt
    for z in "$tmp"/fw-*.zip; do
      b=$(basename "$z" .zip)
      b=${b#fw-}
      mkdir -p "$fw/$v/$b"
      unzip -q -o "$z" -d "$fw/$v/$b"
    done
    t=$(head -n 1 "$tmp/version.txt")
    bash .github/make-manifests.sh "$fw/$v" "$t"
    list="$list,"$'\n'"$(entry "$v" "$t" "fw/$v" "$fw/$v")"
  done
  printf '{\n  "versions": [\n%s\n  ]\n}\n' "$list" > "$fw/versions.json"
  cat "$fw/versions.json"
}

case "${1:-}" in
  seed) seed "$2" ;;
  this) this "$2" "$3" "$4" ;;
  site) site "$2" "$3" "$4" ;;
  *) echo "archive.sh seed <list> | this <fw> <ver> <text> | site <fw> <ver> <text>" >&2; exit 1 ;;
esac
