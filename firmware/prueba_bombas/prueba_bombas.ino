/*
  Prueba de bombas · Monitor Hídrico
  ----------------------------------
  Programa sencillo para comprobar que cada relé y cada mini bomba funcionan,
  antes de cargar el programa completo (monitor_hidrico.ino).

  Mismas conexiones que el programa completo (módulo de relés HW-316, se activa con LOW):
    B1: pozo -> tanque central          (IN1, GPIO 25)
    B2: tanque central -> bebedero 1    (IN2, GPIO 26)
    B3: tanque central -> bebedero 2    (IN3, GPIO 27)

  Qué hace:
    Al encender, prueba las bombas una por una: B1, luego B2, luego B3,
    cada una TIEMPO_PRUEBA_MS milisegundos. Nunca enciende dos a la vez.
    Después puedes usar el Monitor Serie (115200, "Nueva línea") para escribir:
      1, 2 o 3  -> enciende esa bomba TIEMPO_PRUEBA_MS y la apaga sola
      0         -> apaga todo
      t         -> repite la prueba de las 3 bombas

  ANTES DE PROBAR:
    - Pon cada bomba dentro de un vaso o recipiente con agua. Las mini bombas
      se dañan si trabajan en seco.
    - Pon la punta de cada manguera en otro vaso o en el lavabo.
    - Debes oír el "clic" del relé y ver que el LED del canal se prende.
      Si suena el clic pero la bomba no gira: revisa el cable de la bomba,
      COM/NO del relé y la alimentación del MB102.
      Si no suena el clic: revisa VCC, GND y el cable IN del relé.

  Placa en Arduino IDE: "ESP32 Dev Module".
*/

// El HW-316 se activa con LOW. Si tu módulo se activa con HIGH, invierte estas dos líneas.
#define RELE_ON  LOW
#define RELE_OFF HIGH

const unsigned long TIEMPO_PRUEBA_MS = 3000;  // cuánto tiempo gira cada bomba
const unsigned long PAUSA_MS = 1000;          // pausa entre una bomba y la siguiente

const int N_BOM = 3;
const int PIN_BOMBA[N_BOM] = {25, 26, 27};
const char* NOMBRE_BOMBA[N_BOM] = {
  "B1 (pozo -> tanque)",
  "B2 (tanque -> bebedero 1)",
  "B3 (tanque -> bebedero 2)",
};

void apagarTodas() {
  for (int i = 0; i < N_BOM; i++) digitalWrite(PIN_BOMBA[i], RELE_OFF);
}

void probarBomba(int i) {
  apagarTodas();  // nunca dos bombas a la vez
  Serial.print("Encendiendo ");
  Serial.print(NOMBRE_BOMBA[i]);
  Serial.print(" (GPIO ");
  Serial.print(PIN_BOMBA[i]);
  Serial.print(") por ");
  Serial.print(TIEMPO_PRUEBA_MS / 1000.0, 1);
  Serial.println(" s...");

  digitalWrite(PIN_BOMBA[i], RELE_ON);
  delay(TIEMPO_PRUEBA_MS);
  digitalWrite(PIN_BOMBA[i], RELE_OFF);

  Serial.println("  Apagada. ¿Salió agua por la manguera?");
}

void probarTodas() {
  Serial.println("\n--- Prueba de las 3 bombas, una por una ---");
  for (int i = 0; i < N_BOM; i++) {
    probarBomba(i);
    delay(PAUSA_MS);
  }
  Serial.println("--- Fin de la prueba ---");
  Serial.println("Escribe 1, 2 o 3 para probar una bomba, 0 para apagar todo, t para repetir.\n");
}

void setup() {
  // Primero los relés en apagado, para que ninguna bomba arranque sola al encender
  for (int i = 0; i < N_BOM; i++) {
    digitalWrite(PIN_BOMBA[i], RELE_OFF);
    pinMode(PIN_BOMBA[i], OUTPUT);
    digitalWrite(PIN_BOMBA[i], RELE_OFF);
  }

  Serial.begin(115200);
  delay(1500);  // tiempo para abrir el Monitor Serie
  Serial.println("\n=== Prueba de bombas ===");
  Serial.println("Revisa que cada bomba esté dentro del agua.");
  delay(2000);
  probarTodas();
}

void loop() {
  if (!Serial.available()) return;
  char c = Serial.read();

  if (c >= '1' && c <= '0' + N_BOM) {
    probarBomba(c - '1');
  } else if (c == '0') {
    apagarTodas();
    Serial.println("Todas las bombas apagadas.");
  } else if (c == 't' || c == 'T') {
    probarTodas();
  }
  // los demás caracteres (como el salto de línea) se ignoran
}
