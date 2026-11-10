# Prototipo de Auto-Ventilación Inteligente (ESP32 + MQ-9)

Prototipo de ventilación reactiva diseñado con un microcontrolador **ESP32** para solucionar el problema del aire viciado en habitaciones cerradas. Detecta variaciones en la calidad del aire o exhalaciones directas mediante un sensor **MQ-9**, activando automáticamente un ventilador (simulado mediante un **servomotor**) y un **LED indicador**.

## Componentes Utilizados

- **ESP32 DevKit v1 / v4**
- **Sensor de Gas MQ-9** (Flying Fish, 4 pines: `AO`, `DO`, `GND`, `VCC`)
- **Servomotor Micro Servo 9g (SG90)** (simula las aspas oscilantes del ventilador)
- **LED Amarillo** (indicador visual de estado activo)
- **Resistencia 220Ω** (protección de corriente para el LED)
- **Protoboard y jumpers macho-macho**

---

## Diagrama de Conexiones

| Componente | Pin del Componente | Conexión en ESP32 / Protoboard |
| :--- | :--- | :--- |
| **MQ-9** | `VCC` | Riel Rojo (+5V / `VIN`) |
| | `GND` | Riel Azul (`GND`) |
| | `AO` (Analógico) | `GPIO 34` |
| | `DO` (Digital) | *Desconectado* |
| **Servo Motor** | VCC (Rojo) | Riel Rojo (+5V / `VIN`) |
| | GND (Marrón/Negro) | Riel Azul (`GND`) |
| | Señal PWM (Naranja) | `GPIO 18` |
| **LED Amarillo** | Ánodo (+) [Pata Larga] | Vía resistencia 220Ω a `GPIO 2` |
| | Cátodo (-) [Pata Corta] | Riel Azul (`GND`) |

---

## Características del Firmware

- **Sin dependencias externas:** Utiliza el control nativo PWM (`ledc`) del ESP32 para controlar el servomotor a 50Hz.
- **Filtro pasa-bajas digital:** Suaviza caídas abruptas causadas por el enfriamiento del sensor durante un soplido directo.
- **Tiempo mínimo de extracción:** Una vez activado, garantiza un ciclo mínimo de **6 segundos** de ventilación continua para dispersar el aire viciado.
- **Telemetría en tiempo real:** Emite datos continuos a 115200 baudios compatibles con el **Serial Plotter** de Arduino IDE (`Crudo`, `Filtrado`, `Umbral`, `Ventilando`).
