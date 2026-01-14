# 🌞 IoT Solar Radiation Tracking Device

This is a simple IoT project using an **ESP32** that
automatically **tracks the strongest sunlight direction**
using **two LDR sensors**, and measures solar energy
from a small solar cell.  
All collected readings are uploaded live to **ThingSpeak**.

---

## 🎯 Project Summary

- **ESP32** controls the system  
- **Two LDRs** detect which side receives more sunlight  
- The system tilts/rotates toward the brighter direction  
- A **solar cell** measures real solar power received  
- Data is logged online in **ThingSpeak**
- The whole circuit was first tested in **simulation** before hardware

> 🔧 No resistors were required because the LDR modules include built‑in resistors.

---

## 🎥 Project Demo Video

👉 Video link:  
[
](https://drive.google.com/file/d/1ld1btmJL3rvuPaAHK-2wN7EaDeImo01P/view?usp=sharing)


## 🔗 Simulations & Dashboard

- ▶️ **Wokwi Simulation:**  
[
](https://wokwi.com/projects/451123109909415937)-
📊 **ThingSpeak Channel (Live data):**  
[](https://thingspeak.mathworks.com/channels/3211422)

  

## 🧪 Hardware Used

- ESP32 DevKit
- 2x LDR modules (with internal resistors)
- Solar cell
- Jumper wires
- Breadboard
- Servo/motor if rotating panel

---

## 📌 How It Works

1. Each LDR reads light intensity.
2. ESP32 compares the values:
   - If left > right → rotates toward left
   - If right > left → rotates toward right
3. Solar cell voltage is measured
4. ESP32 uploads readings to ThingSpeak every few seconds
5. You can monitor data online in real time 🌍

---

## 🛠 Firmware Flashing

```bash
idf.py set-target esp32
idf.py build
idf.py -p PORT flash monitor
