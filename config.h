// ========================================================
// CONFIGURACIÓN NO SECRETA (sí se sube al repositorio)
// Las credenciales van en secrets.h (ver secrets.example.h).
// ========================================================
#pragma once

#define FW_VERSION "2.0.0"

// --- Funcionalidades (1 = activada, 0 = desactivada) ---
// Con ambas en 0 el equipo funciona igual que el prototipo original (solo local).
#define ENABLE_BLYNK  1   // Control y monitoreo desde Blynk IoT (TLS, puerto 443)
#define ENABLE_MQTT   1   // Telemetría y comandos vía broker Mosquitto
#define MQTT_USE_TLS  0   // 0 = TCP/1883 (laboratorio)  1 = TLS/8883 (fase 2)

// --- Pines ---
const int PIN_MQ9   = 34; // AO del MQ-9 (solo entrada, ADC1: compatible con Wi-Fi)
const int PIN_SERVO = 18; // Señal PWM del servo (cable naranja)
const int PIN_LED   = 2;  // LED amarillo indicador (+ LED integrado de la placa)

// --- Servo (PWM nativo LEDC) ---
const int SERVO_CHANNEL  = 0;     // Solo se usa con core ESP32 2.x
const int SERVO_FREQ     = 50;    // 50 Hz para servos estándar
const int SERVO_RES_BITS = 16;
const int DUTY_MIN       = 1638;  // ~0°
const int DUTY_MAX       = 7864;  // ~180°
const unsigned long SERVO_INTERVAL = 15; // ms entre pasos del abanico

// --- Control de ventilación ---
// Medido en la placa real con el servo detenido: aire normal entre ~1170 y
// ~1520 (deriva con el calentamiento) y un soplido sube +200 a +330.
// Ajustar en vivo con el slider V3 o "UMBRAL=n" por MQTT.
const int UMBRAL_DEFECTO = 1450;
const int UMBRAL_MIN     = 500;   // Rango aceptado al cambiarlo por Blynk/MQTT
const int UMBRAL_MAX     = 4000;
const int HISTERESIS     = 60;    // Apaga cuando filtrado < umbral - histéresis
const unsigned long TIEMPO_MINIMO_VENTILANDO = 6000;  // ms
// El servo comparte los 5V con el MQ-9 y en marcha eleva la lectura +90 a +400,
// por eso el aire se evalúa con el servo detenido durante esta pausa.
const unsigned long TIEMPO_ESTABILIZACION    = 600;   // ms
// Evita quedar encendido si el aire normal deriva a la banda de histéresis.
const unsigned long TIEMPO_MAXIMO_VENTILANDO = 60000; // ms
const int ADC_SATURADO = 4000;    // AO cerca de 3.3V: límite del GPIO 34 (sin divisor)

// --- Comunicaciones ---
const unsigned long WIFI_TIMEOUT_INICIAL = 15000; // ms esperando Wi-Fi en setup()
const unsigned long WIFI_REINTENTO_MS    = 10000;
const unsigned long MQTT_REINTENTO_MS    = 5000;
const unsigned long MQTT_PUBLICAR_MS     = 5000;  // Telemetría periódica MQTT
const unsigned long BLYNK_PUBLICAR_MS    = 2000;  // Respeta el límite de mensajes de Blynk
const unsigned long LOG_SERIAL_MS        = 150;   // Serial Plotter

// --- MQTT ---
#if MQTT_USE_TLS
const int MQTT_PORT = 8883;
#else
const int MQTT_PORT = 1883;
#endif
#define MQTT_CLIENT_ID   "ESP32-VENTILACION-01"
#define TOPIC_DATOS      "laboratorio/esp32/datos"     // ESP32 publica telemetría (JSON)
#define TOPIC_COMANDO    "laboratorio/esp32/comando"   // ESP32 recibe comandos
#define TOPIC_ESTADO     "laboratorio/esp32/estado"    // online/offline (retenido + LWT)
const unsigned int MQTT_MAX_PAYLOAD = 32;              // Comandos más largos se rechazan

// --- Blynk: pines virtuales (deben coincidir con los Datastreams del template) ---
// V0  Ventilación manual  Integer 0-1     (Switch)     App -> ESP32
// V1  Calidad de aire     Integer 0-4095  (Gauge/Chart) ESP32 -> App
// V2  Ventilando          Integer 0-1     (LED)        ESP32 -> App
// V3  Umbral              Integer 500-4000 (Slider)    App <-> ESP32
// V4  Señal Wi-Fi (RSSI)  Integer -100-0  (Label)      ESP32 -> App
#define VPIN_MANUAL     V0
#define VPIN_GAS        V1
#define VPIN_VENTILANDO V2
#define VPIN_UMBRAL     V3
#define VPIN_RSSI       V4
#define BLYNK_EVENTO_ALERTA "aire_viciado" // Event code opcional en el template
