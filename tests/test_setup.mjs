import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

const source = readFileSync(new URL('../web/assets/setup.js', import.meta.url), 'utf8');
new vm.Script(readFileSync(new URL('../web/assets/panel.js', import.meta.url), 'utf8'));
const flush = () => new Promise(resolve => setImmediate(resolve));
const response = body => ({ ok: true, json: async () => body });

function harness(fetch) {
  const fields = {
    '#setup': { values: { ssid: 'home', password: 'password' }, addEventListener(_, handler) { this.submit = handler; } },
    '#connect': { disabled: false }, '#status': { textContent: '' }, '#password': { value: 'password' },
  };
  let nextId = 0;
  const timers = new Map();
  vm.runInNewContext(source, {
    document: { querySelector: key => fields[key] },
    AbortController, URLSearchParams,
    FormData: class {
      constructor(form) { this.entries = Object.entries(form.values); }
      [Symbol.iterator]() { return this.entries[Symbol.iterator](); }
    },
    fetch,
    setTimeout(callback, delay) { const id = ++nextId; timers.set(id, { callback, delay }); return id; },
    clearTimeout(id) { timers.delete(id); },
  });
  return {
    fields, timers,
    async tick() {
      assert.equal(timers.size, 1, 'Exactly one next poll should remain after request completion');
      const [id, timer] = timers.entries().next().value;
      timers.delete(id);
      timer.callback();
      await flush();
    },
    async submit() { await fields['#setup'].submit({ preventDefault() {} }); await flush(); },
  };
}

// A delayed pre-submit status response must not overwrite the submitted state
// or create a second polling chain.
let finishInitial;
let gets = 0;
const race = harness(async (url, options) => {
  if (options.method === 'POST') return response({ state: 'connecting' });
  if (++gets === 1) return new Promise(resolve => { finishInitial = resolve; });
  return response({ state: 'saved', ip: '192.0.2.10' });
});
await race.submit();
finishInitial(response({ state: 'idle' }));
await flush();
assert.equal(race.fields['#connect'].disabled, true);
assert.equal(race.fields['#password'].value, '');
await race.tick();
assert.match(race.fields['#status'].textContent, /http:\/\/192\.0\.2\.10\//);
assert.equal(race.timers.size, 0);

for (const failure of ['connection_failed', 'storage_failed']) {
  let state = 'idle';
  const page = harness(async (_, options) => {
    if (options.method === 'POST') { state = failure; return response({ state: 'connecting' }); }
    return response({ state });
  });
  await flush();
  await page.submit();
  await page.tick();
  assert.equal(page.fields['#connect'].disabled, false);
  assert.match(page.fields['#status'].textContent, /failed/);
  assert.equal(page.timers.size, 0);
}

// Temporary channel-change failures retry; persistent loss eventually becomes actionable.
let networkWorks = false;
const reconnect = harness(async () => {
  if (!networkWorks) throw new Error('channel changing');
  return response({ state: 'saved', ip: '192.0.2.10' });
});
await flush();
assert.match(reconnect.fields['#status'].textContent, /Waiting/);
networkWorks = true;
await reconnect.tick();
assert.match(reconnect.fields['#status'].textContent, /Connected and saved/);
const lost = harness(async () => { throw new Error('offline'); });
await flush();
for (let retry = 0; retry < 3; ++retry) await lost.tick();
assert.match(lost.fields['#status'].textContent, /Connection lost/);
assert.equal(lost.fields['#connect'].disabled, false);
assert.equal(lost.timers.size, 0);
console.log('Setup UI submission, stale-response, retry and save-state tests passed');
