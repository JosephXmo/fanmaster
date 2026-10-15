    (() => {
      "use strict";

      let pollIntervalMs = 1000;
      const connection = document.getElementById("connection");
      const connectionText = document.getElementById("connectionText");
      const rpm = document.getElementById("rpm");
      const rotor = document.getElementById("rotor");
      const currentDuty = document.getElementById("currentDuty");
      const temperature = document.getElementById("temperature");
      const humidity = document.getElementById("humidity");
      const lastUpdated = document.getElementById("lastUpdated");
      const form = document.getElementById("dutyForm");
      const dutyInput = document.getElementById("duty");
      const applyButton = document.getElementById("applyButton");
      const formMessage = document.getElementById("formMessage");
      let dutyIsBeingEdited = false;
      let statusReceived = false;
      let statusErrorVisible = false;

      function setConnectionState(online) {
        connection.classList.toggle("online", online);
        connectionText.textContent = online ? "Controller online" : "Controller unavailable";
      }

      function showMessage(message, isError = false) {
        formMessage.textContent = message;
        formMessage.classList.toggle("error", isError);
      }

      function showOptionalReading(element, value, suffix) {
        element.textContent = value === null || value === undefined ? "--" : `${value}${suffix}`;
      }

      async function requestJson(url, options = {}) {
        const response = await fetch(url, { cache: "no-store", ...options });
        let body;

        try {
          body = await response.json();
        } catch (_) {
          throw new Error(`Controller returned HTTP ${response.status}`);
        }

        if (!response.ok) {
          throw new Error(body.error || `Controller returned HTTP ${response.status}`);
        }

        return body;
      }

      async function refreshStatus() {
        try {
          const state = await requestJson("/api/status");
          const interval = Number(state.poll_interval_ms);
          if (Number.isFinite(interval) && interval > 0) pollIntervalMs = interval;
          const roundedRpm = Math.max(0, Math.round(Number(state.rpm) || 0));

          rpm.textContent = String(roundedRpm);
          currentDuty.textContent = `${state.duty_percent}%`;
          showOptionalReading(temperature, state.temperature_c, "\u00b0C");
          showOptionalReading(humidity, state.humidity_percent, "%");

          rotor.classList.toggle("spinning", roundedRpm > 0);
          rotor.style.animationDuration = `${Math.max(.3, Math.min(2.4, 1800 / Math.max(roundedRpm, 1)))}s`;

          if (!dutyIsBeingEdited) {
            dutyInput.value = state.duty_percent;
          }

          setConnectionState(Boolean(state.wifi_connected));
          lastUpdated.textContent = `Telemetry updated ${new Date().toLocaleTimeString()}`;

          if (!statusReceived || statusErrorVisible) {
            showMessage("Controller ready. Choose a duty cycle when needed.");
          }
          statusReceived = true;
          statusErrorVisible = false;
        } catch (error) {
          setConnectionState(false);
          lastUpdated.textContent = "Telemetry request failed";
          showMessage(error.message, true);
          statusErrorVisible = true;
        }
      }

      async function pollStatus() {
        await refreshStatus();
        window.setTimeout(pollStatus, pollIntervalMs);
      }

      dutyInput.addEventListener("input", () => {
        dutyIsBeingEdited = true;
      });

      form.addEventListener("submit", async (event) => {
        event.preventDefault();
        const requestedDuty = Number(dutyInput.value);

        if (!Number.isInteger(requestedDuty) || requestedDuty < 0 || requestedDuty > 100) {
          showMessage("Enter a whole number from 0 through 100.", true);
          dutyInput.focus();
          return;
        }

        applyButton.disabled = true;
        showMessage("Applying new fan setting...");

        try {
          const body = new URLSearchParams({ duty: String(requestedDuty) });
          const result = await requestJson("/api/fan/duty", {
            method: "POST",
            headers: { "Content-Type": "application/x-www-form-urlencoded;charset=UTF-8" },
            body,
          });

          dutyIsBeingEdited = false;
          dutyInput.value = result.duty_percent;
          currentDuty.textContent = `${result.duty_percent}%`;
          showMessage(`Fan duty set to ${result.duty_percent}%.`);
          await refreshStatus();
        } catch (error) {
          showMessage(error.message, true);
        } finally {
          applyButton.disabled = false;
        }
      });

      pollStatus();
    })();
