# Si5351 VFO 40m LSB + BFO Ajustable + CAL 25 MHz

VFO para radioaficionados basado en **Si5351**, diseñado para la banda de **40 metros LSB**, con:

- VFO principal
- Salida de calibración de 25 MHz
- BFO ajustable por software
- Display de 6 dígitos con MAX7219
- Encoder rotativo + botón STEP
- Almacenamiento en EEPROM

Proyecto educativo y experimental, compatible con plataformas **AVR / LGT8Fx (Nano / Uno)**.

---

## 🧰 Hardware utilizado

- MCU: Arduino Nano / Uno / LGT8Fx
- Si5351 (I2C)
- Encoder rotativo con pulsador
- Display MAX7219 (6 dígitos)
- Botón STEP
- EEPROM interna

### Asignación de salidas Si5351

| CLK | Función |
|-----|--------|
| CLK0 | VFO (RX 40m LSB) |
| CLK1 | NC |
| CLK2 | BFO (12 MHz + offset) |

---

## 🎛️ Funciones principales

- **Rango 40m:** 7.000 – 7.300 MHz
- **Pasos de sintonía:** 50 Hz / 1 kHz
- **Calibración de cristal (PPB)** con encoder
- **BFO ajustable ±8 kHz**
- Valores guardados automáticamente en EEPROM
- Timeout automático de menús (30 s)

---

## 🕹️ Controles

### Encoder
- Giro: cambia frecuencia / CAL / BFO
- Dirección CW / CCW detectada por interrupciones

### Botón STEP
- Click corto: cambia paso de sintonía
- Pulsación larga (>2 s):
  - En modo normal → entra a CAL/CLK)-VFO
  - En CAL → entra a BFO
  - En BFO → vuelve a modo normal

---

## 📚 Bibliotecas utilizadas

- Rotary Encoder
- Etherkit Si5351
- LedControl (MAX7219)
- EEPROM (Arduino)

> Asegúrate de tener estas bibliotecas instaladas o incluidas en la carpeta `src`.

---

## ⚠️ Notas importantes

- El VFO genera **frecuencia invertida** (−RX + IF) para arquitectura superheterodina.
- El BFO incluye un pequeño "jump" para evitar clicks audibles.
- Proyecto pensado para uso experimental / educativo.

---

## 📸 Imágenes / Diagramas

*(Puedes agregar aquí esquemas, fotos o capturas del display)*

---

## 📜 Licencia

Este proyecto se publica como **open hardware / open source** para uso educativo.
Puedes modificarlo y adaptarlo libremente, mencionando la fuente.

---

## 👤 Autor
"QSO" Qrp Sponsoring Organization

Juan Carlos Berberena Gonzalez  / WJ6C/exCO6BG
Radioaficionado – Experimentación RF y QRP  
2025

