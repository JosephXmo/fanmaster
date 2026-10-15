(() => {
  const form = document.querySelector('#setup');
  const button = document.querySelector('#connect');
  const status = document.querySelector('#status');
  let timer;
  let generation = 0;
  let failures = 0;
  async function request(url, options = {}) {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 6000);
    try {
      const response = await fetch(url, { cache: 'no-store', ...options, signal: controller.signal });
      const body = await response.json();
      if (!response.ok) throw new Error(body.error || 'Request failed.');
      return body;
    } finally { clearTimeout(timeout); }
  }
  function schedule(token, delay = 1000) {
    timer = setTimeout(() => poll(token), delay);
  }
  async function poll(token) {
    try {
      const result = await request('/api/wifi/status');
      if (token !== generation) return;
      failures = 0;
      if (result.state === 'saved') {
        status.textContent = `Connected and saved. Open http://${result.ip}/ after joining your usual network.`;
        button.disabled = true;
        return;
      }
      if (result.state === 'connection_failed' || result.state === 'storage_failed') {
        status.textContent = result.state === 'connection_failed'
          ? 'Connection failed. Check the network name and password, then retry.'
          : 'Connected, but saving failed. Credentials have not been saved. Retry or check device storage.';
        button.disabled = false;
        return;
      }
      button.disabled = result.state === 'connecting';
      status.textContent = button.disabled ? 'Connecting…' : 'Ready to configure.';
      schedule(token);
    } catch (_) {
      if (token !== generation) return;
      // AP+STA can briefly change channel when joining the target network.
      if (++failures <= 3) {
        status.textContent = 'Waiting for the setup network…';
        schedule(token, 1500);
      } else {
        status.textContent = 'Connection lost. Rejoin the setup network to retry; if setup has closed, check the device display or serial console for its address.';
        button.disabled = false;
      }
    }
  }
  form.addEventListener('submit', async event => {
    event.preventDefault();
    clearTimeout(timer);
    const token = ++generation;
    failures = 0;
    button.disabled = true;
    status.textContent = 'Submitting…';
    try {
      const body = new URLSearchParams(new FormData(form));
      await request('/api/wifi/configure', { method: 'POST', body });
      document.querySelector('#password').value = '';
      status.textContent = 'Connecting…';
      schedule(token, 500);
    } catch (error) {
      status.textContent = error.message;
      button.disabled = false;
      // The request may have been accepted before a channel change broke TCP.
      schedule(token);
    }
  });
  poll(generation);
})();
