# AGENTS.md — instrucciones para agentes (Claude Code, Codex, etc.)

Contexto: prototipo de auto-ventilación (ESP32 + MQ-9 + servo + LED) para la **Evaluación 3 de Seguridad IoT**. Se evalúa con dos documentos:

1. **Rúbrica Eva 3** (50 pts, Blynk IoT): programación, proyecto Blynk, datastreams/dashboard, manejo seguro del token, conectividad, plan/ejecución/análisis de pruebas, vulnerabilidades y mitigación.
2. **Guía "Auditoría IoT Mosquitto + ESP32"** (18 pts): Mosquitto en Windows con autenticación, ESP32 publicando y recibiendo por MQTT, auditoría física/lógica y matriz de riesgo.

El firmware y la documentación ya están hechos. **Lo que falta requiere el hardware y las cuentas del usuario:** crear el template en Blynk, configurar Mosquitto en Windows, cargar el firmware, ejecutar las pruebas y sacar fotos y capturas.

## Mapa del repositorio

| Archivo | Qué es |
|---|---|
| `esp32-auto-ventilacion.ino` | Firmware: control local + Blynk (TLS) + MQTT con validación de comandos |
| `config.h` | Pines, umbrales, temas MQTT, pines virtuales y flags `ENABLE_BLYNK` / `ENABLE_MQTT` / `MQTT_USE_TLS` |
| `secrets.example.h` | Plantilla de credenciales. **Copiar a `secrets.h`**, que está en `.gitignore` |
| `mosquitto/` | `mosquitto.conf.example` (fragmento a agregar) y `acl` |
| `docs/blynk.md` | Router WPA2, template, datastreams V0–V4, dashboard y manejo del token |
| `docs/mosquitto-mqtt.md` | Instalación en Windows, usuarios, firewall, pruebas, ACL y TLS (fase 2) |
| `docs/plan-pruebas.md` | Casos A1–D4 con resultados esperados. **Completar los resultados obtenidos** |
| `docs/seguridad.md` | Vulnerabilidades, matriz de riesgo, mejoras y respuestas. **Completar las observaciones físicas y los roles** |
| `diagram.json` | Simulación en Wokwi (el MQ-9 se simula con un potenciómetro) |

## Reglas

- **Nunca** commitear `secrets.h`, `mosquitto/passwd`, llaves o certificados, ni pegar el Auth Token o las claves en archivos versionados, issues o chats. Antes de cada commit, ejecutar `git status` y `git diff --cached | grep -iE "token|password|pass"`.
- No imprimir secretos por Serial. No activar `BLYNK_DEBUG`.
- El control local (sensor → servo/LED) no debe depender de la red. Nada de bucles `while` bloqueantes esperando Wi-Fi, MQTT o Blynk.
- Los temas MQTT y los pines virtuales están definidos en `config.h`. Si cambian, actualizar también `mosquitto/acl`, `docs/blynk.md` y `docs/mosquitto-mqtt.md`.
- Comandos MQTT válidos: `ON`, `OFF`, `AUTO`, `UMBRAL=<500-4000>`, `ESTADO`. Cualquier otro se rechaza.
- Pedir confirmación al usuario antes de hacer push.

## Puesta en marcha con la placa conectada

### 1. Toolchain

Opción A, **Arduino IDE 2**:
1. Agregar la URL de placas `https://espressif.github.io/arduino-esp32/package_esp32_index.json`.
2. Instalar **esp32 by Espressif**. Compilación verificada con core 3.3.12, Blynk 1.4.0 y PubSubClient 2.8 en las 5 combinaciones de flags; la 2.x también es compatible. **Aún no se ha probado en la placa física.**
3. En el Gestor de librerías, instalar **Blynk** (Volodymyr Shymanskyy) y **PubSubClient** (Nick O'Leary).

Opción B, **arduino-cli** (preferida para un agente):

```bash
arduino-cli config init --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install Blynk PubSubClient
```

### 2. Credenciales

```bash
cp secrets.example.h secrets.h     # Windows CMD: copy secrets.example.h secrets.h
```

Pedir al usuario los valores y escribirlos **solo** en `secrets.h`:
- SSID y clave del Wi-Fi de 2.4 GHz.
- `BLYNK_TEMPLATE_ID`, `BLYNK_TEMPLATE_NAME`, `BLYNK_AUTH_TOKEN` y el servidor regional (de blynk.cloud → Device Info; ver `docs/blynk.md` §2–3).
- La IPv4 del PC con Mosquitto (`ipconfig`), el usuario `esp32lab` y su contraseña (ver `docs/mosquitto-mqtt.md` §1).

Si el usuario todavía no tiene Blynk o el broker configurado, se puede compilar igual poniendo `ENABLE_BLYNK 0` y/o `ENABLE_MQTT 0` en `config.h`.

### 3. Compilar y cargar

```bash
arduino-cli board list                         # Detecta el puerto: Linux /dev/ttyUSB0 o ttyACM0, Windows COMx
arduino-cli compile --fqbn esp32:esp32:esp32 .
arduino-cli upload  --fqbn esp32:esp32:esp32 -p <PUERTO> .
arduino-cli monitor -p <PUERTO> -c baudrate=115200
```

- Si la carga se queda en `Connecting....`, mantener presionado el botón **BOOT** de la placa durante la carga.
- En Linux, si da *permission denied*, agregar el usuario al grupo `dialout` (o `uucp` en Arch/Fedora) y volver a iniciar sesión.
- En Windows, si no aparece el puerto COM, instalar el driver CP210x o CH340 según el chip USB de la placa.

### 4. Salida esperada en Serial

```
==================================================
  AUTO-VENTILACION v2.0.0  (Blynk:si MQTT:si)
==================================================
Conectando a Wi-Fi.....
Wi-Fi OK. IP ESP32: 192.168.x.x  RSSI: -5x dBm
Conectando a MQTT (puerto 1883, sin TLS)... conectado
[...] Ready (ping: ..ms).
[BLYNK] Conectado a la nube
Crudo:1460 Filtrado:1452 Umbral:1580 Ventilando:1350
```

Diagnóstico:

| Síntoma | Causa probable |
|---|---|
| Wi-Fi nunca conecta | Red de 5 GHz, SSID o clave mal escritos, o filtrado MAC en el router |
| MQTT `estado: -2` | IP del broker incorrecta, firewall de Windows, red marcada como Pública o aislamiento de clientes en el AP |
| MQTT `estado: 4` o `5` | Usuario o contraseña incorrectos, o el usuario no existe en `C:\mosquitto\passwd` |
| Blynk `Invalid auth token` | El token no corresponde o el servidor regional no es el correcto (`BLYNK_SERVER`) |
| Reinicios al mover el servo (`Brownout detector`) | Alimentación insuficiente: usar una fuente de 5 V externa con GND común |
| `Filtrado` siempre sobre el umbral | MQ-9 sin precalentar (esperar 3 min o más) o umbral demasiado bajo: ajustarlo con el slider V3 |

### 5. Después de que funcione

1. Ejecutar los casos de `docs/plan-pruebas.md` junto con el usuario y anotar los resultados obtenidos.
2. Guardar las capturas y fotos en `docs/evidencias/`, **tapando tokens y contraseñas**.
3. Completar las observaciones físicas y la tabla de roles en `docs/seguridad.md`.
4. Hacer commit (sin secretos) y pedir confirmación antes del push.
