// Cloudflare Pages Function for https://blueash.pages.dev/download/<file>: streams the setup program from the relay, so
// the download comes from the BlueAsh address (and does not depend on a GitHub Release existing for that version).
// ?lang=<tag> (the language picked on the site) goes into the file name; the setup reads it as BlueAsh's first language.
const TAGS = ["ko","en","zhcn","ja","ru","es","ptbr","de","fr","it","pl","tr","zhtw"];
export async function onRequest({ params, request }) {
  const name = String(params.name || "");
  if (!/^BlueAsh-Setup-\d+\.\d+\.\d+\.exe$/.test(name)) return new Response("Not found", { status: 404 });
  const r = await fetch("https://modepulse-relay.modepulse.workers.dev/download/" + name);
  if (!r.ok) return new Response("Not found", { status: 404 });
  const tag = new URL(request.url).searchParams.get("lang") || "";
  const file = TAGS.includes(tag) ? name.replace(/\.exe$/, "-" + tag + ".exe") : name;
  const h = new Headers({ "content-type": "application/octet-stream", "content-disposition": `attachment; filename="${file}"`, "cache-control": "public, max-age=3600" });
  if (r.headers.get("content-length")) h.set("content-length", r.headers.get("content-length"));
  return new Response(r.body, { status: 200, headers: h });
}
