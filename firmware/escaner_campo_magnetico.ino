/*
 * Escáner de Campo Magnético 2D — firmware ESP32
 *
 * Barre un plano en coordenadas polares combinando dos motores a pasos:
 *   - motorAngular : gira el imán 360° en incrementos de 5°
 *   - motorLineal  : desplaza el sensor Hall radialmente vía piñón-cremallera
 *
 * En cada punto lee el SS49E, convierte la lectura a militeslas y la envía
 * por serial como "angulo,radio,campo". El script de Python del lado del PC
 * consume ese stream y arma el heatmap.
 *
 * Protocolo serial (115200 baud):
 *   'a' / 'd'  -> ajuste manual del carro antes de arrancar (centrado)
 *   'S'        -> inicia el barrido
 *   "FIN"      -> emitido al terminar; el carro vuelve al origen solo
 */

#include <Stepper.h>

// --- PARÁMETROS MECÁNICOS ---
const int PASOS_POR_VUELTA   = 2048;  // 28BYJ-48 con reductora
const int PASOS_POR_MM_LINEAL = 75;   // calibrado sobre la cremallera impresa

// --- PARÁMETROS DEL SENSOR HALL ---
const int   PIN_SENSOR   = 34;
const int   VALOR_ZERO   = 1968;      // lectura ADC sin imán cerca (calibración)
const float SENSIBILIDAD = 0.00165;   // V/Gauss del SS49E ajustado a 3.3 V
const float VCC          = 3.3;

// --- CONFIGURACIÓN DE ESCANEO ---
float     radioActual_mm = 0.0;
const int PASO_RADIAL_MM  = 2;
const int RADIO_MAXIMO_MM = 35;

// --- MOTORES ---
Stepper motorAngular(PASOS_POR_VUELTA, 13, 14, 12, 27);
Stepper motorLineal (PASOS_POR_VUELTA, 26, 33, 25, 32);

bool sistemaIniciado = false;

void setup() {
  Serial.begin(115200);
  motorAngular.setSpeed(8);
  motorLineal.setSpeed(8);
  Serial.println("SISTEMA_LISTO");
}

void loop() {
  // ---------- ESTADO 1: espera y ajuste manual ----------
  if (!sistemaIniciado) {
    if (Serial.available() > 0) {
      char comando = Serial.read();

      if (comando == 'a') {
        motorLineal.step(150);
      }
      else if (comando == 'd') {
        motorLineal.step(-150);
      }
      else if (comando == 'S') {
        sistemaIniciado = true;
        Serial.println("START_ACK");
      }
    }
  }

  // ---------- ESTADO 2: escaneo activo ----------
  else {
    if (radioActual_mm <= RADIO_MAXIMO_MM) {

      // 1. Barrido angular: 72 muestras por anillo (360° / 5°)
      for (int angulo = 0; angulo < 360; angulo += 5) {
        float mTesla = leerSensor();

        Serial.print(angulo);
        Serial.print(",");
        Serial.print(radioActual_mm);
        Serial.print(",");
        Serial.println(mTesla);

        motorAngular.step(28);   // 2048 / 72 ≈ 28 pasos por incremento
        delay(30);
      }

      // 2. Avance lineal al siguiente anillo
      int pasosAvanzar = PASOS_POR_MM_LINEAL * PASO_RADIAL_MM;
      motorLineal.step(-pasosAvanzar);   // negativo = hacia afuera
      radioActual_mm += PASO_RADIAL_MM;
    }

    // ---------- ESTADO 3: fin y regreso al origen ----------
    else {
      Serial.println("FIN");

      long pasosDeRetorno = radioActual_mm * PASOS_POR_MM_LINEAL;
      motorLineal.step(pasosDeRetorno);  // homing

      radioActual_mm  = 0.0;
      sistemaIniciado = false;
      Serial.println("SISTEMA_REINICIADO. LISTO PARA NUEVA ORDEN.");
    }
  }
}

/*
 * Promedia 10 lecturas del ADC para bajar el ruido y devuelve el campo en mT.
 * El SS49E es ratiométrico: la salida en reposo se sitúa en VCC/2, así que el
 * campo real es la desviación respecto a VALOR_ZERO dividida por la sensibilidad.
 */
float leerSensor() {
  long suma = 0;
  for (int i = 0; i < 10; i++) {
    suma += analogRead(PIN_SENSOR);
    delay(1);
  }
  int lectura = suma / 10;

  float voltaje     = (lectura    / 4095.0) * VCC;
  float voltajeZero = (VALOR_ZERO / 4095.0) * VCC;

  if (SENSIBILIDAD == 0) return 0;

  return (voltaje - voltajeZero) / SENSIBILIDAD / 10.0;  // Gauss -> mT
}
