
# Proyecto: Selector de Secuencia de Alarmas Semanales con ESP32

Este proyecto permite programar y ejecutar automáticamente **eventos semanales** en una placa ESP32, controlando periféricos como un **relé, LEDs y un buzzer piezoeléctrico**, con ayuda de un **reloj de tiempo real (RTC DS3231)** y una **interfaz web en modo punto de acceso (AP)**. El usuario puede seleccionar entre **dos secuencias diferentes de eventos**, tanto para la semana actual como para la siguiente.

---

## Tabla de Contenidos

- Descripción General
- Características Principales
- Componentes y Hardware
- Esquema de Conexiones
- Funcionamiento del Sistema
  - Modo Access Point y Servidor Web
  - Lógica de Cambio de Semana
  - Estructura de la Secuencia
  - Ejecución de Eventos
- Interfaz Web
- Instrucciones de Uso
- Posibles Mejoras Futuras
- Créditos y Licencia

---

## Descripción General

La ESP32 actúa como un sistema autónomo de alarmas semanales. Cada semana se ejecuta una de dos secuencias predefinidas, que pueden ser seleccionadas desde un navegador web al conectarse a la red WiFi creada por el propio ESP32. Cada secuencia contiene **28 eventos distribuidos entre lunes a viernes y el sábado**, incluyendo:

- Activación de relé
- Encendido de LEDs
- Sonido del buzzer en distintos patrones
- Reproducción de una melodía

---

## Características Principales

- Reloj de tiempo real **RTC DS3231** para mantener precisión aún sin energía.
- Pantalla **LCD I2C 16x2** que muestra fecha y hora actualizadas cada segundo.
- Interfaz web moderna (HTML + CSS) con dos selectores:
  - Para la **secuencia actual** (semana en curso)
  - Para la **secuencia de la próxima semana**
- Cambio automático de secuencia cada **lunes a las 00:00**.
- Dos secuencias de eventos con estructura fija para evitar conflictos de solapamiento.
- Controla periféricos a través de pines GPIO de la ESP32.
- Basado en **ESPAsyncWebServer** para respuesta rápida sin recargar la página.

---

## Componentes y Hardware

- 1x ESP32 DevKit
- 1x Módulo RTC DS3231
- 1x Pantalla LCD 16x2 con interfaz I2C
- 1x Módulo relé (activo en bajo)
- 1x Buzzer piezoeléctrico (pasivo o activo)
- 3x LEDs de colores
- 3x Resistencias 330Ω para los LEDs
- Cables Dupont, protoboard o PCB

---

## Esquema de Conexiones

| Componente | Pin ESP32 |
|------------|-----------|
| LCD SDA    | GPIO 21   |
| LCD SCL    | GPIO 22   |
| RTC SDA    | GPIO 21   |
| RTC SCL    | GPIO 22   |
| Buzzer     | GPIO 32   |
| Relé       | GPIO 33   |
| LED 1      | GPIO 25   |
| LED 2      | GPIO 26   |
| LED 3      | GPIO 27   |

**Nota:** LCD y RTC comparten el bus I2C. Asegúrate de no tener conflictos de dirección (0x27 para LCD, 0x68 para DS3231).

---

## Funcionamiento del Sistema

### Modo Access Point y Servidor Web

El ESP32 crea su propia red WiFi llamada `AlarmaESP32` con contraseña `123456789`. Al conectarse desde un celular o PC, el navegador carga la interfaz web (al ingresar la IP local), donde se puede seleccionar la secuencia de eventos para esta semana y la próxima.

### Lógica de Cambio de Semana

Cada **lunes a las 00:00**, el sistema actualiza automáticamente la secuencia activa con la que se haya seleccionado como "próxima semana". Esto permite preparar cambios con anticipación y dejar el sistema funcionando sin intervención.

```cpp
if (diaActual == 1 && horaActual == 0 && minutoActual == 0 && Eleccion != EleccionProximaSemana) {
  Eleccion = EleccionProximaSemana;
  copiarSecuencia();
  delay(60000);  // Evita cambio múltiple en el mismo minuto
}
```

### Estructura de la Secuencia

Cada secuencia es un arreglo de estructuras `Alarma`, con 28 eventos por semana. Los días están codificados como:

- `1`: lunes a viernes
- `6`: sábado

```cpp
struct Alarma {
  byte dia;     // Día del evento
  byte hora;    // Hora del evento
  byte minuto;  // Minuto exacto
  byte Evento;  // Tipo de acción (0 a 10)
};
```

### Ejecución de Eventos

Cuando coincide el día, la hora y el minuto, se ejecuta el evento correspondiente:

| Evento | Acción                          |
|--------|---------------------------------|
| 0      | Activar relé                    |
| 1–3    | Encender LED 1, 2 o 3           |
| 4      | LEDs en secuencia rítmica       |
| 5–9    | Buzzer en distintos patrones    |
| 10     | Melodía "Zelda" por buzzer      |

Cada evento usa `delay()` porque nunca se solapan dos eventos. Por tanto, el uso de delay no interfiere con la lógica general.

---

## Interfaz Web

Diseñada con HTML y CSS moderno. Dos menús desplegables permiten al usuario seleccionar:

- La **secuencia de esta semana** (inmediatamente activa)
- La **secuencia de la próxima semana** (se activará automáticamente el próximo lunes)

El sitio es **responsive**, lo cual lo hace cómodo para visualizar y operar desde un celular.

---

## Instrucciones de Uso

1. Cargar el código en la ESP32 desde el IDE de Arduino.
2. Encender el sistema con todos los módulos conectados.
3. Conectarse a la red WiFi `AlarmaESP32`.
4. Abrir el navegador y acceder a la IP local (mostrada por el monitor serie).
5. Seleccionar la secuencia deseada.
6. ¡Listo! El sistema ejecutará eventos según lo programado.

---

## Posibles Mejoras Futuras

- Panel web para **crear nuevas secuencias personalizadas**.
- Almacenamiento de configuraciones en **EEPROM o SPIFFS**.
- Integración con sensores (movimiento, luz, temperatura).
- Envío de **notificaciones por Telegram o email**.
- Sincronización automática con **servidor NTP** para mantener la hora actualizada.

---

## Créditos y Licencia

Proyecto desarrollado por **[Tu Nombre]**, 2025.  
Basado en bibliotecas de código abierto como `ESPAsyncWebServer`, `RTClib`, y `LiquidCrystal_I2C`.

Este proyecto se encuentra bajo la **Licencia MIT**. Siéntete libre de modificarlo, adaptarlo y compartirlo, siempre reconociendo el autor original.
