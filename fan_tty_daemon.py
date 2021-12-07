#!/usr/bin/python3

import subprocess
import serial
import daemon
import lockfile
import time

import logging
LOGFILE = "/var/log/fan_tty_daemon.log"
logging.basicConfig(
    filename=LOGFILE,
    format="%(asctime)s %(levelname)-8s %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
    level=logging.DEBUG)

PORT_NAME = "/dev/ttyUSB0"
PORT_BAUD = 115200
def open_serial():
    ser = None
    while ser is None:
        try:
            ser = serial.Serial(PORT_NAME, PORT_BAUD)
        except:
            logging.warning("Unable to establish serial connection; retrying in 10 seconds")
            time.sleep(10)
    logging.info(f"Established serial connection {ser.name}")
    return ser

def main():
    ser = open_serial()

    failsafe = False
    while True:
        try:
            proc = subprocess.run("nvidia-smi --query-gpu=temperature.gpu --format=csv,noheader,nounits", shell=True, check=True, stdout=subprocess.PIPE)
            temperatures = [int(x) for x in proc.stdout.decode().split("\n")[:-1]]
            logging.debug(f"Raw profiled GPU temperatures are f{temperatures}")
            high_temp = min(max(max(temperatures), 10), 100)
            logging.debug(f"Will pass temperature value of {high_temp}")
            if failsafe:
                logging.info("Transient nvidia-smi issue seems to have recovered; exiting failsafe mode")
                failsafe = False
        except:
            if not failsafe:
                logging.warning("nvidia-smi returned an error or unexpected output; entering failsafe mode")
                failsafe = True
            high_temp = 100

        packet = (str(high_temp) + "\n").encode()

        try:
            ser.write(packet)
        except:
            logging.warning("Failed to write to serial port; was the board unplugged?")
            try:
                ser.close()
            except:
                pass
            success = False
            while not success:
                try:
                    open_serial()
                    ser.write(packet)
                    logging.info("Successfully re-established serial communication")
                    success = True
                except:
                    try:
                        ser.close()
                    except:
                        pass
        time.sleep(1)

if __name__ == "__main__":
    with daemon.DaemonContext(pidfile=lockfile.FileLock("/var/run/fan_tty_daemon.pid")):
        main()

