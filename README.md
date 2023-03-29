# esp32-smartBMSdisplay
ESP32 version of JBD BMS display with Bluetooth connection

This is a BMS monitor for JBD and Overkill Solar BMS. It supports bluetooth connection to the BMS, and can alarm prior to BMS disconnect per ABYC recommendation. It is inspired by a project at https://github.com/vagueDirector/ArduinoXiaoxiangSmartBMSDisplay, but bears little resemblance at this point.

This project uses the Overkill Solar library for accessing the BMS, and modifiyed code from https://github.com/avinabmalla/ESP32_BleSerial

I used an ESP32-WROOM. Others might work also. BLE enabled Arduinos will not work. The ESP32 will connect to the first BMS it finds via bluetooth. Button one cycles through screens, and button two silences the alarm if it is triggered.

To use the alarm requires some changes in the BMS programming. There is a parameter for the delay in seconds before the cutoff occurs. Normally this is set very small, to 1 or 2 seconds. This value is how long of a warning you will get before the cutoff occurs. Some versions of the app do not allow this to be set greater than 10 seconds, but using the Overkill Solar app allows higher values.

Care should be taken when selecting the delay value. If an overvoltage or undervoltage situation occurs, there should be no issue waiting 20 or 30 seconds. If an overcurrent situation occurs, you really should not wait to disconnect to allow for an alarm, regardless of what ABYC says. So, I suggest leaving the delay for overcurrent very low, and changing the delay for overvoltage to 20 or 30 seconds.

ESP32 WROOM - https://www.amazon.com/gp/product/B08MQDS6WV/ref=ppx_yo_dt_b_search_asin_title
128x64 OLED screen - https://www.amazon.com/gp/product/B08BRCBKNR/ref=ppx_yo_dt_b_asin_title_o01_s00
Button - https://www.digikey.com/en/products/detail/c-k/KSA0A531-LFTR/5975610?s=N4IgTCBcDaINIGUCCAGJBWAzARgDIDEAVAJQFoA5AERAF0BfIA
Button cap - https://www.digikey.com/en/products/detail/c-k/BTNK0120/498768?s=N4IgTCBcDaICwAYCMBaJYDMBOFA5AIiALoC%2BQA
Buzzer - https://www.amazon.com/gp/product/B07TRK5M2T/ref=ppx_yo_dt_b_search_asin_title
