# Marcelino ESP32

Firmware de los módulos de hardware del asistente Marcelino. Cada carpeta es
un sketch de Arduino para una placa distinta.

## Módulos

### MarcelinoCam — ESP32-CAM (AI-Thinker)
Detector de presencia. Compara fotogramas en blanco y negro dentro de la
propia placa y, al detectar movimiento, avisa al PC para que lance el atajo
de llegada. Las imágenes no salen del dispositivo: solo viaja un aviso de
texto. Localiza el PC por descubrimiento UDP, así que no depende de una IP
fija.

### MarcelinoHuella — ESP32 + lector de huellas
Identifica al dueño y arranca el asistente. Las huellas se almacenan dentro
del módulo lector, que solo responde con un número de usuario: la huella en
sí nunca llega al PC. Incluye la cara animada en pantalla TFT.

### MarcelinoPantalla — ESP32 + TFT 2.4" bus paralelo 8 bits
Utilidad de diagnóstico. Las pantallas de 2.4" montan controladores distintos
según el lote (ILI9341, ILI9486, ST7789, HX8347, R61505). Este sketch
interroga los registros de identificación para averiguar cuál lleva la placa.
El cableado completo está documentado en la cabecera del sketch.

### MarcelinoCara — ESP32 + TFT
Banco de pruebas del rostro: recorre los cinco estados de la cara sin
depender del lector ni de la red.

## Dependencias

TFT_eSPI, Adafruit Fingerprint Sensor Library, esp32-camera.

`User_Setup_Marcelino.h` contiene la configuración de pines que hay que
copiar dentro de la librería TFT_eSPI.

## Configuración

Los sketches llevan las constantes de WiFi vacías. Rellénalas antes de
compilar.

## Licencia

MIT. Ver `LICENSE`.
