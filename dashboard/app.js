const API_BASE_URL = "http://127.0.0.1:8000";
const API_KEY = "freshfruit-secret-key-2026";
const POLLING_INTERVAL_MS = 2500;

const FRUITS = {
  Banana:  { label: "Banana",  color: "#e3b92f", range: [45, 80, 92] },
  Manga:   { label: "Manga",   color: "#f0a030", range: [40, 78, 90] },
  Mamao:   { label: "Mamão",   color: "#ef8a47", range: [42, 76, 88] },
  Maca:    { label: "Maçã",    color: "#c8433a", range: [35, 75, 88] },
  Uva:     { label: "Uva",     color: "#6b4a9e", range: [45, 82, 92] },
  Laranja: { label: "Laranja", color: "#ee8a1c", range: [38, 75, 88] },
  Abacate: { label: "Abacate", color: "#4f7a3a", range: [40, 78, 90] }
};
const DEFAULT_FRUIT = { color: "#7a8a80", range: [45, 80, 92] };

const WAVELENGTHS = [410, 435, 460, 485, 510, 535, 560, 585, 610, 645, 680, 705, 730, 760, 810, 860, 900, 940];
const BAR_COLORS = [
  "#6a2fc9", "#4a3fd6", "#2f5fe0", "#1f9bd6", "#19b86a", "#4cc13a", "#9acb2a", "#e3c41f", "#ee8a1c",
  "#e5471f", "#c4201a", "#8c1f1c", "#6e2a24", "#55302b", "#6f7a74", "#7e8a83", "#8f9a93", "#a0aaa3"
];
const REGIONS = [
  { from: 0, to: 10, label: "Visível", alpha: 0 },
  { from: 11, to: 13, label: "Borda vermelha", alpha: 0.05 },
  { from: 14, to: 17, label: "Infravermelho próximo", alpha: 0.09 }
];

const VERDICTS = {
  immature: { state: "attention", title: "Ainda verde", hint: "Espere amadurecer antes de vender." },
  ripe:     { state: "ok", title: "No ponto ideal", hint: "Boa hora para vender." },
  overripe: { state: "attention", title: "Passou do ponto", hint: "Venda hoje ou aproveite em preparos." },
  spoiled:  { state: "critical", title: "Fruta passada", hint: "Separe do lote e descarte." },
  fault:    { state: "critical", title: "Falha na leitura", hint: "Verifique o sensor do dispositivo." },
  idle:     { state: "idle", title: "Aguardando leitura", hint: "Escolha uma fruta no dispositivo ou use Simular leitura." }
};

const SIM_STAGES = {
  imature:  { mat: 25, stage: "Imaturo / Verde", state: "ATENCAO", brix: 9, chl: 78, moisture: 85, life: 8 },
  ripe:     { mat: 65, stage: "Maduro / Ideal", state: "NORMAL", brix: 14.5, chl: 35, moisture: 75, life: 4 },
  overripe: { mat: 85, stage: "Muito Maduro", state: "ATENCAO", brix: 18, chl: 18, moisture: 68, life: 1.2 },
  spoiled:  { mat: 98, stage: "Passado / Deteriorado", state: "CRITICO", brix: 21, chl: 10, moisture: 55, life: 0 }
};

const $ = (id) => document.getElementById(id);
let spectralChartInstance = null;
let toastTimer = null;

const fmt = (n, d = 1) =>
  Number(n ?? 0).toLocaleString("pt-BR", { minimumFractionDigits: d, maximumFractionDigits: d });

const esc = (s) =>
  String(s ?? "").replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));

function fruitMeta(name) {
  const key = String(name || "").normalize("NFD").replace(/[\u0300-\u036f]/g, "");
  return FRUITS[key] ? { ...FRUITS[key], key } : { ...DEFAULT_FRUIT, label: name || "Fruta", key: "" };
}

function verdictKey(reading) {
  if (reading.state === "FALHA") return "fault";
  const stage = (reading.stage || "").toLowerCase();
  if (stage.includes("imaturo")) return "immature";
  if (stage.includes("muito")) return "overripe";
  if (stage.includes("passado")) return "spoiled";
  if (stage.includes("maduro")) return "ripe";
  return "idle";
}

function stageOf(reading) {
  const k = verdictKey(reading);
  return k === "fault" ? "idle" : k;
}

function stateKey(state) {
  if (state === "NORMAL") return "ok";
  if (state === "ATENCAO") return "attention";
  if (state === "CRITICO" || state === "FALHA") return "critical";
  return "idle";
}

document.addEventListener("DOMContentLoaded", () => {
  $("api-url").textContent = API_BASE_URL;
  initSpectralChart();
  setupEventListeners();
  setRuler(DEFAULT_FRUIT.range, 0);
  fetchHealthAndData();
  setInterval(fetchHealthAndData, POLLING_INTERVAL_MS);
});

const regionsPlugin = {
  id: "regions",
  beforeDatasetsDraw(chart) {
    const { ctx, chartArea, scales: { x } } = chart;
    const half = (x.getPixelForValue(1) - x.getPixelForValue(0)) / 2;
    ctx.save();
    REGIONS.forEach((r) => {
      const left = x.getPixelForValue(r.from) - half;
      const right = x.getPixelForValue(r.to) + half;
      if (r.alpha) {
        ctx.fillStyle = `rgba(22, 37, 28, ${r.alpha})`;
        ctx.fillRect(left, chartArea.top, right - left, chartArea.height);
      }
      ctx.fillStyle = Chart.defaults.color;
      ctx.font = `600 11px ${Chart.defaults.font.family}`;
      ctx.textAlign = "center";
      ctx.textBaseline = "top";
      ctx.fillText(r.label, (left + right) / 2, chartArea.top + 6);
    });
    ctx.restore();
  }
};

function initSpectralChart() {
  const canvas = $("spectralChart");
  if (!canvas) return;

  const css = getComputedStyle(document.documentElement);
  Chart.defaults.font.family = css.getPropertyValue("--font-body").trim();
  Chart.defaults.color = css.getPropertyValue("--ink-3").trim();

  spectralChartInstance = new Chart(canvas, {
    type: "bar",
    data: {
      labels: WAVELENGTHS.map(String),
      datasets: [{ data: new Array(18).fill(0), backgroundColor: BAR_COLORS, borderRadius: 4, borderSkipped: false }]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      animation: { duration: 400 },
      plugins: {
        legend: { display: false },
        tooltip: {
          callbacks: {
            title: (items) => `${items[0].label} nm`,
            label: (ctx) => `Reflectância: ${fmt(ctx.parsed.y)}%`
          }
        }
      },
      scales: {
        y: {
          min: 0,
          max: 100,
          ticks: { stepSize: 25, callback: (v) => `${v}%` },
          grid: { color: css.getPropertyValue("--line").trim() },
          border: { display: false }
        },
        x: {
          grid: { display: false },
          title: { display: true, text: "Comprimento de onda (nm)" }
        }
      }
    },
    plugins: [regionsPlugin]
  });
}

async function fetchHealthAndData() {
  try {
    const healthRes = await fetch(`${API_BASE_URL}/health`, { cache: "no-store" });
    if (healthRes.ok) {
      setBackendStatus(true);
      await fetchLatestReading();
      await fetchHistory();
    } else {
      setBackendStatus(false);
    }
  } catch (error) {
    setBackendStatus(false);
  }
}

async function fetchLatestReading() {
  try {
    const res = await fetch(`${API_BASE_URL}/readings/latest`, { cache: "no-store" });
    if (res.ok) updateDashboardUI(await res.json());
    else if (res.status === 404) updateDashboardUI(null);
  } catch (e) {
    console.warn("Nenhuma leitura recente encontrada.");
  }
}

async function fetchHistory() {
  try {
    const res = await fetch(`${API_BASE_URL}/readings?limit=15`, { cache: "no-store" });
    if (res.ok) renderHistoryTable(await res.json());
  } catch (e) {
    console.error("Erro ao buscar histórico:", e);
  }
}

function setBackendStatus(online) {
  $("backend-status-badge").className = `status-badge ${online ? "online" : "offline"}`;
  $("backend-status-text").textContent = online ? "Backend conectado" : "Backend desconectado";
  $("offline-alert").hidden = online;
}

function setRuler([a, b, c], value) {
  const ruler = $("ruler");
  ruler.style.setProperty("--z1", `${a}%`);
  ruler.style.setProperty("--z2", `${b}%`);
  ruler.style.setProperty("--z3", `${c}%`);
  ruler.style.setProperty("--pos", value);
  ruler.setAttribute("aria-valuenow", Math.round(value));
  $("maturity-pct-text").textContent = `${Math.round(value)}%`;
  $("ruler-ticks").innerHTML = [0, a, b, c, 100]
    .map((v) => `<span style="left:${v}%">${v}</span>`)
    .join("");
}

function updateDashboardUI(reading) {
  const verdict = VERDICTS[reading ? verdictKey(reading) : "idle"];
  const fruit = reading ? fruitMeta(reading.fruit) : DEFAULT_FRUIT;
  const mat = reading ? Math.min(Math.max(parseFloat(reading.maturity_index) || 0, 0), 100) : 0;
  const dash = (value, digits) => (reading ? fmt(value, digits) : "--");

  $("hero").dataset.state = verdict.state;
  document.documentElement.style.setProperty("--fruit", fruit.color);
  $("hero").dataset.stage = reading ? stageOf(reading) : "idle";
  const art = $("fruit-art");
  const artKey = reading && FRUIT_ART[fruit.key] ? fruit.key : "idle";
  if (art.dataset.k !== artKey) { art.innerHTML = FRUIT_ART[artKey]; art.dataset.k = artKey; }
  $("fruit-name").textContent = reading ? fruit.label : "Nenhuma fruta";
  $("device-id-text").textContent = reading?.device_id || "Nenhum dispositivo ativo";
  $("verdict-title").textContent = verdict.title;
  $("verdict-hint").textContent = verdict.hint;

  setRuler(fruit.range, mat);

  $("val-brix").textContent = dash(reading?.soluble_solids_proxy, 1);
  $("val-chlorophyll").textContent = dash(reading?.chlorophyll_proxy, 0);
  $("val-moisture").textContent = dash(reading?.relative_moisture_proxy, 0);
  $("val-shelflife").textContent = dash(reading?.estimated_shelf_life_days, 1);
  $("last-timestamp").textContent = reading?.timestamp
    ? new Date(reading.timestamp).toLocaleTimeString("pt-BR")
    : "--:--:--";

  if (spectralChartInstance) {
    const channels = reading?.spectral_channels;
    spectralChartInstance.data.datasets[0].data =
      Array.isArray(channels) && channels.length === 18 ? channels : new Array(18).fill(0);
    spectralChartInstance.update();
  }
}

function renderHistoryTable(items) {
  const tbody = $("history-table-body");
  if (!tbody) return;

  if (!items || items.length === 0) {
    tbody.innerHTML = `<tr><td colspan="6" class="empty">Nenhuma medição ainda. Envie uma leitura pelo dispositivo ou pelo simulador.</td></tr>`;
    return;
  }

  tbody.innerHTML = items.map((item) => {
    const date = item.timestamp ? new Date(item.timestamp) : null;
    const meta = fruitMeta(item.fruit);
    return `
      <tr>
        <td>
          <span class="time">${date ? date.toLocaleTimeString("pt-BR") : "--:--:--"}</span>
          <span class="date">${date ? date.toLocaleDateString("pt-BR") : "--/--/----"}</span>
        </td>
        <td><span class="fruit-cell"><span class="mini" data-stage="${stageOf(item)}" style="--ripe:${meta.color}">${FRUIT_ART[meta.key] || ""}</span>${esc(meta.label)}</span></td>
        <td>${fmt(item.maturity_index)}%</td>
        <td>${fmt(item.soluble_solids_proxy)} °Bx</td>
        <td>${fmt(item.estimated_shelf_life_days)} dias</td>
        <td><span class="state-cell"><i class="dot" data-state="${stateKey(item.state)}"></i>${esc(item.stage)}</span></td>
      </tr>`;
  }).join("");
}

function showToast(message, kind = "info") {
  const toast = $("toast");
  toast.textContent = message;
  toast.dataset.kind = kind;
  toast.hidden = false;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => { toast.hidden = true; }, 3500);
}

function setupEventListeners() {
  const dialog = $("simulator-panel");

  $("btn-refresh").addEventListener("click", fetchHealthAndData);

  $("btn-toggle-sim").addEventListener("click", () => {
    $("sim-error").hidden = true;
    dialog.showModal();
  });

  $("btn-close-sim").addEventListener("click", () => dialog.close());
  dialog.addEventListener("click", (e) => { if (e.target === dialog) dialog.close(); });

  $("btn-clear-history").addEventListener("click", async () => {
    if (!confirm("Deseja realmente limpar todo o histórico de medições?")) return;
    try {
      const res = await fetch(`${API_BASE_URL}/readings`, { method: "DELETE" });
      if (!res.ok) throw new Error(res.status);
      await fetchHistory();
      showToast("Histórico limpo");
    } catch (e) {
      showToast("Não foi possível limpar o histórico no backend.", "error");
    }
  });

  $("btn-send-sim").addEventListener("click", sendSimulatedReading);
}

async function sendSimulatedReading() {
  const button = $("btn-send-sim");
  const errorBox = $("sim-error");
  const sim = SIM_STAGES[$("sim-mat-range").value];
  const norm = sim.mat / 100;

  const channels = WAVELENGTHS.map((wl) => {
    let val = 20 + (wl - 400) * 0.06;
    if (wl >= 510 && wl <= 560) val += (1 - norm) * 35;
    else if (wl === 680) val += norm * 25 - (1 - norm) * 15;
    else if (wl >= 810 && wl <= 860) val += norm * 30;
    else val += norm * 15;
    return Math.min(Math.max(val, 2), 98);
  });

  const payload = {
    device_id: "SIMULADOR-WEB-01",
    fruit: $("sim-fruit").value,
    maturity_index: sim.mat,
    chlorophyll_proxy: sim.chl,
    soluble_solids_proxy: sim.brix,
    relative_moisture_proxy: sim.moisture,
    estimated_shelf_life_days: sim.life,
    stage: sim.stage,
    state: sim.state,
    spectral_channels: channels
  };

  errorBox.hidden = true;
  button.disabled = true;

  try {
    const res = await fetch(`${API_BASE_URL}/readings`, {
      method: "POST",
      headers: { "Content-Type": "application/json", "x-api-key": API_KEY },
      body: JSON.stringify(payload)
    });

    if (!res.ok) throw new Error(`HTTP ${res.status}`);

    updateDashboardUI(await res.json());
    fetchHistory();
    $("simulator-panel").close();
    showToast("Leitura enviada");
  } catch (err) {
    errorBox.textContent = `Não foi possível enviar a leitura. Verifique se o backend está rodando em ${API_BASE_URL}.`;
    errorBox.hidden = false;
  } finally {
    button.disabled = false;
  }
}