/*
  MarcelinoPantalla — ¿quién eres, pantalla?
  ------------------------------------------
  Estas pantallas rojas de 2.4" llevan controladores distintos según el lote
  (ILI9341, ILI9486, ST7789, HX8347, R61505...) y no se puede saber cuál es
  mirándolas por fuera. Este programa le habla por el bus de 8 bits y le pide
  que se identifique, leyendo los registros donde cada fabricante guarda su
  número de modelo.

  Con la respuesta elegiremos la librería correcta para dibujar la cara de
  Marcelino. Este programa NO dibuja nada: solo pregunta.

  CONEXIONES (ESP32 DevKit AZ-Delivery, 38 pines):
      LCD_D0 -> G13     LCD_D4 -> G25     LCD_RST -> G32
      LCD_D1 -> G14     LCD_D5 -> G4      LCD_CS  -> G23
      LCD_D2 -> G27     LCD_D6 -> G5      LCD_RS  -> G21
      LCD_D3 -> G26     LCD_D7 -> G19     LCD_WR  -> G17
                                          LCD_RD  -> G22
      5V  -> pin 5V del ESP32 (el de la esquina del USB)
      GND -> un GND de la COLUMNA IZQUIERDA
             (el de debajo del 5V esta MUERTO en esta placa)

  El lector de huellas sigue en G16 (amarillo) y G18 (blanco): este programa
  no los toca.
*/

// Los ocho cables de datos, en orden D0..D7.
const int DATOS[8] = {13, 14, 27, 26, 25, 4, 5, 19};

const int PIN_RST = 32;
const int PIN_CS  = 23;
const int PIN_RS  = 21;   // 0 = le mando una orden, 1 = le mando/pido un dato
const int PIN_WR  = 17;
const int PIN_RD  = 22;

// Registros donde los distintos fabricantes guardan su identificación.
struct Registro { uint8_t codigo; const char* pista; };
const Registro REGISTROS[] = {
  {0x00, "ID clasico (R61505, ILI9325, HX8347...)"},
  {0x04, "RDDID (ILI9341, ILI9486, ST7789)"},
  {0x09, "estado del visor"},
  {0xBF, "ID extendido (ILI9486/ILI9488)"},
  {0xD3, "ID4 (ILI9341: sale 00 93 41)"},
  {0xEF, "ID de algunos ST"},
  {0xFC, "ID de algunos HX"},
};
const int CUANTOS = sizeof(REGISTROS) / sizeof(REGISTROS[0]);


void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println();
  Serial.println(F("=== Marcelino: identificando la pantalla ==="));

  for (int i = 0; i < 8; i++) pinMode(DATOS[i], OUTPUT);
  for (int p : {PIN_RST, PIN_CS, PIN_RS, PIN_WR, PIN_RD}) {
    pinMode(p, OUTPUT);
    digitalWrite(p, HIGH);
  }

  reiniciarPantalla();

  Serial.println(F("\nPreguntando registro por registro:"));
  Serial.println(F("  (00 en todo = no contesta;  FF en todo = bus al aire)"));
  Serial.println();

  bool algunaRespuesta = false;
  for (int i = 0; i < CUANTOS; i++) {
    uint8_t leido[5] = {0};
    leerRegistro(REGISTROS[i].codigo, leido, 5);

    Serial.print(F("  0x"));
    if (REGISTROS[i].codigo < 16) Serial.print('0');
    Serial.print(REGISTROS[i].codigo, HEX);
    Serial.print(F(" -> "));
    for (int b = 0; b < 5; b++) {
      if (leido[b] < 16) Serial.print('0');
      Serial.print(leido[b], HEX);
      Serial.print(' ');
    }
    Serial.print(F("   ("));
    Serial.print(REGISTROS[i].pista);
    Serial.println(')');

    for (int b = 0; b < 5; b++) {
      if (leido[b] != 0x00 && leido[b] != 0xFF) algunaRespuesta = true;
    }
  }

  Serial.println();
  if (algunaRespuesta) {
    Serial.println(F("[OK] La pantalla contesta. Probamos a encenderla como un ILI9341."));
    arrancarComoILI9341();
    Serial.println(F("Pintando ROJO..."));   pintarTodo(0xF800); delay(1500);
    Serial.println(F("Pintando VERDE..."));  pintarTodo(0x07E0); delay(1500);
    Serial.println(F("Pintando AZUL..."));   pintarTodo(0x001F); delay(1500);
    Serial.println(F("Pintando BLANCO...")); pintarTodo(0xFFFF); delay(1500);
    Serial.println(F("Y una franja de cada color, de arriba abajo."));
    franjas();
    Serial.println(F("\nSi has visto los colores, es compatible con ILI9341."));
    Serial.println(F("Si sigue en blanco o en negro, el chip es otro."));
  } else {
    Serial.println(F("[X] Solo ceros o solo efes: la pantalla no dice nada."));
    Serial.println(F("    Repasa el cableado, sobre todo GND, WR, RD, RS y CS."));
  }
}

void loop() {
  delay(1000);
}


// --- Hablarle al bus de 8 bits ------------------------------------------

void reiniciarPantalla() {
  digitalWrite(PIN_RST, HIGH); delay(10);
  digitalWrite(PIN_RST, LOW);  delay(30);    // un reset de verdad, por cable
  digitalWrite(PIN_RST, HIGH); delay(200);   // le damos tiempo a despertar
}

void modoSalida() {
  for (int i = 0; i < 8; i++) pinMode(DATOS[i], OUTPUT);
}

void modoEntrada() {
  for (int i = 0; i < 8; i++) pinMode(DATOS[i], INPUT);
}

void ponerEnBus(uint8_t valor) {
  for (int i = 0; i < 8; i++) digitalWrite(DATOS[i], (valor >> i) & 1);
}

uint8_t recogerDelBus() {
  uint8_t valor = 0;
  for (int i = 0; i < 8; i++) {
    if (digitalRead(DATOS[i])) valor |= (1 << i);
  }
  return valor;
}

void pulsoEscritura() {
  digitalWrite(PIN_WR, LOW);
  delayMicroseconds(1);
  digitalWrite(PIN_WR, HIGH);
  delayMicroseconds(1);
}

void leerRegistro(uint8_t registro, uint8_t* destino, int cuantos) {
  // 1) Se le manda la ORDEN (RS en bajo).
  modoSalida();
  digitalWrite(PIN_CS, LOW);
  digitalWrite(PIN_RS, LOW);
  ponerEnBus(registro);
  pulsoEscritura();

  // 2) Se leen sus respuestas (RS en alto, y el bus pasa a ser entrada).
  digitalWrite(PIN_RS, HIGH);
  modoEntrada();
  for (int i = 0; i < cuantos; i++) {
    digitalWrite(PIN_RD, LOW);
    delayMicroseconds(2);          // le damos tiempo a poner el dato
    destino[i] = recogerDelBus();
    digitalWrite(PIN_RD, HIGH);
    delayMicroseconds(2);
  }
  digitalWrite(PIN_CS, HIGH);
  modoSalida();
}


// --- Encenderla y pintar (a la manera del ILI9341) ----------------------

const int ANCHO = 240;
const int ALTO  = 320;

void escribirOrden(uint8_t orden) {
  modoSalida();
  digitalWrite(PIN_CS, LOW);
  digitalWrite(PIN_RS, LOW);
  ponerEnBus(orden);
  pulsoEscritura();
  digitalWrite(PIN_RS, HIGH);
}

void escribirDato(uint8_t dato) {
  ponerEnBus(dato);
  pulsoEscritura();
}

void terminarOrden() {
  digitalWrite(PIN_CS, HIGH);
}

void orden(uint8_t o, const uint8_t* datos, int cuantos) {
  escribirOrden(o);
  for (int i = 0; i < cuantos; i++) escribirDato(datos[i]);
  terminarOrden();
}

void arrancarComoILI9341() {
  reiniciarPantalla();

  escribirOrden(0x01); terminarOrden();          // reinicio por software
  delay(150);

  const uint8_t formato[] = {0x55};              // 16 bits por punto
  orden(0x3A, formato, 1);

  const uint8_t giro[] = {0x48};                 // orientacion y orden de colores
  orden(0x36, giro, 1);

  escribirOrden(0x11); terminarOrden();          // despertar
  delay(150);
  escribirOrden(0x29); terminarOrden();          // encender el visor
  delay(50);
}

void ventana(int x0, int y0, int x1, int y1) {
  uint8_t cols[] = {(uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8), (uint8_t)x1};
  orden(0x2A, cols, 4);
  uint8_t filas[] = {(uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)(y1 >> 8), (uint8_t)y1};
  orden(0x2B, filas, 4);
}

void pintar(int x0, int y0, int ancho, int alto, uint16_t color) {
  ventana(x0, y0, x0 + ancho - 1, y0 + alto - 1);
  escribirOrden(0x2C);                           // a partir de aqui, puntos
  uint8_t alto8 = color >> 8, bajo8 = color & 0xFF;
  for (long i = 0; i < (long)ancho * alto; i++) {
    escribirDato(alto8);
    escribirDato(bajo8);
  }
  terminarOrden();
}

void pintarTodo(uint16_t color) {
  pintar(0, 0, ANCHO, ALTO, color);
}

void franjas() {
  const uint16_t colores[] = {0xF800, 0xFD20, 0xFFE0, 0x07E0, 0x001F, 0x781F};
  int altoFranja = ALTO / 6;
  for (int i = 0; i < 6; i++) pintar(0, i * altoFranja, ANCHO, altoFranja, colores[i]);
}
