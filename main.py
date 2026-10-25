from fastapi import FastAPI
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles

app = FastAPI()
app.mount("/static", StaticFiles(directory="."), name="static")

#linking python algorithm to webpage
@app.get("/")
def home():
    return FileResponse("index.html")

#fake streetlamp temperature data
streetlamps = [
    {
        "id": "A",
        "temperature": 78,
        "solar_energy_per_min": 2.8
    },
    {
        "id": "B",
        "temperature": 82,
        "solar_energy_per_min": 3.5
    },
    {
        "id": "C",
        "temperature": 76,
        "solar_energy_per_min": 3.2
    },
    {
        "id": "D",
        "temperature": 88,
        "solar_energy_per_min": 4.1
    }
]

#streetlamp-to-streetlamp path types
paths = [
    {
        "start": "A",
        "end": "B",
        "distance": 100
    },
    {
        "start": "A",
        "end": "C",
        "distance": 140
    },
    {
        "start": "B",
        "end": "D",
        "distance": 120
    },
    {
        "start": "C",
        "end": "D",
        "distance": 100
    }
]

#see temperature data
@app.get("/streetlamps")
def get_streetlamps():
    return streetlamps

#see path data
@app.get("/paths")
def get_paths():
    return paths

#idea to continue: for temperature, will calculate the average T of the trip. path just calculates culmulative distance
# score: distance + 20(avg T - 75)
# ideal baseline Temp= 75 deg
# willing to trade off 20 meters for one degree less in temperature!
@app.get("/best-route")
def get_best_route():

    walking_speed = 1.4  # meters per second

    route_1_distance = 220
    route_1_temperature = (78 + 82 + 88) / 3

    route_2_distance = 240
    route_2_temperature = (78 + 76 + 88) / 3

    route_1_eta = route_1_distance / walking_speed / 60
    route_2_eta = route_2_distance / walking_speed / 60

    route_1_score = route_1_distance + 20 * (route_1_temperature - 75)
    route_2_score = route_2_distance + 20 * (route_2_temperature - 75)

    route_1_energy_rate = 2.8 + 3.5 + 4.1
    route_2_energy_rate = 2.8 + 3.2 + 4.1

    route_1_estimated_energy = route_1_energy_rate * route_1_eta
    route_2_estimated_energy = route_2_energy_rate * route_2_eta

    if route_1_score < route_2_score:
        best_route = "A-B-D"
    else:
        best_route = "A-C-D"

    return {
        "route_1": {
            "path": "A-B-D",
            "distance": route_1_distance,
            "average_temperature": round(route_1_temperature, 1),
            "score": round(route_1_score, 1),
            "eta": round(route_1_eta, 1),
            "estimated_energy": round(route_1_estimated_energy, 1)
        },
        "route_2": {
            "path": "A-C-D",
            "distance": route_2_distance,
            "average_temperature": round(route_2_temperature, 1),
            "score": round(route_2_score, 1),
            "eta": round(route_2_eta, 1),
            "estimated_energy": round(route_2_estimated_energy, 1)
        },
        "recommended_route": best_route
    }