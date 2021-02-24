#!/usr/bin/python3

import subprocess
import serial
import daemon
import lockfile
import time

def main():
    ser = serial.Serial("/dev/ttyUSB0", 115200)
    print(ser.name)

    while True:
        proc = subprocess.run("nvidia-smi --query-gpu=temperature.gpu --format=csv,noheader,nounits", shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        temperatures = [int(x) for x in proc.stdout.decode().split("\n")[:-1]]
        print(temperatures)
        high_temp = min(max(max(temperatures), 10), 100)
        print(high_temp)

        packet = (str(high_temp) + "\n").encode()

        ser.write(packet)
        time.sleep(1)

if __name__ == "__main__":
    with daemon.DaemonContext(pidfile=lockfile.FileLock("/var/run/fan_tty_daemon.pid")):
        main()

