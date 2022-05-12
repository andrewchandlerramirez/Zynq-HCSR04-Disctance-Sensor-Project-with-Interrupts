# Zynq-HCSR04-Disctance-Sensor-Project-with-Interrupts


The basic system is an led counter using buttons for control. 
The leds will count every second. From left to right, the first button on the board will reset the count to zero and continue counting, 
the second button will reset the count to the value show on the switches and set the counting mode up, 
the third button  will reset the count to the value show on the switches and set the counting mode down, 
and the fourth button will toggle the timer that counts on the leds. 
This needs the Zynq PS, an axi timer, gpio switches, gpio buttons, gpio leds, and a concat block. 

The advanced feature of the system will be the addition of an HCSR04 distance sensor that will be connected to the system through the JC PMOD port. 
A custom hcsr04 ip block will be used to control the output and read the input from the pmod port. 
The software will use the trigger and echo pins as well as an axi timer to estimate distance from the sensor. 
The distance will be displayed on the sdk terminal.
The HCSR04 will be connected to a breadboard and powered by a 7805. The 7805 will be powered by a 9v battery pack. 
The only connections between the distance sensor and the Zybo Z7 10 board will be ground, trigger, and an indirect connection with the echo pin of the HCSR04.

I quickly put together a lab manual that goes into what I consider extreme detail about how to create most everything in the project. This will be included in the repo. 
There may be somethings that someone who decided to create this project needs to know. You need to know how to create a block diagram using the Zynq system
and gpio blocks. You may not completely need to know how to create custom axi light peripherals but it would help. Understanding how breadboards work is necessary 
but easy to look up. Knowing how the sensor works is necessary but easy to look up as well.

This is project was created using verilog in Vivado. C was used for the software portions of this project. This was project was created using Vivado 2018.3
so the Vivado SDK was used for creating the software application.

The project Overview pdf includes some extra information

