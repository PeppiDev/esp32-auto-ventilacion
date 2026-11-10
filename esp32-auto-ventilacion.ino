// ========================================================
// PROTOTIPO AUTO-VENTILACIÓN: ESP32 + MQ-9 + SERVO + LED
// v2: control remoto vía Blynk IoT (TLS) y MQTT (Mosquitto)
//
// - El control local (sensor -> servo/LED) funciona SIEMPRE,
//   aunque no haya Wi-Fi, Blynk ni broker (disponibilidad).
// - Credenciales fuera del código: secrets.h (no versionado).
// - Comandos remotos validados (longitud, formato y rango).
// - El Monitor Serie nunca imprime contraseñas ni tokens.
// ========================================================

#if !__has_include("secrets.h")
#error "Falta secrets.h: copia secrets.example.h a secrets.h y completa tus credenciales."
#endif
#include "secrets.h"   // Debe ir antes de Blynk (define BLYNK_TEMPLATE_ID, etc.)
#include "config.h"

#include <WiFi.h>

#if ENABLE_BLYNK
  #define BLYNK_PRINT Serial                 // Estado de conexión (no imprime el token)
  #define BLYNK_FIRMWARE_VERSION FW_VERSION
  #include <BlynkSimpleEsp32_SSL.h>         // Blynk sobre TLS (443), no TCP plano (80)
#endif

#if ENABLE_MQTT
  #include <PubSubClient.h>
  #if MQTT_USE_TLS
    #include <WiFiClientSecure.h>
    WiFiClientSecure mqttNet;
  #else
    WiFiClient mqttNet;
  #endif
  PubSubClient mqtt(mqttNet);
#endif

// --- Estado del sistema ---
int  umbral = UMBRAL_DEFECTO;
bool modoManual = false;          // true = forzado desde Blynk (V0) o MQTT ("ON")
bool ventilando = false;
bool estadoCambio = false;        // Forzar envío inmediato de telemetría
unsigned long tiempoInicioVentilacion = 0;

int lecturaInstantanea = 0;
long lecturaFiltrada = 1450;      // Filtro pasa-bajas

int servoPos = 0;
int servoDir = 10;
unsigned long prevServoTime = 0;

// ========================================================
// Actuadores
// ========================================================
void moverServo(int angulo) {
  int duty = map(angulo, 0, 180, DUTY_MIN, DUTY_MAX);
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcWrite(PIN_SERVO, duty);
#else
  ledcWrite(SERVO_CHANNEL, duty);
#endif
}

void activarVentilacion(const char* origen) {
  if (!ventilando) {
    Serial.printf("--> [ON] Ventilador activado (%s)\n", origen);
    estadoCambio = true;
#if ENABLE_BLYNK
    if (strcmp(origen, "sensor") == 0 && Blynk.connected()) {
      Blynk.logEvent(BLYNK_EVENTO_ALERTA, "Aire viciado detectado: ventilando");
    }
#endif
  }
  ventilando = true;
  tiempoInicioVentilacion = millis();
  digitalWrite(PIN_LED, HIGH);
}

void desactivarVentilacion(const char* motivo) {
  if (ventilando) {
    Serial.printf("--> [OK] Ventilador OFF (%s)\n", motivo);
    estadoCambio = true;
  }
  ventilando = false;
  digitalWrite(PIN_LED, LOW);
  moverServo(0);
  servoPos = 0;
  servoDir = 10;
}

// ========================================================
// Comandos remotos (comunes a Blynk y MQTT)
// ========================================================
void setModoManual(bool on, const char* origen) {
  if (on == modoManual) return;
  modoManual = on;
  Serial.printf("[CMD] Modo %s (%s)\n", on ? "MANUAL ON" : "AUTO", origen);
  if (on) {
    activarVentilacion(origen);
  } else {
    // Vuelve a automático: deja que el sensor decida sin esperar el tiempo mínimo.
    tiempoInicioVentilacion = millis() - TIEMPO_MINIMO_VENTILANDO;
  }
  estadoCambio = true;
}

bool setUmbral(long valor, const char* origen) {
  if (valor < UMBRAL_MIN || valor > UMBRAL_MAX) {
    Serial.printf("[CMD] Umbral rechazado (%s): fuera de rango %d-%d\n", origen, UMBRAL_MIN, UMBRAL_MAX);
    return false;
  }
  if (valor != umbral) {
    umbral = (int)valor;
    Serial.printf("[CMD] Umbral = %d (%s)\n", umbral, origen);
    estadoCambio = true;
  }
  return true;
}

// Convierte texto a entero estricto (solo dígitos). Evita que "12abc" pase como 12.
bool parseEntero(const char* s, long& out) {
  if (*s == '\0') return false;
  long v = 0;
  for (const char* p = s; *p; p++) {
    if (*p < '0' || *p > '9') return false;
    v = v * 10 + (*p - '0');
    if (v > 100000) return false;
  }
  out = v;
  return true;
}

// ========================================================
// Wi-Fi
// ========================================================
void iniciarWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("esp32-ventilacion");
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);   // SSID/clave no se imprimen

  Serial.print("Conectando a Wi-Fi");
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < WIFI_TIMEOUT_INICIAL) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Wi-Fi OK. IP ESP32: ");
    Serial.print(WiFi.localIP());
    Serial.printf("  RSSI: %d dBm\n", WiFi.RSSI());
  } else {
    Serial.println("Wi-Fi no disponible: modo local (se reintenta en segundo plano)");
  }
}

void mantenerWiFi() {
  static unsigned long ultimoIntento = 0;
  static bool estabaConectado = true;
  bool conectado = WiFi.status() == WL_CONNECTED;
  if (conectado && !estabaConectado) {
    Serial.print("Wi-Fi reconectado. IP: ");
    Serial.println(WiFi.localIP());
  }
  estabaConectado = conectado;
  if (!conectado && millis() - ultimoIntento >= WIFI_REINTENTO_MS) {
    ultimoIntento = millis();
    Serial.println("Wi-Fi caído: reintentando...");
    WiFi.reconnect();
  }
}

// ========================================================
// MQTT (Mosquitto)
// ========================================================
#if ENABLE_MQTT
void publicarTelemetriaMQTT() {
  if (!mqtt.connected()) return;
  char json[200];
  snprintf(json, sizeof(json),
           "{\"fw\":\"%s\",\"gas\":%ld,\"crudo\":%d,\"umbral\":%d,\"ventilando\":%d,"
           "\"modo\":\"%s\",\"rssi\":%d,\"uptime_s\":%lu}",
           FW_VERSION, lecturaFiltrada, lecturaInstantanea, umbral, ventilando ? 1 : 0,
           modoManual ? "manual" : "auto", WiFi.RSSI(), millis() / 1000);
  mqtt.publish(TOPIC_DATOS, json);
}

void callbackMQTT(char* topic, byte* payload, unsigned int length) {
  Serial.print("[MQTT] Tema recibido: ");
  Serial.println(topic);

  // 1) Validación de longitud
  if (length == 0 || length > MQTT_MAX_PAYLOAD) {
    Serial.printf("[MQTT] Rechazado: largo %u (max %u)\n", length, MQTT_MAX_PAYLOAD);
    return;
  }
  // 2) Solo ASCII imprimible; se normaliza a mayúsculas y sin espacios en los extremos
  char msg[MQTT_MAX_PAYLOAD + 1];
  for (unsigned int i = 0; i < length; i++) {
    char c = (char)payload[i];
    if (c < 32 || c > 126) {
      Serial.println("[MQTT] Rechazado: caracteres no permitidos");
      return;
    }
    msg[i] = toupper(c);
  }
  msg[length] = '\0';
  char* cmd = msg;
  while (*cmd == ' ') cmd++;
  for (int i = strlen(cmd) - 1; i >= 0 && cmd[i] == ' '; i--) cmd[i] = '\0';

  Serial.print("[MQTT] Mensaje: ");
  Serial.println(cmd);

  // 3) Lista blanca de comandos
  long valor;
  if (strcmp(cmd, "ON") == 0) {
    setModoManual(true, "mqtt");
  } else if (strcmp(cmd, "OFF") == 0 || strcmp(cmd, "AUTO") == 0) {
    setModoManual(false, "mqtt");
  } else if (strncmp(cmd, "UMBRAL=", 7) == 0 && parseEntero(cmd + 7, valor)) {
    setUmbral(valor, "mqtt");
  } else if (strcmp(cmd, "ESTADO") == 0) {
    publicarTelemetriaMQTT();
  } else {
    Serial.println("[MQTT] Comando no reconocido (validos: ON, OFF, AUTO, UMBRAL=n, ESTADO)");
  }
}

void iniciarMQTT() {
#if MQTT_USE_TLS
  mqttNet.setCACert(MQTT_CA_CERT);   // Verifica la identidad del broker
#endif
  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt.setCallback(callbackMQTT);
  mqtt.setKeepAlive(30);
  mqtt.setBufferSize(256);
}

// Reconexión NO bloqueante: un intento cada MQTT_REINTENTO_MS
void mantenerMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (mqtt.connected()) {
    mqtt.loop();
    return;
  }
  static unsigned long ultimoIntento = 0;
  if (ultimoIntento != 0 && millis() - ultimoIntento < MQTT_REINTENTO_MS) return;
  ultimoIntento = millis();

  Serial.printf("Conectando a MQTT (puerto %d, %s)... ", MQTT_PORT, MQTT_USE_TLS ? "TLS" : "sin TLS");
  // Last Will: si el ESP32 se desconecta sin avisar, el broker publica "offline"
  if (mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD, TOPIC_ESTADO, 1, true, "offline")) {
    Serial.println("conectado");
    mqtt.publish(TOPIC_ESTADO, "online", true);
    mqtt.subscribe(TOPIC_COMANDO, 1);
    publicarTelemetriaMQTT();
  } else {
    // rc 4/5 = credenciales rechazadas / no autorizado; -2 = broker inalcanzable
    Serial.printf("error, estado: %d\n", mqtt.state());
  }
}
#endif

// ========================================================
// Blynk IoT
// ========================================================
#if ENABLE_BLYNK
// Lecturas periódicas. Los controles (V0, V3) solo se reenvían si cambiaron,
// para no "pelear" con el usuario mientras mueve el slider.
void publicarBlynk(bool incluirControles) {
  if (!Blynk.connected()) return;
  Blynk.virtualWrite(VPIN_GAS, lecturaFiltrada);
  Blynk.virtualWrite(VPIN_VENTILANDO, ventilando ? 1 : 0);
  Blynk.virtualWrite(VPIN_RSSI, WiFi.RSSI());
  if (incluirControles) {
    Blynk.virtualWrite(VPIN_MANUAL, modoManual ? 1 : 0);
    Blynk.virtualWrite(VPIN_UMBRAL, umbral);
  }
}

BLYNK_CONNECTED() {
  Serial.println("[BLYNK] Conectado a la nube");
  Blynk.syncVirtual(VPIN_UMBRAL);   // Recupera el umbral guardado en el servidor
  Blynk.virtualWrite(VPIN_MANUAL, modoManual ? 1 : 0);
  publicarBlynk(false);
}

// V0: Switch de ventilación manual (0/1)
BLYNK_WRITE(VPIN_MANUAL) {
  int v = param.asInt();
  if (v == 0 || v == 1) {
    setModoManual(v == 1, "blynk");
  } else {
    Serial.println("[BLYNK] V0 rechazado: solo 0 o 1");
  }
}

// V3: Slider de umbral
BLYNK_WRITE(VPIN_UMBRAL) {
  if (!setUmbral(param.asInt(), "blynk")) {
    Blynk.virtualWrite(VPIN_UMBRAL, umbral);   // Devuelve el valor vigente al widget
  }
}
#endif

// ========================================================
// setup / loop
// ========================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  pinMode(PIN_MQ9, INPUT);

#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcAttach(PIN_SERVO, SERVO_FREQ, SERVO_RES_BITS);
#else
  ledcSetup(SERVO_CHANNEL, SERVO_FREQ, SERVO_RES_BITS);
  ledcAttachPin(PIN_SERVO, SERVO_CHANNEL);
#endif
  moverServo(0);

  // Test visual: 3 parpadeos
  for (int i = 0; i < 3; i++) {
    digitalWrite(PIN_LED, HIGH); delay(80);
    digitalWrite(PIN_LED, LOW);  delay(80);
  }
  lecturaFiltrada = analogRead(PIN_MQ9);

  Serial.println("\n==================================================");
  Serial.printf("  AUTO-VENTILACION v%s  (Blynk:%s MQTT:%s)\n", FW_VERSION,
                ENABLE_BLYNK ? "si" : "no", ENABLE_MQTT ? (MQTT_USE_TLS ? "TLS" : "si") : "no");
  Serial.println("==================================================");

  iniciarWiFi();

#if ENABLE_MQTT
  iniciarMQTT();
#endif
#if ENABLE_BLYNK
  Blynk.config(BLYNK_AUTH_TOKEN, BLYNK_SERVER, 443);
  if (WiFi.status() == WL_CONNECTED) {
    Blynk.connect(5000);   // Con timeout: no bloquea el control local indefinidamente
  }
#endif
}

void loop() {
  unsigned long ahora = millis();

  // ---- 1. Control local (siempre activo) ----
  lecturaInstantanea = analogRead(PIN_MQ9);
  lecturaFiltrada = (lecturaFiltrada * 8 + lecturaInstantanea * 2) / 10;

  bool aireViciado = lecturaFiltrada > umbral || lecturaInstantanea > umbral;
  if (modoManual) {
    if (!ventilando) activarVentilacion("manual");
  } else if (aireViciado) {
    activarVentilacion("sensor");     // Reinicia el cronómetro mínimo
  } else if (ventilando) {
    bool tiempoCumplido = (ahora - tiempoInicioVentilacion >= TIEMPO_MINIMO_VENTILANDO);
    bool aireLimpio = (lecturaFiltrada < umbral - HISTERESIS);
    if (tiempoCumplido && aireLimpio) desactivarVentilacion("aire limpio");
  }

  if (ventilando && ahora - prevServoTime >= SERVO_INTERVAL) {
    prevServoTime = ahora;
    servoPos += servoDir;
    if (servoPos >= 180 || servoPos <= 0) servoDir = -servoDir;
    moverServo(servoPos);
  }

  // ---- 2. Comunicaciones ----
  mantenerWiFi();

#if ENABLE_MQTT
  mantenerMQTT();
  static unsigned long ultimoMQTT = 0;
  bool tocaMQTT = ahora - ultimoMQTT >= MQTT_PUBLICAR_MS;
#else
  bool tocaMQTT = false;
#endif

#if ENABLE_BLYNK
  if (WiFi.status() == WL_CONNECTED) Blynk.run();
  static unsigned long ultimoBlynk = 0;
  bool tocaBlynk = ahora - ultimoBlynk >= BLYNK_PUBLICAR_MS;
#else
  bool tocaBlynk = false;
#endif

  if (estadoCambio || tocaMQTT) {
#if ENABLE_MQTT
    ultimoMQTT = ahora;
    publicarTelemetriaMQTT();
#endif
  }
  if (estadoCambio || tocaBlynk) {
#if ENABLE_BLYNK
    ultimoBlynk = ahora;
    publicarBlynk(estadoCambio);
#endif
  }
  estadoCambio = false;

  // ---- 3. Telemetría Serial (compatible con Serial Plotter) ----
  static unsigned long lastLog = 0;
  if (ahora - lastLog >= LOG_SERIAL_MS) {
    lastLog = ahora;
    Serial.printf("Crudo:%d Filtrado:%ld Umbral:%d Ventilando:%d\n",
                  lecturaInstantanea, lecturaFiltrada, umbral, ventilando ? 1800 : 1350);
  }
}
