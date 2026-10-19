#!/bin/sh
# Download page https://blueash.pages.dev (Cloudflare Pages project "blueash"): relay/public/index.html and privacy.html
# with the relay's update files linked absolutely (the relay keeps its old modepulse address: installed apps use it).
# Run from the repo: sh source/deploy-site.sh
set -e
R=https://modepulse-relay.modepulse.workers.dev
S="${TMPDIR:-${TEMP:-/tmp}}/blueash-site"
rm -rf "$S" && mkdir -p "$S/brand" && cp relay/public/brand/icon-192.png relay/public/brand/icon-512.png "$S/brand/"
sed -e "s#fetch('/update/check.txt'#fetch('$R/update/check.txt'#" -e "s#a.href='/update/BlueAsh-'#a.href='$R/update/BlueAsh-'#" \
    -e "s#href=\"/update/check.txt\"#href=\"$R/update/check.txt\"#" -e 's#href="/privacy.html"#href="/privacy"#g' relay/public/index.html > "$S/index.html"
sed -e 's#href="/privacy.html"#href="/privacy"#g' relay/public/privacy.html > "$S/privacy.html"
cd "$S" && npx --yes wrangler@4 pages deploy . --project-name blueash --branch main
