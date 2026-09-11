---
id: project.drums.configurations
type: project-configuration
status: active
updated: 2026-09-11
confidence: confirmed
tags: [drums, modbus, daihatsu, gateway]
---

# DRUMS Project Configurations

## Daihatsu ECU Communication

The known DRUMS configuration for a Daihatsu ECU is:

- Interface: RS-422
- Baud rate: 9600
- Data bits: 8
- Parity: None
- Stop bits: 2
- Slave ID: 16
- Gateway example: Advantech EKI-1221, RS-422 to Modbus TCP

## DRUMS Example Discrete Inputs

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

## DRUMS Example Input Registers

- LO temperature engine inlet: 30003
- HT cooling water outlet temperature: 30004

## GCP Gateway Test

The known DRUMS GCP gateway test configuration is:

- Modbus TCP IP: 192.168.100.20
- Port: 502
- Slave ID: 20
- Digital inputs: 0-1999
- Holding/Input registers: 0-999
- The 32-bit analog word order requires confirmation during integration.

## Voltage and Frequency Reference

For a DRUMS 380 V / 50 Hz system:

- Voltage normal ±5%: 361-399 V
- Voltage alarm ±10%: 342-418 V
- Typical voltage warning limits: 360 V low and 400 V high
- Typical voltage shutdown limits: 340 V low and 420 V high
- Frequency normal ±2.5%: 48.75-51.25 Hz
- Frequency alarm ±5%: 47.5-52.5 Hz
- Typical frequency warning limits: 49 Hz low and 51 Hz high
- Typical frequency shutdown limits: 47.5 Hz low and 52.5 Hz high
