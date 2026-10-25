let selectedRoute = null;
let readings = [];
let walkRoute = ["A", "C", "D"];
let currentLampIndex = 0;

const badge = document.getElementById("data-badge");
const canvas = document.getElementById("heatmap-layer");
const context = canvas.getContext("2d");
const lampEnergy = { A: 2.8, B: 3.5, C: 3.2, D: 4.1 };

function heatColor(t) {
    const stops = [[38,59,250], [0,202,220], [100,236,133], [255,230,93], [255,55,55]];
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
        if (label) {
            label.textContent = lamp.display_value;
            label.title = lamp.device ? `Live from ${lamp.device}` : "Simulated fallback";
        }
    });
    badge.className = hasLiveData ? "data-badge live" : "data-badge";
    badge.textContent = hasLiveData ? "● live ESP data" : "● simulated data";
}

async function loadInitialData() {
    try {
        const response = await fetch("/api/readings?limit=5000");
        const data = await response.json();
        readings = data.readings || [];
        drawHeatmap();
        await refreshLamps();
    } catch (error) {
        badge.textContent = "● server unavailable";
        badge.className = "data-badge reconnecting";
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
        badge.textContent = "● reconnecting to sensors";
        badge.className = "data-badge reconnecting";
    };
}

document.getElementById("find-route-button").addEventListener("click", async () => {
    const response = await fetch("/best-route");
    const data = await response.json();
    document.querySelectorAll(".path").forEach(path => path.classList.remove("selected"));
    if (data.recommended_route === "A-C-D") {
        document.querySelector(".path-ac").classList.add("selected");
        document.querySelector(".path-cd").classList.add("selected");
    } else {
        document.querySelector(".path-ab").classList.add("selected");
        document.querySelector(".path-bd").classList.add("selected");
    }

    selectedRoute = data.recommended_route === "A-C-D" ? data.route_2 : data.route_1;
    walkRoute = selectedRoute.path.split("-");
    document.getElementById("recommended-path").textContent = selectedRoute.path.replaceAll("-", " → ");
    document.getElementById("route-distance").textContent = `${selectedRoute.distance} m`;
    document.getElementById("route-temperature").textContent = `${selectedRoute.average_temperature}°F`;
    document.getElementById("route-eta").textContent = `${selectedRoute.eta} min`;
    document.getElementById("estimated-energy").textContent = `${selectedRoute.estimated_energy} kJ`;
    document.getElementById("route-explanation").textContent = data.using_live_temperature
        ? "Route score includes live ESP temperature readings."
        : "Using simulated temperatures until numerical ESP readings arrive.";
    document.getElementById("route-result").style.display = "block";
});

function moveMarker(lampID) {
    const marker = document.getElementById("user-marker");
    const lamp = document.querySelector(`.lamp-${lampID.toLowerCase()}`);
    marker.style.display = "block";
    marker.style.left = `${lamp.offsetLeft + 15}px`;
    marker.style.top = `${lamp.offsetTop - 30}px`;
}

function showLampReward() {
    const lampID = walkRoute[currentLampIndex];
    const reward = document.getElementById("energy-reward");
    const continueButton = document.getElementById("continue-walk-button");
    document.getElementById("energy-amount").textContent = `${lampEnergy[lampID]} kJ/min`;
    document.getElementById("energy-message").textContent =
        `You reached Lamp ${lampID}! This solar panel is currently generating renewable energy.`;
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
loadInitialData().then(connectTelemetry);
