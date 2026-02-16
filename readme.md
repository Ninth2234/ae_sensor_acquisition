# AE Sensor Data Logger (RP2350)

## Overview

This project implements an **Acoustic Emission (AE) sensor acquisition system** running on an RP2350-based microcontroller platform.
The system samples analog signals using the **internal ADC** at **2 kHz**, buffers the data, and records it to an SD card for later analysis.

A compact graphical user interface is provided using the **MKS MINI12864 display module**, allowing standalone operation without a connected PC.

The goal of the project is to create a lightweight, reproducible embedded data logger suitable for laboratory experiments, condition monitoring, and prototyping of AE-based sensing workflows.

---

## Hardware Platform

* **MCU Board:** Maker Pi RP2350 by Cytron
* **Display / UI:** MKS MINI12864 (by Makerbase)
* **Storage:** SPI SD Card (FAT filesystem)
* **Signal Source:** AE sensor → internal ADC

---

## System Architecture

```
System Architecture
AE Sensor
   ↓
Analog Conditioning
   ↓
RP2350 Internal ADC (2 kHz Sampling)
   ↓
DMA Transfer
   ↓
Ping-Pong Buffer (Double Buffering in RAM)
   ├─ Buffer A → being filled by DMA
   └─ Buffer B → being written to SD card
          (buffers swap automatically)
   ↓
SPI Interface
   ↓
SD Card (FAT Filesystem Logging)AE Sensor

---
