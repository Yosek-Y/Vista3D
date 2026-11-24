# Prueba con 2 bebederos: 2 sensores HC-SR04 + 2 bombas (relé HW-316) · MicroPython para ESP32
#
# Conexiones:
#   Bebedero 1: sensor TRIG -> GPIO 19, ECHO -> GPIO 39 (VN) · bomba en K1 (IN1 -> GPIO 25)
#   Bebedero 2: sensor TRIG -> GPIO 18, ECHO -> GPIO 36 (VP) · bomba en K2 (IN2 -> GPIO 26)
#   VCC de los sensores -> riel 3.3 V · VCC del relé y COM de K1/K2 -> riel 5 V · todos los GND -> riel GND
#
# Qué hace:
#   - Mide el nivel de cada bebedero (%) cada medio segundo y lo muestra en la consola.
#   - Si un bebedero baja de ENCENDER %, prende su bomba; al llegar a APAGAR % la apaga.
#   - NUNCA prende las 2 bombas juntas: si los 2 necesitan agua, llena primero el más bajo y después el otro.
#   - Si una bomba lleva VENTANA_S segundos prendida y el nivel no sube, la apaga y la bloquea
#     (tanque vacío o manguera suelta). Para desbloquear, presionen el botón EN del ESP32.

from machine import Pin, time_pulse_us
import time

# ---------- Ajustes ----------
ENCENDER = 30      # % : debajo de esto se llena
APAGAR = 90        # % : al llegar aquí se apaga sola
VENTANA_S = 20     # segundos para comprobar que el nivel sube
SUBIDA_MIN = 3     # % que debe subir en esa ventana


class Bebedero:
    def __init__(self, nombre, trig, echo, rele, cm_vacio, cm_lleno):
        self.nombre = nombre
        self.trig = Pin(trig, Pin.OUT, value=0)
        self.echo = Pin(echo, Pin.IN)
        self.rele = Pin(rele, Pin.OUT, value=1)   # 1 = bomba apagada (el relé se activa con 0)
        self.cm_vacio = cm_vacio                   # distancia del sensor al fondo (bebedero vacío)
        self.cm_lleno = cm_lleno                   # distancia del sensor al agua (bebedero lleno)
        self.cm = None
        self.nivel = None
        self.on = False
        self.bloqueada = False
        self.t_ventana = 0
        self.nivel_ventana = 0

    def medir_cm(self):
        lecturas = []
        for _ in range(5):
            self.trig.value(0)
            time.sleep_us(3)
            self.trig.value(1)
            time.sleep_us(10)
            self.trig.value(0)
            us = time_pulse_us(self.echo, 1, 30000)
            if us > 0:
                cm = us * 0.0343 / 2
                if 1.5 < cm < 400:
                    lecturas.append(cm)
            time.sleep_ms(30)
        if len(lecturas) < 3:
            return None
        lecturas.sort()
        return lecturas[len(lecturas) // 2]

    def leer(self):
        self.cm = self.medir_cm()
        if self.cm is None:
            self.nivel = None
            return
        pct = (self.cm_vacio - self.cm) / (self.cm_vacio - self.cm_lleno) * 100
        self.nivel = max(0, min(100, pct))

    def bomba(self, on, motivo=""):
        self.on = on
        self.rele.value(0 if on else 1)
        if on:
            self.t_ventana = time.ticks_ms()
            self.nivel_ventana = self.nivel
        print(">>", self.nombre, "bomba", "ENCENDIDA" if on else "APAGADA", motivo)


# cm_vacio y cm_lleno: mídanlos con la consola (vacío y lleno) y cámbienlos aquí
bebederos = [
    Bebedero("Bebedero 1", trig=19, echo=39, rele=25, cm_vacio=13.0, cm_lleno=5.0),
    Bebedero("Bebedero 2", trig=18, echo=36, rele=26, cm_vacio=13.0, cm_lleno=5.0),
]


def controlar():
    for b in bebederos:
        if not b.on:
            continue
        if b.nivel is None:
            b.bomba(False, "(el sensor no responde)")
        elif b.nivel >= APAGAR:
            b.bomba(False, "(lleno)")
        elif time.ticks_diff(time.ticks_ms(), b.t_ventana) >= VENTANA_S * 1000:
            if b.nivel - b.nivel_ventana < SUBIDA_MIN:
                b.bomba(False, "(el nivel no sube)")
                b.bloqueada = True
                print("!! CUIDADO:", b.nombre, "no sube. Tanque vacío o manguera suelta. Bomba bloqueada.")
            else:
                b.t_ventana = time.ticks_ms()
                b.nivel_ventana = b.nivel

    # una sola bomba a la vez: si ninguna está prendida, llena el bebedero más bajo que lo necesite
    if not any(b.on for b in bebederos):
        pendientes = [b for b in bebederos
                      if not b.bloqueada and b.nivel is not None and b.nivel <= ENCENDER]
        if pendientes:
            mas_bajo = min(pendientes, key=lambda x: x.nivel)
            mas_bajo.bomba(True, "(nivel bajo: %d%%)" % mas_bajo.nivel)


def mostrar():
    partes = []
    for b in bebederos:
        if b.nivel is None:
            txt = "sin lectura"
        else:
            txt = "%.1f cm = %d%%" % (b.cm, b.nivel)
        if b.on:
            txt += " [llenando]"
        if b.bloqueada:
            txt += " [BLOQUEADA]"
        partes.append(b.nombre + ": " + txt)
    print("  |  ".join(partes))


try:
    ultimo = time.ticks_ms()
    while True:
        for b in bebederos:
            b.leer()
        controlar()
        if time.ticks_diff(time.ticks_ms(), ultimo) >= 2000:
            mostrar()
            ultimo = time.ticks_ms()
        time.sleep_ms(300)
finally:
    for b in bebederos:
        b.rele.value(1)   # al detener el programa, todas las bombas quedan apagadas
    print("Programa detenido. Bombas apagadas.")
