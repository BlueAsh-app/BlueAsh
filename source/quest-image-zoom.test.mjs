import { test } from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import vm from "node:vm";

test("zoom shows a thumbnail immediately and ignores a previous image finishing late", () => {
  const source=readFileSync(new URL("../companion/CompanionUI.cs",import.meta.url),"utf8");
  const start=source.indexOf("let shown=0;function show(x)");
  const end=source.indexOf("\nbig.onclick",start);
  assert.ok(start>0&&end>start);
  const posts=[], big={dataset:{},style:{}}, bi={src:""}, bc={};
  class Image { constructor(){this.complete=false;this.naturalWidth=0;} }
  const ctx=vm.createContext({big,bi,bc,Image,post:m=>posts.push(m)});
  vm.runInContext(source.slice(start,end),ctx);
  const a={thumb:"a-small",large:"a-large",caption:"A"},b={thumb:"b-small",large:"b-large",caption:"B"};
  ctx.show(a);assert.equal(bi.src,"a-small");assert.equal(big.style.display,"block");
  ctx.show(b);a.largeImage.onload();assert.equal(bi.src,"b-small");
  b.largeImage.onload();assert.equal(bi.src,"b-large");ctx.hide();assert.equal(big.style.display,"none");
  b.largeImage.complete=true;b.largeImage.naturalWidth=1600;ctx.show(b);assert.equal(bi.src,"b-large");
  assert.deepEqual(posts,["open","open","shut","open"]);
});
