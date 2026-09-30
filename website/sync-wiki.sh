#!/usr/bin/env bash
# Copies docs/ to the GitHub wiki, kept as a backup of the documentation website.
# Usage: website/sync-wiki.sh
set -euo pipefail

cd "$(dirname "$0")/.."
WIKI_DIR="$(mktemp -d)"
trap 'rm -rf "$WIKI_DIR"' EXIT

git clone -q git@github.com:balthazar/react-native-zeroconf.wiki.git "$WIKI_DIR"
find "$WIKI_DIR" -maxdepth 1 -name '*.md' -delete
cp docs/*.md "$WIKI_DIR"/

cd "$WIKI_DIR"
git add -A
if git diff --cached --quiet; then
  echo "Wiki already up to date"
  exit 0
fi
git commit -q -m "Sync from docs/"
git push -q
echo "Wiki updated"
