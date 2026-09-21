/*
  MarcelinoHuella — prueba del lector de huellas
  ----------------------------------------------
  Primer paso del "rostro" de Marcelino: comprobar que el lector de huellas
  responde y poder registrar el dedo del usuario. Todavía sin WiFi y sin
  pantalla: una cosa cada vez.

  El lector guarda las huellas DENTRO de él y solo contesta "es el usuario 3"
  o "no le conozco". La huella en sí nunca sale del módulo ni llega al PC.

  CONEXIONES REALES, LEÍDAS EN LA PROPIA PLACA DEL LECTOR (18/9/2026):
  Quitando el conector blanco se ven los rótulos: RX TX GND TCH VA D+ D-

      AMARILLO = TX  del lector -> G16   (el ESP32 escucha por aquí)
      BLANCO   = RX  del lector -> G18   (el ESP32 le habla por aquí)
      ROJO     = V+             -> 3V3   (a 3,3 V funciona: LED azul encendido)
      NEGRO    = GND            -> GND
      AZUL  = TCH  y  VERDE = VA  -> SIN CONECTAR (son del sensor táctil)

  57600 baudios.

  LECCIÓN QUE COSTÓ DOS HORAS: al principio se dio con el cableado bueno
  probando a ciegas, pero se anotó mal cuál era el cable de RX (se apuntó
  "azul" cuando era el blanco). Al desmontar y volver a montar siguiendo esa
  nota, el lector se quedó mudo y no había forma de encontrarlo: la búsqueda
  a ciegas no sirve de nada si el cable de RX ni siquiera está conectado.
  MORALEJA: para saber qué cable es cada uno, se QUITA EL CONECTOR y se leen
  los rótulos de la placa. Nunca se deduce del color ni de un acierto previo.

  AVISO DE VOLTAJE: se empezó a 3,3 V a propósito. Si el módulo pidiera 5 V
  no arrancaría, pero no sufre daño. Al revés (meterle 5 V a uno de 3,3 V)
  se estropea.

  MENÚ por el Monitor Serie (115200 baudios):
      i -> información del lector
      r -> registrar una huella nueva
      b -> buscar: ¿de quién es este dedo?
      c -> contar cuántas huellas hay guardadas
      x -> BORRAR todas las huellas
*/

#include <Adafruit_Fingerprint.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <TFT_eSPI.h>
#include "cara.h"

// ---------- La pantalla ----------
// El cableado va en User_Setup.h, DENTRO de la libreria TFT_eSPI (hay copia
// en la carpeta MarcelinoPantalla, como User_Setup_Marcelino.h). El reset por
// cable se maneja aqui y no en la libreria: TFT_eSPI va mas fina con patillas
// por debajo de la 32 y no nos sobraba ninguna.
const int PIN_RESET_PANTALLA = 32;

TFT_eSPI tft = TFT_eSPI();
Cara cara(tft);

unsigned long volverAEsperar = 0;   // cuando devolver la cara al reposo
const unsigned long MOSTRAR_RESULTADO = 5000;

// ---------- WiFi y portero ----------
const char* WIFI_NOMBRE     = "";
const char* WIFI_CONTRASENA = "";
const char* CLAVE_PORTERO   = "";   // la misma que dice Marcelino
const int   PUERTO_BUSQUEDA = 8767;                 // por aqui se pregunta donde esta

WiFiUDP udp;
String direccionPortero = "";
unsigned long ultimoAviso = 0;
const unsigned long ESPERA_ENTRE_AVISOS = 4000;   // no repetir con el dedo apoyado

const int PIN_RX = 16;        // amarillo: por aquí escuchamos al lector
const int PIN_TX = 18;        // BLANCO: por aquí le hablamos (RX del lector)
const uint32_t VELOCIDAD = 57600;   // la suya, ya comprobada

// Velocidades a probar, empezando por la más común en estos módulos.
const uint32_t VELOCIDADES[] = {57600};   // ya sabemos la suya
const int CUANTAS_VELOCIDADES = sizeof(VELOCIDADES) / sizeof(VELOCIDADES[0]);

HardwareSerial puertoLector(2);
Adafruit_Fingerprint lector(&puertoLector);

uint32_t velocidadBuena = 0;
int pinRxBueno = 0;
int pinTxBueno = 0;

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println();
  Serial.println(F("=== Marcelino: lector de huellas y pantalla ==="));

  arrancarPantalla();

  if (!buscarLector()) {
    cara.cambiar(SIN_RED, "lector no responde");
    Serial.println(F("\n[X] No he encontrado el lector a ninguna velocidad."));
    Serial.println(F("    Repasa, por este orden:"));
    Serial.println(F("    1. TX del lector al GPIO 16 y RX del lector al GPIO 17"));
    Serial.println(F("       (van CRUZADOS: lo que uno habla, el otro escucha)"));
    Serial.println(F("    2. GND comun entre los dos"));
    Serial.println(F("    3. Si sigue mudo, prueba a alimentarlo en 5V en vez de 3V3"));
    return;
  }

  Serial.print(F("\n[OK] Lector encontrado a "));
  Serial.print(velocidadBuena);
  Serial.println(F(" baudios."));
  mostrarInformacion();

  conectarWifi();
  buscarPortero();
  Serial.println(F("\nVigilando el lector. Pon el dedo cuando quieras."));
  mostrarMenu();
}

void loop() {
  if (velocidadBuena == 0) {
    delay(1000);
    return;
  }
  cara.latir();

  if (volverAEsperar && millis() > volverAEsperar) {
    cara.cambiar(ESPERANDO, "");
    volverAEsperar = 0;
  }

  vigilarDedo();

  if (!Serial.available()) {
    delay(50);
    return;
  }

  char opcion = Serial.read();
  while (Serial.available()) Serial.read();   // tira el resto de la línea

  switch (opcion) {
    case 'i': mostrarInformacion(); break;
    case 'r': registrarHuella(); break;
    case 'b': buscarDedo(); break;
    case 'c': contarHuellas(); break;
    case 'x': borrarTodas(); break;
    case '\n': case '\r': return;
    default: Serial.println(F("No conozco esa opcion."));
  }
  mostrarMenu();
}

// --- Puesta en marcha ---------------------------------------------------

// Red de seguridad por si algún día se mueve un cable: se prueban todas las
// parejas de pines libres. OJO: esto solo sirve si el RX y el TX del lector
// ESTÁN conectados a algún pin. Si uno se queda al aire, no hay búsqueda que
// valga (lección del 18/9/2026).
const int CANDIDATOS[] = {16, 18, 17, 19, 15, 2, 0};
const int CUANTOS_CANDIDATOS = sizeof(CANDIDATOS) / sizeof(CANDIDATOS[0]);

bool buscarLector() {
  // Primero, lo que ya sabemos que funciona. Solo si fallara (un cable
  // movido, otro lector) se rebusca a ciegas por todas las parejas.
  Serial.print(F("Probando el cableado conocido (G"));
  Serial.print(PIN_RX);
  Serial.print(F(" / G"));
  Serial.print(PIN_TX);
  Serial.print(F(", "));
  Serial.print(VELOCIDAD);
  Serial.print(F(" baudios)... "));
  puertoLector.begin(VELOCIDAD, SERIAL_8N1, PIN_RX, PIN_TX);
  delay(250);
  if (lector.verifyPassword()) {
    velocidadBuena = VELOCIDAD;
    pinRxBueno = PIN_RX;
    pinTxBueno = PIN_TX;
    Serial.println(F("responde."));
    return true;
  }
  Serial.println(F("nada. Rebusco por todas las combinaciones."));
  puertoLector.end();
  delay(100);

  int intento = 0;
  const int total = CUANTOS_CANDIDATOS * (CUANTOS_CANDIDATOS - 1) * CUANTAS_VELOCIDADES;

  for (int a = 0; a < CUANTOS_CANDIDATOS; a++) {
    for (int b = 0; b < CUANTOS_CANDIDATOS; b++) {
      if (a == b) continue;                 // no puede escuchar y hablar por el mismo
      int rx = CANDIDATOS[a];               // por donde escuchamos al lector
      int tx = CANDIDATOS[b];               // por donde le hablamos

      for (int i = 0; i < CUANTAS_VELOCIDADES; i++) {
        intento++;
        puertoLector.begin(VELOCIDADES[i], SERIAL_8N1, rx, tx);
        delay(250);

        if (lector.verifyPassword()) {
          velocidadBuena = VELOCIDADES[i];
          pinRxBueno = rx;
          pinTxBueno = tx;
          Serial.print(F("\n*** RESPONDE en el intento "));
          Serial.print(intento);
          Serial.println(F(" ***"));
          return true;
        }
        puertoLector.end();
        delay(60);
      }
      Serial.print(F("  escuchando G"));
      Serial.print(rx);
      Serial.print(F(" / hablando G"));
      Serial.print(tx);
      Serial.print(F(": nada  ("));
      Serial.print(intento);
      Serial.print(F("/"));
      Serial.print(total);
      Serial.println(F(")"));
    }
  }
  return false;
}

void mostrarInformacion() {
  lector.getParameters();
  Serial.println(F("\n--- El lector dice de si mismo ---"));
  Serial.print(F("  Capacidad     : "));
  Serial.print(lector.capacity);
  Serial.println(F(" huellas"));
  Serial.print(F("  Huellas dentro: "));
  lector.getTemplateCount();
  Serial.println(lector.templateCount);
  Serial.print(F("  Nivel de rigor: "));
  Serial.print(lector.security_level);
  Serial.println(F(" (1 = permisivo, 5 = estricto)"));
}

void mostrarMenu() {
  Serial.println(F("\n--- Que hacemos? ---"));
  Serial.println(F("  i = informacion    r = registrar una huella"));
  Serial.println(F("  b = de quien es este dedo    c = contar    x = borrar todas"));
}

// --- Acciones -----------------------------------------------------------

void contarHuellas() {
  lector.getTemplateCount();
  Serial.print(F("\nHay "));
  Serial.print(lector.templateCount);
  Serial.println(F(" huellas guardadas."));
}

void buscarDedo() {
  Serial.println(F("\nPon el dedo en el lector..."));

  uint8_t r = FINGERPRINT_NOFINGER;
  unsigned long limite = millis() + 10000;
  while (r != FINGERPRINT_OK) {
    r = lector.getImage();
    if (millis() > limite) {
      Serial.println(F("Se acabo el tiempo, no has puesto el dedo."));
      return;
    }
    delay(50);
  }

  if (lector.image2Tz() != FINGERPRINT_OK) {
    Serial.println(F("No he sacado nada en claro de esa huella. Limpia el cristal y repite."));
    return;
  }

  if (lector.fingerSearch() == FINGERPRINT_OK) {
    Serial.print(F("[OK] Es el usuario numero "));
    Serial.print(lector.fingerID);
    Serial.print(F(" (confianza "));
    Serial.print(lector.confidence);
    Serial.println(F(")"));
  } else {
    Serial.println(F("[--] Este dedo no lo conozco."));
  }
}

void registrarHuella() {
  Serial.println(F("\nQue numero le doy a esta huella? (1 a 127, y luego Enter)"));
  int id = leerNumero();
  if (id < 1 || id > 127) {
    Serial.println(F("Numero fuera de rango. Lo dejo."));
    return;
  }

  Serial.print(F("Registrando la huella numero "));
  Serial.println(id);

  // Se toman DOS lecturas del mismo dedo y el modulo las combina: con una
  // sola, el reconocimiento posterior falla mucho mas.
  if (!tomarLectura(1, F("Pon el dedo..."))) return;

  Serial.println(F("Quita el dedo."));
  delay(1500);
  while (lector.getImage() != FINGERPRINT_NOFINGER) delay(100);

  if (!tomarLectura(2, F("Ahora pon el MISMO dedo otra vez..."))) return;

  if (lector.createModel() != FINGERPRINT_OK) {
    Serial.println(F("Las dos lecturas no se parecen. Vuelve a intentarlo."));
    return;
  }

  if (lector.storeModel(id) == FINGERPRINT_OK) {
    Serial.print(F("[OK] Huella guardada con el numero "));
    Serial.println(id);
  } else {
    Serial.println(F("No he podido guardarla."));
  }
}

bool tomarLectura(uint8_t ranura, const __FlashStringHelper *aviso) {
  Serial.println(aviso);

  uint8_t r = FINGERPRINT_NOFINGER;
  unsigned long limite = millis() + 15000;
  while (r != FINGERPRINT_OK) {
    r = lector.getImage();
    if (millis() > limite) {
      Serial.println(F("Se acabo el tiempo."));
      return false;
    }
    delay(50);
  }
  Serial.println(F("  ...cogida."));

  if (lector.image2Tz(ranura) != FINGERPRINT_OK) {
    Serial.println(F("Esa lectura no vale. Limpia el cristal y repite."));
    return false;
  }
  return true;
}

void borrarTodas() {
  Serial.println(F("\nEsto BORRA todas las huellas del lector."));
  Serial.println(F("Escribe SI (en mayusculas) y Enter para confirmar:"));

  String respuesta = leerTexto();
  if (respuesta != "SI") {
    Serial.println(F("No se ha borrado nada."));
    return;
  }
  if (lector.emptyDatabase() == FINGERPRINT_OK) {
    Serial.println(F("Borradas todas."));
  } else {
    Serial.println(F("No he podido borrarlas."));
  }
}

// --- Utilidades ---------------------------------------------------------

String leerTexto() {
  while (!Serial.available()) delay(50);
  String s = Serial.readStringUntil('\n');
  s.trim();
  return s;
}

int leerNumero() {
  return leerTexto().toInt();
}


// --- Vigilancia y aviso al portero --------------------------------------

void vigilarDedo() {
  // Sin el cable TCH conectado no hay forma de que el lector nos despierte:
  // hay que preguntarle cada poco si nota un dedo. Es barato.
  if (lector.getImage() != FINGERPRINT_OK) return;
  if (millis() - ultimoAviso < ESPERA_ENTRE_AVISOS) return;

  cara.cambiar(LEYENDO, "no levante el dedo");

  if (lector.image2Tz() != FINGERPRINT_OK) {
    Serial.println(F("\n[..] Dedo borroso, no he podido leerlo."));
    cara.cambiar(RECHAZADO, "dedo borroso, repita");
    volverAEsperar = millis() + MOSTRAR_RESULTADO;
    ultimoAviso = millis();
    return;
  }

  int id = 0, confianza = 0;
  if (lector.fingerSearch() == FINGERPRINT_OK) {
    id = lector.fingerID;
    confianza = lector.confidence;
    Serial.print(F("\n[OK] Usuario "));
    Serial.print(id);
    Serial.print(F(", confianza "));
    Serial.println(confianza);
    cara.cambiar(ACEPTADO, "usuario - confianza " + String(confianza));
  } else {
    Serial.println(F("\n[--] Dedo desconocido."));
    cara.cambiar(RECHAZADO, "huella no registrada");
  }

  volverAEsperar = millis() + MOSTRAR_RESULTADO;
  avisarAlPortero(id, confianza);
  ultimoAviso = millis();
}

void conectarWifi() {
  Serial.print(F("\nConectando al WiFi "));
  Serial.print(WIFI_NOMBRE);
  WiFi.begin(WIFI_NOMBRE, WIFI_CONTRASENA);
  unsigned long limite = millis() + 20000;
  while (WiFi.status() != WL_CONNECTED && millis() < limite) {
    delay(400);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F(" conectado, soy "));
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(F(" no he podido. El lector sigue funcionando por cable."));
  }
}

// Preguntar a voces por la red donde esta el portero. Igual que hace la
// ESP32-CAM: asi la IP del portatil puede cambiar sin romper nada.
void buscarPortero() {
  if (WiFi.status() != WL_CONNECTED) return;
  Serial.println(F("Buscando al portero en la red..."));
  udp.begin(0);

  for (int intento = 0; intento < 4; intento++) {
    udp.beginPacket(IPAddress(255, 255, 255, 255), PUERTO_BUSQUEDA);
    udp.print("portero?");
    udp.endPacket();

    unsigned long limite = millis() + 1500;
    while (millis() < limite) {
      if (udp.parsePacket() > 0) {
        char buzon[200] = {0};
        udp.read(buzon, sizeof(buzon) - 1);
        String respuesta = String(buzon);

        // La IP buena es la que viene DENTRO del mensaje, no la del
        // remitente: el portatil tiene tarjetas de red virtuales (VMware) y
        // a veces contesta desde una que no se alcanza desde aqui.
        String ip = "";
        int marca = respuesta.indexOf("\"ip\":\"");
        if (marca >= 0) {
          int fin = respuesta.indexOf('"', marca + 6);
          if (fin > marca) ip = respuesta.substring(marca + 6, fin);
        }
        if (ip.length() == 0) ip = udp.remoteIP().toString();

        int puerto = 8780;
        int marcaPuerto = respuesta.indexOf("\"puerto\":");
        if (marcaPuerto >= 0) puerto = respuesta.substring(marcaPuerto + 9).toInt();

        direccionPortero = "http://" + ip + ":" + String(puerto);
        Serial.print(F("El portero esta en "));
        Serial.println(direccionPortero);
        return;
      }
      delay(50);
    }
  }
  Serial.println(F("No he encontrado al portero. Lo reintentare al leer un dedo."));
}

void avisarAlPortero(int id, int confianza) {
  if (direccionPortero == "") buscarPortero();
  if (direccionPortero == "") {
    Serial.println(F("     (sin portero a la vista: no aviso a nadie)"));
    return;
  }

  HTTPClient http;
  http.begin(direccionPortero + "/api/huella");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Clave", CLAVE_PORTERO);
  http.setTimeout(8000);

  String cuerpo = String("{\"id\":") + id + ",\"confianza\":" + confianza + "}";
  int codigo = http.POST(cuerpo);
  Serial.print(F("     El portero dice: "));
  Serial.print(codigo);
  Serial.print(F(" "));
  Serial.println(http.getString());
  http.end();

  // Si no contesta, quiza el portatil ha cambiado de IP: se vuelve a buscar.
  if (codigo <= 0) direccionPortero = "";
}


// --- La pantalla --------------------------------------------------------

void arrancarPantalla() {
  // Reinicio por cable ANTES de que hable la libreria: sin esto, la pantalla
  // a veces arranca a medias y se queda en blanco.
  pinMode(PIN_RESET_PANTALLA, OUTPUT);
  digitalWrite(PIN_RESET_PANTALLA, HIGH); delay(10);
  digitalWrite(PIN_RESET_PANTALLA, LOW);  delay(30);
  digitalWrite(PIN_RESET_PANTALLA, HIGH); delay(150);

  tft.init();
  tft.setRotation(0);        // vertical, el conector arriba
  cara.empezar();
  Serial.println(F("Pantalla lista."));
}
