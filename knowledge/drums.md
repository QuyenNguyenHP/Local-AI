# DRUMS Knowledge

## System Name
DRUMS = Daikai Remote Ubiquitous Monitoring System

## Purpose
DRUMS is a remote monitoring system for diesel engines and onboard machinery.

It collects operational data such as:
- Temperature
- Pressure
- Vibration
- Engine speed
- Load
- Running hours
- Alarm and status signals

## Basic Architecture

Sensors / Engine ECU / AMS
↓
DRUMS CPU
↓
Compression and encryption
↓
Internet / vessel network
↓
DRUMS Server
↓
Dashboard / alarm / analysis

## DRUMS CPU
Typical platform:
- Raspberry Pi
- Debian Linux

Main tasks:
- Collect data from ECU / AMS / gateways
- Parse industrial protocols
- Store temporary data
- Compress collected data
- Encrypt data
- Send data to cloud server

## Typical Data Volume
Example configuration:
- 112 data points
- 56 points per engine
- Sampling interval: 2 seconds
- Raw CSV sample: approximately 11.2 KB
- Raw data: approximately 24 MB per hour
- Compressed and encrypted data: approximately 1 MB per hour

## Daihatsu ECU Communication
Known configuration:
- Interface: RS-422
- Baud rate: 9600
- Data bits: 8
- Parity: None
- Stop bits: 2
- Slave ID: 16

Gateway example:
- Advantech EKI-1221
- RS-422 to Modbus TCP

## Example Discrete Inputs
- LO filter differential pressure high
- Control air pressure low
- Turbocharger LO pressure low
- Fuel oil leak tank level high
- LO sump level low
- Engine run
- Ready to start
- Overspeed
- HT cooling water outlet temperature high stop
- LO inlet pressure low stop

## Example Input Registers
- LO temperature engine inlet: 30003
- HT cooling water outlet temperature: 30004

## Database
Known schema:
- drums01

Known tables / vessels:
- dubai_world
- rose
- queenie
- NDY1271

Known view:
- NDY1271_v1

Common fields:
- IMO_No
- SerialNo
- ChannelNo
- ChannelDescription
- TimeStamp
- TimeStampOriginal
- Value
- Unit
- FileName

## GCP Gateway Test
Known test configuration:
- Modbus TCP IP: 192.168.100.20
- Port: 502
- Slave ID: 20
- Digital inputs: 0-1999
- Holding/Input registers: 0-999
- 32-bit analog word order requires confirmation when integrating

## Voltage and Frequency Reference
For a 380 V / 50 Hz system:

Voltage:
- Normal ±5%: 361-399 V
- Alarm ±10%: 342-418 V
- Typical low warning: 360 V
- Typical low shutdown: 340 V
- Typical high warning: 400 V
- Typical high shutdown: 420 V

Frequency:
- Normal ±2.5%: 48.75-51.25 Hz
- Alarm ±5%: 47.5-52.5 Hz
- Typical low warning: 49 Hz
- Typical low shutdown: 47.5 Hz
- Typical high warning: 51 Hz
- Typical high shutdown: 52.5 Hz

## Important Integration Rule
When troubleshooting DRUMS communication:
1. Verify physical wiring
2. Verify RS-422 / RS-485 polarity
3. Verify baud rate and framing
4. Verify slave ID
5. Verify gateway IP and port
6. Test network connectivity
7. Test Modbus registers manually
8. Compare values with AMS / ECU display
