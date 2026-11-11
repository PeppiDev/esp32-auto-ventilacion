# Plan de pruebas

Cubre la rúbrica Eva 3, criterios 6 (planificación), 7 (ejecución) y 8 (análisis), y la guía MQTT (secciones 6 y 7).

**Cómo usarlo:** ejecuta cada prueba con el equipo físico y completa las columnas **Obtenido** y **Evidencia** (nombre del archivo en `docs/evidencias/`). Si un resultado no coincide con lo esperado, regístralo como desviación en la sección 3 en vez de borrarlo.

## 1. Variables y parámetros

| Variable | Origen | Rango / unidad | Parámetro de control |
|---|---|---|---|
| Lectura MQ-9 cruda | GPIO 34 (ADC 12 bits) | 0–4095 cuentas | – |
| Lectura filtrada | Firmware (filtro 80/20) | 0–4095 | – |
| Umbral de disparo | `config.h`, Blynk V3 o MQTT | 500–4000 (por defecto 1450) | Histéresis 60, evaluada con el servo detenido |
| Tiempo mínimo de ventilación | `config.h` | 6000 ms | Pausa de 600 ms del servo para medir |
| Tiempo máximo de ventilación | `config.h` | 60000 ms | Se reactiva si la lectura sigue sobre el umbral |
| Estado del ventilador | Servo GPIO 18 + LED GPIO 2 | 0/1 | Barrido 0–180° en pasos de 10° cada 15 ms |
| Modo | Blynk V0 / MQTT | auto / manual | – |
| RSSI Wi-Fi | `WiFi.RSSI()` | dBm (aceptable > -75) | – |
| Periodo de telemetría | `config.h` | Blynk 2 s, MQTT 5 s | – |

**Entorno de prueba:** ESP32 DevKit, MQ-9 precalentado **al menos 3 minutos** (el sensor necesita calentar la resistencia), red WPA2 de 2.4 GHz, PC (Windows o Linux) con Mosquitto en la misma red, y la app Blynk en el celular.

## 2. Casos de prueba

### A. Hardware local (sin red)

| ID | Prueba | Procedimiento | Resultado esperado | Obtenido | Evidencia |
|---|---|---|---|---|---|
| A1 | Arranque | Energizar el ESP32 por USB | 3 parpadeos del LED, servo en 0° y banner `AUTO-VENTILACION v2.0.0` en Serial | | |
| A2 | Lectura base | Aire limpio durante 1 min, con Serial Plotter abierto | `Filtrado` estable bajo el umbral (medido en la placa: ~1170–1520 según el calentamiento) | | |
| A3 | Disparo por sensor | Exhalar sobre el MQ-9 | `Filtrado` supera el umbral, LED encendido, servo oscilando y `[ON] (sensor)` | | |
| A4 | Tiempo mínimo | Exhalar una sola vez y medir con cronómetro | Ventila **6 s o más**; el servo se detiene 600 ms y se apaga solo si `Filtrado < umbral - 60` (si no, otro ciclo de 6 s; máximo 60 s) | | |
| A5 | Autonomía | Apagar el router y repetir A3 | Ventila igual. Serial muestra `Wi-Fi caído: reintentando...` | | |

### B. Blynk

| ID | Prueba | Procedimiento | Resultado esperado | Obtenido | Evidencia |
|---|---|---|---|---|---|
| B1 | Conexión | Encender con Wi-Fi | Serial `Ready (ping ...)` y `[BLYNK] Conectado`. Dispositivo **Online** en la consola | | |
| B2 | Monitoreo | Observar el Gauge V1 y exhalar | V1 sube en 2 s o menos y el LED V2 se enciende en la app | | |
| B3 | Actuador remoto | Switch V0 = 1 desde el celular | Servo oscila y LED encendido. Serial `[CMD] Modo MANUAL ON (blynk)` | | |
| B4 | Volver a auto | Switch V0 = 0 | Si el aire está limpio, se apaga tras la pausa de medición (~600 ms) | | |
| B5 | Umbral remoto | Slider V3 unos 100 bajo el `Filtrado` en reposo (ver A2) | Ventila en automático sin exhalar. Volver a 1450 lo apaga | | |
| B6 | Persistencia | Reiniciar el ESP32 con V3 ≠ 1450 | Al reconectar, recupera el umbral del servidor (`syncVirtual`) | | |
| B7 | Reconexión | Apagar el router 30 s y volver a encenderlo | Vuelve a Online sin reiniciar el ESP32 | | |
| B8 | Estabilidad | Encendido 10 min | Sin desconexiones en Device → Timeline | | |
| B9 | Notificación | Disparo por sensor con el evento `aire_viciado` creado | Aparece el evento en la app | | |

### C. MQTT / Mosquitto

| ID | Prueba | Procedimiento | Resultado esperado | Obtenido | Evidencia |
|---|---|---|---|---|---|
| C1 | Broker | Windows: `netstat -ano \| findstr :1883`. Linux: `ss -ltn \| grep 1883` | `LISTENING` / `LISTEN` | | |
| C2 | Autenticación | `mosquitto_sub` sin `-u` / `-P` | `not authorised` | | |
| C3 | ESP32 → PC | `mosquitto_sub -t "laboratorio/esp32/#"` | JSON cada 5 s y `estado online` | | |
| C4 | PC → ESP32 | Publicar `ON` y luego `AUTO` | Servo y LED responden. Serial muestra el tema y el mensaje | | |
| C5 | Validación | `UMBRAL=99999`, `UMBRAL=12abc` y texto de más de 32 caracteres | Los tres se rechazan con su mensaje | | |
| C6 | ACL | Publicar en `comando` usando `esp32lab` | El ESP32 **no** lo recibe | | |
| C7 | LWT | Desconectar la alimentación del ESP32 | Unos 45 s después aparece `estado offline` | | |
| C8 | Coherencia | Cambiar el umbral por MQTT | El slider V3 de Blynk se actualiza al mismo valor | | |

### D. Seguridad

| ID | Prueba | Procedimiento | Resultado esperado | Obtenido | Evidencia |
|---|---|---|---|---|---|
| D1 | Secretos en el repositorio | `git status` y `git log -p \| grep -i token` | `secrets.h` no está versionado y no hay tokens reales en el historial | | |
| D2 | Secretos en Serial | Revisar el log completo desde el arranque | No aparecen la clave Wi-Fi, el Auth Token ni la contraseña MQTT | | |
| D3 | Cifrado Wi-Fi | Captura del router | WPA2-PSK AES, WPS desactivado | | |
| D4 | TLS de Blynk | Serial al conectar | `blynk.cloud:443` | | |

## 3. Análisis de resultados (completar)

Para cada desviación, registra qué se esperaba, qué ocurrió, la causa probable y la acción tomada.

| ID | Esperado | Obtenido | Causa probable | Acción |
|---|---|---|---|---|
| | | | | |

Desviaciones conocidas o probables, para orientar el análisis:

- **Deriva del MQ-9:** la lectura base cambia con la temperatura, la humedad y el tiempo de calentamiento. En la placa real se midió entre ~1170 y ~1520 en pocos minutos; con el umbral original de 1580 un soplido dejó de detectarse. Por eso el umbral se ajusta desde Blynk (V3).
- **Caída por enfriamiento:** al soplar, la lectura sube +200 a +330 y luego el aire frío la deja varios minutos bajo la base anterior. El filtro pasa-bajas evita que esa caída corte el ciclo.
- **Picos sueltos del ADC:** se midieron picos de una sola muestra de hasta +300. Por eso la activación usa solo la lectura filtrada (la v2 original también aceptaba la instantánea y esos picos la disparaban).
- **Ruido del servo en la lectura:** con el servo en marcha la lectura sube +90 a +400 por compartir los 5V. En la v2 original esto mantenía el ventilador encendido indefinidamente; ahora el aire se evalúa con el servo detenido 600 ms.
- **Latencia de Blynk:** la actualización de V1 puede tardar hasta 2 s, porque se envía cada 2 s para respetar el límite de mensajes.
- **Ruido del ADC con Wi-Fi activo:** GPIO 34 pertenece a ADC1, que es compatible con Wi-Fi. ADC2 no funcionaría mientras el Wi-Fi está encendido.
- **Alimentación:** si el servo y el MQ-9 toman 5 V del USB, la tensión puede caer al mover el servo y provocar reinicios (*brownout*). Si ocurre, usa una fuente de 5 V externa con GND común.

**Conclusión (completar):** indica qué funcionó, qué no, qué tan estable fue la conexión y qué mejoras se comprobaron.
