# Reusable Environmental Control Platform

Welcome to the documentation of the **Reusable Environmental Control Platform**,  
a long-term, profile-driven embedded system built using **ESP32 + FreeRTOS (Arduino IDE)**.

This project is designed as a **platform**, not a single-use product.

---

## 🚀 What Is This Project?

This platform is a **modular environmental controller** capable of managing:

- 🌡️ Temperature (heating & cooling)
- 💧 Humidity
- 🚰 Water level
- ⏱️ Timers & scheduled actions
- 🚨 Alarms & safety handling

Using **profiles**, the same firmware can function as:

- Thermostat  
- Egg Incubator  
- Air Cooler  
- Environmental Test Chamber  

> **Build once. Reuse forever.**

---

## 🧠 Design Philosophy

This project is built with **long-term thinking** and follows a **system-first approach**:

- ✅ FreeRTOS-based task architecture  
- ✅ Finite State Machines (FSM) for control logic  
- ✅ Hardware Abstraction Layer (HAL) for sensors & actuators  
- ✅ Profile-driven behavior (no logic rewrite)  
- ✅ Nokia-style OLED UI for reliability and simplicity  

The goal is **clarity, safety, and reusability**, not quick hacks.

---

## 🏗️ High-Level Architecture

Sensors → Sensor HAL → Control FSM → Actuator HAL → Outputs
↑
Profiles
↑
UI FSM ← Encoder + OLED

- **Sensors** are isolated from logic  
- **Control logic** is hardware-independent  
- **UI** never touches actuators directly  
- **Profiles** decide behavior  

---

## 🧩 Key Features

- ESP32 with built-in FreeRTOS
- DS18B20 temperature sensing
- DHT22 humidity sensing
- Relay-based actuator control
- Rotary encoder input
- OLED text-based (Nokia-style) UI
- Alarm & logging system
- Expandable for Wi-Fi & cloud (future)

---

## 📂 How to Use This Documentation

Each section of this documentation explains **one layer of the system**:

- **Overview** → Vision, philosophy, architecture  
- **Hardware** → Pinout, sensors, relays, electrical design  
- **Software** → RTOS, FSMs, profiles  
- **User Interface** → OLED screens & encoder behavior  
- **Safety & Logging** → Alarms, logs, fault handling  
- **Development** → MVP, integration flow, roadmap  

Read **top-down** if you are new, or jump to specific sections if you are implementing.

---

## 🎯 Current Status

- ✔ System architecture finalized  
- ✔ Hardware pinout locked  
- ✔ UI style finalized  
- ✔ Profiles defined  
- ✔ Documentation-first approach adopted  

Next step after documentation:
➡️ **Arduino firmware implementation**

---

## 👨‍💻 Intended Audience

This project is intended for:
- Embedded engineers
- Hardware developers
- Makers building reliable systems
- Anyone learning **RTOS-based system design**

---

## 📌 Final Note

> This is not just an incubator.  
> This is not just a thermostat.  

This is a **Reusable Environmental Control Platform**.

---

➡️ Proceed to **Overview → Vision & Purpose** to understand the long-term goals of this system.
