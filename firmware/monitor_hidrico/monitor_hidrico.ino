/*
  Monitor Hídrico · Maqueta IoT con ESP32
  ---------------------------------------
  MAQUETA BÁSICA (3 bombas):
    - 3 sensores HC-SR04: tanque central, bebedero 1 y bebedero 2 (el pozo no lleva sensor)
    - 1 módulo de 4 relés de 5 V HW-316 (se usan 3 canales):
        B1: pozo -> tanque central          (IN1, GPIO 25)
        B2: tanque central -> bebedero 1    (IN2, GPIO 26)
        B3: tanque central -> bebedero 2    (IN3, GPIO 27)

  Qué hace:
    - Mide el nivel de cada depósito (%) con el ultrasonido.
    - Enciende la bomba cuando el destino baja de NIVEL_ENCENDER y la apaga sola en NIVEL_APAGAR.
      Aunque la enciendas a mano, se apaga sola al llenarse. Si ya está lleno, no enciende.
    - No deja trabajar una bomba en seco (origen casi vacío).
    - Detecta fugas: si una bomba lleva VENTANA_FUGA_MS encendida y el destino no sube, la apaga y la bloquea.
    - Nunca enciende más de MAX_BOMBAS_A_LA_VEZ bombas (para no sobrecargar el MB102).
    - Crea una página web en el ESP32: ábrela desde el celular con la IP que sale en el Monitor Serie.
      Si se cae el WiFi, el control sigue funcionando igual.

  El pozo no tiene sensor: si se queda sin agua, el tanque no sube mientras B1 trabaja,
  la detección de fugas lo nota a los VENTANA_FUGA_MS y apaga B1 (así no trabaja en seco mucho tiempo).
  Para agregar más depósitos o bombas, agrega filas en dep[] y bom[].
  Placa en Arduino IDE: "ESP32 Dev Module". Monitor Serie a 115200.
*/

#include <WiFi.h>
#include <WebServer.h>

// ================== 1. AJUSTES ==================
const char* WIFI_SSID = "NOMBRE_DE_TU_WIFI";
const char* WIFI_PASS = "CLAVE_DE_TU_WIFI";

// El módulo HW-316 se activa con LOW: el relé se enciende cuando el pin del ESP32 está en 0 V.
// Si usan un módulo que se activa con HIGH, inviertan estas dos líneas.
#define RELE_ON  LOW
#define RELE_OFF HIGH

const float NIVEL_ENCENDER = 30.0;   // % : debajo de esto se enciende la bomba (modo automático)
const float NIVEL_APAGAR   = 90.0;   // % : al llegar aquí se apaga sola
const float NIVEL_SECO     = 10.0;   // % : si el origen baja de esto, la bomba se detiene
const unsigned long VENTANA_FUGA_MS = 20000;  // tiempo para revisar si el nivel sube
const float SUBIDA_MINIMA = 3.0;     // % que debe subir el destino en esa ventana
const int MAX_BOMBAS_A_LA_VEZ = 2;

// ================== 2. DEPÓSITOS ==================
// cmVacio: distancia del sensor al fondo (depósito vacío)
// cmLleno: distancia del sensor al agua cuando está lleno (mínimo 3 cm)
// Mídelos con el Monitor Serie y corrígelos aquí (paso de calibración).
struct Deposito {
  const char* nombre;
  int trig, echo;
  float cmVacio, cmLleno;
  float cm;      // última distancia medida
  float nivel;   // %
  bool ok;       // el sensor respondió
};

Deposito dep[] = {
  // nombre               TRIG ECHO  vacío lleno
  {"Tanque central",       5,  35,  21.0,  5.0, 0, 0, false},   // índice 0
  {"Bebedero 1",          19,  39,  13.0,  5.0, 0, 0, false},   // índice 1 (corral derecho)
  {"Bebedero 2",          18,  36,  13.0,  5.0, 0, 0, false},   // índice 2 (corral izquierdo)
};
const int N_DEP = sizeof(dep) / sizeof(dep[0]);

// ================== 3. BOMBAS ==================
struct Bomba {
  const char* nombre;
  int pin;
  int origen;     // índice en dep[] (-1 si no tiene, ej. la red de agua)
  int destino;    // índice en dep[]
  bool on, bloqueada, manual;
  unsigned long tVentana, esperaHasta;
  float nivelVentana;
};

Bomba bom[] = {
  // nombre          GPIO origen destino
  {"B1 (pozo -> tanque)",     25, -1, 0, false, false, false, 0, 0, 0},  // relé IN1 (el pozo no tiene sensor)
  {"B2 (tanque -> bebedero 1)", 26, 0, 1, false, false, false, 0, 0, 0},  // relé IN2
  {"B3 (tanque -> bebedero 2)", 27, 0, 2, false, false, false, 0, 0, 0},  // relé IN3
};
const int N_BOM = sizeof(bom) / sizeof(bom[0]);

bool modoAuto = true;
WebServer server(80);

// ================== 4. ALERTAS ==================
const int N_ALERTAS = 8;
String alertas[N_ALERTAS];
int alertaPos = 0;

void alerta(const String& txt) {
  String linea = "[" + String(millis() / 1000) + " s] " + txt;
  Serial.println(linea);
  alertas[alertaPos] = linea;
  alertaPos = (alertaPos + 1) % N_ALERTAS;
}

// ================== 5. SENSORES ==================
float medirCm(int trig, int echo) {
  digitalWrite(trig, LOW);
  delayMicroseconds(3);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);
  unsigned long us = pulseIn(echo, HIGH, 30000UL);  // máx. ~5 m
  if (us == 0) return -1;
  return us * 0.0343 / 2.0;
}

// 5 lecturas y se queda con la del medio (quita lecturas locas)
float medianaCm(int trig, int echo) {
  float v[5];
  int n = 0;
  for (int i = 0; i < 5; i++) {
    float d = medirCm(trig, echo);
    if (d > 1.5 && d < 400) v[n++] = d;
    delay(25);
  }
  if (n < 3) return -1;
  for (int i = 1; i < n; i++) {        // ordenar
    float k = v[i];
    int j = i - 1;
    while (j >= 0 && v[j] > k) { v[j + 1] = v[j]; j--; }
    v[j + 1] = k;
  }
  return v[n / 2];
}

void leerDeposito(int i) {
  Deposito& d = dep[i];
  float cm = medianaCm(d.trig, d.echo);
  bool antes = d.ok;
  if (cm < 0) {
    d.ok = false;
    if (antes) alerta(String("Sensor de ") + d.nombre + " sin lectura. Revisa VCC, GND, TRIG y ECHO.");
    return;
  }
  d.ok = true;
  d.cm = cm;
  float pct = (d.cmVacio - cm) / (d.cmVacio - d.cmLleno) * 100.0;
  d.nivel = constrain(pct, 0.0f, 100.0f);
  if (!antes) alerta(String("Sensor de ") + d.nombre + " OK: " + String(cm, 1) + " cm");
}

// ================== 6. BOMBAS ==================
int bombasEncendidas() {
  int n = 0;
  for (int i = 0; i < N_BOM; i++) if (bom[i].on) n++;
  return n;
}

void apagar(int i, const String& motivo) {
  Bomba& b = bom[i];
  if (!b.on) return;
  b.on = false;
  b.manual = false;
  digitalWrite(b.pin, RELE_OFF);
  alerta(String(b.nombre) + " apagada: " + motivo);
}

bool origenOk(const Bomba& b, float margen) {
  if (b.origen < 0) return true;
  const Deposito& o = dep[b.origen];
  return o.ok && o.nivel > NIVEL_SECO + margen;
}

bool encender(int i, bool manual) {
  Bomba& b = bom[i];
  Deposito& d = dep[b.destino];
  if (b.on) return true;
  if (b.bloqueada) { if (manual) alerta(String(b.nombre) + " bloqueada por posible fuga. Repara y presiona Rearmar."); return false; }
  if (!d.ok) { if (manual) alerta(String("No se puede llenar ") + d.nombre + ": su sensor no responde."); return false; }
  if (d.nivel >= NIVEL_APAGAR) { if (manual) alerta(String(d.nombre) + " ya está lleno. La bomba no se enciende."); return false; }
  if (!origenOk(b, 0)) { if (manual) alerta(String("No hay agua suficiente en el origen de ") + b.nombre + "."); return false; }
  if (bombasEncendidas() >= MAX_BOMBAS_A_LA_VEZ) { if (manual) alerta(String("Ya hay ") + String(MAX_BOMBAS_A_LA_VEZ) + " bombas encendidas. Espera a que termine una."); return false; }
  b.on = true;
  b.manual = manual;
  b.tVentana = millis();
  b.nivelVentana = d.nivel;
  digitalWrite(b.pin, RELE_ON);
  alerta(String(b.nombre) + (manual ? " encendida a mano" : " encendida (nivel bajo)") + ". Se apaga sola al " + String((int)NIVEL_APAGAR) + "%.");
  return true;
}

// ¿otra bomba encendida saca agua del destino de esta? (entonces no se puede juzgar si sube)
bool destinoSeVacia(int i) {
  for (int k = 0; k < N_BOM; k++)
    if (k != i && bom[k].on && bom[k].origen == bom[i].destino) return true;
  return false;
}

void controlar() {
  unsigned long ahora = millis();
  for (int i = 0; i < N_BOM; i++) {
    Bomba& b = bom[i];
    Deposito& d = dep[b.destino];

    if (b.on) {
      if (!d.ok) { apagar(i, String("el sensor de ") + d.nombre + " no responde"); continue; }
      if (b.origen >= 0 && !dep[b.origen].ok) { apagar(i, "el sensor del origen no responde"); continue; }
      if (d.nivel >= NIVEL_APAGAR) { apagar(i, String(d.nombre) + " lleno"); continue; }
      if (!origenOk(b, 0)) { apagar(i, "origen casi vacío (protección contra trabajo en seco)"); continue; }

      // detección de fuga
      if (destinoSeVacia(i)) { b.tVentana = ahora; b.nivelVentana = d.nivel; continue; }
      if (ahora - b.tVentana >= VENTANA_FUGA_MS) {
        float subio = d.nivel - b.nivelVentana;
        if (subio < SUBIDA_MINIMA) {
          apagar(i, "posible fuga");
          b.bloqueada = true;
          alerta(String("CUIDADO: ") + b.nombre + " estuvo " + String(VENTANA_FUGA_MS / 1000) +
                 " s encendida y " + d.nombre + " no subió. " +
                 (b.origen < 0 ? "El pozo puede estar vacío o la manguera rota o suelta."
                               : "Puede haber una manguera rota o suelta."));
          // si se bloquean varias bombas del mismo origen, el problema es el tramo común
          int mismas = 0;
          if (b.origen < 0) continue;
          for (int k = 0; k < N_BOM; k++) if (bom[k].bloqueada && bom[k].origen == b.origen) mismas++;
          if (mismas >= 2) alerta("B2 y B3 fallaron: revisa que sus bombas sigan sumergidas en el tanque central y que las mangueras no estén sueltas.");
        } else {
          b.tVentana = ahora;
          b.nivelVentana = d.nivel;
        }
      }
    } else if (modoAuto && !b.bloqueada && ahora >= b.esperaHasta && d.ok && d.nivel <= NIVEL_ENCENDER && origenOk(b, 5)) {
      encender(i, false);
    }
  }
}

// ================== 7. PÁGINA WEB (la "app") ==================
String paginaHtml() {
  String h;
  h.reserve(4000);
  h += F("<!doctype html><html lang='es'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
         "<meta http-equiv='refresh' content='3'><title>Monitor Hidrico</title><style>"
         "body{font-family:system-ui,sans-serif;background:#0d171d;color:#e3edf2;margin:0;padding:16px}"
         "h1{font-size:20px;margin:0 0 12px}.c{background:#16252e;border-radius:10px;padding:12px;margin-bottom:10px}"
         ".bar{height:10px;background:#2a3a44;border-radius:5px;overflow:hidden;margin:6px 0}.bar i{display:block;height:100%;background:#4ab6e6}"
         "a{display:inline-block;margin:4px 6px 0 0;padding:6px 10px;border-radius:6px;background:#1d7fb3;color:#fff;text-decoration:none;font-size:14px}"
         "a.g{background:#2a3a44}.b{color:#f0604f;font-weight:600}.m{color:#8fa7b4;font-size:13px}</style></head><body>");
  h += F("<h1>Monitor Hídrico</h1>");
  h += "<div class='c'>Modo: <b>" + String(modoAuto ? "automático" : "manual") + "</b> <a class='g' href='/auto'>Cambiar</a></div>";
  for (int i = 0; i < N_DEP; i++) {
    Deposito& d = dep[i];
    h += "<div class='c'><b>" + String(d.nombre) + "</b> ";
    if (d.ok) {
      String estado = d.nivel <= NIVEL_ENCENDER ? "BAJO" : (d.nivel >= 75 ? "ALTO" : "MEDIO");
      h += String(d.nivel, 0) + "% · " + estado + "<div class='bar'><i style='width:" + String(d.nivel, 0) + "%'></i></div>";
      h += "<span class='m'>HC-SR04: " + String(d.cm, 1) + " cm</span>";
    } else {
      h += "<span class='b'>sin lectura del sensor</span>";
    }
    for (int k = 0; k < N_BOM; k++) {
      if (bom[k].destino != i) continue;
      Bomba& b = bom[k];
      h += "<div class='m'>" + String(b.nombre) + ": ";
      h += b.bloqueada ? "<span class='b'>BLOQUEADA (posible fuga)</span>" : (b.on ? "encendida" : "apagada");
      h += "</div><a href='/llenar?b=" + String(k) + "'>Llenar</a><a class='g' href='/detener?b=" + String(k) + "'>Detener</a>";
      if (b.bloqueada) h += "<a class='g' href='/rearmar?b=" + String(k) + "'>Rearmar</a>";
    }
    h += "</div>";
  }
  h += F("<div class='c'><b>Alertas</b>");
  for (int j = 1; j <= N_ALERTAS; j++) {
    String& a = alertas[(alertaPos - j + N_ALERTAS) % N_ALERTAS];
    if (a.length()) h += "<div class='m'>" + a + "</div>";
  }
  h += F("</div></body></html>");
  return h;
}

String datosJson() {
  String j = "{\"modo\":\"" + String(modoAuto ? "auto" : "manual") + "\",\"depositos\":[";
  for (int i = 0; i < N_DEP; i++) {
    if (i) j += ",";
    j += "{\"nombre\":\"" + String(dep[i].nombre) + "\",\"ok\":" + String(dep[i].ok ? "true" : "false") +
         ",\"nivel\":" + String(dep[i].nivel, 1) + ",\"cm\":" + String(dep[i].cm, 1) + "}";
  }
  j += "],\"bombas\":[";
  for (int k = 0; k < N_BOM; k++) {
    if (k) j += ",";
    j += "{\"nombre\":\"" + String(bom[k].nombre) + "\",\"on\":" + String(bom[k].on ? "true" : "false") +
         ",\"bloqueada\":" + String(bom[k].bloqueada ? "true" : "false") + "}";
  }
  j += "]}";
  return j;
}

int bombaDeUrl() {
  if (!server.hasArg("b")) return -1;
  int k = server.arg("b").toInt();
  return (k >= 0 && k < N_BOM) ? k : -1;
}
void volver() { server.sendHeader("Location", "/"); server.send(303); }

void configurarWeb() {
  server.on("/", []() { server.send(200, "text/html; charset=utf-8", paginaHtml()); });
  server.on("/datos", []() { server.send(200, "application/json", datosJson()); });
  server.on("/llenar", []() { int k = bombaDeUrl(); if (k >= 0) encender(k, true); volver(); });
  server.on("/detener", []() {
    int k = bombaDeUrl();
    if (k >= 0) { apagar(k, "detenida a mano"); bom[k].esperaHasta = millis() + 60000; }
    volver();
  });
  server.on("/rearmar", []() {
    int k = bombaDeUrl();
    if (k >= 0) { bom[k].bloqueada = false; alerta(String(bom[k].nombre) + " rearmada."); }
    volver();
  });
  server.on("/auto", []() { modoAuto = !modoAuto; alerta(modoAuto ? "Modo automático" : "Modo manual"); volver(); });
  server.begin();
}

// ================== 8. SETUP Y LOOP ==================
void setup() {
  // Primero los relés en apagado, antes que cualquier otra cosa
  for (int k = 0; k < N_BOM; k++) { digitalWrite(bom[k].pin, RELE_OFF); pinMode(bom[k].pin, OUTPUT); digitalWrite(bom[k].pin, RELE_OFF); }
  for (int i = 0; i < N_DEP; i++) { pinMode(dep[i].trig, OUTPUT); digitalWrite(dep[i].trig, LOW); pinMode(dep[i].echo, INPUT); }

  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== Monitor Hidrico ===");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Conectando al WiFi");
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) { delay(300); Serial.print("."); }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\nListo. Abre en el celular: http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nSin WiFi por ahora. El control funciona igual; se reintenta solo.");
  }
  configurarWeb();
}

unsigned long tSensor = 0, tControl = 0, tWifi = 0, tSerial = 0;
int sensorTurno = 0;
bool teniaWifi = false;

void loop() {
  server.handleClient();
  unsigned long ahora = millis();

  // un sensor cada 150 ms (así no se cruzan los ecos y la página responde rápido)
  if (ahora - tSensor >= 150) {
    tSensor = ahora;
    leerDeposito(sensorTurno);
    sensorTurno = (sensorTurno + 1) % N_DEP;
  }
  if (ahora - tControl >= 500) { tControl = ahora; controlar(); }

  // WiFi: avisa y reintenta
  bool hayWifi = WiFi.status() == WL_CONNECTED;
  if (hayWifi != teniaWifi) {
    if (hayWifi) alerta(String("WiFi conectado: http://") + WiFi.localIP().toString());
    else alerta(String("Se perdió el WiFi. El control sigue funcionando."));
    teniaWifi = hayWifi;
  }
  if (!hayWifi && ahora - tWifi >= 10000) { tWifi = ahora; WiFi.reconnect(); }

  // lecturas en el Monitor Serie cada 2 s (para calibrar)
  if (ahora - tSerial >= 2000) {
    tSerial = ahora;
    for (int i = 0; i < N_DEP; i++) {
      Serial.print(dep[i].nombre); Serial.print(": ");
      if (dep[i].ok) { Serial.print(dep[i].cm, 1); Serial.print(" cm = "); Serial.print(dep[i].nivel, 0); Serial.print("%"); }
      else Serial.print("sin lectura");
      Serial.print(i < N_DEP - 1 ? "  |  " : "\n");
    }
  }
}
