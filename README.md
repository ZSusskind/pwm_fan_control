# Fan Control
This repository contains two files useful for implementing a GPU fan controller using an Arduino Nano: `fan_control.ino`, the Arduino program, and `fan_tty_daemon.py`, a program which can be run as a daemon process to send updates to the Arduino via UART.

## Motivation
Consumer GPUs have integrated coolers that make the management of fan speeds a non-issue. However, NVIDIA's Tesla-class datacenter GPUs rely on external cooling.
In a traditional supercomputing environment, where noise is not a primary consideration, it may be sufficient to just attach an external fan and allow it to run at max speed.
However, when building my home HPC server, I found this obnoxiously loud, and was determined to implement a solution to slow down or stop the fans when the GPU was lightly loaded.

## Overview of Functionality
There are essentially three major components to this solution.

The first is the daemon, written in Python, which periodically polls the GPU temperatures using the `nvidia-smi` utility, takes the maximum value among all GPUs, and sends this result out via a USB configured as a serial port (default `/dev/ttyUSB0`).
This daemon should be run as root on startup, which is most easily handled by modifying the root crontab (`sudo crontab -e`):
```
@reboot         /usr/local/bin/fan_tty_daemon.py
```

The second component is the Arduino source code, designed for use on an Arduino Nano or compatible third-party board.
This code uses `PD5` on the board as a PWM with a 25 kHz frequency and variable duty cycle.
If no new data is received over the USB serial connection for about 3 seconds, it defaults to an "alarm" mode, in which the output is set to max (duty cycle = 100%).
The on-board LED is also toggled each time a new message is received.

The final component is the PWM controller itself. In theory, any four-pin fan should have a PWM implemented on the fourth pin.
In practice, this is sometimes counterfeited in the name of cutting costs - for instance, the fans I have "implement" the PWM as a 1kΩ resistor to ground.
In this case, it is necessary to implement a custom PWM.
I used a FQP30N06L nMOS MOSFET for this purpose, with the gate connected to the Arduino, the drain connected to the negative line on the fan, and the source connected to ground.
I used a 220Ω resistor between the Arduino and the transistor, which limits the current draw to about 23 mA; this is necessary due to the capacitance of the transistor's gate.
Note that the current limiting resistor value does not have to be precise, but must not be less than 125Ω so as to not exceed the absolute maximum of 40 mA current draw.
I also put in a 56kΩ pull-up resistor between the gate and a 5V source (equivalent to the Arduino logic voltage); I used the 5V line off of a 4-pin MOLEX connector for this.
Again, the resistor value is not precise - anything between 10kΩ and 100kΩ should work just fine.
