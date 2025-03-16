# Whoop at Home Monitor

This project collects Whoop data using the developer Whoop API and displays information on several simple peripheral devices. The project extends Whoop's "no frills" design by keeping information simple non-distracting.

## Devices
 - ESP8266 Dev Board
 - [Sunfounder touch switch built on TTP223-BA6 IC](https://www.sunfounder.com/products/touch-switch)
 - [Sunfounder PWM RGB LED](https://www.sunfounder.com/products/rgb-led-module)
 - [SPI enabled LCD Display](https://www.sunfounder.com/collections/lcd-display/products/i2c-lcd1602-module)

 ## Software
 - [ESP8266_RTOS_SDK](https://github.com/espressif/ESP8266_RTOS_SDK)

 ## Build Steps
 1. Download ESP8266_RTOS_SDK and lx106 toolchain from ESPRESSIF.
 2. Run **make menuconfig** and configure wifi settings and Whoop client ID and Secret.
 3. **make app-flash** to build and flash software

 ## Description
 During operation the ESP8266 will attempt to retrieve a User's Whoop Data on a five minute interval. The user can cycle data selection by pressing the capacitance touch button. An RGB LED will give an indication of score, while the LCD will display the selected data metric and its value.

 ## Example
![Example Dev](media/dev_example.gif)

Excuse the poor quality of the video. The LCD dispaly does not show up well, but this should give a general example of how the prototype works.

## Notes 
- The authentication process for OAUTH2.0 requires the user to manually redirect the access code... this could improved by having the redirect uri link directly to the ESP8266. This will require the ESP8266 to enable SSL verification with Whoop's server.
- Network communication can enter a state where the TLS socket times out and is not restarted. This will need to be improved for reliability.
- At the moment the device does not attempt to refresh its token until it recieves a 401 response. This could be configured to refresh its token prior to timeout.
- Right now the data point to display is hard coded, but another button could be implement to allow the user to cycle data points within a Whoop category.