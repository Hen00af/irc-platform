(() => {
  "use strict";

  const MAX_REQUESTS = 100;
  const MAX_CONCURRENCY = 8;
  const LOCAL_HOSTS = new Set(["localhost", "127.0.0.1", "::1"]);
  const SAFE_TARGETS = new Set([
    "/health/",
    "/",
    "/cgi-bin/hello.py",
    "/cgi-bin/hello.php",
    "/cgi-bin/slow.py?seconds=0.2"
  ]);

  const elements = {
    form: document.querySelector("#loadForm"),
    target: document.querySelector("#target"),
    requests: document.querySelector("#requests"),
    requestsOutput: document.querySelector("#requestsOutput"),
    concurrency: document.querySelector("#concurrency"),
    concurrencyOutput: document.querySelector("#concurrencyOutput"),
    runButton: document.querySelector("#runButton"),
    stopButton: document.querySelector("#stopButton"),
    localBadge: document.querySelector("#localBadge"),
    controlNote: document.querySelector("#controlNote"),
    runState: document.querySelector("#runState"),
    pulseRail: document.querySelector("#pulseRail"),
    railCaption: document.querySelector("#railCaption"),
    completed: document.querySelector("#completedMetric"),
    success: document.querySelector("#successMetric"),
    throughput: document.querySelector("#throughputMetric"),
    median: document.querySelector("#medianMetric"),
    p95: document.querySelector("#p95Metric"),
    errors: document.querySelector("#errorMetric"),
    rows: document.querySelector("#sampleRows"),
    clearButton: document.querySelector("#clearButton"),
    probeButton: document.querySelector("#probeButton"),
    serverLamp: document.querySelector("#serverLamp"),
    serverStatus: document.querySelector("#serverStatus"),
    clock: document.querySelector("#clock"),
    serviceNote: document.querySelector("#serviceNote"),
    chatEndpoint: document.querySelector("#chatEndpoint"),
    gatewayEndpoint: document.querySelector("#gatewayEndpoint"),
    ircEndpoint: document.querySelector("#ircEndpoint"),
    ircHint: document.querySelector("#ircHint")
  };

  let activeRun = null;
  const samples = [];
  const isLocal = LOCAL_HOSTS.has(window.location.hostname);

  function clamp(value, minimum, maximum) {
    return Math.min(Math.max(Number(value), minimum), maximum);
  }

  function percentile(values, amount) {
    if (!values.length) return null;
    const sorted = [...values].sort((a, b) => a - b);
    const index = Math.min(
      sorted.length - 1,
      Math.ceil((amount / 100) * sorted.length) - 1
    );
    return sorted[Math.max(0, index)];
  }

  function formatNumber(value, digits = 0) {
    return Number.isFinite(value) ? value.toFixed(digits) : "—";
  }

  function updateClock() {
    const now = new Date();
    elements.clock.dateTime = now.toISOString();
    elements.clock.textContent = now.toLocaleTimeString("ja-JP", {
      hour12: false,
      hour: "2-digit",
      minute: "2-digit",
      second: "2-digit"
    });
  }

  /* 公開先ごとにgatewayとIRCの到達点が変わる。分岐は chat/app.js の
     gatewayUrl() と揃えてある。 */
  function serviceEndpoints() {
    const { protocol, hostname, host, origin } = window.location;
    const wsProtocol = protocol === "https:" ? "wss:" : "ws:";

    if (isLocal) {
      return {
        deployment: "Local / ports published directly",
        chat: `${origin}/chat/`,
        gateway: `${wsProtocol}//${hostname}:3001`,
        irc: `${hostname}:6667`,
        ircHint: "IRCクライアントから直接接続できます"
      };
    }
    if (hostname.endsWith(".fly.dev")) {
      return {
        deployment: "Fly.io / TLS handlers per service",
        chat: `${origin}/chat/`,
        gateway: `${wsProtocol}//${hostname}:8443`,
        irc: `${hostname}:6697`,
        ircHint: "TLSを有効にしたIRCクライアントで接続します"
      };
    }
    return {
      deployment: "Single port / edge proxy",
      chat: `${origin}/chat/`,
      gateway: `${wsProtocol}//${host}/gateway/ws`,
      irc: "127.0.0.1:6667",
      ircHint: "コンテナ内部のみ。TCPポートを公開しないため外部からは繋がりません"
    };
  }

  function renderServiceEndpoints() {
    const endpoints = serviceEndpoints();
    elements.serviceNote.textContent = endpoints.deployment;
    elements.chatEndpoint.href = endpoints.chat;
    elements.chatEndpoint.textContent = endpoints.chat;
    elements.gatewayEndpoint.textContent = endpoints.gateway;
    elements.ircEndpoint.textContent = endpoints.irc;
    elements.ircHint.textContent = endpoints.ircHint;
  }

  function configureSafety() {
    if (isLocal) {
      elements.localBadge.textContent = "Local / enabled";
      elements.localBadge.classList.add("enabled");
      elements.controlNote.textContent =
        "Safety cap: 100 requests, concurrency 8, same origin only.";
      return;
    }
    elements.localBadge.textContent = "Public / disabled";
    elements.runButton.disabled = true;
    elements.controlNote.textContent =
      "Load generation is disabled outside localhost. Endpoint probes remain available.";
  }

  function updateRangeOutputs() {
    elements.requestsOutput.value = clamp(
      elements.requests.value,
      1,
      MAX_REQUESTS
    );
    elements.concurrencyOutput.value = clamp(
      elements.concurrency.value,
      1,
      MAX_CONCURRENCY
    );
  }

  function setRunning(running) {
    elements.runButton.disabled = running || !isLocal;
    elements.stopButton.disabled = !running;
    elements.target.disabled = running;
    elements.requests.disabled = running;
    elements.concurrency.disabled = running;
    elements.runState.textContent = running ? "RUNNING" : "IDLE";
    elements.runState.classList.toggle("running", running);
  }

  function renderMetrics(runSamples, elapsedMs) {
    const completed = runSamples.length;
    const successful = runSamples.filter((sample) => sample.ok).length;
    const errors = runSamples.filter((sample) => !sample.ok).length;
    const latencies = runSamples
      .filter((sample) => !sample.aborted)
      .map((sample) => sample.latency);
    elements.completed.textContent = String(completed);
    elements.success.textContent = completed
      ? formatNumber((successful / completed) * 100, 1)
      : "—";
    elements.throughput.textContent = elapsedMs > 0
      ? formatNumber(completed / (elapsedMs / 1000), 1)
      : "—";
    elements.median.textContent = latencies.length
      ? formatNumber(percentile(latencies, 50), 0)
      : "—";
    elements.p95.textContent = latencies.length
      ? formatNumber(percentile(latencies, 95), 0)
      : "—";
    elements.errors.textContent = String(errors);
  }

  function renderRail(runSamples) {
    elements.pulseRail.replaceChildren();
    runSamples.slice(-100).forEach((sample) => {
      const pulse = document.createElement("i");
      pulse.className = `pulse ${sample.aborted ? "aborted" : sample.ok ? "" : "error"}`;
      const height = Math.max(12, Math.min(62, 12 + sample.latency / 8));
      pulse.style.setProperty("--pulse-height", `${height}px`);
      pulse.title = `${sample.status} / ${Math.round(sample.latency)} ms`;
      elements.pulseRail.append(pulse);
    });
    elements.railCaption.textContent = runSamples.length
      ? `${runSamples.length} samples / latest ${Math.round(
          runSamples[runSamples.length - 1].latency
        )} ms`
      : "No samples yet";
  }

  function renderRows() {
    elements.rows.replaceChildren();
    if (!samples.length) {
      const row = document.createElement("tr");
      row.className = "empty-row";
      row.innerHTML = '<td colspan="4">Run a profile to collect samples.</td>';
      elements.rows.append(row);
      return;
    }
    samples.slice(-12).reverse().forEach((sample) => {
      const row = document.createElement("tr");
      const statusClass = sample.ok ? "sample-ok" : "sample-error";
      row.innerHTML = `
        <td>${sample.id}</td>
        <td class="${statusClass}">${sample.status}</td>
        <td>${Math.round(sample.latency)} ms</td>
        <td>${sample.target}</td>
      `;
      elements.rows.append(row);
    });
  }

  async function executeRequest(target, run, id) {
    const controller = new AbortController();
    run.controllers.add(controller);
    const started = performance.now();
    let status = "network";
    let ok = false;
    let aborted = false;
    try {
      const response = await fetch(target, {
        method: "GET",
        cache: "no-store",
        credentials: "same-origin",
        signal: controller.signal,
        headers: { "X-Webserv-Control-Plane": "load-sample" }
      });
      status = String(response.status);
      ok = response.ok || (response.status >= 300 && response.status < 400);
      await response.arrayBuffer();
    } catch (error) {
      aborted = error.name === "AbortError";
      status = aborted ? "aborted" : "network";
    } finally {
      run.controllers.delete(controller);
    }
    const sample = {
      id,
      target,
      status,
      ok,
      aborted,
      latency: performance.now() - started
    };
    samples.push(sample);
    run.samples.push(sample);
    const elapsed = performance.now() - run.started;
    renderMetrics(run.samples, elapsed);
    renderRail(run.samples);
    renderRows();
  }

  async function worker(run) {
    while (!run.stopped) {
      const index = run.nextIndex++;
      if (index >= run.total) return;
      await executeRequest(run.target, run, index + 1);
    }
  }

  async function runProfile(event) {
    event.preventDefault();
    if (!isLocal || activeRun) return;
    const target = elements.target.value;
    if (!SAFE_TARGETS.has(target)) {
      elements.controlNote.textContent = "Blocked: target is not in the local allowlist.";
      return;
    }
    const total = clamp(elements.requests.value, 1, MAX_REQUESTS);
    const concurrency = clamp(elements.concurrency.value, 1, MAX_CONCURRENCY);
    const run = {
      target,
      total,
      nextIndex: 0,
      samples: [],
      controllers: new Set(),
      started: performance.now(),
      stopped: false
    };
    activeRun = run;
    renderMetrics([], 0);
    renderRail([]);
    setRunning(true);
    await Promise.all(Array.from({ length: concurrency }, () => worker(run)));
    const elapsed = performance.now() - run.started;
    renderMetrics(run.samples, elapsed);
    elements.runState.textContent = run.stopped ? "STOPPED" : "COMPLETE";
    elements.runState.classList.remove("running");
    setRunning(false);
    activeRun = null;
  }

  function stopProfile() {
    if (!activeRun) return;
    activeRun.stopped = true;
    activeRun.controllers.forEach((controller) => controller.abort());
    elements.runState.textContent = "STOPPING";
  }

  async function probeEndpoint(card) {
    const target = card.dataset.probe;
    const readout = card.querySelector("span");
    const started = performance.now();
    card.classList.remove("ok", "error");
    readout.textContent = "probing";
    try {
      const response = await fetch(target, { cache: "no-store" });
      const latency = Math.round(performance.now() - started);
      const ok = response.status >= 200 && response.status < 400;
      card.classList.add(ok ? "ok" : "error");
      readout.textContent = `${response.status} / ${latency} ms`;
      await response.arrayBuffer();
      return ok;
    } catch {
      card.classList.add("error");
      readout.textContent = "unreachable";
      return false;
    }
  }

  async function probeAll() {
    elements.probeButton.disabled = true;
    const cards = [...document.querySelectorAll("[data-probe]")];
    const results = await Promise.all(cards.map(probeEndpoint));
    const serverOk = results[1] === true;
    elements.serverLamp.classList.toggle("ok", serverOk);
    elements.serverStatus.textContent = serverOk ? "Server online" : "Probe failed";
    elements.probeButton.disabled = false;
  }

  elements.requests.addEventListener("input", updateRangeOutputs);
  elements.concurrency.addEventListener("input", updateRangeOutputs);
  elements.form.addEventListener("submit", runProfile);
  elements.stopButton.addEventListener("click", stopProfile);
  elements.probeButton.addEventListener("click", probeAll);
  elements.clearButton.addEventListener("click", () => {
    samples.length = 0;
    renderRows();
    renderRail([]);
    renderMetrics([], 0);
  });

  updateRangeOutputs();
  configureSafety();
  renderServiceEndpoints();
  updateClock();
  setInterval(updateClock, 1000);
  probeAll();
})();
