(() => {
  "use strict";
  const el = id => document.getElementById(id);
  let pollIntervalMs = 1000, state = null, busy = false, epoch = 0;
  let dutyEdited = false, rpmEdited = false;
  const listeners = [];
  function message(text, error = false) {
    el('formMessage').textContent = text;
    el('formMessage').classList.toggle('error', error);
  }
  function controls() {
    const locked = busy || !state || state.power_enabled === false || state.mode === 'thermal';
    el('duty').disabled = el('targetRpm').disabled = el('applyButton').disabled = locked;
    el('applyRpmButton').disabled = locked || !Number.isInteger(state?.min_rpm);
    el('powerToggle').disabled = busy || !state;
    listeners.forEach(fn => fn(state, busy));
  }
  function accept(next) {
    if (!state || state.mode !== next.mode || state.power_enabled !== next.power_enabled) {
      message(next.power_enabled === false ? 'Fan power is off. Power on to change fixed controls.'
        : next.mode === 'thermal' ? 'Fixed controls are paused during THERMAL.' : 'Controller ready. Choose manual duty or fixed RPM.');
    }
    state = next;
    const speed = Math.max(0, Math.round(Number(state.rpm) || 0));
    el('rpm').textContent = String(speed);
    el('currentDuty').textContent = `${state.duty_percent}%`;
    const modeText = state.mode === 'thermal' ? 'THERMAL · Temperature curve active'
      : state.mode === 'rpm' ? `RPM control · Target ${state.target_rpm} RPM` : 'Manual duty control';
    el('controlMode').textContent = state.power_enabled === false ? `POWER OFF · Retained: ${modeText}` : modeText;
    el('powerToggle').textContent = state.power_enabled === false ? 'Power on fan' : 'Power off fan';
    el('powerToggle').setAttribute('aria-pressed', String(state.power_enabled !== false));
    el('powerState').textContent = state.power_enabled === false
      ? state.power_waiting_for_key ? 'Fan power is off. Press any local key or power on here.'
        : 'Fan power is off. Hold ^ + * (buttons 1 + 4) for 1 second, or power on here.'
      : state.power_starting ? 'Fan power is on. Startup boost is active.'
        : 'Fan power is on. Hold * (button 4) alone for 8 seconds to power off locally.';
    if (!dutyEdited) el('duty').value = state.manual_duty_percent ?? state.duty_percent;
    if (!rpmEdited) el('targetRpm').value = state.target_rpm ?? '';
    el('targetRpm').min = state.min_rpm;
    el('targetRpm').max = state.max_rpm;
    el('temperature').textContent = state.temperature_c == null ? '--' : `${state.temperature_c}°C`;
    el('humidity').textContent = state.humidity_percent == null ? '--' : `${state.humidity_percent}%`;
    el('rotor').classList.toggle('spinning', speed > 0);
    el('rotor').style.animationDuration = `${Math.max(.3, Math.min(2.4, 1800 / Math.max(speed, 1)))}s`;
    el('connection').classList.toggle('online', Boolean(state.wifi_connected));
    el('connectionText').textContent = state.wifi_connected ? 'Controller online' : 'Controller unavailable';
    el('lastUpdated').textContent = `Telemetry updated ${new Date().toLocaleTimeString()}`;
    controls();
  }
  async function requestJson(url, options = {}) {
    const response = await fetch(url, { cache: 'no-store', ...options });
    let body;
    try { body = await response.json(); }
    catch (_) { throw new Error(`Controller returned HTTP ${response.status}`); }
    if (!response.ok) throw new Error(body.error || `Controller returned HTTP ${response.status}`);
    return body;
  }
  async function refresh() {
    if (busy) return;
    const requestEpoch = epoch;
    try {
      const next = await requestJson('/api/status');
      if (requestEpoch !== epoch) return; // A control command superseded this poll.
      const interval = Number(next.poll_interval_ms);
      if (Number.isFinite(interval) && interval > 0) pollIntervalMs = interval;
      accept(next);
    } catch (error) {
      if (requestEpoch !== epoch) return;
      el('connection').classList.toggle('online', false);
      el('connectionText').textContent = 'Controller unavailable';
      el('lastUpdated').textContent = 'Telemetry request failed';
      message(error.message, true);
    }
  }
  async function command(url, values, onSuccess = () => {}) {
    if (busy) throw new Error('Another setting is being applied. Please wait.');
    busy = true;
    ++epoch;
    controls();
    try {
      const next = await requestJson(url, {
        method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded;charset=UTF-8' },
        body: new URLSearchParams(values),
      });
      onSuccess();
      accept(next);
      return next;
    } finally {
      ++epoch;
      busy = false;
      controls();
    }
  }
  // All controls share one poller, command lock and authoritative state.
  window.fanmaster = {
    command,
    subscribe(fn) { listeners.push(fn); fn(state, busy); },
  };
  el('powerToggle').addEventListener('click', async () => {
    if (busy || !state) return;
    try {
      const next = await command('/api/fan/power', { enabled: state.power_enabled === false ? '1' : '0' });
      el('powerMessage').textContent = next.power_enabled ? 'Fan power enabled.' : 'Fan power disabled.';
      el('powerMessage').classList.toggle('error', false);
    } catch (error) {
      el('powerMessage').textContent = error.message;
      el('powerMessage').classList.toggle('error', true);
    }
  });
  el('duty').addEventListener('input', () => { dutyEdited = true; });
  el('targetRpm').addEventListener('input', () => { rpmEdited = true; });
  el('rpmForm').addEventListener('submit', async event => {
    event.preventDefault();
    if (busy || !state || state.power_enabled === false || state.mode === 'thermal') return;
    const input = el('targetRpm'), target = Number(input.value);
    if (!input.value.trim() || !Number.isInteger(target) || target < state.min_rpm || target > state.max_rpm) {
      message(`Enter a whole RPM from ${state.min_rpm} through ${state.max_rpm}.`, true);
      input.focus(); return;
    }
    try {
      await command('/api/fan/rpm', { rpm: String(target) }, () => { rpmEdited = false; });
      message(`Holding target ${target} RPM.`);
    } catch (error) { message(error.message, true); }
  });
  el('dutyForm').addEventListener('submit', async event => {
    event.preventDefault();
    if (busy || !state || state.power_enabled === false || state.mode === 'thermal') return;
    const input = el('duty'), duty = Number(input.value);
    if (!input.value.trim() || !Number.isInteger(duty) || duty < 0 || duty > 100) {
      message('Enter a whole number from 0 through 100.', true); input.focus(); return;
    }
    try {
      await command('/api/fan/duty', { duty: String(duty) }, () => { dutyEdited = false; });
      message(`Fan duty set to ${duty}%.`);
    } catch (error) { message(error.message, true); }
  });
  async function poll() {
    await refresh();
    window.setTimeout(poll, pollIntervalMs);
  }
  controls();
  poll();
})();
