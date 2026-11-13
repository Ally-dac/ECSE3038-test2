import os
import secrets
from fastapi import HTTPException
from pymongo.errors import DuplicateKeyError

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