// ========================================================
// PROTOTIPO AUTO-VENTILACIÓN: ESP32 + MQ-9 + SERVO + LED
// Calibrado con valores reales y tiempo mínimo de extracción
// ========================================================

// --- Definición de Pines ---
const int PIN_MQ9   = 34; // Pin AO del MQ-9
const int PIN_SERVO = 18; // Señal del servo (cable naranja)
const int PIN_LED   = 2;  // LED amarillo indicador

// --- Configuración PWM Servo (Nativo ESP32) ---
const int SERVO_CHANNEL  = 0;
const int SERVO_FREQ     = 50;     // 50 Hz para servos estándar
const int SERVO_RES_BITS = 16;    // Resolución de 16 bits
const int DUTY_MIN       = 1638;  // ~0°
const int DUTY_MAX       = 7864;  // ~180°

// --- Parámetros de Sensibilidad y Control ---
const int UMBRAL_DISPARO = 1580; // Se activa al exhalar sobre el sensor
const unsigned long TIEMPO_MINIMO_VENTILANDO = 6000; // Mínimo 6 segundos activo
unsigned long tiempoInicioVentilacion = 0;

bool ventilando = false;
int servoPos = 0;
int servoDir = 10;
unsigned long prevServoTime = 0;
const unsigned long SERVO_INTERVAL = 15; // Velocidad del abanico (ms entre pasos)

// Filtro de promedio para suavizar fluctuaciones de temperatura
long lecturaFiltrada = 1450;

// Función para posicionar el servo
void moverServo(int angulo) {
  int duty = map(angulo, 0, 180, DUTY_MIN, DUTY_MAX);
  ledcWrite(PIN_SERVO, duty);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  pinMode(PIN_MQ9, INPUT);

  // Inicialización de PWM nativo según versión del Core ESP32
  #if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    ledcAttach(PIN_SERVO, SERVO_FREQ, SERVO_RES_BITS);
  #else
    ledcSetup(SERVO_CHANNEL, SERVO_FREQ, SERVO_RES_BITS);
    ledcAttachPin(PIN_SERVO, SERVO_CHANNEL);
  #endif

  moverServo(0); // Posición inicial reposo

  // Test de confirmación visual (3 parpadeos del LED)
  for (int i = 0; i < 3; i++) {
    digitalWrite(PIN_LED, HIGH);
    delay(80);
    digitalWrite(PIN_LED, LOW);
    delay(80);
  }

  lecturaFiltrada = analogRead(PIN_MQ9);

  Serial.println("\n==================================================");
  Serial.println("  SISTEMA DE AUTO-VENTILACION INICIADO (115200)   ");
  Serial.println("==================================================");
}

void loop() {
  int lecturaInstantanea = analogRead(PIN_MQ9);

  // Filtro pasa-bajas para evitar falsas caídas por choque de aire frío
  lecturaFiltrada = (lecturaFiltrada * 8 + lecturaInstantanea * 2) / 10;

  unsigned long ahora = millis();

  // 1. ACTIVACIÓN: Si la lectura sube del umbral
  if (lecturaFiltrada > UMBRAL_DISPARO || lecturaInstantanea > UMBRAL_DISPARO) {
    ventilando = true;
    tiempoInicioVentilacion = ahora; // Reinicia o fija el cronómetro de 6 segundos
    digitalWrite(PIN_LED, HIGH);
  }

  // 2. APAGADO: Solo si ya cumplió los 6 segundos mínimos Y el aire ya bajó
  if (ventilando) {
    bool tiempoCumplido = (ahora - tiempoInicioVentilacion >= TIEMPO_MINIMO_VENTILANDO);
    bool aireLimpio = (lecturaFiltrada < 1520);

    if (tiempoCumplido && aireLimpio) {
      ventilando = false;
      digitalWrite(PIN_LED, LOW);
      moverServo(0); // Vuelve a reposo
      Serial.println("--> [OK] Ciclo completado. Aire limpio. Ventilador OFF.");
    }
  }

  // 3. Movimiento oscilante del servo mientras esté activo
  if (ventilando) {
    if (ahora - prevServoTime >= SERVO_INTERVAL) {
      prevServoTime = ahora;
      servoPos += servoDir;
      if (servoPos >= 180 || servoPos <= 0) {
        servoDir = -servoDir; // Invertir sentido de giro
      }
      moverServo(servoPos);
    }
  }

  // 4. Telemetría por Monitor Serie cada 150ms
  static unsigned long lastLog = 0;
  if (ahora - lastLog >= 150) {
    lastLog = ahora;
    Serial.print("Crudo:");
    Serial.print(lecturaInstantanea);
    Serial.print(" Filtrado:");
    Serial.print(lecturaFiltrada);
    Serial.print(" Umbral:");
    Serial.print(UMBRAL_DISPARO);
    Serial.print(" Ventilando:");
    Serial.println(ventilando ? 1800 : 1350);
  }
}
