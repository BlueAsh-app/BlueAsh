// Cloudflare Pages Function for https://blueash.pages.dev/download/<file>: streams the setup program from the relay, so
// the download comes from the BlueAsh address (and does not depend on a GitHub Release existing for that version).
export async function onRequest({ params }) {
  const name = String(params.name || "");
  if (!/^BlueAsh-Setup-\d+\.\d+\.\d+\.exe$/.test(name)) return new Response("Not found", { status: 404 });
  const r = await fetch("https://modepulse-relay.modepulse.workers.dev/download/" + name);
  if (!r.ok) return new Response("Not found", { status: 404 });
  const h = new Headers({ "content-type": "application/octet-stream", "content-disposition": `attachment; filename="${name}"`, "cache-control": "public, max-age=3600" });
  if (r.headers.get("content-length")) h.set("content-length", r.headers.get("content-length"));
  return new Response(r.body, { status: 200, headers: h });
}
