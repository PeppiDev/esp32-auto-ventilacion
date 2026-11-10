# Mosquitto en Windows + MQTT con el ESP32

Sigue la guía "Auditoría IoT Mosquitto + ESP32", con dos mejoras ya aplicadas: **ACL** (mínimo privilegio) y **validación de comandos** en el firmware.

> ⚠️ MQTT en el puerto 1883 **no cifra**: la contraseña viaja en claro dentro de la red local. Úsalo solo en la red del laboratorio, con credenciales exclusivas, y **nunca** abras el 1883 en el router. La fase 2 (TLS en 8883) está al final.

## 1. Instalación (CMD como administrador)

1. Descarga el instalador **x64** desde el sitio oficial, https://mosquitto.org/download/. Instálalo dejando marcada la opción **Service**.
2. Verifica la instalación:
   ```bat
   cd /d "C:\Program Files\mosquitto"
   mosquitto -h
   ```
3. Crea los usuarios, uno para el dispositivo y otro para el PC que monitorea. Usa contraseñas exclusivas del laboratorio:
   ```bat
   mkdir C:\mosquitto
   "C:\Program Files\mosquitto\mosquitto_passwd.exe" -c C:\mosquitto\passwd esp32lab
   "C:\Program Files\mosquitto\mosquitto_passwd.exe" C:\mosquitto\passwd monitor
   ```
   La opción `-c` crea el archivo, así que úsala **solo con el primer usuario**. Si la repites, borras el usuario anterior.
4. Copia `mosquitto/acl` de este repositorio a `C:\mosquitto\acl`.
5. Abre `C:\Program Files\mosquitto\mosquitto.conf` con el Bloc de notas como administrador y pega al final el contenido de `mosquitto/mosquitto.conf.example`.
6. Reinicia el servicio:
   ```bat
   sc query mosquitto
   net stop mosquitto
   net start mosquitto
   ```
   Si no arranca, revisa `C:\mosquitto\mosquitto.log` o ejecútalo en primer plano. Primero detén el servicio, porque no pueden correr dos instancias en el mismo puerto:
   ```bat
   "C:\Program Files\mosquitto\mosquitto.exe" -c "C:\Program Files\mosquitto\mosquitto.conf" -v
   ```

## 2. Red y firewall

```bat
ipconfig
netsh advfirewall firewall add rule name="Mosquitto MQTT 1883" dir=in action=allow protocol=TCP localport=1883 profile=private
netstat -ano | findstr :1883
```

- La IPv4 del adaptador conectado a la misma red que el ESP32 va en `MQTT_SERVER` dentro de `secrets.h`.
- La red de Windows debe estar marcada como **Privada**. Si figura como Pública, la regla de firewall no aplica.
- `netstat` debe mostrar `LISTENING` en el puerto 1883.

## 3. Prueba local del broker

Abre dos ventanas de CMD en `C:\Program Files\mosquitto`. En la primera:

```bat
mosquitto_sub -h 127.0.0.1 -t laboratorio/prueba -u monitor -P "CLAVE_MONITOR" -v
```

En la segunda:

```bat
mosquitto_pub -h 127.0.0.1 -t laboratorio/prueba -m "Broker funcionando" -u monitor -P "CLAVE_MONITOR"
```

**Evidencia de autenticación:** repite la prueba sin `-u` y `-P`. Debe fallar con `Connection Refused: not authorised`.

## 4. ESP32: temas y comandos

| Tema | Quién publica | Contenido |
|---|---|---|
| `laboratorio/esp32/datos` | ESP32 (cada 5 s y en cada cambio) | JSON `{"fw","gas","crudo","umbral","ventilando","modo","rssi","uptime_s"}` |
| `laboratorio/esp32/estado` | ESP32 (retenido) / broker (LWT) | `online` / `offline` |
| `laboratorio/esp32/comando` | PC (`monitor`) | `ON`, `OFF`, `AUTO`, `UMBRAL=1600`, `ESTADO` |

Para recibir los datos desde el ESP32 (paso 16 de la guía):

```bat
mosquitto_sub -h 127.0.0.1 -t "laboratorio/esp32/#" -u monitor -P "CLAVE_MONITOR" -v
```

Para enviar comandos al ESP32 (paso 17):

```bat
mosquitto_pub -h 127.0.0.1 -t laboratorio/esp32/comando -m "ON" -u monitor -P "CLAVE_MONITOR"
mosquitto_pub -h 127.0.0.1 -t laboratorio/esp32/comando -m "UMBRAL=1700" -u monitor -P "CLAVE_MONITOR"
mosquitto_pub -h 127.0.0.1 -t laboratorio/esp32/comando -m "AUTO" -u monitor -P "CLAVE_MONITOR"
```

El Monitor Serie muestra `[MQTT] Tema recibido: ...` y `[MQTT] Mensaje: ...`. Cada comando actúa sobre el servo y el LED.

**Evidencia de validación de mensajes:**

| Mensaje enviado | Resultado esperado en Serial |
|---|---|
| `Prueba de recepcion` (texto de la guía) | Se muestra y luego aparece `Comando no reconocido` |
| `UMBRAL=99999` | `Umbral rechazado: fuera de rango` |
| `UMBRAL=12abc` | `Comando no reconocido` |
| Texto de más de 32 caracteres | `Rechazado: largo N (max 32)` |

**Evidencia de ACL:** publica en `laboratorio/esp32/comando` con el usuario `esp32lab`. El ESP32 **no** recibe el mensaje, porque el broker descarta lo que la ACL no permite y lo registra en el log.

**Evidencia de LWT:** desconecta la alimentación del ESP32. Después de unos 45 s (1,5 × keepalive) el suscriptor recibe `laboratorio/esp32/estado offline`.

## 5. Fase 2 (opcional): MQTT con TLS

Usa OpenSSL, que viene con Git Bash. Ejecuta los comandos en `C:\mosquitto\certs`:

```bash
openssl req -new -x509 -days 365 -nodes -keyout ca.key -out ca.crt -subj "/CN=Lab-CA"
openssl req -new -nodes -keyout server.key -out server.csr -subj "/CN=IP_DEL_PC"
echo "subjectAltName=IP:IP_DEL_PC" > san.ext
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out server.crt -days 365 -extfile san.ext
```

Reemplaza `IP_DEL_PC` por la IPv4 real del PC. `ca.key` es la llave de la CA: guárdala fuera del PC compartido y **nunca** la subas al repositorio.

1. En `mosquitto.conf`, descomenta el bloque `listener 8883` y comenta el de 1883. Reinicia el servicio y abre el puerto 8883 en el firewall (perfil privado).
2. En `config.h`, pon `MQTT_USE_TLS 1`. En `secrets.h`, pega el contenido de `ca.crt` en `MQTT_CA_CERT`.
3. Para probar desde el PC:
   ```bat
   mosquitto_sub -h IP_DEL_PC -p 8883 --cafile C:\mosquitto\certs\ca.crt -t "laboratorio/esp32/#" -u monitor -P "..." -v
   ```

La IP del certificado (CN/SAN) debe coincidir con `MQTT_SERVER`. Si el PC cambia de IP, hay que regenerar `server.crt`, o bien fijar la IP con una reserva DHCP en el router.
