#!/bin/sh
# Download page https://blueash.pages.dev (Cloudflare Pages project "blueash"): relay/public/index.html, privacy.html,
# code-signing.html and changelog.html, with the relay's update files linked absolutely (the relay keeps its old modepulse address: installed
# apps use it), plus site/functions (Pages Functions: /download/<setup>.exe streams the relay's copy under this address).
# A Pages deploy replaces the whole site, so every page must be listed here.
# Run from the repo: sh source/deploy-site.sh
set -e
R=https://modepulse-relay.modepulse.workers.dev
S="${TMPDIR:-${TEMP:-/tmp}}/blueash-site"
rm -rf "$S" && mkdir -p "$S/brand" && cp relay/public/brand/icon-192.png relay/public/brand/icon-512.png "$S/brand/"
sed -e "s#fetch('/update/check.txt'#fetch('$R/update/check.txt'#" -e "s#'/update/BlueAsh-'#'$R/update/BlueAsh-'#" \
    -e "s#href=\"/update/check.txt\"#href=\"$R/update/check.txt\"#" -e 's#href="/privacy.html"#href="/privacy"#g' -e 's#href="/code-signing.html"#href="/code-signing"#g' -e 's#href="/changelog.html"#href="/changelog"#g' relay/public/index.html > "$S/index.html"
for p in privacy code-signing changelog; do sed -e "s#fetch(./update/history.json.#fetch('$R/update/history.json'#" -e 's#href="/privacy.html"#href="/privacy"#g' -e 's#href="/code-signing.html"#href="/code-signing"#g' -e 's#href="/changelog.html"#href="/changelog"#g' relay/public/$p.html > "$S/$p.html"; done
cp -r site/functions "$S/functions"
cd "$S" && npx --yes wrangler@4 pages deploy . --project-name blueash --branch main
