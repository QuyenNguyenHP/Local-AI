---
id: project.drums.overview
type: project
status: active
updated: 2026-09-11
confidence: confirmed
tags: [drums, marine-automation, modbus, monitoring]
---

# DRUMS Project Overview

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

