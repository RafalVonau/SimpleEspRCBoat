# Simple ESP8266 based RC boat.
See this boat in action at [https://www.youtube.com/watch?v=-y2VSufjR1s](https://www.youtube.com/watch?v=-y2VSufjR1s).

This is my very old weekend project for my kid. The first version could only swim forward. The second one because of H-bridge mounted could swim both forward and backward. 
The design is based on two motors on the sides of the boat controlled by PWM signal from application on Adnroid system or specially constructed joystick. 
In my opinion joystick steering is more pleasant than from a phone.

The ESP8266 in the boat creates an access point and the phone or joystick connects to this AP and then controls it by sending UDP packets.

Nowadays I would build this system on ESP-NOW, which would result in faster connection with the boat. Maybe in the future it will update these sources to ESP-NOW.

# Building ESP8266 code
See [https://github.com/pfalcon/esp-open-sdk](https://github.com/pfalcon/esp-open-sdk)

# Parts needed for boat
* ESP8266
* [DIY Solar boat](https://aliexpress.com/item/4000259998999.html?spm=a2g0o.productlist.0.0.3d22160b95i6em&algo_pvid=ad6c91c5-80a5-40c0-8528-fc2ce496e7c1&algo_expid=ad6c91c5-80a5-40c0-8528-fc2ce496e7c1-20&btsid=0b0a187915935477344432896e3529&ws_ab_test=searchweb0_0,searchweb201602_,searchweb201603_)
* 2 FET transistors for version 1 or small size H bridge like DRV8833
* 18650 cell
* battery charger like TP4056
* box for electronics

# Boat ESP8266 connecions
    LED                - GPIO2
    H-Bridge PWMA      - GPIO4
    H-Bridge PWMB      - GPIO13
    H-Bridge AIN1/BIN1 - GPIO5
    H-Bridge AIN2/BIN2 - GPIO12
    H-Bridge STBY      - GPIO14

# Parts needed for joystick
* ESP8266
* old phone battery cell
* battery charger like TP4056
* two analoge joysticks 
* CD4051 chip as analog channel multiplexer (ESP8266 has only one ADC channel)
* reference voltage of 1V or less
* resistors for voltage dividers

# Joystick ESP8266 connections
    LED             - GPIO2,
    CD4051 MUXA     - GPIO5,
    CD4051 MUXB     - GPIO4,
    CD4051 MUXC     - GPIO15,
    BAT_LED1        - GPIO16,
    BAT_LED2        - GPIO14,
    BUTTON0         - GPIO12,
    BUTTON1         - GPIO13,

Analog multilpexer CD4051 inputs:

    Channel 0 - X1,
    Channel 1 - Unused,
    Channel 2 - Unused,
    Channel 3 - Y,
    Channel 4 - Y1,
    Channel 5 - Reference for potentiometers (reference voltage of 1V or less).
    Channel 6 - X,
    Channel 7 - Battery (over voltage divider, R1 = 20k, R2 = 5k)
   
X, X1, Y , Y1 - joystick potentiometer middle point.

   
