let selectedRoute = null;
const button = document.getElementById("find-route-button");

button.addEventListener("click", async function() {

    // Ask the Python backend for the best route
    const response = await fetch("/best-route");

    // Convert the response into JavaScript data
    const data = await response.json();

    // Remove any old route highlighting
    document.querySelectorAll(".path").forEach(function(path) {
        path.classList.remove("selected");
    });

    // Look at the route Python recommended
    if (data.recommended_route === "A-C-D") {
        document.querySelector(".path-ac").classList.add("selected");
        document.querySelector(".path-cd").classList.add("selected");
    }

    if (data.recommended_route === "A-B-D") {
        document.querySelector(".path-ab").classList.add("selected");
        document.querySelector(".path-bd").classList.add("selected");
    }

    const winningRoute =
    data.recommended_route === "A-C-D"
        ? data.route_2
        : data.route_1;
    selectedRoute = winningRoute;

document.getElementById("recommended-path").textContent =
    winningRoute.path.replaceAll("-", " → ");

document.getElementById("route-distance").textContent =
    winningRoute.distance + " m";

document.getElementById("route-temperature").textContent =
    winningRoute.average_temperature + "°F";

document.getElementById("route-eta").textContent =
    winningRoute.eta + " min";

document.getElementById("estimated-energy").textContent =
    winningRoute.estimated_energy + " kJ";

document.getElementById("route-result").style.display = "block";

});

const lamps = {
    A: {
        energy: 2.8
    },
    C: {
        energy: 3.2
    },
    D: {
        energy: 4.1
    }
};

const walkRoute = ["A", "C", "D"];


let currentLampIndex = 0;

const startWalkButton = document.getElementById("start-walk-button");

startWalkButton.addEventListener("click", function() {

    currentLampIndex = 0;

    showLampReward();
    moveMarker("A");

});

function showLampReward() {

    const lampID = walkRoute[currentLampIndex];
    const lamp = lamps[lampID];

    const reward = document.getElementById("energy-reward");
    const continueButton = document.getElementById("continue-walk-button");

    document.getElementById("energy-amount").textContent =
        lamp.energy + " kJ/min";

    document.getElementById("energy-message").textContent =
        "You reached Lamp " + lampID +
        "! This solar panel is currently generating renewable energy.";

    reward.style.display = "block";

    if (lampID === "D") {

    setTimeout(function() {
        showWalkSummary();
    }, 700);

    } else {

    continueButton.textContent = "continue walking →";
    continueButton.disabled = false;

    }
}

function showWalkSummary() {

    const reward = document.getElementById("energy-reward");

    reward.innerHTML = `
        <p class="summary-label">walk complete</p>

        <h2 class="summary-title">you made it!</h2>

        <div class="summary-energy">
            ${selectedRoute.estimated_energy} kJ
        </div>

        <p class="summary-energy-label">
            renewable energy generated during your walk
        </p>

        <div class="summary-stats">
            <div>
                <strong>${selectedRoute.distance} m</strong>
                <span>walked</span>
            </div>

            <div>
                <strong>${selectedRoute.eta} min</strong>
                <span>walking time</span>
            </div>

            <div>
                <strong>${selectedRoute.average_temperature}°F</strong>
                <span>average temp</span>
            </div>
        </div>

        <p class="summary-checkpoints">
            3 solar checkpoints visited
        </p>

        <p class="summary-message">
            you took the cooler way
        </p>
    `;
}

function moveMarker(lampID) {

    const marker = document.getElementById("user-marker");

    const lamp = document.querySelector(
        ".lamp-" + lampID.toLowerCase()
    );

    marker.style.display = "block";

    marker.style.left =
        lamp.offsetLeft + 15 + "px";

    marker.style.top =
        lamp.offsetTop - 30 + "px";
}

const continueWalkButton =
    document.getElementById("continue-walk-button");

continueWalkButton.addEventListener("click", function() {

    if (currentLampIndex < walkRoute.length - 1) {

        currentLampIndex++;

        const currentLamp = walkRoute[currentLampIndex];

        showLampReward();
        moveMarker(currentLamp);
        

    }

});