# Configuración de red y Blynk IoT

Cubre la rúbrica Eva 3, criterios 2 (proyecto Blynk), 3 (Datastreams y Dashboard), 4 (manejo seguro de datos) y 5 (conectividad).

## 1. Router / red Wi-Fi (guía "Red y ESP32 con Blynk IoT")

1. **Cableado:** puerto LAN del módem → puerto **WAN** del router. Enciende el router y espera a que los LED Power/WAN se estabilicen.
2. **Acceso de administración:** conecta el PC a un puerto LAN (o al Wi-Fi de fábrica) y abre `http://192.168.0.1` o `http://192.168.1.1`.
3. **SSID en 2.4 GHz:** el ESP32 **no** se conecta a redes de 5 GHz. Si el router es dual-band, usa un SSID propio para 2.4 GHz.
4. **Seguridad:** usa **WPA2-PSK (AES)** o WPA2/WPA3 Personal (mixto). La contraseña debe tener 12 caracteres o más, con mayúsculas, minúsculas y números. Desactiva **WPS**.
5. **Cambia la contraseña del usuario `admin`** del router, guarda y reinicia.
6. Toma una captura de la página de seguridad inalámbrica, **tapando la clave**, como evidencia.

| Parámetro | Valor | Motivo |
|---|---|---|
| Frecuencia | 2.4 GHz (802.11 b/g/n) | Es la única banda que soporta la radio del ESP32 |
| Seguridad | WPA2-PSK (AES) | Cifra el enlace Wi-Fi; `WiFi.h` lo soporta de forma nativa |
| Protocolo IoT | Blynk Cloud sobre **TLS (443)** | El firmware usa `BlynkSimpleEsp32_SSL.h` y no TCP/80 en claro |
| Dispositivo | ESP32 DevKit v1/v4 | LED en GPIO 2, MQ-9 en GPIO 34, servo en GPIO 18 |

## 2. Template en blynk.cloud

1. **Developer Zone → My Templates → New Template**
   - Name: `Auto Ventilacion`
   - Hardware: **ESP32**, Connection type: **WiFi**
2. **Datastreams** (todos de tipo *Virtual Pin*). Deben coincidir con `config.h`:

| Pin | Nombre | Tipo | Mín | Máx | Por defecto | Dirección | Widget |
|---|---|---|---|---|---|---|---|
| V0 | Ventilacion manual | Integer | 0 | 1 | 0 | App → ESP32 | Switch |
| V1 | Calidad de aire (MQ-9) | Integer | 0 | 4095 | – | ESP32 → App | Gauge + SuperChart |
| V2 | Ventilando | Integer | 0 | 1 | 0 | ESP32 → App | LED |
| V3 | Umbral | Integer | 500 | 4000 | 1580 | App ↔ ESP32 | Slider |
| V4 | Senal WiFi (RSSI) | Integer | -100 | 0 | – | ESP32 → App | Label/Value |

   En V1 activa la opción **Save raw data / history** para que SuperChart muestre historial.
3. **Events** (opcional, para notificaciones): crea un evento con código `aire_viciado`, tipo *Warning*, y activa *Notifications*. El firmware lo dispara cuando el sensor activa la ventilación. Si no creas el evento, el resto sigue funcionando.
4. **Web Dashboard:** agrega los widgets de la tabla y vincula cada uno a su datastream.
5. **Mobile Dashboard** (app Blynk IoT para Android/iOS): agrega como mínimo el **Switch** en V0 (igual que en la guía) y también un Gauge (V1), un LED (V2) y un Slider (V3).
6. **Save**.

## 3. Dispositivo y credenciales

1. **Devices → New Device → From template → Auto Ventilacion**.
2. Copia los tres valores de **Device Info**: `BLYNK_TEMPLATE_ID`, `BLYNK_TEMPLATE_NAME` y `BLYNK_AUTH_TOKEN`. Anota también el **servidor regional** que aparece ahí (por ejemplo `ny3.blynk.cloud`).
3. Pégalos en `secrets.h`, **no** en el `.ino`. Ese archivo está en `.gitignore`.

### Manejo seguro del Auth Token (criterio 4)

- El token identifica al dispositivo. Quien lo tenga puede leer y escribir sus datastreams.
- Va en `secrets.h`, que no se versiona. El repositorio solo incluye `secrets.example.h` con valores ficticios.
- Antes de cada commit, verifica con `git status` que `secrets.h` **no** aparezca.
- El firmware **no** imprime el token ni la clave Wi-Fi por Serial. La librería se compila sin `BLYNK_DEBUG`.
- La conexión con Blynk usa **TLS (puerto 443)**: el token no viaja en texto plano.
- **Si el token se filtra** (captura de pantalla, commit, chat), ve a Device Info → **Reset Auth Token**, actualiza `secrets.h` y vuelve a cargar el firmware.
- Las capturas para el informe deben tapar el token y las claves.

## 4. Verificación de conectividad (criterio 5)

En el Monitor Serie a 115200 debe aparecer algo como:

```
Wi-Fi OK. IP ESP32: 192.168.x.x  RSSI: -55 dBm
[...] Connecting to blynk.cloud:443
[...] Ready (ping: XXms).
[BLYNK] Conectado a la nube
```

En la consola Blynk el dispositivo debe figurar **Online**. Para probar la estabilidad, deja el equipo encendido 10 minutos y revisa que no haya desconexiones en *Device → Timeline*. Luego apaga el router 30 segundos: el equipo debe seguir ventilando en modo local y reconectarse solo cuando vuelva la red.
