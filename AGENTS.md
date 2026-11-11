# AGENTS.md — instrucciones para agentes (Claude Code, Codex, etc.)

Contexto: prototipo de auto-ventilación (ESP32 + MQ-9 + servo + LED) para la **Evaluación 3 de Seguridad IoT**. Se evalúa con dos documentos:

1. **Rúbrica Eva 3** (50 pts, Blynk IoT): programación, proyecto Blynk, datastreams y dashboard, manejo seguro del token, conectividad, plan, ejecución y análisis de pruebas, vulnerabilidades y mitigación.
2. **Guía "Auditoría IoT Mosquitto + ESP32"** (18 pts): broker Mosquitto con autenticación, ESP32 publicando y recibiendo por MQTT, auditoría física y lógica, matriz de riesgo e informe.

La guía original instala Mosquitto en Windows, pero **el profesor autorizó hacerlo en Linux**. Las dos opciones están documentadas en `docs/mosquitto-mqtt.md` (§1A Windows, §1B Linux). Si el agente corre en Linux, conviene usar §1B, porque se puede automatizar entera desde la terminal.

## Estado actual

| Parte | Estado |
|---|---|
| Firmware v2.0.0 (Blynk TLS + MQTT + validación) | ✅ Compila sin warnings: core esp32 3.3.12, Blynk 1.4.0, PubSubClient 2.8, en las 5 combinaciones de flags |
| Configuración de Mosquitto (Linux) + ACL | ✅ Probada en un contenedor con Mosquitto 2.1.2: rechaza anónimos, deja pasar datos y comandos, y la ACL bloquea a `esp32lab` cuando publica comandos |
| Configuración de Mosquitto (Windows) | ⚠️ Escrita según la guía del profesor, sin probar en Windows |
| Documentación (Blynk, MQTT, pruebas, seguridad) | ✅ Lista, con columnas para completar |
| Control local en la placa física | ✅ Cargado con `ENABLE_BLYNK 0` y `ENABLE_MQTT 0` (sin `secrets.h` real): reposo estable 135 s sin disparos falsos, soplido detectado y apagado automático. Umbral calibrado a 1450 |
| **Firmware completo (Wi-Fi + Blynk + MQTT) en la placa** | ❌ **Nunca se ha cargado ni probado** |
| Template y dispositivo en Blynk | ❌ Por crear (requiere la cuenta del usuario) |
| Pruebas, evidencias e informe | ❌ Pendiente: ver la sección **Pendiente después del deploy** |

## Mapa del repositorio

| Archivo | Qué es |
|---|---|
| `esp32-auto-ventilacion.ino` | Firmware: control local + Blynk (TLS) + MQTT con validación de comandos |
| `config.h` | Pines, umbrales, temas MQTT, pines virtuales y flags `ENABLE_BLYNK` / `ENABLE_MQTT` / `MQTT_USE_TLS` |
| `secrets.example.h` | Plantilla de credenciales. **Copiar a `secrets.h`**, que está en `.gitignore` |
| `mosquitto/acl` | ACL, igual para los dos sistemas operativos |
| `mosquitto/mosquitto.conf.example` | Fragmento de configuración para **Windows** |
| `mosquitto/linux/ventilacion.conf` | Drop-in para **Linux**: va en `/etc/mosquitto/conf.d/` |
| `docs/blynk.md` | Router WPA2, template, datastreams V0–V4, dashboard y manejo del token |
| `docs/mosquitto-mqtt.md` | Instalación (Windows o Linux), usuarios, firewall, pruebas, ACL y TLS (fase 2) |
| `docs/plan-pruebas.md` | Casos A1–D4 con resultados esperados. **Completar los resultados obtenidos** |
| `docs/seguridad.md` | Vulnerabilidades, matriz de riesgo, mejoras y respuestas. **Completar las observaciones físicas y los roles** |
| `docs/evidencias/` | Fotos y capturas, nombradas con el ID de la prueba |
| `diagram.json` | Simulación en Wokwi (el MQ-9 se simula con un potenciómetro) |

## Reglas

- **Nunca** commitear `secrets.h`, `passwd`, llaves o certificados, ni pegar el Auth Token o las claves en archivos versionados, issues o chats. Antes de cada commit, ejecutar `git status` y `git diff --cached | grep -iE "token|password|pass"`.
- Las contraseñas del broker y de Blynk las escribe **el usuario**. Usar `mosquitto_passwd` en modo interactivo (sin `-b`) para que la clave no quede en el historial de la shell.
- No imprimir secretos por Serial. No activar `BLYNK_DEBUG`.
- El control local (sensor → servo/LED) no debe depender de la red. Nada de bucles `while` bloqueantes esperando Wi-Fi, MQTT o Blynk.
- Los temas MQTT y los pines virtuales están definidos en `config.h`. Si cambian, actualizar también `mosquitto/acl`, `docs/blynk.md` y `docs/mosquitto-mqtt.md`.
- Comandos MQTT válidos: `ON`, `OFF`, `AUTO`, `UMBRAL=<500-4000>`, `ESTADO`. Cualquier otro se rechaza.
- Pedir confirmación al usuario antes de hacer push. Antes de cambios grandes en un sistema ajeno (instalar paquetes, abrir puertos en el firewall), decirle al usuario qué se va a ejecutar.

## Puesta en marcha

Orden recomendado: **broker → Blynk → credenciales → compilar y cargar → verificar**.

### 1. Broker Mosquitto

Seguir `docs/mosquitto-mqtt.md`: **§1A** en Windows o **§1B** en Linux, y luego §2 (prueba local). Resumen para Linux:

```bash
sudo dnf install mosquitto          # o: sudo apt install mosquitto mosquitto-clients / sudo pacman -S mosquitto
sudo mosquitto_passwd -c /etc/mosquitto/passwd esp32lab   # el usuario escribe la clave
sudo mosquitto_passwd /etc/mosquitto/passwd monitor
sudo install -m 600 mosquitto/acl /etc/mosquitto/acl
sudo mkdir -p /etc/mosquitto/conf.d
sudo install -m 644 mosquitto/linux/ventilacion.conf /etc/mosquitto/conf.d/
sudo chown mosquitto:mosquitto /etc/mosquitto/passwd /etc/mosquitto/acl
# Solo en Fedora/Arch, si mosquitto.conf no tiene include_dir:
grep -q '^include_dir /etc/mosquitto/conf.d' /etc/mosquitto/mosquitto.conf || echo 'include_dir /etc/mosquitto/conf.d' | sudo tee -a /etc/mosquitto/mosquitto.conf
sudo systemctl enable --now mosquitto && sudo systemctl restart mosquitto
ss -ltn | grep 1883                 # debe mostrar 0.0.0.0:1883
# Firewall solo para la LAN (ajustar la subred): firewalld o ufw, ver §1B paso 7
hostname -I                         # esta IP va en MQTT_SERVER
```

Importante: el PC del broker y el ESP32 tienen que estar en la **misma red**. Las redes institucionales o de invitados suelen aislar a los clientes entre sí; en ese caso, usar un router propio o el hotspot de un celular en 2.4 GHz.

### 2. Blynk

Seguir `docs/blynk.md` §2–3: crear el template con los datastreams **V0–V4 exactamente como en la tabla**, el dashboard web y móvil, y el dispositivo. Esto lo hace el usuario en blynk.cloud y en la app; el agente puede guiarlo paso a paso.

### 3. Toolchain y credenciales

Opción A, **Arduino IDE 2**:
1. Agregar la URL de placas `https://espressif.github.io/arduino-esp32/package_esp32_index.json`.
2. Instalar **esp32 by Espressif**. Se verificó con la 3.3.12; la 2.x también es compatible.
3. En el Gestor de librerías, instalar **Blynk** (Volodymyr Shymanskyy) y **PubSubClient** (Nick O'Leary).

Opción B, **arduino-cli** (preferida para un agente):

```bash
arduino-cli config init --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install Blynk PubSubClient
cp secrets.example.h secrets.h       # Windows CMD: copy secrets.example.h secrets.h
```

Pedir al usuario los valores y escribirlos **solo** en `secrets.h`:
- SSID y clave del Wi-Fi de 2.4 GHz.
- `BLYNK_TEMPLATE_ID`, `BLYNK_TEMPLATE_NAME`, `BLYNK_AUTH_TOKEN` y el servidor regional (de blynk.cloud → Device Info).
- La IP del PC del broker (paso 1), el usuario `esp32lab` y su contraseña.

Si Blynk o el broker todavía no están listos, se puede probar por partes poniendo `ENABLE_BLYNK 0` y/o `ENABLE_MQTT 0` en `config.h`.

### 4. Compilar y cargar

```bash
arduino-cli board list                         # Linux: /dev/ttyUSB0 o /dev/ttyACM0. Windows: COMx
arduino-cli compile --fqbn esp32:esp32:esp32 .
arduino-cli upload  --fqbn esp32:esp32:esp32 -p <PUERTO> .
arduino-cli monitor -p <PUERTO> -c baudrate=115200
```

- Si la carga se queda en `Connecting....`, mantener presionado el botón **BOOT** de la placa durante la carga.
- **Linux, *permission denied* en el puerto:** `sudo usermod -aG dialout $USER` (en Arch el grupo es `uucp`) y volver a iniciar sesión.
- **Linux, el puerto aparece y desaparece** (placas con chip CH340 en Ubuntu): el servicio `brltty` se apropia del puerto. Desinstalarlo con `sudo apt remove brltty`.
- **Windows, no aparece el puerto COM:** instalar el driver CP210x o CH340 según el chip USB de la placa.
- Usar un cable USB **de datos**: algunos cables solo cargan.

### 5. Salida esperada en Serial

```
==================================================
  AUTO-VENTILACION v2.0.0  (Blynk:si MQTT:si)
==================================================
Conectando a Wi-Fi.....
Wi-Fi OK. IP ESP32: 192.168.x.x  RSSI: -5x dBm
Conectando a MQTT (puerto 1883, sin TLS)... conectado
[...] Ready (ping: ..ms).
[BLYNK] Conectado a la nube
Crudo:1162 Filtrado:1182 Umbral:1450 Ventilando:1350
```

Diagnóstico:

| Síntoma | Causa probable |
|---|---|
| Wi-Fi nunca conecta | Red de 5 GHz, SSID o clave mal escritos, o filtrado MAC en el router |
| MQTT `estado: -2` | IP del broker incorrecta, firewall (Windows: red marcada como Pública; Linux: falta la regla de firewalld/ufw), o aislamiento de clientes en el AP. Desde otro equipo de la LAN, probar `mosquitto_sub -h <IP> ...` |
| MQTT `estado: 4` o `5` | Usuario o contraseña incorrectos, o el usuario no existe en el archivo `passwd` |
| El broker Linux no arranca: `Unable to open pwfile` | Permisos: `sudo chown mosquitto:mosquitto /etc/mosquitto/passwd /etc/mosquitto/acl` |
| El broker Linux solo escucha en `127.0.0.1:1883` | No está cargando `conf.d`: falta `include_dir` en `mosquitto.conf` (Fedora/Arch) |
| Blynk `Invalid auth token` | El token no corresponde o el servidor regional no es el correcto (`BLYNK_SERVER`) |
| Blynk no conecta y MQTT sí | El handshake TLS falla o la red bloquea el puerto 443: revisar la versión de la librería y el firewall de la red. Como diagnóstico temporal, cambiar a `BlynkSimpleEsp32.h` (puerto 80) y **volver a TLS** después |
| Reinicios al mover el servo (`Brownout detector`) | Alimentación insuficiente: usar una fuente de 5 V externa con GND común |
| `Filtrado` siempre sobre el umbral | MQ-9 sin precalentar (esperar 3 min o más) o umbral demasiado bajo: ajustarlo con el slider V3. Mientras ventila, `Filtrado` sube +90 a +400 por el ruido del servo; la decisión de apagar se toma con el servo detenido |

## Pendiente después del deploy

Cuando el broker esté arriba, Blynk configurado y la ESP32 conectada mostrando la salida del paso 5, queda lo siguiente. Hacerlo en este orden, junto con el usuario, que es quien opera el hardware y saca las fotos.

### 1. Validar en la placa lo que todavía no se ha probado en hardware

Si algo falla aquí, corregir el firmware, volver a compilar y anotar la desviación en `docs/plan-pruebas.md` §3.

- [ ] La conexión TLS de Blynk (`BlynkSimpleEsp32_SSL.h`) funciona con el core 3.3.x. Debe aparecer `Ready` en Serial.
- [ ] El slider V3 recupera el umbral del servidor al reiniciar (`syncVirtual`, prueba B6).
- [ ] `Blynk.logEvent("aire_viciado")` llega a la app, si se creó el evento (prueba B9).
- [ ] El servo oscila suave y la placa no se reinicia por *brownout* con servo + MQ-9 + Wi-Fi activos.
- [ ] Los comandos MQTT llegan y se validan (pruebas C4 y C5).

### 2. Calibrar el MQ-9

Ya hecho en la placa real (2026-11-11), con el servo detenido: aire normal entre ~1170 y ~1520 según el calentamiento, soplido +200 a +330, picos sueltos de la cruda de hasta +300 y ruido del servo en marcha de +90 a +400. `UMBRAL_DEFECTO` quedó en 1450.

- [ ] Precalentar el sensor 3 minutos o más y anotar la lectura `Filtrado` en reposo (prueba A2). Leerla con el servo detenido: en marcha aparece inflada.
- [ ] Si la base del día deja menos de ~100 de margen bajo 1450, ajustar con el slider V3 (base + 100 a 150) y anotar el valor final en el plan de pruebas.

### 3. Ejecutar el plan de pruebas (`docs/plan-pruebas.md`)

- [ ] Completar las columnas **Obtenido** y **Evidencia** de A1–A5, B1–B9, C1–C8 y D1–D4.
- [ ] Registrar las desviaciones en §3 (esperado, obtenido, causa y acción) y escribir la conclusión.

### 4. Reunir evidencias (`docs/evidencias/`, nombrar con el ID de la prueba)

Antes de guardar cada captura, tapar tokens y claves. Lo exigen la rúbrica (criterio 7) y los entregables de la guía MQTT (§9):

- [ ] Fotos del montaje físico (placa sin gabinete, USB, cables, alimentación) → auditoría física.
- [ ] Captura del router: WPA2-AES, 2.4 GHz, WPS desactivado (D3).
- [ ] Capturas de Blynk: template, datastreams, dashboard web, app móvil y dispositivo Online (B1–B5).
- [ ] Captura de la configuración de Mosquitto **sin contraseñas** (el `.conf` y la ACL; el archivo `passwd` no).
- [ ] Monitor Serie con Wi-Fi + MQTT + Blynk conectados (paso 5).
- [ ] `mosquitto_sub` recibiendo el JSON del ESP32 (C3) y el comando enviado desde el PC visto en Serial (C4).
- [ ] Rechazo anónimo (C2), ACL (C6), LWT `offline` (C7) y validación (C5).
- [ ] Opcional: un video corto del sensor activando el servo y del switch V0 desde el celular.

### 5. Completar la auditoría (`docs/seguridad.md`)

- [ ] Llenar la columna **Observado en el montaje** de F1–F6 con lo que se ve en las fotos.
- [ ] Ajustar P e I de la matriz de riesgo según la evidencia real, sin copiar los valores estimados tal cual.
- [ ] Llenar la tabla de **trabajo colaborativo** con los nombres y roles reales (criterio 10).

### 6. Informe técnico

La guía MQTT pide un informe. Crear `docs/informe.md` (o un .docx o .pdf si el profesor lo exige) con los 10 entregables de la guía (§9):

1. Identificación de la placa.
2. Diagrama de arquitectura (está en `docs/seguridad.md` §1).
3. Vulnerabilidades físicas y lógicas.
4. Fotos.
5. Configuración de Mosquitto.
6. Captura del Monitor Serie.
7. Captura del cliente Mosquitto.
8. Prueba en sentido inverso (PC → ESP32).
9. Matriz de riesgo.
10. Conclusión: qué debilidad fue la más relevante y cómo se comprobó la mejora.

Incluir también el resumen del plan de pruebas y el análisis de resultados (criterios 6–8 de la rúbrica).

### 7. Opcional, suma puntos en mitigación

- [ ] Fase 2 con TLS en MQTT (`docs/mosquitto-mqtt.md` §4) y una captura de `mosquitto_sub` en 8883 como prueba de la mejora.
- [ ] Gabinete o caja para la placa (mitiga F1/F2/F3) con foto de antes y después.

### 8. Cierre

- [ ] Commit sin secretos (ejecutar `git status` y verificar que `secrets.h` no aparezca) y pedir confirmación antes del push.
- [ ] Al terminar el laboratorio: detener el broker si el PC es compartido (`sudo systemctl disable --now mosquitto`), quitar la regla de firewall y no dejar contraseñas de prueba en equipos ajenos.
- [ ] Si el Auth Token apareció en alguna captura o commit: **Reset Auth Token** en Blynk.
