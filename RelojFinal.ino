#include <Wire.h>                 // Librería para comunicación I2C.
#include <RTClib.h>               // Librería para manejar el RTC DS3231.
#include <LiquidCrystal_I2C.h>    // Librería para controlar LCD I2C.
#include <WiFi.h>                 // Librería para conexión WiFi (modo AP).
#include <AsyncTCP.h>             // TCP asíncrono.
#include <ESPAsyncWebServer.h>    // Servidor web asíncrono.
#include <piezo-music.h>          // Librería para reproducir melodías.
#include <example-music.h>        // Melodía de ejemplo (Zelda).

// Definición de pines.
#define PIN_BUZZER 32   // Pin del zumbador piezoeléctrico.
#define PIN_RELE   33   // Pin del relé (activo en bajo).
const int pinesLED[] = {25, 26, 27};    // Pines de los tres LEDs.

// Objetos para periféricos.
// RTC, LCD, etc.
RTC_DS3231 rtc;                         // Objeto para manejar el reloj RTC.
LiquidCrystal_I2C lcd(0x27, 16, 2);     // LCD I2C con dirección 0x27 y 16x2 caracteres.
AsyncWebServer servidor(80);            // Servidor web en puerto 80.

// Credenciales para red WiFi en modo AP.
const char* ssid = "AlarmaESP32";
const char* password = "123456789";

// Cantidad de notas de la melodía cargada.
int cantidad = sizeof(zelda_main_theme_melody) / sizeof(zelda_main_theme_melody[0]);

// Variables para seleccionar secuencia actual y de la próxima semana.
byte Eleccion = 0;                // 0: secuencia 1, 1: secuencia 2.
byte EleccionProximaSemana = 0;   // Para la siguiente semana.

// Estructuras de Horas y dias en los que debe activarse algun evento.
struct Alarma {
  byte dia;    // 1 = lunes a viernes, 6 = sábado
  byte hora; 
  byte minuto;
  byte Evento; // Ultimo elemento dentro de {x,y,z,a}, o sea "a". 0:RELE, 1: LED1, 2:LED2, 3:LED3, 4:LED4, 5:BUZZER1, 6:BUZZER2, 7:BUZZER3, 8:LED5, 9:BUZZER4, 10:MELODIA.
};

// Definición de secuencia 1 de eventos.
Alarma sec1[] = {
   {1,7,0,0},{1,7,30,1},{1,8,0,5},{1,8,30,2},{1,9,0,6},{1,9,30,3},{1,13,0,0},{1,13,30,4},{1,14,0,7},{1,14,30,8},{1,15,0,9},{1,15,30,10},{1,19,0,0},{1,19,30,2},{1,20,0,5},{1,20,30,8},{1,21,0,10},
   {6,7,0,0},{6,7,30,1},{6,8,0,5},{6,8,30,2},{6,13,0,0},{6,13,30,3},{6,14,0,6},{6,14,30,4},{6,19,0,0},{6,19,30,8},{6,20,0,10}
};

// Definición de secuencia 2 de eventos.
Alarma sec2[] = {
  {1,6,0,0},{1,6,30,3},{1,7,0,7},{1,7,30,4},{1,8,0,9},{1,8,30,8},{1,12,0,0},{1,12,30,10},{1,13,0,1},{1,13,30,5},{1,14,0,2},{1,14,30,6},{1,18,0,0},{1,18,30,3},{1,19,0,7},{1,19,30,4},{1,20,0,10},
  {6,6,0,0},{6,6,30,1},{6,7,0,5},{6,7,30,2},{6,12,0,0},{6,12,30,3},{6,13,0,6},{6,13,30,4},{6,18,0,0},{6,18,30,8},{6,19,0,10}
};

// Arreglo de alarmas activas (se copia desde sec1 o sec2 dependiendo cual se elija).
Alarma elegida[28];
const int totalAlarmas = sizeof(elegida) / sizeof(elegida[0]);  // Número total de alarmas.

void setup() {
  Serial.begin(115200);   // Iniciar comunicación serial.
  Wire.begin();           // Iniciar bus I2C.
  lcd.init();             // Inicializar pantalla LCD.
  lcd.backlight();        // Encender luz de fondo.
  copiarSecuencia();      // Cargar la secuencia elegida.

// Inicializar el RTC.
  if (!rtc.begin()) {
    Serial.println("No se detecta el RTC"); // Detener programa si no hay RTC.
    while (true);
  }

// Ajustar el RTC si perdió la hora.
  DateTime now = rtc.now();
  if (rtc.lostPower() || now.year() < 2025 || now.year() > 2025) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

// Configurar pines de salida.
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_RELE, OUTPUT); 
  digitalWrite(PIN_RELE, HIGH); // Inicialmente apagado (activo en bajo).

  for (int i = 0; i < 3; i++) {
    pinMode(pinesLED[i], OUTPUT);
    digitalWrite(pinesLED[i], LOW);
  }

// Crear tarea separada para actualizar LCD en núcleo 1.
  xTaskCreatePinnedToCore(
    tareaLCD,         // función.
    "MostrarHora",    // nombre de la tarea.
    2048,             // tamaño del stack.
    NULL,             // parámetros.
    1,                // prioridad.
    NULL,             // puntero a la tarea (no usamos).
    1                 // núcleo 1.
  );

// Crear red WiFi en modo Access Point (AP).
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("IP local: ");
  Serial.println(IP);

// Página principal HTML para seleccionar secuencias.
  servidor.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>Selector de Eventos</title>";
    html += "<style>body{font-family:sans-serif;text-align:center;background:#0f0f0f;color:#39ff14;}h2{margin-top:20px;}form{margin:10px;}select,button{font-size:16px;padding:5px;border-radius:5px;}button{background:#39ff14;color:#0f0f0f;border:none;margin-top:10px;}button:hover{background:#2fdc0a;}</style></head><body>";
    html += "<h2>Selector de Eventos</h2>";
    html += "<form action='/set' method='GET'>";
    html += "<label>Secuencia esta semana: </label>";
    html += "<select name='ahora'><option value='0'" + String(Eleccion == 0 ? " selected" : "") + ">Secuencia 1</option><option value='1'" + String(Eleccion == 1 ? " selected" : "") + ">Secuencia 2</option></select><br>";
    html += "<label>Secuencia próxima semana: </label>";
    html += "<select name='luego'><option value='0'" + String(EleccionProximaSemana == 0 ? " selected" : "") + ">Secuencia 1</option><option value='1'" + String(EleccionProximaSemana == 1 ? " selected" : "") + ">Secuencia 2</option></select><br><br>";
    html += "<button type='submit'>Aplicar</button></form>";
    html += "<p>Secuencia activa: " + String(Eleccion == 0 ? "Secuencia 1" : "Secuencia 2") + "</p>";
    html += "<p>Secuencia próxima: " + String(EleccionProximaSemana == 0 ? "Secuencia 1" : "Secuencia 2") + "</p></body></html>";
    request->send(200, "text/html", html);
  });

  servidor.on("/set", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("ahora")) {
      Eleccion = request->getParam("ahora")->value().toInt();
      copiarSecuencia();
    }
    if (request->hasParam("luego")) {
      EleccionProximaSemana = request->getParam("luego")->value().toInt();
    }
    request->redirect("/");
  });

  servidor.begin();
}

// LOOP.
void loop() {
  DateTime ahora = rtc.now();            // Obtener fecha y hora actual.
  int diaActual = ahora.dayOfTheWeek();  // 0 = Domingo, 6 = Sábado.
  int horaActual = ahora.hour();         // Hora actual.
  int minutoActual = ahora.minute();     // Minuto actual.

// Resetear actuadores.
  digitalWrite(PIN_RELE, HIGH);          // Apagar relé.
  for (int i = 0; i < 3; i++) {
    digitalWrite(pinesLED[i], LOW);      // Apagar LEDs.
  }
  noTone(PIN_BUZZER);                    // Detener cualquier sonido previo.

// Recorrer todas las alarmas activas
  for (int i = 0; i < totalAlarmas; i++) {
      Alarma a = elegida[i];

// Validar si la alarma corresponde al día actual.
      bool diaCoincide = (a.dia == 1 && diaActual >= 0 && diaActual <= 5) || (a.dia == 6 && diaActual == 6);

// Si coincide día, hora y minuto.
      if (diaCoincide && a.hora == horaActual && a.minuto == minutoActual){
        switch (a.Evento) {
          case 0: // Relé
               digitalWrite(PIN_RELE, LOW);  // ACTIVAR.
               delay(58000);
               break;
          case 1: // LED1
               digitalWrite(pinesLED[0], HIGH);
               delay(58000);
               break;
          case 2: // LED2
                digitalWrite(pinesLED[1], HIGH);
                delay(58000);
                break;
          case 3: // LED3
                digitalWrite(pinesLED[2], HIGH);
                delay(58000);
                break;
          case 4: // LED4 (Prenden todos de a uno).
                for (int i = 0; i < 3; i++){ 
                  digitalWrite(pinesLED[i], HIGH);
                  delay(18000);
                }
                break;
          case 5: // Buzzer un tono.
                for (int i = 0; i < 30; i++){ 
                tone(PIN_BUZZER, 1000, 1000);
                delay(2000);
                }
                break;
          case 6: // Buzzer otro tono.
                for (int i = 0; i < 60; i++){ 
                tone(PIN_BUZZER, 1200, 1000);
                delay(1000);
                }
                break;
          case 7: // Buzzer otro tono.
                for (int i = 0; i < 30; i++){ 
                tone(PIN_BUZZER, 800, 1000);
                delay(2000);
                }
                break;
          case 8: // LED5 (parpadeo)
                for (int k = 0; k < 4; k++) {                     // Parpadea 4 veces (2 ciclos ON/OFF).
                  for (int i = 0; i < 3; i++) {
                    digitalWrite(pinesLED[i], i % 2 == (k % 2));  // Alterna patrón.
                  }
                  delay(300);  // Espera 300 ms entre cambios.
                }   
                // Apaga todos al final.
                 for (int i = 0; i < 3; i++) digitalWrite(pinesLED[i], LOW);
                 break;
          case 9: // Buzzer otro tono.
                for (int i = 0; i < 60; i++){ 
                tone(PIN_BUZZER, 1500, 1000);
                delay(1000);
                }
                break;
          case 10: // Melodía.
                playSong(PIN_BUZZER, zelda_main_theme_melody, zelda_main_theme_rythm, cantidad, 40);
                break;  
      }
  }
  delay(200); // Pausa breve entre iteraciones.
}

// Cambio automático de secuencia al lunes a las 00:00.
  if (diaActual == 1 && horaActual == 0 && minutoActual == 0 && Eleccion != EleccionProximaSemana) {
  Eleccion = EleccionProximaSemana;
  copiarSecuencia();
  delay(60000); // Evitar múltiples cambios ese minuto.
}
}

// TAREA LCD: siempre muestra la hora.
void tareaLCD(void* parametro) {
  for (;;) {
    DateTime ahora = rtc.now();

    // Fecha
    lcd.setCursor(0, 0);
    char fecha[17];
    sprintf(fecha, "%02d/%02d/%04d", ahora.day(), ahora.month(), ahora.year());
    lcd.print(fecha);

    // Hora
    lcd.setCursor(0, 1);
    char hora[17];
    sprintf(hora, "%02d:%02d:%02d", ahora.hour(), ahora.minute(), ahora.second());
    lcd.print(hora);

    delay(1000); // Actualiza cada segundo
  }
}

// Copiar secuencia activa al arreglo 'elegida'.
void copiarSecuencia() {
  if (Eleccion == 0) {
    for (int i = 0; i < 28; i++) {
      elegida[i] = sec1[i];
    }
  } else {
    for (int i = 0; i < 28; i++) {
      elegida[i] = sec2[i];
    }
  }
}
