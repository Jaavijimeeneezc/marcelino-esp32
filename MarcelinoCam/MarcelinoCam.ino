/* ============================================================
   MARCELINO CAM — sensor de llegada para el despacho
   Placa: ESP32-CAM (AI-Thinker)

   Qué hace: mira la habitación con la cámara, y cuando detecta
   movimiento avisa a Marcelino, que ejecuta el atajo "llegada"
   (le saluda y le pone música).

   Detalles importantes:
   - NO envía imágenes a ninguna parte. Compara fotos en blanco y
     negro DENTRO de la propia placa y solo manda un aviso de texto.
     Las fotos nunca salen del cacharro.
   - Encuentra el PC solo, preguntando a voces por la red (UDP), igual
     que hace la app del móvil: el router le cambia la IP al ordenador
     cada dos por tres y así no hay que tocar nada.

   Antes de subirlo, rellena TU_WIFI, TU_CONTRASEÑA y CLAVE_MARCELINO.
   ============================================================ */

#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>

// ---------- LO QUE TIENES QUE RELLENAR ----------
const char* WIFI_NOMBRE     = "";          // el WiFi de 2.4 GHz
const char* WIFI_CONTRASENA = "";
const char* CLAVE_MARCELINO = "";  // la que dice Marcelino al arrancar
const char* NOMBRE_SENSOR   = "despacho";
const char* ATAJO           = "llegada";           // qué atajo ejecuta

// ---------- Ajustes de la detección ----------
// El sensor tiene ruido: con la habitación quieta y poca luz ya "cambian"
// cientos de puntos. Por eso no vale un número fijo (habría que retocarlo
// cada vez que cambia la luz del día). La placa aprende sola cuánto ruido
// hay cuando no pasa nada, y avisa solo si se dispara muy por encima.
const int   UMBRAL_MOVIMIENTO = 25;   // cuánto debe cambiar el brillo de un punto
const float FACTOR_DISPARO    = 2.5;  // hay alguien si supera 2,5 veces lo normal
const int   MINIMO_ABSOLUTO   = 150;  // suelo de seguridad, por si la sala está a oscuras
const int   FOTOS_SEGUIDAS    = 2;    // dos fotos seguidas: evita falsos saltos
const unsigned long TREGUA_MS = 60000;  // no avisar más de una vez por minuto
                                        // (Marcelino tiene su propia tregua de 10 min)

// Cuando Marcelino ya te ha saludado, no tiene sentido seguir mirando: la
// placa apaga la cámara y se duerme del todo. Se despierta sola pasado
// este rato y vuelve a vigilar. Menos calor, menos consumo y ni una foto.
const int MINUTOS_DORMIDO = 10;

// ---------- Patillaje de la ESP32-CAM (AI-Thinker) ----------
#define PWDN_GPIO_NUM  32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM   0
#define SIOD_GPIO_NUM  26
#define SIOC_GPIO_NUM  27
#define Y9_GPIO_NUM    35
#define Y8_GPIO_NUM    34
#define Y7_GPIO_NUM    39
#define Y6_GPIO_NUM    36
#define Y5_GPIO_NUM    21
#define Y4_GPIO_NUM    19
#define Y3_GPIO_NUM    18
#define Y2_GPIO_NUM     5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM  23
#define PCLK_GPIO_NUM  22
#define LED_FLASH       4

// Foto anterior, para comparar (QQVGA en blanco y negro = 160x120)
uint8_t* fotoAnterior = NULL;
size_t   tamanoAnterior = 0;
unsigned long ultimoAviso = 0;
// Sin esto, la cuenta de la tregua empezaba en cero y la placa se pasaba
// el primer minuto tras arrancar viendo movimiento sin avisar a nadie.
bool yaAvisoAlguna = false;

String direccionPC = "";   // se rellena sola al buscar por la red
WiFiUDP udp;

// ============================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Marcelino Cam ===");

  pinMode(LED_FLASH, OUTPUT);
  digitalWrite(LED_FLASH, LOW);   // el flash apagado, que ciega

  if (!arrancarCamara()) {
    Serial.println("ERROR: la camara no arranca. Revisa el cable plano.");
    return;
  }

  Serial.printf("Conectando al WiFi %s", WIFI_NOMBRE);
  WiFi.begin(WIFI_NOMBRE, WIFI_CONTRASENA);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.printf("\nConectado. Mi direccion: %s\n", WiFi.localIP().toString().c_str());

  buscarMarcelino();
  Serial.println("Vigilando la habitacion...");
}

// ============================================================
void loop() {
  camera_fb_t* foto = esp_camera_fb_get();
  if (!foto) { delay(500); return; }

  bool hayMovimiento = compararConLaAnterior(foto);
  guardarComoAnterior(foto);
  esp_camera_fb_return(foto);

  if (hayMovimiento && (!yaAvisoAlguna || millis() - ultimoAviso > TREGUA_MS)) {
    yaAvisoAlguna = true;
    ultimoAviso = millis();
    Serial.println("¡Movimiento! Avisando a Marcelino...");
    avisarAMarcelino();
  }
  delay(400);   // ~2 fotos por segundo: de sobra y no calienta
}

// ============================================================
bool arrancarCamara() {
  camera_config_t cfg;
  cfg.ledc_channel = LEDC_CHANNEL_0;
  cfg.ledc_timer   = LEDC_TIMER_0;
  cfg.pin_d0 = Y2_GPIO_NUM;   cfg.pin_d1 = Y3_GPIO_NUM;
  cfg.pin_d2 = Y4_GPIO_NUM;   cfg.pin_d3 = Y5_GPIO_NUM;
  cfg.pin_d4 = Y6_GPIO_NUM;   cfg.pin_d5 = Y7_GPIO_NUM;
  cfg.pin_d6 = Y8_GPIO_NUM;   cfg.pin_d7 = Y9_GPIO_NUM;
  cfg.pin_xclk = XCLK_GPIO_NUM;   cfg.pin_pclk = PCLK_GPIO_NUM;
  cfg.pin_vsync = VSYNC_GPIO_NUM; cfg.pin_href = HREF_GPIO_NUM;
  cfg.pin_sccb_sda = SIOD_GPIO_NUM; cfg.pin_sccb_scl = SIOC_GPIO_NUM;
  cfg.pin_pwdn = PWDN_GPIO_NUM;   cfg.pin_reset = RESET_GPIO_NUM;
  cfg.xclk_freq_hz = 20000000;
  // Blanco y negro y tamaño pequeño: no queremos una foto bonita, solo
  // saber si ha cambiado algo. Así cabe en memoria y va rápido.
  cfg.pixel_format = PIXFORMAT_GRAYSCALE;
  cfg.frame_size   = FRAMESIZE_QQVGA;   // 160x120
  cfg.fb_count     = 1;
  cfg.grab_mode    = CAMERA_GRAB_LATEST;
  cfg.fb_location  = CAMERA_FB_IN_DRAM;

  return esp_camera_init(&cfg) == ESP_OK;
}

// Cuánto ruido hay cuando la habitación está en calma. Se aprende solo.
float nivelNormal = -1;
int   vecesSeguidas = 0;

// Cuenta cuántos puntos de la imagen han cambiado de brillo y decide si
// eso es "alguien ha entrado" o simplemente el ruido de siempre.
bool compararConLaAnterior(camera_fb_t* foto) {
  if (fotoAnterior == NULL || tamanoAnterior != foto->len) return false;

  int cambiados = 0;
  for (size_t i = 0; i < foto->len; i += 2) {   // de dos en dos: sobra
    int diferencia = (int)foto->buf[i] - (int)fotoAnterior[i];
    if (diferencia < 0) diferencia = -diferencia;
    if (diferencia > UMBRAL_MOVIMIENTO) cambiados++;
  }

  // La primera foto no sirve de referencia: solo se apunta el nivel.
  if (nivelNormal < 0) { nivelNormal = cambiados; return false; }

  float listonDisparo = nivelNormal * FACTOR_DISPARO;
  if (listonDisparo < MINIMO_ABSOLUTO) listonDisparo = MINIMO_ABSOLUTO;

  bool destaca = cambiados > listonDisparo;
  if (destaca) {
    vecesSeguidas++;
  } else {
    vecesSeguidas = 0;
    // Se aprende solo con la sala en calma. Bajar deprisa (para encontrar
    // rápido el nivel de reposo al arrancar) y subir despacio (para que
    // alguien quieto no acabe considerándose "lo normal").
    float peso = (cambiados < nivelNormal) ? 0.30f : 0.02f;
    nivelNormal = nivelNormal * (1.0f - peso) + cambiados * peso;
  }

  Serial.printf("puntos: %d | normal: %.0f | salta a partir de: %.0f%s\n",
                cambiados, nivelNormal, listonDisparo, destaca ? "  <-- ALGO SE MUEVE" : "");

  return vecesSeguidas >= FOTOS_SEGUIDAS;
}

void guardarComoAnterior(camera_fb_t* foto) {
  if (tamanoAnterior != foto->len) {
    if (fotoAnterior) free(fotoAnterior);
    fotoAnterior = (uint8_t*)malloc(foto->len);
    tamanoAnterior = foto->len;
  }
  if (fotoAnterior) memcpy(fotoAnterior, foto->buf, foto->len);
}

// ============================================================
// Preguntar a voces por la red dónde está el PC (igual que la app).
void buscarMarcelino() {
  Serial.println("Buscando a Marcelino en la red...");
  udp.begin(0);
  for (int intento = 0; intento < 5; intento++) {
    udp.beginPacket(IPAddress(255,255,255,255), 8766);
    udp.print("marcelino?");
    udp.endPacket();

    unsigned long limite = millis() + 1500;
    while (millis() < limite) {
      if (udp.parsePacket() > 0) {
        char buzon[200] = {0};
        udp.read(buzon, sizeof(buzon) - 1);
        String respuesta = String(buzon);
        Serial.printf("Me contesta: %s\n", respuesta.c_str());

        // IMPORTANTE: hay que quedarse con la IP que viene DENTRO del
        // mensaje, no con la del remitente. El PC tiene varias tarjetas de
        // red (VMware crea un par), y a veces contesta desde una de ellas,
        // que desde aquí no se puede alcanzar. El propio Marcelino nos
        // dice cuál es su buena.
        String ip = "";
        int marca = respuesta.indexOf("\"ip\":\"");
        if (marca >= 0) {
          int fin = respuesta.indexOf('"', marca + 6);
          if (fin > marca) ip = respuesta.substring(marca + 6, fin);
        }
        if (ip.length() == 0) ip = udp.remoteIP().toString();

        int puerto = 8765;
        int marcaPuerto = respuesta.indexOf("\"puerto\":");
        if (marcaPuerto >= 0) puerto = respuesta.substring(marcaPuerto + 9).toInt();

        direccionPC = "http://" + ip + ":" + String(puerto);
        Serial.printf("Marcelino esta en %s\n", direccionPC.c_str());
        return;
      }
      delay(50);
    }
  }
  Serial.println("No he encontrado el PC. Lo reintentare al detectar movimiento.");
}

void avisarAMarcelino() {
  if (direccionPC == "") buscarMarcelino();
  if (direccionPC == "") return;

  HTTPClient http;
  http.begin(direccionPC + "/api/evento");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Clave", CLAVE_MARCELINO);
  http.setTimeout(8000);

  String cuerpo = String("{\"sensor\":\"") + NOMBRE_SENSOR +
                  "\",\"atajo\":\"" + ATAJO + "\"}";
  int codigo = http.POST(cuerpo);
  String respuesta = http.getString();
  Serial.printf("Marcelino responde: %d %s\n", codigo, respuesta.c_str());
  http.end();

  // Si el PC ya no está donde creíamos, se vuelve a buscar la próxima vez.
  if (codigo <= 0) { direccionPC = ""; return; }

  // Aviso entregado: a dormir. Ya no hace falta seguir haciendo fotos.
  if (codigo == 200) dormirUnRato();
}

// Apaga la cámara y duerme la placa entera. Al despertar, el programa
// empieza otra vez por setup(), así que se reconecta y vuelve a vigilar.
void dormirUnRato() {
  Serial.printf("Marcelino ya le ha saludado. Dejo de hacer fotos y me duermo %d minutos.\n",
                MINUTOS_DORMIDO);
  Serial.flush();

  esp_camera_deinit();                      // la cámara, apagada de verdad
  digitalWrite(LED_FLASH, LOW);
  esp_sleep_enable_timer_wakeup((uint64_t)MINUTOS_DORMIDO * 60ULL * 1000000ULL);
  esp_deep_sleep_start();                   // aquí se para todo
}
