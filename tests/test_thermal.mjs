import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';
const fields = new Map();
function el(id) {
  if (!fields.has(id)) fields.set(id, {
    _value: '', get value() { return this._value; }, set value(v) { this._value = String(v); },
    style: {}, attributes: {}, classList: { toggle() {} }, focus() {},
    setAttribute(name, value) { this.attributes[name] = String(value); },
    addEventListener(type, fn) { this[type] = fn; },
  });
  return fields.get(id);
}
let state = {
  mode: 'duty', fixed_mode: 'duty', manual_duty_percent: 42, duty_percent: 42, target_rpm: 930,
  min_rpm: 550, max_rpm: 1700, rpm: 900, wifi_connected: true, thermal_sensor_ready: true,
  thermal_exit_reason: 'none', thermal_points: [
    {temperature_c: 25, duty_percent: 35}, {temperature_c: 40, duty_percent: 65}, {temperature_c: 80, duty_percent: 90},
  ],
};
let nextPoll, holdPoll = false, releasePoll, holdPost = false, releasePost, rejectPost = false;
const requests = [];
const clone = () => structuredClone(state);
const context = vm.createContext({
  document: { getElementById: el }, URLSearchParams,
  window: { setTimeout(fn) { nextPoll = fn; } },
  async fetch(url, options) {
    requests.push(url);
    if (options.method === 'POST') {
      if (holdPost) await new Promise(resolve => { releasePost = resolve; });
      if (rejectPost) return { ok: false, status: 409, json: async () => ({ error: 'Sensor unavailable' }) };
      if (url === '/api/thermal/mode') {
        if (options.body.get('enabled') === '1') {
          state.fixed_mode = state.mode;
          state.mode = 'thermal'; state.duty_percent = 50;
        } else {
          state.mode = state.fixed_mode; state.duty_percent = state.manual_duty_percent;
        }
      } else if (url === '/api/thermal/curve') {
        state.thermal_points = [1,2,3].map(i => ({
          temperature_c: Number(options.body.get(`t${i}`)), duty_percent: Number(options.body.get(`d${i}`)),
        }));
      }
    } else if (holdPoll) {
      const old = clone();
      await new Promise(resolve => { releasePoll = resolve; });
      return { ok: true, json: async () => old };
    }
    return { ok: true, json: async () => clone() };
  },
});
for (const name of ['panel', 'thermal']) vm.runInContext(readFileSync(new URL(`../web/assets/${name}.js`, import.meta.url), 'utf8'), context);
const flush = () => new Promise(resolve => setImmediate(resolve));
const poll = async () => { const pending = nextPoll(); await flush(); return pending; };
const submit = () => el('thermalForm').submit({ preventDefault() {} });
await flush();
assert.equal(el('thermalT1').value, '25.0');
assert.equal(el('thermalT1').max, '39.9');
assert.equal(el('thermalT2').min, '25.1');
assert.equal(el('thermalD1').max, 65);
assert.match(el('thermalLine').attributes.points, /^40,127.5 /, 'Curve holds at the first endpoint');

// An already-running poll must not undo a later mode command.
holdPoll = true;
const oldPoll = poll(); await flush();
await el('thermalToggle').click();
assert.equal(el('thermalToggle').attributes['aria-pressed'], 'true');
assert.equal(el('applyButton').disabled, true);
assert.equal(el('duty').disabled, true);
assert.match(el('controlMode').textContent, /THERMAL/);
releasePoll(); await oldPoll; holdPoll = false;
assert.match(el('controlMode').textContent, /THERMAL/, 'Discard stale poll response');
const count = requests.length;
await el('dutyForm').submit({preventDefault() {}});
await el('rpmForm').submit({preventDefault() {}});
assert.equal(requests.length, count, 'Fixed submit handlers cannot bypass disabled controls');

// Edits survive polls, and bounds use 0.1°C rather than whole degrees.
el('thermalT1').value = '39.95'; el('thermalT1').input();
assert.equal(el('thermalPreview').hidden, true);
await submit(); assert.equal(requests.length, count);
el('thermalT1').change();
assert.equal(el('thermalT1').value, '39.9');
el('thermalD2').value = '20'; el('thermalD2').input(); el('thermalD2').change();
assert.equal(el('thermalD2').value, '35', 'Do not cross the lower duty boundary');
await poll(); assert.equal(el('thermalT1').value, '39.9', 'Polling preserves unsaved curve edits');
await submit();
assert.equal(state.thermal_points[0].temperature_c, 39.9);
assert.equal(state.thermal_points[1].duty_percent, 35, 'Equal duty values are allowed');

// Local button 3 exits to fixed on the next shared poll.
state.mode = 'duty'; state.thermal_exit_reason = 'manual'; state.duty_percent = 42;
await poll();
assert.equal(el('thermalToggle').attributes['aria-pressed'], 'false');
assert.equal(el('applyButton').disabled, false);
assert.equal(el('duty').value, '42');
state.mode = 'rpm'; await poll();
await el('thermalToggle').click();
await el('thermalToggle').click();
assert.equal(state.mode, 'rpm', 'Web exit restores the previous fixed mode');

// Sensor failure disarms THERMAL and requires explicit reentry after recovery.
state.thermal_sensor_ready = false; state.thermal_exit_reason = 'sensor';
await poll();
assert.equal(el('thermalToggle').disabled, true);
assert.match(el('thermalState').textContent, /restored/);
state.thermal_sensor_ready = true; await poll();
assert.equal(el('thermalToggle').attributes['aria-pressed'], 'false');
assert.equal(el('thermalToggle').disabled, false);
rejectPost = true; await el('thermalToggle').click();
assert.match(el('thermalMessage').textContent, /Sensor unavailable/);
assert.equal(el('applyButton').disabled, false);
rejectPost = false;

// Both widgets share the command lock; polling cannot unlock an active command.
holdPost = true;
const pending = el('thermalToggle').click(); await flush();
assert.equal(el('saveThermal').disabled, true);
assert.equal(el('applyButton').disabled, true);
await poll();
assert.equal(el('thermalToggle').disabled, true);
releasePost(); await pending;
assert.equal(el('thermalToggle').disabled, false);
assert.equal(el('applyButton').disabled, true);
console.log('Thermal widget bounds, preview, commands, local synchronization, sensor fallback and stale-poll tests passed');
