import serial
import numpy as np
from scipy.io.wavfile import write
import time

SAMPLE_RATE = 8000
PORT = 'COM7'
BAUD = 125000

START_SEN = b"##START##"
STOP_SEN  = b"##STOP##"

ser = serial.Serial(PORT, BAUD, timeout=0.1)

ser.set_buffer_size(rx_size=65536)

print(f"Opened {PORT} at {BAUD} baud")
print("Waiting for ##START##...")

while True:
    samples = b""
    buffer  = b""
    ser.reset_input_buffer()
    while True:
        buffer += ser.read(256)
        idx = buffer.find(START_SEN)
        if idx != -1:
            buffer = buffer[idx + len(START_SEN):]
            print("Recording started...")
            break

    while True:
        chunk = ser.read(256)
        if chunk:
            buffer += chunk

        idx = buffer.find(STOP_SEN)
        if idx != -1:
            samples += buffer[:idx]
            print("Recording stopped.")
            break
        else:
            safe = len(buffer) - len(STOP_SEN)
            if safe > 0:
                samples += buffer[:safe]
                buffer   = buffer[safe:]
                
    print(f"Samples   : {len(samples)}")

    audio = np.frombuffer(samples, dtype=np.uint8)

    write("open2.wav", SAMPLE_RATE, audio)
    print(f"Saved output_py.wav")
    print("\nWaiting for next ##START##...")