//                  CONFIGURACION DE MARCELINO
//
// Pantalla "2.4 TFT LCD SHIELD" (chip compatible ILI9341) conectada por bus
// PARALELO de 8 bits a un ESP32 DevKit de AZ-Delivery.
//
// OJO: este archivo vive DENTRO de la libreria. Si algun dia se actualiza
// TFT_eSPI, se sobrescribe y hay que volver a ponerlo. Hay copia en
// MarcelinoPantalla/User_Setup_Marcelino.h, en este repositorio.
// El original de la libreria esta al lado, como User_Setup.h.original
//
// El cableado se comprobo a mano el 20/9/2026: se leyo el identificador del
// chip (0x93 02) y se pinto la pantalla de colores SIN libreria, para estar
// seguros del hardware antes de meter software por medio.

#define USER_SETUP_INFO "Marcelino ESP32 paralelo 8 bits"

// --- Que chip lleva la pantalla ---
#define ILI9341_DRIVER

// --- Bus paralelo de 8 bits (NO es SPI) ---
#define TFT_PARALLEL_8_BIT

// --- Patillas de mando ---
#define TFT_CS   23    // LCD_CS
#define TFT_DC   21    // LCD_RS: 0 = orden, 1 = dato
#define TFT_RST  -1    // LCD_RST se maneja a mano desde el programa (G32)
#define TFT_WR   17    // LCD_WR
#define TFT_RD   22    // LCD_RD

// --- Los ocho cables de datos ---
#define TFT_D0   13
#define TFT_D1   14
#define TFT_D2   27
#define TFT_D3   26
#define TFT_D4   25
#define TFT_D5    4
#define TFT_D6    5
#define TFT_D7   19

// --- Tipos de letra disponibles ---
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_GFXFF
#define SMOOTH_FONT
