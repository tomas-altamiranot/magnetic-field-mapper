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

// --- CALIBRACIÓN MECÁNICA ---
const int PASOS_POR_VUELTA = 2048; 
// Mantenemos 75 para corregir que no avance pasos gigantes
const int PASOS_POR_MM_LINEAL = 75; 

// --- CALIBRACIÓN SENSOR ---
const int PIN_SENSOR = 34;
const int VALOR_ZERO = 1936; // <--- TU VALOR EXACTO
const float SENSIBILIDAD = 0.00165; 
const float VCC = 3.3;

// --- CONFIGURACIÓN DEL ESCANEO ---
float radioActual_mm = 0.0; 
const int PASO_RADIAL_MM = 2; // Avanza 2mm por vuelta
const int RADIO_MAXIMO_MM = 35; // <--- Límite en 35mm (3.5 cm)

// --- PINES DE MOTORES ---
Stepper motorAngular(PASOS_POR_VUELTA, 13, 14, 12, 27);
Stepper motorLineal(PASOS_POR_VUELTA, 26, 33, 25, 32); 

bool sistemaIniciado = false; 

void setup() {
  Serial.begin(115200);
  motorAngular.setSpeed(8); 
  motorLineal.setSpeed(10); 

  // --- MENÚ DE AJUSTE (HANDSHAKE) ---
  while (!sistemaIniciado) {
    if (Serial.available() > 0) {
      char comando = Serial.read();
      
      // AJUSTE MANUAL (Coherente con tu dirección invertida)
      if (comando == 'a') {
        // Si "Afuera" es negativo en el loop, aquí también debe serlo
        motorLineal.step(-150); 
      }
      else if (comando == 'd') {
        // "Adentro" (regresar al centro) sería positivo
        motorLineal.step(150); 
      }
      else if (comando == 'S') { 
        sistemaIniciado = true;
        Serial.println("START_ACK"); 
      }
    }
  }

  motorLineal.setSpeed(8); 
  Serial.println("INICIANDO_ESCANEO");
}

void loop() {
  if (radioActual_mm <= RADIO_MAXIMO_MM) {
    
    // FASE 1: GIRO COMPLETO (Cada 5 Grados)
    for (int angulo = 0; angulo < 360; angulo += 5) {
      float mTesla = leerSensor();
      
      // Enviamos datos en MM
      Serial.print(angulo);
      Serial.print(",");
      Serial.print(radioActual_mm);
      Serial.print(",");
      Serial.println(mTesla);
      
      motorAngular.step(28); 
      delay(30); 
    }

    // FASE 2: AVANCE LINEAL
    // Calculamos pasos
    int pasosAvanzar = PASOS_POR_MM_LINEAL * PASO_RADIAL_MM;
    
    // <--- CAMBIO IMPORTANTE: Signo NEGATIVO para ir afuera
    motorLineal.step(-pasosAvanzar); 
    
    radioActual_mm += PASO_RADIAL_MM;
    
  } else {
    Serial.println("FIN");
    while(true) { delay(100); }
  }
}

float leerSensor() {
  long suma = 0;
  for(int i=0; i<10; i++) { 
    suma += analogRead(PIN_SENSOR);
    delay(1);
  }
  int lectura = suma / 10;
  float voltaje = (lectura / 4095.0) * VCC;
  float voltajeZero = (VALOR_ZERO / 4095.0) * VCC;
  if (SENSIBILIDAD == 0) return 0;
  float gauss = (voltaje - voltajeZero) / SENSIBILIDAD;
  return gauss / 10.0; 
}