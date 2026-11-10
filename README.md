# Prototipo de Auto-Ventilación Inteligente (ESP32 + MQ-9)

> **v2.0:** monitoreo y control remoto vía **Blynk IoT (TLS)** y **MQTT (Mosquitto)**, con credenciales fuera del código, ACL y validación de comandos. Puesta en marcha: [AGENTS.md](AGENTS.md).

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

- **Librerías:** `Blynk` y `PubSubClient` (se pueden desactivar en `config.h`). El servo utiliza el control nativo PWM (`ledc`) del ESP32 para controlar el servomotor a 50Hz.
- **Filtro pasa-bajas digital:** Suaviza caídas abruptas causadas por el enfriamiento del sensor durante un soplido directo.
- **Tiempo mínimo de extracción:** Una vez activado, garantiza un ciclo mínimo de **6 segundos** de ventilación continua para dispersar el aire viciado.
- **Telemetría en tiempo real:** Emite datos continuos a 115200 baudios compatibles con el **Serial Plotter** de Arduino IDE (`Crudo`, `Filtrado`, `Umbral`, `Ventilando`).
- **Blynk IoT:** switch de ventilación manual (V0), gauge de calidad de aire (V1), LED de estado (V2), slider de umbral (V3) y RSSI (V4). Conexión por TLS (443). → [docs/blynk.md](docs/blynk.md)
- **MQTT:** publica telemetría JSON en `laboratorio/esp32/datos`, estado `online`/`offline` (LWT) y recibe `ON`/`OFF`/`AUTO`/`UMBRAL=n`/`ESTADO` en `laboratorio/esp32/comando`. → [docs/mosquitto-mqtt.md](docs/mosquitto-mqtt.md)
- **Autonomía:** el control local sigue funcionando sin Wi-Fi, sin Blynk o sin broker. La reconexión no bloquea.

---

## Inicio rápido

1. Instalar el core **esp32** y las librerías **Blynk** y **PubSubClient**.
2. `cp secrets.example.h secrets.h` y completar Wi-Fi, Blynk y MQTT. `secrets.h` **no se sube** al repositorio.
3. Configurar Blynk ([docs/blynk.md](docs/blynk.md)) y Mosquitto ([docs/mosquitto-mqtt.md](docs/mosquitto-mqtt.md)).
4. Compilar y cargar a la placa, y abrir el Monitor Serie a 115200.

## Seguridad y evaluación

| Documento | Contenido |
|---|---|
| [docs/seguridad.md](docs/seguridad.md) | Vulnerabilidades físicas y lógicas, matriz de riesgo, mitigaciones y preguntas de análisis |
| [docs/plan-pruebas.md](docs/plan-pruebas.md) | Variables, parámetros, casos de prueba con resultados esperados y análisis |
| [mosquitto/](mosquitto/) | Configuración del broker para Windows (`mosquitto.conf.example`) y Linux (`linux/ventilacion.conf`): sin acceso anónimo, con log y ACL de mínimo privilegio |
