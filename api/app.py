import os
import secrets
from fastapi import HTTPException
from pymongo.errors import DuplicateKeyError
from datetime import datetime, timezone
from fastapi import Header
from pydantic import Field
from fastapi import Query

from dotenv import load_dotenv
from fastapi import FastAPI
from pydantic import BaseModel
from pymongo import MongoClient

load_dotenv()

app = FastAPI(title="Bathroom Fan API")

client = MongoClient(os.environ["MONGODB_URI"])
db = client["ecse3038_test2"]
fans = db["fans"]
fans.create_index("mac", unique=True)


class Fan(BaseModel):
    mac: str
    room: str
@app.post("/fans", status_code=201)
def register_fan(fan: Fan):
    key = secrets.token_urlsafe(16)

    try:
        fans.insert_one({"mac": fan.mac, "room": fan.room, "key": key})
    except DuplicateKeyError:
        raise HTTPException(status_code=409, detail="MAC already registered")

    return {"mac": fan.mac, "room": fan.room, "key": key}    
@app.get("/fans")
def list_fans():
    return list(fans.find({}, {"_id": 0, "mac": 1, "room": 1}))


@app.get("/fans/{mac}")
def get_fan(mac: str):
    fan = fans.find_one({"mac": mac}, {"_id": 0, "mac": 1, "room": 1})
    if fan is None:
        raise HTTPException(status_code=404, detail="Fan not found")
    return fan
class Reading(BaseModel):
    mac: str
    humidity: float = Field(ge=0, le=100)


@app.post("/readings", status_code=201)
def create_reading(reading: Reading, x_api_key: str | None = Header(default=None)):
    if not x_api_key:
        raise HTTPException(status_code=401, detail="Missing API key")

    fan = fans.find_one({"key": x_api_key})
    if fan is None:
        raise HTTPException(status_code=401, detail="Unknown API key")

    if fan["mac"] != reading.mac:
        raise HTTPException(status_code=403, detail="Key belongs to another fan")

    record = {
        "mac": reading.mac,
        "humidity": reading.humidity,
        "time": datetime.now(timezone.utc),
    }
    db["readings"].insert_one(record.copy())
    return record
@app.get("/readings")
def get_readings(
    mac: str | None = None,
    limit: int = Query(default=10, ge=1, le=100),
):
    query = {}
    if mac is not None:
        query["mac"] = mac

    readings = db["readings"].find(query, {"_id": 0})
    return list(readings.sort("time", -1).limit(limit))