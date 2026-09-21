/*
  MarcelinoCara — prueba del rostro en la pantalla
  ------------------------------------------------
  Solo la pantalla: recorre los cinco estados de la cara para verlos todos
  sin depender del lector ni del WiFi. Cuando esto se vea bien, la cara se
  mete en el programa de verdad (MarcelinoHuella).

  CABLEADO: está en User_Setup.h, dentro de la librería TFT_eSPI. Hay copia
  en User_Setup_Marcelino.h, en esta misma carpeta.

  El LCD_RST va a G32 y se maneja AQUÍ, no en la librería: TFT_eSPI trabaja
  más rápido con patillas por debajo de la 32 y no nos sobraba ninguna.
*/
#include <TFT_eSPI.h>
#include "cara.h"

const int PIN_RESET_PANTALLA = 32;

TFT_eSPI tft = TFT_eSPI();
Cara cara(tft);

Estado ronda[] = {ESPERANDO, LEYENDO, ACEPTADO, RECHAZADO, SIN_RED};
const char* mensajes[] = {"", "no levante el dedo", "usuario - confianza 229",
                          "huella no registrada", "buscando el portero"};
int paso = 0;
unsigned long cambio = 0;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(F("\n=== Marcelino: probando la cara ==="));

  // Reinicio por cable de la pantalla, antes de que hable la librería.
  pinMode(PIN_RESET_PANTALLA, OUTPUT);
  digitalWrite(PIN_RESET_PANTALLA, HIGH); delay(10);
  digitalWrite(PIN_RESET_PANTALLA, LOW);  delay(30);
  digitalWrite(PIN_RESET_PANTALLA, HIGH); delay(150);

  tft.init();
  tft.setRotation(0);          // vertical, el conector arriba
  cara.empezar();

  Serial.println(F("Recorriendo los estados, uno cada 4 segundos."));
  cambio = millis();
}

void loop() {
  cara.latir();

  if (millis() - cambio > 4000) {
    paso = (paso + 1) % 5;
    cara.cambiar(ronda[paso], mensajes[paso]);
    Serial.print(F("Estado: "));
    Serial.println(paso);
    cambio = millis();
  }
  delay(16);                   // ~60 veces por segundo, suficiente para el latido
}
