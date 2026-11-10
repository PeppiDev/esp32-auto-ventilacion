# Auditoría de seguridad IoT

Cubre la rúbrica Eva 3, criterios 9 (vulnerabilidades físicas y lógicas) y 10 (mitigación y trabajo colaborativo), y las actividades A–D de la guía "Auditoría IoT Mosquitto + ESP32".

Una vulnerabilidad se clasifica según **dónde se origina la debilidad** (física o lógica), no según su consecuencia. Las vulnerabilidades son debilidades posibles, **no** incidentes confirmados.

## 1. Arquitectura

```
[MQ-9]──GPIO34─┐
[Servo]─GPIO18─┤ ESP32 ──Wi-Fi WPA2 2.4GHz──► Router ──Internet──► Blynk Cloud (TLS 443) ◄── App móvil / Web
[LED]──GPIO2───┘          │
                          └──MQTT TCP 1883 (LAN)──► PC Windows o Linux: Mosquitto (auth + ACL) ◄── mosquitto_sub/pub
```

## 2. Vulnerabilidades físicas (Actividad A)

Completa la columna **Observado en el montaje** con tus propias fotos (`docs/evidencias/`). No incluyas datos personales en las fotos.

| # | Vulnerabilidad | Riesgo (CIA) | Observado en el montaje |
|---|---|---|---|
| F1 | Placa y protoboard sin gabinete: circuitos y pines expuestos | Disponibilidad (cortocircuito, contacto accidental) e integridad (manipulación de pines) | |
| F2 | Puerto USB accesible: cualquiera puede reprogramar el equipo o leer el flash | Confidencialidad (extraer secretos del firmware) e integridad (cargar otro firmware) | |
| F3 | Jumpers sueltos sin fijar: el sensor o el servo pueden desconectarse | Disponibilidad (el sistema deja de ventilar sin avisar) | |
| F4 | Alimentación desde el USB del PC, sin protección. El servo provoca caídas de tensión | Disponibilidad (*brownout* y reinicios) | |
| F5 | Equipo en un lugar sin control de acceso | Todas | |
| F6 | El sensor se puede tapar, soplar o manipular para forzar o impedir la ventilación | Integridad del dato | |

## 3. Vulnerabilidades lógicas (Actividad B)

| # | Componente | Vulnerabilidad | Estado en v1 (original) | Estado en v2 (este repositorio) |
|---|---|---|---|---|
| L1 | Firmware | Credenciales escritas en el sketch | No había red | Movidas a `secrets.h` (en `.gitignore`). Sigue existiendo el riesgo en el flash (F2) |
| L2 | Firmware | Secretos impresos en el Monitor Serie | – | No se imprimen el SSID, las claves ni el token. Se compila sin `BLYNK_DEBUG` |
| L3 | Firmware | Mensajes recibidos sin validar (desbordamiento, comandos arbitrarios) | – | Límite de 32 bytes, solo ASCII imprimible, lista blanca de comandos y rango del umbral |
| L4 | Firmware | Bucles de reconexión bloqueantes: sin red se detiene el control | – | Reconexión no bloqueante. El control local nunca se detiene |
| L5 | Red | Wi-Fi abierto o WEP/WPA, WPS activo, clave de fábrica del router | Depende del router | WPA2-AES, clave de 12 caracteres o más, WPS desactivado y admin cambiado (`docs/blynk.md`) |
| L6 | MQTT | Broker con acceso anónimo | – | `allow_anonymous false` y `password_file` |
| L7 | MQTT | Tráfico sin cifrar en TCP/1883: usuario, contraseña y datos visibles en la LAN | – | **Pendiente (aceptado en laboratorio).** Fase 2 con TLS en 8883 lista (`MQTT_USE_TLS`) |
| L8 | MQTT | Permisos excesivos: cualquier cliente publica en cualquier tema | – | ACL: `esp32lab` solo publica datos y lee comandos; `monitor` hace lo inverso |
| L9 | MQTT | El puerto 1883 expuesto a Internet | – | Firewall abierto solo en la red privada (perfil privado en Windows, o regla para la subred LAN con firewalld/ufw en Linux) y sin redirección de puertos |
| L10 | Blynk | Auth Token filtrado (commit, captura, chat) permite controlar el equipo | – | Token en `secrets.h`. Procedimiento de *Reset Auth Token* documentado |
| L11 | Blynk | Conexión a Blynk por TCP/80 sin cifrar | La guía usa `BlynkSimpleEsp32.h` (80) | `BlynkSimpleEsp32_SSL.h`: TLS en 443 |
| L12 | Blynk | Cuenta Blynk con contraseña débil o compartida | – | Contraseña única. Compartir el dashboard con roles, no con la cuenta |
| L13 | Firmware | Sin cifrado de flash ni Secure Boot: el firmware y los secretos se pueden extraer por USB | Vigente | **Pendiente**: se propone para producción |

## 4. Matriz de riesgo (Actividad C)

P = probabilidad (1–3), I = impacto (1–3), R = P × I. Interpretación: 1–2 bajo, 3–4 medio, 6–9 alto. Los valores siguientes son la estimación inicial del montaje **antes** de mitigar. Ajústalos con tu evidencia.

| N.º | Hallazgo (tipo) | Evidencia | P | I | R | Mitigación |
|---|---|---|---|---|---|---|
| 1 | Auth Token o credenciales en el código o el repositorio (lógica, L1/L10) | Revisión del sketch de la guía (token en el `.ino`) | 3 | 3 | **9 alto** | `secrets.h` fuera de git, `secrets.example.h`, Reset Token si se filtra ✅ |
| 2 | MQTT sin cifrado en 1883 (lógica, L7) | `mosquitto.conf`, listener 1883 sin TLS | 2 | 3 | **6 alto** | Red aislada y credenciales exclusivas ✅. TLS en 8883 (fase 2) |
| 3 | Puerto USB accesible (física, F2) | Foto del montaje | 2 | 3 | **6 alto** | Gabinete con el USB cubierto y ubicación controlada. En producción: Secure Boot y cifrado de flash |
| 4 | Broker anónimo o sin ACL (lógica, L6/L8) | Prueba C2 / C6 | 2 | 3 | **6 alto** | `allow_anonymous false` y ACL por usuario ✅ |
| 5 | Placa y cables expuestos, jumpers sueltos (física, F1/F3) | Foto del montaje | 3 | 2 | **6 alto** | Gabinete, soldar o usar borneras, fijar los cables |
| 6 | Comandos sin validar (lógica, L3) | Prueba C5 | 2 | 2 | 4 medio | Validación de longitud, formato y rango ✅ |
| 7 | Alimentación inestable con el servo (física, F4) | Reinicios observados (si los hay) | 2 | 2 | 4 medio | Fuente de 5 V dedicada con GND común y condensador de 470 µF |
| 8 | Secretos en Serial (lógica, L2) | Prueba D2 | 1 | 3 | 3 medio | No imprimir secretos ✅ |

✅ = mitigación ya implementada en este repositorio (verificar con la prueba indicada).

## 5. Propuesta de mejora priorizada (Actividad D)

1. **Gestión de secretos** (riesgo 9). Implementado: `secrets.h` fuera de git y TLS para Blynk. Siguiente paso: provisionar las credenciales en NVS mediante Blynk.Edgent o WiFiManager, para no compilarlas en el binario.
2. **Cifrado de MQTT** (riesgo 6). Fase 2: activar `MQTT_USE_TLS` con una CA propia, cerrar 1883 y abrir solo 8883 en el perfil privado.
3. **Protección física** (riesgo 6). Usar un gabinete impreso o una caja de registro con el USB tapado, el sensor expuesto solo por una rejilla y los cables fijados.
4. **Mínimo privilegio** (riesgo 6). Implementado: ACL. Siguiente paso: un usuario distinto por dispositivo y rotación de contraseñas.
5. **Producción:** habilitar Secure Boot v2 y Flash Encryption del ESP32, actualizaciones OTA firmadas (Blynk Air) y monitoreo del log de Mosquitto.

## 6. Preguntas de análisis (guía MQTT, sección 10)

- **¿Por qué un USB accesible es un riesgo físico?** Permite reprogramar el equipo con un firmware malicioso o volcar el flash, donde están las credenciales compiladas, sin tocar la red.
- **¿Por qué la clave Wi-Fi en el código es un riesgo lógico?** Queda en el repositorio, en las capturas y en el binario. Quien la obtenga entra a la red y desde ahí alcanza el broker y los demás equipos.
- **¿Autenticación vs. cifrado?** La autenticación verifica *quién* se conecta (usuario y contraseña). El cifrado impide que un tercero *lea* el tráfico. En 1883 la contraseña MQTT viaja en claro: hay autenticación, pero no confidencialidad.
- **¿Qué pasa si el broker acepta anónimos?** Cualquier equipo de la red podría leer la telemetría o publicar en `comando` y encender o apagar el ventilador.
- **¿Por qué no exponer 1883 a Internet?** Los escáneres automáticos buscan brokers abiertos. Las credenciales viajarían en claro por Internet y el broker quedaría expuesto a fuerza bruta y DoS.
- **¿Qué aporta una ACL?** Limita *qué* puede hacer cada usuario autenticado. Si se roban las credenciales del ESP32, con ellas no se pueden enviar comandos.
- **¿Qué hace falta para MQTT con TLS?** Una CA y un certificado del servidor, el listener 8883 con `cafile`, `certfile` y `keyfile`, `WiFiClientSecure` con `setCACert` en el ESP32 y abrir 8883 en el firewall. Ver `docs/mosquitto-mqtt.md` §4.

## 7. Trabajo colaborativo (completar)

| Integrante | Rol | Tareas | Evidencia |
|---|---|---|---|
| | Hardware y montaje | Cableado, fotos, auditoría física | |
| | Firmware y Blynk | Template, datastreams, dashboard, carga del firmware | |
| | Red y MQTT | Router, Mosquitto, ACL, pruebas C1–C8 | |
| | Documentación | Plan de pruebas, matriz de riesgo, informe | |

Coordinación: los cambios se registran en commits de git (`git log`). Se recomienda una rama y un PR por integrante.
