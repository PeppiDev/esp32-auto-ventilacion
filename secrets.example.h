// ========================================================
// PLANTILLA DE CREDENCIALES — copiar a "secrets.h" y completar.
//
//   cp secrets.example.h secrets.h        (Linux / macOS / Git Bash)
//   copy secrets.example.h secrets.h      (CMD de Windows)
//
// "secrets.h" está en .gitignore: NUNCA se sube al repositorio.
// Este archivo de ejemplo SÍ se sube y no debe contener datos reales.
// ========================================================
#pragma once

// --- Wi-Fi (red 2.4 GHz, WPA2-PSK AES; el ESP32 no ve redes de 5 GHz) ---
#define WIFI_SSID      "NOMBRE_DE_TU_RED_2GHZ"
#define WIFI_PASSWORD  "CLAVE_WIFI"

// --- Blynk IoT (blynk.cloud > Devices > tu dispositivo > Device Info) ---
// Deben definirse ANTES de incluir la librería Blynk (el .ino ya lo hace).
#define BLYNK_TEMPLATE_ID    "TMPLxxxxxxxxx"
#define BLYNK_TEMPLATE_NAME  "Auto Ventilacion"
#define BLYNK_AUTH_TOKEN     "TU_AUTH_TOKEN"
// Servidor regional que muestra Device Info (ej. "ny3.blynk.cloud", "sgp1.blynk.cloud").
// "blynk.cloud" funciona en la mayoría de los casos.
#define BLYNK_SERVER         "blynk.cloud"

// --- Broker MQTT (Mosquitto en el PC del laboratorio, Windows o Linux) ---
#define MQTT_SERVER    "192.168.1.50"   // IPv4 REAL del PC (ipconfig / hostname -I). Ejemplo, no copiar.
#define MQTT_USER      "esp32lab"
#define MQTT_PASSWORD  "CONTRASENA_MQTT"

// --- Solo si MQTT_USE_TLS = 1 en config.h (fase 2, puerto 8883) ---
// Pegar el contenido de ca.crt (certificado de la CA propia del laboratorio).
#define MQTT_CA_CERT \
  "-----BEGIN CERTIFICATE-----\n" \
  "PEGAR_AQUI_EL_CERTIFICADO_DE_LA_CA\n" \
  "-----END CERTIFICATE-----\n"
