let selectedRoute = null;
let readings = [];
let walkRoute = ["A", "C", "D"];
let currentLampIndex = 0;

const badge = document.getElementById("data-badge");
const canvas = document.getElementById("heatmap-layer");
const context = canvas.getContext("2d");
const lampEnergy = { A: 2.8, B: 3.5, C: 3.2, D: 4.1 };

function heatColor(t) {
    const stops = [[93,116,72], [143,166,109], [188,184,103], [229,200,94], [168,95,75]];
    t = Math.max(0, Math.min(1, t));
    const scaled = t * (stops.length - 1);
    const index = Math.min(stops.length - 2, Math.floor(scaled));
    const fraction = scaled - index;
    return stops[index].map((value, channel) =>
        Math.round(value + (stops[index + 1][channel] - value) * fraction));
}

function drawHeatmap() {
    const rect = canvas.getBoundingClientRect();
    const scale = window.devicePixelRatio || 1;
    const width = Math.round(rect.width * scale);
    const height = Math.round(rect.height * scale);
    if (canvas.width !== width || canvas.height !== height) {
        canvas.width = width;
        canvas.height = height;
    }
    context.clearRect(0, 0, width, height);
    if (!readings.length) return;

    const latest = new Map();
    readings.forEach(reading => latest.set(`${reading.device}:${reading.x}:${reading.y}`, reading));
    const points = [...latest.values()];
    const values = points.map(point => Number(point.value));
    let minimum = Math.min(...values), maximum = Math.max(...values);
    if (maximum === minimum) { minimum -= 1; maximum += 1; }
    const radius = Math.min(width, height) * .38;

    context.globalCompositeOperation = "screen";
    points.forEach(point => {
        const x = Number(point.x) * width, y = Number(point.y) * height;
        const normalized = (Number(point.value) - minimum) / (maximum - minimum);
        const [red, green, blue] = heatColor(normalized);
        const gradient = context.createRadialGradient(x, y, 0, x, y, radius);
        gradient.addColorStop(0, `rgba(${red},${green},${blue},.72)`);
        gradient.addColorStop(.55, `rgba(${red},${green},${blue},.28)`);
        gradient.addColorStop(1, `rgba(${red},${green},${blue},0)`);
        context.fillStyle = gradient;
        context.fillRect(x - radius, y - radius, radius * 2, radius * 2);
    });
    context.globalCompositeOperation = "source-over";
}

async function refreshLamps() {
    const response = await fetch("/streetlamps");
    const lamps = await response.json();
    const hasLiveData = lamps.some(lamp => lamp.live);
    lamps.forEach(lamp => {
        const label = document.querySelector(`.lamp-${lamp.id.toLowerCase()} span`);
        const nodeValue = document.getElementById(`node-${lamp.id.toLowerCase()}-value`);
        if (label) {
            label.textContent = lamp.display_value;
            label.title = lamp.device ? `Live from ${lamp.device}` : "Simulated fallback";
        }
        if (nodeValue) nodeValue.textContent = lamp.display_value;
    });
    if (badge) {
        badge.className = hasLiveData ? "data-badge live" : "data-badge";
        badge.innerHTML = `<span></span>${hasLiveData ? "ESP CONNECTED · LIVE DATA" : "WAITING FOR ESP DATA"}`;
    }

    const liveCount = lamps.filter(lamp => lamp.live).length;
    const freshest = lamps
        .filter(lamp => lamp.last_seen_seconds !== null)
        .sort((a, b) => a.last_seen_seconds - b.last_seen_seconds)[0];
    const linkStatus = document.getElementById("esp-link-status");
    if (linkStatus) {
        if (hasLiveData && freshest) {
            const signal = freshest.wifi_rssi ? ` · ${freshest.wifi_rssi} dBm` : "";
            linkStatus.textContent = `${freshest.device}${signal}`;
        } else if (freshest) {
            linkStatus.textContent = `offline · last seen ${freshest.last_seen_seconds}s ago`;
        } else {
            linkStatus.textContent = "waiting for telemetry";
        }
    }
    const temperatures = lamps.map(lamp => Number(lamp.temperature));
    const coolest = lamps.reduce((best, lamp) => lamp.temperature < best.temperature ? lamp : best, lamps[0]);
    document.getElementById("network-count").textContent = `${liveCount} / ${lamps.length}`;
    document.getElementById("network-bar").style.width = `${liveCount / lamps.length * 100}%`;
    document.getElementById("network-average").textContent =
        (temperatures.reduce((sum, value) => sum + value, 0) / temperatures.length).toFixed(1);
    document.getElementById("coolest-node").textContent = `${coolest.id} · ${coolest.temperature.toFixed(0)}°`;
}

async function loadInitialData() {
    try {
        const response = await fetch("/api/readings?limit=5000");
        const data = await response.json();
        readings = data.readings || [];
        drawHeatmap();
        await refreshLamps();
    } catch (error) {
        if (badge) {
            badge.innerHTML = "<span></span>SERVER UNAVAILABLE";
            badge.className = "data-badge reconnecting";
        }
    }
}

function connectTelemetry() {
    const events = new EventSource("/api/events");
    events.addEventListener("reading", event => {
        readings.push(JSON.parse(event.data));
        if (readings.length > 5000) readings.shift();
        drawHeatmap();
        refreshLamps();
    });
    events.onerror = () => {
        if (badge) {
            badge.innerHTML = "<span></span>RECONNECTING TO SENSORS";
            badge.className = "data-badge reconnecting";
        }
    };
}

document.getElementById("find-route-button").addEventListener("click", async () => {
    const start = document.getElementById("route-start").value;
    const end = document.getElementById("route-end").value;
    const response = await fetch(`/best-route?start=${encodeURIComponent(start)}&end=${encodeURIComponent(end)}`);
    if (!response.ok) return;
    const data = await response.json();
    document.querySelectorAll(".path").forEach(path => path.classList.remove("selected"));
    const routeNodes = data.selected_route.path.split("-");
    for (let index = 0; index < routeNodes.length - 1; index += 1) {
        const edge = [routeNodes[index], routeNodes[index + 1]].sort().join("").toLowerCase();
        document.querySelector(`.path-${edge}`)?.classList.add("selected");
    }

    selectedRoute = data.selected_route;
    walkRoute = selectedRoute.path.split("-");
    currentLampIndex = 0;
    document.getElementById("energy-reward").style.display = "none";
    document.getElementById("user-marker").style.display = "none";
    document.getElementById("recommended-path").textContent = selectedRoute.path.replaceAll("-", " → ");
    document.getElementById("route-distance").textContent = `${selectedRoute.distance} m`;
    document.getElementById("route-temperature").textContent = `${selectedRoute.average_temperature}°F`;
    document.getElementById("route-eta").textContent = `${selectedRoute.eta} min`;
    document.getElementById("estimated-energy").textContent = `${selectedRoute.estimated_energy} kJ`;
    document.getElementById("route-explanation").textContent = data.using_live_temperature
        ? "Fresh from the ESPs: this route follows the coolest live readings."
        : "Less heat, more main-character energy. This is the cooler way.";
    document.getElementById("route-result").style.display = "block";
});

["route-start", "route-end"].forEach(id => {
    document.getElementById(id).addEventListener("change", () => {
        selectedRoute = null;
        document.getElementById("route-result").style.display = "none";
        document.getElementById("energy-reward").style.display = "none";
        document.getElementById("user-marker").style.display = "none";
        document.querySelectorAll(".path").forEach(path => path.classList.remove("selected"));
    });
});

function moveMarker(lampID) {
    const marker = document.getElementById("user-marker");
    const lamp = document.querySelector(`.lamp-${lampID.toLowerCase()}`);
    marker.style.display = "grid";
    marker.style.left = `${lamp.offsetLeft + 15}px`;
    marker.style.top = `${lamp.offsetTop - 30}px`;
}

function showLampReward() {
    const lampID = walkRoute[currentLampIndex];
    const reward = document.getElementById("energy-reward");
    const continueButton = document.getElementById("continue-walk-button");
    document.getElementById("energy-amount").textContent = `${lampEnergy[lampID]} kJ/min`;
    document.getElementById("energy-message").textContent =
        `You made it to shade ${lampID}! This little solar overachiever is making renewable energy.`;
    reward.style.display = "block";
    if (currentLampIndex === walkRoute.length - 1) setTimeout(showWalkSummary, 700);
    else { continueButton.textContent = "continue walking →"; continueButton.disabled = false; }
}

function showWalkSummary() {
    document.getElementById("energy-reward").innerHTML = `
        <p class="summary-label">walk complete</p><h2 class="summary-title">you made it!</h2>
        <div class="summary-energy">${selectedRoute.estimated_energy} kJ</div>
        <p class="summary-energy-label">renewable energy generated during your walk</p>
        <div class="summary-stats"><div><strong>${selectedRoute.distance} m</strong><span>walked</span></div>
        <div><strong>${selectedRoute.eta} min</strong><span>walking time</span></div>
        <div><strong>${selectedRoute.average_temperature}°F</strong><span>average temp</span></div></div>
        <p class="summary-checkpoints">${walkRoute.length} solar checkpoints visited</p>
        <p class="summary-message">you took the cooler way</p>`;
}

document.getElementById("start-walk-button").addEventListener("click", () => {
    if (!selectedRoute) return;
    currentLampIndex = 0; showLampReward(); moveMarker(walkRoute[0]);
});

document.getElementById("continue-walk-button").addEventListener("click", () => {
    if (currentLampIndex < walkRoute.length - 1) {
        currentLampIndex += 1; showLampReward(); moveMarker(walkRoute[currentLampIndex]);
    }
});

new ResizeObserver(drawHeatmap).observe(document.querySelector(".map"));
loadInitialData().then(() => {
    connectTelemetry();
    setInterval(refreshLamps, 2000);
});
