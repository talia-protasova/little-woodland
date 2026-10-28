/* Optional end-to-end check. Install Playwright outside the distribution and run
 * with NODE_PATH pointing at its node_modules; see WEB_DEMO.md. No game test hooks.
 * Result screenshots also require visual review for their illustrated labels. */
const { chromium, firefox } = require(process.env.WOODLAND_PLAYWRIGHT || 'playwright');
const fs = require('node:fs');
const assert = require('node:assert/strict');
const path = require('node:path');
const kind = process.env.WOODLAND_BROWSER || 'chromium';
const output = process.env.WOODLAND_SMOKE_OUTPUT || '/tmp/woodland-web-smoke';
const fallback = process.env.WOODLAND_TEST_PNG_FALLBACK === '1';
const url = process.env.WOODLAND_SMOKE_URL || 'http://127.0.0.1:8000/little-woodland/';
fs.mkdirSync(output, { recursive: true });
const lossRoute = [
 [0,2,0,3,.58],[0,1,0,2,1.23],[0,2,0,3,.58],[0,0,0,1,.58],
 [1,1,1,2,1.88],[0,0,0,1,.58],[0,3,1,3,1.01],[0,2,0,3,1.01],
 [0,3,0,4,1.01],[1,5,2,5,.8],[0,0,0,1,.58],[2,1,2,2,1.45],
 [0,1,0,2,.58],[0,2,0,3,1.01],[0,0,1,0,.58],[0,0,1,0,.8],
 [0,4,0,5,1.23],[1,1,1,2,1.45],[1,1,2,1,.8],[0,0,0,1,1.83]
];
(async () => {
 const options = kind === 'chromium' ? {
   executablePath: process.env.WOODLAND_CHROME || undefined,
   args: ['--no-sandbox', '--use-angle=swiftshader', '--enable-unsafe-swiftshader']
 } : { firefoxUserPrefs: { 'webgl.force-enabled': true } };
 const browser = await ({chromium, firefox}[kind]).launch({headless: true, ...options});
 try {
  console.log('Browser:', kind, browser.version());
  const page = await browser.newPage({viewport: {width:800, height:1000}, hasTouch:true});
  const errors = [], requests = [];
  page.on('pageerror', error => errors.push(error.stack));
  page.on('console', message => {
   if (message.type() === 'error' && !(fallback && /^(Happy|Worried) animation unavailable; using PNG:/.test(message.text()))) errors.push(message.text());
  });
  page.on('requestfailed', request => errors.push(request.url() + ': ' + request.failure().errorText));
  page.on('request', request => requests.push(request.url()));
  page.on('response', response => { if (response.status() >= 400) errors.push(response.url() + ': ' + response.status()); });
  if (fallback) {
   // Deliberately invalidate both VFS animation manifests in the HTTP response.
   // This tests actual C++ exception/fallback behavior without altering the build.
   await page.route('**/little_woodland.data', async route => {
    const response = await route.fetch();
    const data = await response.body();
    const marker = Buffer.from('LWAN1\n256 256 151 0\n');
    let start = 0, changed = 0;
    while ((start = data.indexOf(marker, start)) !== -1) { data[start + 4] = 48; changed++; start++; }
    assert.equal(changed, 2);
    await route.fulfill({response, body:data});
   });
  }
  await page.goto(url);
  await page.waitForFunction(() => document.body.dataset.game === 'ready', null, {timeout:60000});
  await page.waitForTimeout(600);
  const canvas = page.locator('canvas');
  async function healthy() {
   assert.equal(await page.evaluate(() => document.body.dataset.game), 'ready');
   assert.equal(await page.locator('#status').isVisible(), false);
   assert.equal(await page.evaluate(() => Module.ctx.getError()), 0);
   assert.deepEqual(errors, []);
  }
  async function shot(name) { await page.screenshot({path:path.join(output, `${kind}-${name}.png`)}); }
  async function point(x,y,touch=false) {
   const box = await canvas.boundingBox();
   const px=box.x+x*box.width/480, py=box.y+y*box.height/800;
   if (touch) await page.touchscreen.tap(px,py); else await page.mouse.click(px,py);
   await page.waitForTimeout(100);
  }
  async function cell(r,c,touch=false) { await point(30+(c+.5)*60,192+(r+.5)*60,touch); }
  async function swap(r,c,rr,cc,seconds=2) { await cell(r,c); await cell(rr,cc); await page.waitForTimeout(seconds*1000+450); }
  async function key(name) { await page.keyboard.down(name); await page.evaluate(() => new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)))); await page.waitForTimeout(100); await page.keyboard.up(name); await page.waitForTimeout(200); }
  async function reset() { await key('r'); await page.mouse.move(0,0); }
  async function character() {
   const box=await canvas.boundingBox();
   return page.screenshot({clip:{x:box.x+288*box.width/480,y:box.y+277*box.height/800,width:126*box.width/480,height:126*box.height/800}});
  }
  await healthy(); await page.mouse.move(0,0);
  const initial = await canvas.screenshot();
  await shot('initial');
  await cell(4,6); assert.notDeepEqual(await canvas.screenshot(), initial);
  await shot('selected'); await reset(); assert.deepEqual(await canvas.screenshot(), initial);
  await point(430,20); await key('m'); await healthy(); // unavailable audio stays safe
  // Small CSS-scaled portrait: click/touch and restart must use the same cells.
  await page.setViewportSize({width:360,height:640});
  const small=await canvas.boundingBox(); assert(Math.abs(small.width/small.height-.6)<.001);
  await cell(4,6,true); await cell(5,6,true); await page.waitForTimeout(3500);
  await shot('resized-touch-swap');
  await page.setViewportSize({width:800,height:1000});
  assert.notDeepEqual(await canvas.screenshot(), initial);
  await reset(); assert.deepEqual(await canvas.screenshot(), initial);
  await swap(4,6,5,6,3);
  await cell(5,3); await cell(6,3);
  await page.waitForTimeout(200); await shot('gravity');
  await page.waitForTimeout(4800); await shot('victory');
  const happy = await character(); await page.waitForTimeout(250); if (fallback) assert.deepEqual(await character(), happy); else assert.notDeepEqual(await character(), happy);
  await page.waitForTimeout(5100); await healthy(); await shot('happy-loop');
  await point(240,684); await page.mouse.move(0,0); assert.deepEqual(await canvas.screenshot(), initial);
  for (const step of lossRoute) await swap(...step);
  await healthy(); await shot('defeat');
  const worried = await character(); await page.waitForTimeout(250); if (fallback) assert.deepEqual(await character(), worried); else assert.notDeepEqual(await character(), worried);
  await page.waitForTimeout(5100); await healthy(); await shot('worried-loop');
  await point(240,684); await page.mouse.move(0,0); assert.deepEqual(await canvas.screenshot(), initial);
  assert(requests.every(request => request.startsWith(new URL(url).origin + new URL(url).pathname)));
  assert(!requests.some(request => /\.wav(?:$|\?)/i.test(request)));
  console.log('Graphics:', await page.evaluate(() => Module.ctx.getParameter(Module.ctx.VERSION)));
  console.log('WASM heap bytes:', await page.evaluate(() => HEAPU8.byteLength));
  await key('Escape');
  await page.waitForFunction(() => document.body.dataset.game === 'stopped');
  assert(await page.locator('#reload').isVisible()); assert.deepEqual(errors, []);
  await shot('stopped');
  // A browser without WebGL2 must receive useful feedback instead of a blank canvas.
  const unsupported=await browser.newPage();
  await unsupported.addInitScript(() => {
   const original=HTMLCanvasElement.prototype.getContext;
   HTMLCanvasElement.prototype.getContext=function(type,...args) { return type==='webgl2'?null:original.call(this,type,...args); };
  });
  await unsupported.goto(url);
  assert.match(await unsupported.locator('#detail').textContent(), /WebGL 2 is required/);
  console.log(fallback ? 'PASS: both invalid animation manifests use stable PNG result characters' : 'PASS: initialization, selection, touch/scaling, swaps, gravity, delivery, both result animations/loops, both restart buttons, silent audio, Escape, unsupported-WebGL feedback; no console/network errors.');
 } finally { await browser.close(); }
})().catch(error => { console.error(error); process.exit(1); });
