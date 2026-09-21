/*
  cara.h — el rostro de Marcelino en la pantalla de 2.4"

  Es el mismo orbe que tiene en la ventana del ordenador, pero dibujado a
  mano: un círculo con halo que cambia de color y de tamaño según lo que
  esté pasando, y un par de líneas de texto debajo.

  Estados:
    ESPERANDO   azul, latiendo despacio     "Ponga el dedo"
    LEYENDO     azul claro, latiendo rápido "Leyendo..."
    ACEPTADO    verde, brillo fijo          "Adelante, señor"
    RECHAZADO   rojo, brillo fijo           "No le conozco"
    SIN_RED     gris                        "Sin conexión"

  Por qué dibujado a mano y no con imágenes: la pantalla no tiene memoria
  suficiente para guardar fotogramas, y el ESP32 tampoco. Círculos
  concéntricos con el color degradado dan sensación de halo y salen casi
  gratis de calcular.
*/
#ifndef CARA_H
#define CARA_H

#include <TFT_eSPI.h>

enum Estado { ESPERANDO, LEYENDO, ACEPTADO, RECHAZADO, SIN_RED };

class Cara {
 public:
  Cara(TFT_eSPI& pantalla) : tft(pantalla) {}

  void empezar() {
    tft.fillScreen(FONDO);
    dibujarTitulo();
    cambiar(ESPERANDO, "");
  }

  void cambiar(Estado nuevo, const String& mensaje) {
    estado = nuevo;
    texto = mensaje;
    ultimoRadio = -1;          // fuerza a redibujar el orbe entero
    borrarZonaTexto();
    dibujarTexto();
  }

  // Se llama continuamente desde el loop: anima el latido.
  void latir() {
    float velocidad = (estado == LEYENDO) ? 0.008 : 0.0022;
    float onda = (sin(millis() * velocidad) + 1.0) / 2.0;   // de 0 a 1

    int radio;
    if (estado == ACEPTADO || estado == RECHAZADO) {
      radio = RADIO_MAX;                 // quieto: la respuesta ya está dada
    } else {
      radio = RADIO_MIN + (int)((RADIO_MAX - RADIO_MIN) * onda);
    }

    if (radio == ultimoRadio) return;    // nada que redibujar
    dibujarOrbe(radio);
    ultimoRadio = radio;
  }

 private:
  TFT_eSPI& tft;
  Estado estado = ESPERANDO;
  String texto = "";
  int ultimoRadio = -1;

  static const uint16_t FONDO = 0x0000;          // negro
  static const int CENTRO_X = 120;
  static const int CENTRO_Y = 140;
  static const int RADIO_MIN = 44;
  static const int RADIO_MAX = 62;
  static const int RADIO_HALO = 76;

  uint16_t colorEstado() {
    switch (estado) {
      case ACEPTADO:  return tft.color565(34, 197, 94);     // verde
      case RECHAZADO: return tft.color565(244, 63, 94);     // rojo
      case LEYENDO:   return tft.color565(125, 211, 252);   // azul claro
      case SIN_RED:   return tft.color565(100, 116, 139);   // gris
      default:        return tft.color565(56, 130, 246);    // azul
    }
  }

  // Mezcla un color con el negro del fondo. 0 = negro, 255 = color entero.
  uint16_t atenuar(uint16_t color, uint8_t fuerza) {
    uint8_t r = ((color >> 11) & 0x1F) * fuerza / 255;
    uint8_t v = ((color >> 5) & 0x3F) * fuerza / 255;
    uint8_t a = (color & 0x1F) * fuerza / 255;
    return (r << 11) | (v << 5) | a;
  }

  void dibujarTitulo() {
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(tft.color565(100, 116, 139), FONDO);
    tft.drawString("MARCELINO", 120, 14, 2);
  }

  void dibujarOrbe(int radio) {
    uint16_t color = colorEstado();

    // El halo: anillos cada vez más tenues hacia fuera.
    for (int r = RADIO_HALO; r > radio; r--) {
      uint8_t fuerza = map(r, radio, RADIO_HALO, 70, 0);
      tft.drawCircle(CENTRO_X, CENTRO_Y, r, atenuar(color, fuerza));
    }

    // El cuerpo del orbe, más claro por arriba para que parezca redondo.
    for (int r = radio; r >= 0; r--) {
      uint8_t fuerza = map(r, 0, radio, 255, 140);
      tft.drawCircle(CENTRO_X, CENTRO_Y, r, atenuar(color, fuerza));
    }
  }

  void borrarZonaTexto() {
    tft.fillRect(0, 232, 240, 70, FONDO);
  }

  void dibujarTexto() {
    const char* titulo;
    switch (estado) {
      case ACEPTADO:  titulo = "Adelante, senor"; break;
      case RECHAZADO: titulo = "No le conozco";   break;
      case LEYENDO:   titulo = "Leyendo...";      break;
      case SIN_RED:   titulo = "Sin conexion";    break;
      default:        titulo = "Ponga el dedo";   break;
    }

    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(colorEstado(), FONDO);
    tft.drawString(titulo, 120, 238, 4);

    if (texto.length()) {
      tft.setTextColor(tft.color565(148, 163, 184), FONDO);
      tft.drawString(texto, 120, 274, 2);
    }
  }
};

#endif
