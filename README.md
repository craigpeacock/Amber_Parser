# Amber_Parser
Parser for Amber Electric JSON electricity pricing data found at https://api.amberelectric.com.au/prices/listprices

https://www.amberelectric.com.au/ 
Amber is an Australian Electricity Retailer passing through real-time wholesale pricing. 
This allows IoT enabled devices to switch on and off energy hungry loads based on price signals from the market. For example, I am using it to control charging of an Electric Vehicle.

Example output:

```
JSON File Parser
Amber Electric
Current NEM Time 		2021-03-06 21:16:54
Postcode 			5000
Network Provider 		SA Power

Daily Charges:
Fixed Network Charge		51.24 c/day
Basic Metering Charge		0.00 c/day
Additional Smart Meter Charge	41.56 c/day
Amber Price			32.88 c/day
Total Daily Price		125.67 c/day

Per kWh unit Charges:
Network Charges		15.158 c/kWh
Market Charges			2.678 c/kWh
100 percent Green Power Offset 4.247 c/kWh
Carbon Neutral Offset		0.110 c/kWh
Total 				17.946 c/kWh

Renewables			38 percent
Loss Factor			1.10606

Wholesale Spot Market Price	3.87 c/kWh
Total Price			22.2 c/kWh
All prices include GST

```

Code is written in C and designed to be transferred to embedded MCU such as the ESP32:
https://github.com/craigpeacock/ESP32_DINTimeSwitch

## Dependencies
This code uses the following libraries
* cURL (https://curl.haxx.se/)
* cJSON (https://github.com/DaveGamble/cJSON)

## Install cURL
```sh
$ wget https://curl.se/download/curl-7.74.0.tar.gz
$ tar -xzf curl-7.74.0.tar.gz
$ cd curl-7.74.0.tar.gz
$ make
$ sudo make install
```
## Install cJSON
```sh
$ wget https://github.com/DaveGamble/cJSON/archive/v1.7.14.tar.gz
$ tar -xzf curl-7.74.0.tar.gz
$ cd curl-7.74.0/
$ make
$ sudo make install
```

## Potential errors
### Error loading shared libraries
If you get the following error message 
```
error while loading shared libraries: libcjson.so.1: cannot open shared object file: No such file or directory
```
execute
```
$ ldconfig -v
```
### Unsupported protocol
If you get this message
```
curl_easy_perform() failed: Unsupported protocol
```
It may be because curl hasn't been compiled with SSL. Run
```
$ curl -V
```
and check if https is listed as a protocol.

