# Bathroom Fan Monitor

An ESP32 reads DHT22 humidity every 10 seconds and posts it to FastAPI.
MongoDB stores the readings. MQTT delivers fan settings to the board.
The LED represents the fan and supports auto, on and off modes.

## Run the API
On the course VM, enter the api folder. Run python3 -m venv venv,
then source venv/bin/activate and python -m pip install -r requirements.txt.
Create .env in that folder with MONGODB_URI and MQTT_NAME.
Stop the tutorial service with sudo systemctl stop iot-api, then run
uvicorn app:app --host 127.0.0.1 --port 8000. Open /docs on the VM address.

## Run the board
Open board in VS Code with PlatformIO and Wokwi. Create include/secrets.h
with #define DEVICE_KEY "" for the first build. Build and start Wokwi.
Copy the serial MAC, register it with POST /fans, and put the returned key
between the quotes in secrets.h. Rebuild and restart the simulator.
Set the mode and humidity limit using PUT /fans/{mac}/settings.
In auto mode the fan turns on when humidity exceeds the limit.
