# 🏎️ ESP32-S3 AMOLED Image Gallery

> *"This is my very first code ever, built with the help of Claude AI. The idea was to use this tiny AMOLED dev board to display car logos from a microSD card, and then mount it on my simracing steering wheel."*

---

## 📖 About

A simple but fully functional **image gallery** for the **Waveshare ESP32-S3 Touch AMOLED 1.75"** dev board.  
The sketch reads `.jpg` and `.png` images from a **microSD card** and displays them on the built-in AMOLED screen. Navigate between images with a **left/right swipe**, and tap the screen to show the **battery status overlay**.

> ⚠️ **This code was written with the assistance of [Claude AI](https://www.anthropic.com) by Anthropic.**  
> The author is a beginner and declines all responsibility for any damage or issues arising from the use of this software. Use at your own risk.

---

## 🛠️ Hardware

| Component | Details |
|---|---|
| Board | Waveshare ESP32-S3 Touch AMOLED 1.75" |
| Display | Built-in AMOLED (CO5300 driver, QSPI) |
| Touch | CST touch controller (I2C) |
| Storage | MicroSD card (SDMMC 1-bit mode) |
| PMU | AXP2101 power management |

---

## 📦 Required Libraries

| Library | Source |
|---|---|
| `GFX_Library_for_Arduino` | Included in Waveshare demo package (offline) |
| `pin_config.h` | Included in Waveshare demo package (offline) |
| `SensorLib` v0.3.1 | Arduino Library Manager |
| `XPowersLib` v0.2.6 | Arduino Library Manager |
| `JPEGDEC` | Arduino Library Manager (Bodmer) |
| `PNGdec` | Arduino Library Manager (Bodmer) |
| `SD_MMC` | Built-in with ESP32 Arduino core |

---

## ⚙️ Arduino IDE Board Settings

| Setting | Value |
|---|---|
| Board | ESP32S3 Dev Module |
| USB CDC On Boot | Enabled |
| Flash Size | 16MB |
| PSRAM | OPI PSRAM |

---

## 🚀 How to Use

1. Copy your `.jpg` or `.png` images to the **root of the microSD card**
2. Flash the sketch to the board
3. Swipe **left** → next image
4. Swipe **right** → previous image
5. **Tap** (no swipe) → show battery status overlay

> Images are scanned recursively, so you can organize them in subfolders too.  
> Maximum 200 images supported.

---

## 📌 Pin Configuration

Defined in `pin_config.h` from the official Waveshare demo package:

| Function | GPIO |
|---|---|
| Display QSPI (SDIO0-3) | 4, 5, 6, 7 |
| Display SCLK / CS / RST | 38, 12, 39 |
| Touch SDA / SCL / INT / RST | 15, 14, 11, 40 |
| SD CLK / CMD / DATA | 2, 1, 3 |

---

## 📄 License

This project is released under the **MIT License** — free to use, modify, and distribute with attribution.  
See the [LICENSE](LICENSE) file for details.

---

## 🤖 AI Disclosure

This code was generated with the assistance of **Claude** by [Anthropic](https://www.anthropic.com).  
The human author provided the hardware context, requirements, and testing.
