# Buck Converter Control Example
This project demonstrates how to control a buck converter using an ESP32 microcontroller. The example includes code for adjusting the output voltage of the buck converter and monitoring its performance.

## How to use this example
### Hardware Setup
1. Connect the buck converter to the ESP32 as follows:
    - **Input Voltage (Vin):** Connect to a DC power supply.
    - **Output Voltage (Vout):** Connect to a load or measurement device.
    - **Control Pin:** Connect to the GPIO pin specified in the code.

|GPIO pin | Function |
|:----:|:-----:|
| GPIO18 | PWM Signal |
| GPIO4 | ADC reading |