/*
  PRUEBA DE UNA BOMBA · ESP32 + relé HW-316 + MB102
  -------------------------------------------------
  Conexión:
    Relé VCC  -> riel 5 V de la protoboard
    Relé GND  -> riel GND
    Relé IN1  -> GPIO 25 del ESP32
    ESP32 GND -> riel GND
    Relé K1: COM -> riel 5 V ; NO -> cable rojo de la bomba
    Cable negro de la bomba -> riel GND

  Uso: abre el Monitor Serie a 115200 (con "Nueva línea" o "Sin ajuste de línea") y escribe:
    1 = encender la bomba (se apaga sola a los 10 s, por seguridad)
    0 = apagar la bomba
    p = prueba automática: 3 veces, 2 s encendida y 2 s apagada
*/

const int PIN_RELE = 25;              // IN1 del relé
#define RELE_ON  LOW                  // el HW-316 se activa con LOW
#define RELE_OFF HIGH
const unsigned long MAX_ENCENDIDA_MS = 10000;

bool encendida = false;
unsigned long tEncendida = 0;

void bomba(bool on) {
  encendida = on;
  digitalWrite(PIN_RELE, on ? RELE_ON : RELE_OFF);
  if (on) tEncendida = millis();
  Serial.println(on ? "Bomba ENCENDIDA (se apaga sola en 10 s)" : "Bomba APAGADA");
}

void menu() {
  Serial.println();
  Serial.println("=== Prueba de bomba ===");
  Serial.println("1 = encender   0 = apagar   p = prueba automatica (3 pulsos)");
}

void pruebaAutomatica() {
  Serial.println("Prueba automatica: 3 pulsos de 2 s");
  for (int i = 1; i <= 3; i++) {
    Serial.print("Pulso "); Serial.println(i);
    bomba(true);
    delay(2000);
    bomba(false);
    delay(2000);
  }
  Serial.println("Prueba terminada.");
}

void setup() {
  digitalWrite(PIN_RELE, RELE_OFF);   // primero apagado, para que no arranque sola
  pinMode(PIN_RELE, OUTPUT);
  digitalWrite(PIN_RELE, RELE_OFF);
  Serial.begin(115200);
  delay(300);
  menu();
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == '1') bomba(true);
    else if (c == '0') bomba(false);
    else if (c == 'p' || c == 'P') pruebaAutomatica();
    else if (c != '\n' && c != '\r' && c != ' ') menu();
  }
  if (encendida && millis() - tEncendida >= MAX_ENCENDIDA_MS) {
    bomba(false);
    Serial.println("(apagado automatico por seguridad)");
  }
}
