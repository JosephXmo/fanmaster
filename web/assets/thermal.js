(() => {
  'use strict';
  const el = id => document.getElementById(id);
  const temperatures = [1, 2, 3].map(i => el(`thermalT${i}`));
  const duties = [1, 2, 3].map(i => el(`thermalD${i}`));
  let state = null, busy = false, edited = false;
  function message(text, error = false) {
    el('thermalMessage').textContent = text;
    el('thermalMessage').classList.toggle('error', error);
  }
  function read() {
    return temperatures.map((input, i) => ({
      t: input.value.trim() ? Number(input.value) : NaN,
      d: duties[i].value.trim() ? Number(duties[i].value) : NaN,
    }));
  }
  function valid(points) {
    return points.every((p, i) => Number.isFinite(p.t) && p.t >= 15 && p.t <= 95 &&
      Math.abs(p.t * 10 - Math.round(p.t * 10)) < 1e-7 && Number.isInteger(p.d) && p.d >= 0 && p.d <= 100 &&
      (!i || Math.round(p.t * 10) > Math.round(points[i-1].t * 10) && p.d >= points[i-1].d));
  }
  function preview() {
    const points = read();
    points.forEach((p, i) => {
      temperatures[i].min = i && Number.isFinite(points[i-1].t) ? (points[i-1].t + .1).toFixed(1) : '15.0';
      temperatures[i].max = i < 2 && Number.isFinite(points[i+1].t) ? (points[i+1].t - .1).toFixed(1) : '95.0';
      duties[i].min = i && Number.isFinite(points[i-1].d) ? points[i-1].d : 0;
      duties[i].max = i < 2 && Number.isFinite(points[i+1].d) ? points[i+1].d : 100;
    });
    const ok = valid(points);
    el('thermalPreview').hidden = !ok;
    if (ok) {
      const xy = p => `${40 + (p.t - 15) * 5},${180 - p.d * 1.5}`;
      const line = [{t: 15, d: points[0].d}, ...points, {t: 95, d: points[2].d}];
      el('thermalLine').setAttribute('points', line.map(xy).join(' '));
      points.forEach((p, i) => {
        el(`thermalDot${i+1}`).setAttribute('cx', 40 + (p.t - 15) * 5);
        el(`thermalDot${i+1}`).setAttribute('cy', 180 - p.d * 1.5);
      });
    }
    return ok;
  }
  function editedInput() {
    edited = true;
    const ok = preview();
    message(ok ? 'Curve changed. Save to apply.' : 'Use increasing temperatures (0.1°C steps) and nondecreasing whole duty.', !ok);
  }
  temperatures.forEach(input => input.addEventListener('input', editedInput));
  duties.forEach(input => input.addEventListener('input', editedInput));
  // Clamp committed edits to adjacent points. While typing, partial values may be invalid.
  [...temperatures, ...duties].forEach(input => input.addEventListener('change', () => {
    if (input.value.trim() && Number.isFinite(Number(input.value))) {
      const isTemperature = temperatures.includes(input);
      const rounded = Math.round(Number(input.value) * (isTemperature ? 10 : 1)) / (isTemperature ? 10 : 1);
      input.value = Math.min(Number(input.max), Math.max(Number(input.min), rounded)).toFixed(isTemperature ? 1 : 0);
    }
    editedInput();
  }));
  window.fanmaster.subscribe((next, pending) => {
    state = next; busy = pending;
    const active = state?.mode === 'thermal';
    el('thermalToggle').textContent = active ? 'THERMAL on · Return to fixed' : 'Enable temperature control';
    el('thermalToggle').setAttribute('aria-pressed', String(active));
    el('thermalToggle').disabled = busy || !state || !active && !state.thermal_sensor_ready;
    el('saveThermal').disabled = busy || !state;
    [...temperatures, ...duties].forEach(input => { input.disabled = busy || !state; });
    if (!state) return;
    if (!edited) {
      state.thermal_points.forEach((point, i) => {
        temperatures[i].value = Number(point.temperature_c).toFixed(1);
        duties[i].value = point.duty_percent;
      });
      preview();
    }
    el('thermalState').textContent = active ? 'THERMAL controls the applied duty. Fixed controls are paused.'
      : state.thermal_exit_reason === 'sensor' ? 'Temperature unavailable: restored the previous fixed mode. Enable again after recovery.'
      : !state.thermal_sensor_ready ? 'Waiting for a fresh temperature reading.'
      : 'Fixed control active. Your previous Duty / RPM setting is preserved while THERMAL runs.';
  });
  el('thermalToggle').addEventListener('click', async () => {
    if (busy || !state) return;
    try {
      const next = await window.fanmaster.command('/api/thermal/mode', { enabled: state.mode === 'thermal' ? '0' : '1' });
      message(next.mode === 'thermal' ? 'Temperature control enabled.' : 'Previous fixed setting restored.');
    } catch (error) { message(error.message, true); }
  });
  el('thermalForm').addEventListener('submit', async event => {
    event.preventDefault();
    if (busy || !state) return;
    if (!preview()) { message('Temperatures must strictly increase; duty must not decrease.', true); return; }
    const values = {};
    read().forEach((p, i) => { values[`t${i+1}`] = p.t.toFixed(1); values[`d${i+1}`] = String(p.d); });
    try {
      await window.fanmaster.command('/api/thermal/curve', values, () => { edited = false; });
      message('Temperature curve saved.');
    } catch (error) { message(error.message, true); }
  });
})();
