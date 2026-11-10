# Mosquitto (Windows o Linux) + MQTT con el ESP32

Sigue la guía "Auditoría IoT Mosquitto + ESP32", con dos mejoras ya aplicadas: **ACL** (mínimo privilegio) y **validación de comandos** en el firmware.

El profesor autorizó instalar el broker en **Linux**. Elige **una** de las dos opciones; el firmware y las pruebas son iguales en ambas. Solo cambian la instalación, el firewall y las rutas.

| | Windows (guía original) | Linux |
|---|---|---|
| Config del repo | `mosquitto/mosquitto.conf.example` | `mosquitto/linux/ventilacion.conf` |
| Usuarios | `C:\mosquitto\passwd` | `/etc/mosquitto/passwd` |
| ACL (mismo archivo) | `C:\mosquitto\acl` | `/etc/mosquitto/acl` |
| Log | `C:\mosquitto\mosquitto.log` | `journalctl -u mosquitto` |
| IP del PC | `ipconfig` | `ip -4 addr` o `hostname -I` |
| Puerto escuchando | `netstat -ano \| findstr :1883` | `ss -ltn \| grep 1883` |

> ⚠️ MQTT en el puerto 1883 **no cifra**: la contraseña viaja en claro dentro de la red local. Úsalo solo en la red del laboratorio, con credenciales exclusivas, y **nunca** abras el 1883 en el router. La fase 2 (TLS en 8883) está al final.

## 1A. Instalación en Windows (CMD como administrador)

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
7. Red y firewall:
   ```bat
   ipconfig
   netsh advfirewall firewall add rule name="Mosquitto MQTT 1883" dir=in action=allow protocol=TCP localport=1883 profile=private
   netstat -ano | findstr :1883
   ```
   La red de Windows debe estar marcada como **Privada**. Si figura como Pública, la regla de firewall no aplica.

## 1B. Instalación en Linux

Ejecuta los comandos desde la raíz de este repositorio.

1. Instala el broker y los clientes:
   ```bash
   # Debian / Ubuntu / Mint
   sudo apt install mosquitto mosquitto-clients
   # Fedora / Nobara
   sudo dnf install mosquitto            # incluye mosquitto_pub/sub
   # Arch
   sudo pacman -S mosquitto
   ```
2. Verifica la instalación: `mosquitto -h | head -1`. Se recomienda la versión 2.x.
3. Crea los usuarios (`-c` solo con el primero):
   ```bash
   sudo mosquitto_passwd -c /etc/mosquitto/passwd esp32lab
   sudo mosquitto_passwd /etc/mosquitto/passwd monitor
   ```
4. Instala la ACL y la configuración del repositorio:
   ```bash
   sudo install -m 600 mosquitto/acl /etc/mosquitto/acl
   sudo mkdir -p /etc/mosquitto/conf.d
   sudo install -m 644 mosquitto/linux/ventilacion.conf /etc/mosquitto/conf.d/ventilacion.conf
   # Mosquitto 2.x exige que estos archivos sean del usuario mosquitto
   # (si no, no puede leerlos y no arranca: "Unable to open pwfile")
   sudo chown mosquitto:mosquitto /etc/mosquitto/passwd /etc/mosquitto/acl
   sudo chmod 600 /etc/mosquitto/passwd /etc/mosquitto/acl
   ```
5. Debian y Ubuntu ya leen `conf.d`. En **Fedora y Arch** hay que habilitarlo una sola vez:
   ```bash
   grep -q '^include_dir /etc/mosquitto/conf.d' /etc/mosquitto/mosquitto.conf || \
     echo 'include_dir /etc/mosquitto/conf.d' | sudo tee -a /etc/mosquitto/mosquitto.conf
   ```
   Revisa que `mosquitto.conf` no tenga otro `listener` o `allow_anonymous` sin comentar.
6. Inicia el servicio y deja que arranque con el sistema:
   ```bash
   sudo systemctl enable --now mosquitto
   sudo systemctl restart mosquitto        # después de cada cambio de configuración
   systemctl status mosquitto --no-pager
   journalctl -u mosquitto -n 30 --no-pager
   ```
   Si no arranca, ejecútalo en primer plano para ver el error. Primero detén el servicio:
   `sudo systemctl stop mosquitto && sudo -u mosquitto mosquitto -c /etc/mosquitto/mosquitto.conf -v`
7. Red y firewall. Abre el puerto **solo para la red local** y reemplaza `192.168.1.0/24` por tu subred:
   ```bash
   hostname -I                              # IPv4 del PC -> MQTT_SERVER en secrets.h
   # firewalld (Fedora / Nobara)
   sudo firewall-cmd --permanent --add-rich-rule='rule family="ipv4" source address="192.168.1.0/24" port port="1883" protocol="tcp" accept'
   sudo firewall-cmd --reload
   # ufw (Ubuntu / Mint)
   sudo ufw allow from 192.168.1.0/24 to any port 1883 proto tcp
   ss -ltn | grep 1883                     # debe mostrar LISTEN en 0.0.0.0:1883
   ```

> Configuración verificada el 2026-11-10 con Mosquitto 2.1.2 en un contenedor: rechaza conexiones anónimas (`not authorised`), entrega datos de `esp32lab` a `monitor` y comandos de `monitor` a `esp32lab`, y la ACL descarta los comandos publicados por `esp32lab`.

## 2. Prueba local del broker

Los comandos `mosquitto_sub` y `mosquitto_pub` son iguales en Windows y en Linux. En Windows se ejecutan desde `C:\Program Files\mosquitto`; en Linux, desde cualquier carpeta. Abre dos terminales. En la primera:

```bash
mosquitto_sub -h 127.0.0.1 -t laboratorio/prueba -u monitor -P "CLAVE_MONITOR" -v
```

En la segunda:

```bash
mosquitto_pub -h 127.0.0.1 -t laboratorio/prueba -m "Broker funcionando" -u monitor -P "CLAVE_MONITOR"
```

**Evidencia de autenticación:** repite la prueba sin `-u` y `-P`. Debe fallar con `Connection Refused: not authorised`.

> En Linux, para que la contraseña no quede en el historial de la shell, antepón un espacio al comando o usa `-P "$(read -rsp 'Clave: ' p; echo $p)"`.

## 3. ESP32: temas y comandos

| Tema | Quién publica | Contenido |
|---|---|---|
| `laboratorio/esp32/datos` | ESP32 (cada 5 s y en cada cambio) | JSON `{"fw","gas","crudo","umbral","ventilando","modo","rssi","uptime_s"}` |
| `laboratorio/esp32/estado` | ESP32 (retenido) / broker (LWT) | `online` / `offline` |
| `laboratorio/esp32/comando` | PC (`monitor`) | `ON`, `OFF`, `AUTO`, `UMBRAL=1600`, `ESTADO` |

Para recibir los datos desde el ESP32 (paso 16 de la guía):

```bash
mosquitto_sub -h 127.0.0.1 -t "laboratorio/esp32/#" -u monitor -P "CLAVE_MONITOR" -v
```

Para enviar comandos al ESP32 (paso 17):

```bash
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

Para una captura de la ACL que no dependa del ESP32, en el PC: `mosquitto_sub -u monitor ... -t "laboratorio/esp32/#" -v` en una terminal y `mosquitto_pub -u esp32lab ... -t laboratorio/esp32/comando -m ON` en otra. El mensaje no aparece.

**Evidencia de LWT:** desconecta la alimentación del ESP32. Después de unos 45 s (1,5 × keepalive) el suscriptor recibe `laboratorio/esp32/estado offline`.

## 4. Fase 2 (opcional): MQTT con TLS

Usa OpenSSL: en Windows viene con Git Bash, y en Linux ya está instalado. Ejecuta los comandos en `C:\mosquitto\certs` (Windows) o en una carpeta temporal (Linux):

```bash
openssl req -new -x509 -days 365 -nodes -keyout ca.key -out ca.crt -subj "/CN=Lab-CA"
openssl req -new -nodes -keyout server.key -out server.csr -subj "/CN=IP_DEL_PC"
echo "subjectAltName=IP:IP_DEL_PC" > san.ext
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out server.crt -days 365 -extfile san.ext
```

Reemplaza `IP_DEL_PC` por la IPv4 real del PC. `ca.key` es la llave de la CA: guárdala fuera del PC compartido y **nunca** la subas al repositorio.

1. En la configuración, descomenta el bloque `listener 8883` y comenta el de 1883. Reinicia el servicio y abre el puerto 8883 en el firewall (perfil privado en Windows, o la misma regla de la subred con 8883 en Linux).
   - En Linux, copia los certificados a `/etc/mosquitto/certs/`: `sudo install -o mosquitto -g mosquitto -m 600 server.key /etc/mosquitto/certs/` y `sudo install -m 644 ca.crt server.crt /etc/mosquitto/certs/`.
2. En `config.h`, pon `MQTT_USE_TLS 1`. En `secrets.h`, pega el contenido de `ca.crt` en `MQTT_CA_CERT`.
3. Para probar desde el PC:
   ```bash
   mosquitto_sub -h IP_DEL_PC -p 8883 --cafile ca.crt -t "laboratorio/esp32/#" -u monitor -P "..." -v
   ```

La IP del certificado (CN/SAN) debe coincidir con `MQTT_SERVER`. Si el PC cambia de IP, hay que regenerar `server.crt`, o bien fijar la IP con una reserva DHCP en el router.
