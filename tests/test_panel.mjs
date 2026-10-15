import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

const source = readFileSync(new URL('../web/assets/panel.js', import.meta.url), 'utf8');
const fields = new Map();
const requests = [];
let nextPoll;
let state = {
  rpm: 810, duty_percent: 39, manual_duty_percent: 35, wifi_connected: true,
  mode: 'rpm', target_rpm: 850, min_rpm: 550, max_rpm: 1700, poll_interval_ms: 1000,
};
let failPost = false, releasePost;
let delayPost = false;
function field(id) {
  if (!fields.has(id)) {
    fields.set(id, {
      _value: '', get value() { return this._value; }, set value(v) { this._value = String(v); },
      style: {}, classList: { toggle() {} },
      addEventListener(type, handler) { this[type] = handler; }, focus() {},
    });
  }
  return fields.get(id);
}
vm.runInNewContext(source, {
  document: { getElementById: field }, URLSearchParams,
  window: { setTimeout(callback) { nextPoll = callback; } },
  async fetch(url, options) {
    requests.push({ url, options });
    if (options.method === 'POST') {
      if (delayPost) await new Promise(resolve => { releasePost = resolve; });
      if (failPost) return { ok: false, status: 400, json: async () => ({ error: 'Rejected target' }) };
      if (url === '/api/fan/rpm') {
        state = { ...state, mode: 'rpm', target_rpm: Number(options.body.get('rpm')) };
      } else {
        state = { ...state, mode: 'duty', manual_duty_percent: Number(options.body.get('duty')),
                  duty_percent: Number(options.body.get('duty')) };
      }
    }
    return { ok: true, json: async () => ({ ...state }) };
  },
});
const flush = () => new Promise(resolve => setImmediate(resolve));
const submit = id => field(id).submit({ preventDefault() {} });
const poll = async () => { nextPoll(); await flush(); };
await flush();
assert.match(field('controlMode').textContent, /850 RPM/);
assert.equal(field('duty').value, '35', 'Show remembered manual duty, not PI output');
assert.equal(field('currentDuty').textContent, '39%');
assert.equal(field('targetRpm').min, 550);
assert.equal(field('targetRpm').max, 1700);

field('targetRpm').value = '970';
field('targetRpm').input();
state.target_rpm = 900;
await poll();
assert.equal(field('targetRpm').value, '970', 'Telemetry must not overwrite an edit');
await submit('rpmForm');
assert.equal(requests.at(-1).url, '/api/fan/rpm');
assert.equal(state.target_rpm, 970);
assert.match(field('controlMode').textContent, /970 RPM/);
for (const invalid of ['', '549', '1701', '850.5', '-850']) {
  field('targetRpm').value = invalid;
  const count = requests.length;
  await submit('rpmForm');
  assert.equal(requests.length, count);
}
field('duty').value = '';
const count = requests.length;
await submit('dutyForm');
assert.equal(requests.length, count, 'Blank input must not request zero duty');

field('duty').value = '42';
await submit('dutyForm');
assert.equal(state.mode, 'duty');
assert.equal(field('currentDuty').textContent, '42%');
assert.match(field('controlMode').textContent, /Manual/);

field('targetRpm').value = '1000';
failPost = true;
await submit('rpmForm');
assert.match(field('formMessage').textContent, /Rejected target/);
assert.equal(state.mode, 'duty');
assert.equal(field('applyRpmButton').disabled, false);

failPost = false;
delayPost = true;
const pending = submit('rpmForm');
await flush();
await poll();
assert.equal(field('applyRpmButton').disabled, true, 'Polling must not enable an in-flight submission');
assert.equal(field('applyButton').disabled, true);
releasePost();
await pending;
assert.equal(field('applyButton').disabled, false);
assert.match(field('controlMode').textContent, /1000 RPM/);
console.log('Panel RPM limits, manual setting, edits, mode changes and request-state tests passed');
