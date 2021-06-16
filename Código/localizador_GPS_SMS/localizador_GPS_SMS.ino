#include <TimeLib.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>

const int PIN_LED = LED_BUILTIN, // Pin del Led integrado
          PIN_SIM_RX = 4,        // Pin de NodeMCU que recibe datos del módulo SIM
          PIN_SIM_TX = 5,        // Pin de NodeMCU que envía datos al módulo SIM
          PIN_GPS_RX = 14,       // Pin de NodeMCU que recibe datos del módulo GPS
          PIN_GPS_TX = 12,       // Pin de NodeMCU que envía datos al módulo GPS
          TIMEZONE = -3,         // Huso horario (GMT -3 p/ Argentina)
          CANT_COORD = 10;       // Nº de coordenadas que se guardan para dibujar el recorrido

// Variables globales
String ultimoComandoEnviado = "",         // Se usa para mostrar errores con comandos

       // Comandos específicos
       cmdEnviarSms = "SMS1",             // Enviar un SMS desde el módulo SIM800L
       cmdSetup = "Setup1",               // Hacer Setup del SIM800L
       cmdMarco = "marco",                // Responder un mensaje de prueba ("Polo")
       cmdLeerMemoriaSms = "leer SMS",    // Acceder a la memoria del SIM800L y leer un SMS
       cmdReset = "Reset1",               // Reiniciar Arduino
       cmdLeerGps = "GPS1",               // Leer los datos actuales del GPS
       cmdBateria = "Bateria1",           // Leer el estado actual de la batería (mV y %)
       cmdConectado = "Conectado?",       // Verificar si el SIM800L está conectado a la red 2G
       cmdDormirSim = "Dormir SIM",       // Cambiar el modo actual del SIM800L al modo SLEEP
       cmdDespertarSim = "Despertar SIM", // Cambiar el modo actual del SIM800L al modo ACTIVE
       cmdEstadoRedSim = "Estado?",       // Saber el modo actual del SIM800L
       cmdLeerRecorrido = "Recorrido1",   // Leer el recorrido hecho hasta ahora

       // Variables para guardar datos GPS
       fechaGps = "",
       diaGps = "",
       mesGps = "",
       anioGps = "",
       horaGps = "",
       minutosGps = "",
       segundosGps = "",
       horaCompletaGps = "",  // Hora + Minutos + Segundos
       longitudGps = "",
       latitudGps = "",
       satelitesGps = "",
       caminoRecorrido[CANT_COORD];  // Array que guarda las últimas posiciones GPS obtenidas

unsigned int tiempoEspera = 500,   // Utilizada mayormente en la función esperar() (ms)
             tiempoTimeout = 5000, // Intervalo de espera de confirmación de comandos y otros (ms)
             esperaRecorrido = 1;  // Intervalo de actualización del recorrido hecho (minutos)


unsigned long millisGps = 0,        // Variables de control de intervalos de tiempo
              millisFalloGps = 0,   //
              millisLed = 0,
              millisRecorrido = 0;

bool ultimoComandoOk = false,     // Control de comandos aplicados
     ultimaLecturaGpsOk = true;

TinyGPSPlus ObjectGps;  // Objeto GPS

SoftwareSerial SerialSim(PIN_SIM_RX, PIN_SIM_TX);   // Objeto Serial Sim
/**
    SIM Tx --> NodeMCU D2 (pin 4)
    SIM Rx --> NodeMCU D1 (pin 5)
*/

SoftwareSerial SerialGps(PIN_GPS_RX, PIN_GPS_TX);   // Objeto Serial Gps
/**
    GPS Tx --> NodeMCU D5 (pin 14)
    GPS Rx --> NodeMCU D6 (pin 12)
*/


void setup()  {
  Serial.begin(9600); SerialSim.begin(9600); SerialGps.begin(9600);

  pinMode(PIN_LED, OUTPUT); digitalWrite(PIN_LED, LOW); // Se enciende el LED
  esperar(2 * tiempoEspera); Serial.println();

  Serial.println("Iniciando...");  Serial.println();

  // Hacer Setup del SIM
  setup_sim(); Serial.println();

  Serial.println("Entrando en Loop...");
  digitalWrite(PIN_LED, HIGH);   // Se apaga el LED
}


void loop() {
  procesar_sim();
  procesar_usb();
  procesar_gps();

  if (millis() - millisLed >= 250) {   // El Led titila cada 250ms en Loop
    millisLed = millis();
    bool estadoLed = digitalRead(PIN_LED);
    digitalWrite(PIN_LED, !estadoLed);
  }
}

/**
   Leer si hay información proveniente del módulo SIM800L.
   Si se recibe información, se ejecuta el comando.
*/
void procesar_sim() {
  String lecturaSim = leer_sim_no_bloqueo();
  if (lecturaSim.length() > 0) {
    ejecutar_comando_sim(lecturaSim);
  }
}


/**
   Ejecutar un comando recibido por consola (monitor serie).
   Si se recibe información, se ejecuta el comando.
*/
void procesar_usb() {
  String lecturaUsb = leer_usb_no_bloqueo();
  if (lecturaUsb.length() > 0) {
    Serial.println("Ejecutando comando: " + lecturaUsb);
    ejecutar_comando_usb(lecturaUsb);
  }
}


/**
   Leer si hay info desde el SIM800L, y retornar el texto
*/
String leer_sim_no_bloqueo() {
  String varSimNoBloq = "";
  if (SerialSim.available()) {
    varSimNoBloq = SerialSim.readStringUntil('\n');
    varSimNoBloq.replace("\r", "");   // Quitar retorno de carro del string
  }
  return varSimNoBloq;
}


/**
   Leer si hay info desde el teclado, y retornar el texto.
*/
String leer_usb_no_bloqueo() {
  String varUsbNoBloq = "";
  if (Serial.available()) {
    Serial.println();
    varUsbNoBloq = Serial.readStringUntil('\n');
    varUsbNoBloq.replace("\r", "");   // Quitar retorno de carro del string
  }
  return varUsbNoBloq;
}


/**
   Leer ininterrumpidamente el input del teclado, hasta que reciba al menos un caracter.
   Finalmente devuelve el string leído.
*/
String leer_usb_bloqueo() {
  String varUsbBloq = "";
  bool bucleUsbBloq = true;
  while (bucleUsbBloq) {
    varUsbBloq = leer_usb_no_bloqueo();
    bucleUsbBloq = !(varUsbBloq.length() > 0);
  }
  return varUsbBloq;
}


/**
   Ejecutar el comando recibido desde el SIM800L
*/
void ejecutar_comando_sim(String comando) {
  if (comando.startsWith("+CMTI"))  sms_recibido(comando);
  else  Serial.println(comando);
  if (comando == "OK") {
    Serial.println();
  }
}


/**
   Ejecutar el comando recibido desde el teclado.
   Si no se encuentra algún comando propio,
   igualmente se envía el string al SIM800L.
*/
void ejecutar_comando_usb(String comando) {
  // Se chequea si el comando ingresado es AT o no
  bool esComandoAt = (comando.equalsIgnoreCase("AT"))
                     || (comando.substring(0, 3)).equalsIgnoreCase("AT+");

  // Si es comando AT, se envía el texto al SIM800L
  if (esComandoAt) {
    SerialSim.println(comando);
    ultimoComandoEnviado = comando;
    esperar_confirmacion("OK", tiempoTimeout);

    // Si no es comando AT, se verifica todas las posibilidades de que sea un comando propio
  } else if (comando == cmdEnviarSms) {    //Enviar un SMS desde consola
    digitalWrite(PIN_LED, LOW);            // Se enciende el led mientras se ejecuta el comando

    Serial.println("INGRESE EL NÚMERO DESTINATARIO.");
    String numEnviarSms = leer_usb_bloqueo();

    // Se imprime el número por consola y se espera un momento
    Serial.println(numEnviarSms); Serial.println(); esperar(tiempoEspera);

    Serial.println("INGRESE EL TEXTO A ENVIAR.");
    String txtEnviarSms = leer_usb_bloqueo(); // Se espera hasta que se recibe algún caracter

    // Se imprime el texto por consola y se espera un momento
    Serial.println(txtEnviarSms); Serial.println(); esperar(tiempoEspera);

    // Se envía el mensaje SMS. Ver función para detalles.
    enviar_sms(numEnviarSms, txtEnviarSms);
    digitalWrite(PIN_LED, HIGH);

  } else if (comando == cmdSetup) setup_sim(); // Hacer Setup del SIM800L

  else if (comando == cmdLeerMemoriaSms) {    // Acceder a un SMS guardado en la memoria
    digitalWrite(PIN_LED, LOW);
    Serial.println("Seleccione el número de memoria a buscar (nº, o \"todo\")");
    String numMemElegido = leer_usb_bloqueo();
    if (numMemElegido.equalsIgnoreCase("todo")) {
      SerialSim.println("AT+CMGL=\"ALL\"");   // Comando AT para leer todos los SMS en memoria
      ultimoComandoEnviado = "Leer todos los SMS";
      esperar_confirmacion("OK", 20000);       // Se imprime todo por consola durante 20 segundos
    } else {
      Serial.print("Seleccione el dato a leer ");
      Serial.println("(estado, numero, texto, fecha, hora, gmt, todo)");
      String opcionElegida = leer_usb_bloqueo();
      Serial.println("Leyendo " + opcionElegida
                     + " en la memoria " + numMemElegido + ":");
      ultimoComandoEnviado = "Leer " + opcionElegida +
                             " en la memoria " + numMemElegido;

      // Ver funcion extraer_info_sms
      String resultadoLeido = extraer_info_sms(numMemElegido, opcionElegida);
      Serial.println(resultadoLeido);
      Serial.println();
    }
    digitalWrite(PIN_LED, HIGH);

  } else if (comando == cmdReset) {   // Reiniciar Arduino
    Serial.println("Reiniciando Arduino...");
    reset_arduino();

  } else if (comando == cmdLeerGps) {   // Leer datos del GPS
    Serial.println(mostrar_info_gps());

  } else if (comando == cmdBateria) {   // Leer estado de la bateria
    Serial.println(leer_bateria());

  } else if (comando == cmdConectado) {   // Chequear conexión del SIM800L a la red
    if (sim_esta_conectado()) {
      Serial.println("Módulo SIM conectado a la red. Potencia: "
                     + potencia_red_sim());
    } else Serial.println("El módulo SIM NO está conectado a la red");

  } else if (comando == cmdDormirSim) {   // Activar el modo SLEEP del SIM800L
    dormir_sim();

  } else if (comando == cmdDespertarSim) {  // Activar el modo ACTIVE del SIM800L
    despertar_sim();

  } else if (comando == cmdEstadoRedSim) {  // Chequear el estado del SIM800L
    Serial.println("Chequeando estado de la red SIM...");
    if (sim_dormida()) Serial.println("El módulo SIM está en modo SLEEP.");
    else Serial.println("El módulo SIM está en modo ACTIVE.");

  } else if (comando == cmdLeerRecorrido) {
    Serial.println("Leyendo el camino recorrido...");
    String recorrido = camino_recorrido();
    //Serial.println("Link de Google Maps:");
    Serial.println(recorrido);
  }

  // Si no se recibe ningún comando propio, se envía igualmente el string al SIM800L
  else  SerialSim.println(comando);
}


/**
   Esperar un tiempo igual a [intervalo], en (ms), sin bloquear el microcontrolador.
*/
void esperar(unsigned long intervalo) {
  unsigned long millisPrevio = millis();
  while (millis() - millisPrevio < intervalo);
}


/**
   Enviar por SMS el texto del segundo argumento
   al número del primer argumento.
*/
void enviar_sms(String numSms, String txtSms) {
  digitalWrite(PIN_LED, LOW);    // Se enciende el led mientras se envía el SMS
  Serial.println("ENVIANDO MENSAJE...");

  SerialSim.println("AT+CMGS=\"" + numSms + '"'); // Comando AT para enviar SMS
  esperar(tiempoEspera);

  // Se inicia un cronómetro para saber cuánto se tardó en enviar el SMS
  unsigned long iniCronometro = millis();

  SerialSim.print(txtSms);  // Se envía el texto al SIM800L
  SerialSim.write(26);      // Caracter necesario para enviar SMS
  ultimoComandoEnviado = "Enviar SMS";

  // Se espera hasta 1 minuto la confirmación del envío del SMS
  esperar_confirmacion("+CMGS:", 60000);
  unsigned long finCronometro = millis();

  if (ultimoComandoOk)  {
    Serial.println("MENSAJE ENVIADO CORRECTAMENTE.");
    Serial.println("Tiempo tardado: " +
                   String((finCronometro - iniCronometro) / 1000) +
                   " segundos");

  } else  Serial.println("MENSAJE NO ENVIADO. HUBO UN ERROR.");

  digitalWrite(PIN_LED, HIGH);
}


/**
    La siguiente función se usa luego de haber enviado un comando al SIM800L.
    Primer argumento: el string que se espera recibir. (string)
    Segundo argumento: el tiempo que se quiere esperar para recibir el comando.

    Mientras la función esté esperando el comando, se imprimirán tres asteriscos
    al principio de cada renglón de la consola, por cada línea que se imprime,
    para que la persona que lee la consola sepa que aún se está esperando la confirmación.
    Al recibir el string esperado, se reemplazan los 3 asteriscos por "(*)"
*/
void esperar_confirmacion(String comandoEsperado, unsigned long tiempoDeEspera) {
  String lectSim = "";
  unsigned long millisPrevio = millis();
  ultimoComandoOk = false;

  while (true) {
    if (millis() - millisPrevio >= tiempoDeEspera)  {
      ultimoComandoOk = false;
      Serial.println("(Timeout) No se recibió confirmación del siguiente comando: "
                     + ultimoComandoEnviado);
      break;

    } else {
      lectSim = leer_sim_no_bloqueo();

      if (lectSim.length() > 0) {
        if (lectSim.startsWith(comandoEsperado)) {
          Serial.println("(*)   " + lectSim);
          ultimoComandoOk = true;
          break;

        } else if (lectSim.indexOf("ERROR") >= 0) {
          Serial.println("Hubo un error al procesar el siguiente comando: "
                         + ultimoComandoEnviado);
          break;

        } else  Serial.println("***   " + lectSim);
      }
    }
  }
}


/**
   Reiniciar Arduino provocando un "Soft WatchDog reset"
*/
void reset_arduino() {
  while (true);
}


/**
   Hacer Setup del módulo SIM. Consiste en enviar algunos comandos
   para leer errores, setear la red correctamente y verificar
   el estado general del módulo SIM.
*/
void setup_sim() {
  String comandos[] = {   // Comandos que se van a ejecutar:
    "AT",                 // Chequear comunicación entre SIM y Arduino
    "AT+CFUN=1",          // Modo de funcionalidad. 1 = ACTIVE, 0 = SLEEP
    "AT+CSQ",             // Calidad de señal. 10 a 30 ideal
    "AT+CREG?",           // Registro en la red
    "AT+COPS?",           // Selección de Operador
    "AT+CMGF=1",          // Formato de SMS. 1 = modo texto
    "AT+CNMI=2,1,0,0,0",  // Indicación para configurar SMS recibidos
    "AT+CMEE=2",          // Modo de mostrar los errores
  };

  // Forma de calcular la cantidad de elementos de un Array
  int cantComandos = sizeof(comandos) / sizeof(comandos[0]);

  Serial.println("Iniciando setup del módulo SIM...");
  digitalWrite(PIN_LED, LOW);
  esperar(tiempoEspera);

  for (int i = 0; i < cantComandos; i++) {
    ultimoComandoEnviado = comandos[i];
    SerialSim.println(ultimoComandoEnviado);
    esperar_confirmacion("OK", tiempoTimeout);
    Serial.println();
    esperar(tiempoEspera);
  }
  ultimoComandoEnviado = "Hacer Setup";
  Serial.println("Setup del módulo SIM finalizado.");
  ultimoComandoOk = true;
  digitalWrite(PIN_LED, HIGH);
}


/**
   Analizar un SMS recibido y verificar si tiene algún comando.
*/
void sms_recibido(String textoSms) {
  /**
      Ejemplo SMS recibido:
      +CMTI: "SM",20
  */
  Serial.println("¡Se ha recibido un nuevo SMS!");

  // Se busca la posición de memoria en la que está el nuevo SMS
  String posMem = textoSms.substring(textoSms.indexOf(',') + 1);

  String num = extraer_info_sms(posMem, "numero"),
         txt = extraer_info_sms(posMem, "texto"),
         todo = extraer_info_sms(posMem, "todo");

  Serial.println(todo); Serial.println();   // Se imprime el mensaje con toda la información

  // Se comprueba si el texto contiene algún comando
  if (txt.equalsIgnoreCase(cmdMarco)) {
    Serial.println("Respondiendo el comando...");
    enviar_sms(num, "Polo");

  } else if (txt == cmdSetup) {
    setup_sim();
    enviar_sms(num, "Setup iniciado exitosamente");

  } else if (txt == cmdLeerGps) {
    Serial.println("Leyendo el módulo GPS...");
    digitalWrite(PIN_LED, LOW);
    String txtEnviar = mostrar_info_gps();
    enviar_sms(num, txtEnviar);
    digitalWrite(PIN_LED, HIGH);

  } else if (txt == cmdReset) {
    enviar_sms(num, "Reiniciando Arduino");
    Serial.println("Reiniciando Arduino...");
    reset_arduino();

  } else if (txt == cmdBateria) {
    String txtEnviar = leer_bateria();
    enviar_sms(num, txtEnviar);

  } else if (txt == cmdLeerRecorrido) {
    Serial.println("Leyendo el camino recorrido...");
    String recorrido = camino_recorrido();
    Serial.println(recorrido);
    enviar_sms(num, recorrido);

  }

  esperar(tiempoEspera);
}


/**
    Obtener datos de un SMS alojado en la memoria del módulo SIM800L.
    Datos posibles a buscar: estado, numero, texto, fecha, hora, gmt, todo
*/
String extraer_info_sms(String posMem, String datoBuscado) {
  String datoEncontrado = "";

  /*********************************************************
    Comando de lectura de un SMS en la memoria:
      AT+CMGR=X    // X = numero de memoria a leer (1-30)

    Resultado:
      AT+CMGR=X
      +CMGR: "REC READ","03624164072","","20/10/16,23:34:07-12"  // -12 en cuartos de hora
      texto del mensaje
      OK
  *********************************************************/
  // Leer un SMS en la posición de memoria específica
  ultimoComandoEnviado = "AT+CMGR=" + posMem;
  SerialSim.println(ultimoComandoEnviado);

  SerialSim.readStringUntil('\n');                        // AT+CMGR=...
  String datosMsje = SerialSim.readStringUntil('\n');     // +CMGR: "REC... ó "OK"
  String texto = SerialSim.readStringUntil('\n');         // Texto del msje
  SerialSim.readStringUntil('\n');                        // Salto de linea
  String confirmacion = SerialSim.readStringUntil('\n');  // OK

  datosMsje.replace("\r", ""); datosMsje.replace("\"", "");  // Se quita el CR y las comillas
  String extraerDatos = datosMsje;
  texto.replace("\r", "");
  confirmacion.replace("\r", "");

  if (extraerDatos.startsWith("OK")) {
    datoEncontrado = "ERROR: No se encuentra un SMS en esa dirección de memoria.";

  } else {
    extraerDatos.remove(0, extraerDatos.indexOf(' ') + 1);

    String estado = extraerDatos.substring(0, extraerDatos.indexOf(','));       // REC READ
    extraerDatos.remove(0, extraerDatos.indexOf(',') + 1);                      // Quitar lo leído

    String numero = extraerDatos.substring(0, extraerDatos.indexOf(','));       // 03624164072
    extraerDatos.remove(0, extraerDatos.indexOf(',') + 2);

    String fecha = extraerDatos.substring(0, extraerDatos.indexOf('/'));        // 99
    extraerDatos.remove(0, extraerDatos.indexOf('/') + 1);

    fecha = extraerDatos.substring(0, extraerDatos.indexOf('/')) + '/' + fecha; // 12/99
    extraerDatos.remove(0, extraerDatos.indexOf('/') + 1);

    fecha = extraerDatos.substring(0, extraerDatos.indexOf(',')) + '/' + fecha; // 31/12/99
    extraerDatos.remove(0, extraerDatos.indexOf(',') + 1);

    String hora = extraerDatos.substring(0, 8);   // 00:12:17
    extraerDatos.remove(0, hora.length());

    String signo = extraerDatos.substring(0, 1);  // Signo de la zona horaria (+ o -)
    extraerDatos.remove(0, 1);

    String gmt = signo + String(extraerDatos.toInt() / 4);    // -3

    if (confirmacion == "OK")  {
      if (datoBuscado == "numero")  datoEncontrado = numero;
      else if (datoBuscado == "texto")  datoEncontrado = texto;
      else if (datoBuscado == "estado") datoEncontrado = estado;
      else if (datoBuscado == "hora") datoEncontrado = hora;
      else if (datoBuscado == "fecha")  datoEncontrado = fecha;
      else if (datoBuscado == "gmt")  datoEncontrado = gmt;
      else if (datoBuscado == "todo") {
        datoEncontrado = "Memoria: " + posMem + '\n'
                         + " Número: " + numero + '\n'
                         + "  Fecha: " + fecha + '\n'
                         + "   Hora: " + hora + "  (GMT" + gmt + ")" + '\n'
                         + "Mensaje:" + '\n'
                         + texto;

      } else datoEncontrado = "ERROR DATO BUSCADO";

    } else datoEncontrado = "ERROR LECTURA DE MEMORIA";
  }

  return datoEncontrado;
}


/**
    Leer el módulo GPS cada cierto tiempo, mientras haya información a leer.
    Si los cables están mal conectados, arroja un error por consola.
*/
void procesar_gps() {
  // Codificar el objeto GPS para traducir la información cruda del módulo GPS
  while (SerialGps.available() > 0) ObjectGps.encode(SerialGps.read());

  if (millis() - millisGps >= tiempoEspera) {
    millisGps = millis();
    leer_info_gps();
  }

  if (millis() - millisRecorrido >= (esperaRecorrido * 60000)) {
    millisRecorrido = millis();
    actualizar_recorrido();
  }

  // Detectar si está bien conectado el módulo GPS
  if (millis() - millisFalloGps >= 5000 && ObjectGps.charsProcessed() < 10) {
    millisFalloGps = millis();
    Serial.println("No se detectó el GPS: chequear cableado");
  }
}


/**
   Leer y formatear la información del módulo GPS.
   Esta información se guarda en la variables globales respectivas.
*/
void leer_info_gps() {
  satelitesGps = String(ObjectGps.satellites.value());
  latitudGps = String(ObjectGps.location.lat(), 6);
  longitudGps = String(ObjectGps.location.lng(), 6);

  bool datoUtil = (satelitesGps != "0");    // Sin conexión a satélites no hay datos para leer

  // Si son válidos los datos del GPS, ajustarlos a la zona horaria
  if (ObjectGps.time.isValid() && ObjectGps.date.isValid() && datoUtil) {
    setTime(
      ObjectGps.time.hour(),
      ObjectGps.time.minute(),
      ObjectGps.time.second(),
      ObjectGps.date.day(),
      ObjectGps.date.month(),
      ObjectGps.date.year()
    );
    adjustTime(TIMEZONE * 3600);     //Agrega (+) o quita (-) segundos para zona horaria

    // Leer y formatear los valores de tiempo y fecha actuales
    horaGps = String(hour()); if (horaGps.length() <= 1) horaGps = "0" + horaGps;
    minutosGps = String(minute()); if (minutosGps.length() <= 1) minutosGps = "0" + minutosGps;
    segundosGps = String(second()); if (segundosGps.length() <= 1) segundosGps = "0" + segundosGps;
    horaCompletaGps = horaGps + ":" + minutosGps + ":" + segundosGps;

    diaGps = String(day()); if (diaGps.length() <= 1) diaGps = "0" + diaGps;
    mesGps = String(month()); if (mesGps.length() <= 1) mesGps = "0" + mesGps;
    anioGps = String(year());
    fechaGps = diaGps + "/" + mesGps + "/" + anioGps;

    ultimaLecturaGpsOk = true;

  } else {
    horaGps = "N/A";
    minutosGps = "N/A";
    segundosGps = "N/A";
    horaCompletaGps = "N/A";

    diaGps = "N/A";
    mesGps = "N/A";
    anioGps = "N/A";
    fechaGps = "N/A";

    ultimaLecturaGpsOk = false;
  }
}


/**
   Organiza la información de las variables globales del GPS para que se lean mejor.
   Devuelve un String con toda esta información.
   Este String incluye, al final, un link de Google Maps con la ubicación obtenida.
*/
String mostrar_info_gps() {
  String resultadoGps = "";

  if (ultimaLecturaGpsOk) {
    resultadoGps = "Coord: " + latitudGps + ',' + longitudGps + '\n' +
                   "Fecha: " + fechaGps + '\n' +
                   "Hora: " + horaCompletaGps + '\n' +
                   "Sat: " + satelitesGps + '\n' + '\n' +
                   "https://www.google.com/maps/search/" +
                   latitudGps + ',' + longitudGps;

  } else resultadoGps = "El GPS no tiene datos utiles.";

  return resultadoGps;
}


/**
   Leer los miliVolts que llegan al módulo SIM.
   También estima la carga restante de batería.
*/
String leer_bateria() {
  SerialSim.println("AT+CBC");
  //SerialSim.readStringUntil('\n');    //Recibe "AT+CBC"
  String lecturaDatos = leer_sim_no_bloqueo();

  // Ejemplo de comando recibido:   +CBC: 0,98,4186
  while (!(lecturaDatos.indexOf("+CBC:") >= 0)) lecturaDatos = leer_sim_no_bloqueo();

  SerialSim.readStringUntil('\n');    //Recibe "OK"
  lecturaDatos.remove(0, lecturaDatos.indexOf(',') + 1);
  String porcentaje = lecturaDatos.substring(0, lecturaDatos.indexOf(','));

  lecturaDatos.remove(0, lecturaDatos.indexOf(',') + 1);
  String milivolts = lecturaDatos;

  lecturaDatos = "Carga: " + porcentaje + '%' + '\n'
                 + "Voltaje: " + milivolts + " mV";
  return lecturaDatos;
}


/**
   Actualizar el array que contiene las últimas posiciones guardadas.
*/
void actualizar_recorrido() {
  if (ultimaLecturaGpsOk) {
    for (int i = CANT_COORD - 1; i > 0; i--) {
      caminoRecorrido[i] = caminoRecorrido[i - 1];
    }
    caminoRecorrido[0] = latitudGps + ',' + longitudGps;
  }
}


/**
   Generar un link de Google Maps que muestre el
   recorrido hecho por el GPS luego de cierto tiempo.
*/
String camino_recorrido() {
  /*
    Ejemplo de link en Google Maps
    https://www.google.com/maps/dir/-27.44849,-58.9963085/-27.51111,-59.00000/data=!4m2!4m1!3e2

    data=!4m2!4m1!3e2     para ver los puntos como trayecto a pie
  */
  String resultado = "https://www.google.com/maps/dir/";

  for (int i = CANT_COORD - 1; i >= 0; i--) {
    resultado += caminoRecorrido[i] + '/';
  }
  resultado += "data=!4m2!4m1!3e2";

  return resultado;
}


/**
   Chequear si el módulo SIM está conectado a la red 2G.
   Devuelve TRUE si está conectado, y FALSE si no.
   Puede llegar a responder erróneamente si no está conectado, por lo que se agregó
   un control de timeout que escapa del bucle while si tarda mucho en responder.
*/
bool sim_esta_conectado() {
  String lecturaDatos = "";
  unsigned long millisPrevio = millis();
  SerialSim.println("AT+CREG?");

  while (millis() - millisPrevio <= tiempoTimeout) {
    lecturaDatos = leer_sim_no_bloqueo();
    if (lecturaDatos.startsWith("+CREG:")) break;
    else continue;
  }

  bool estado = lecturaDatos.endsWith("1");
  return estado;
}


/**
   Retornar el valor de potencia de conexión a la red que recibe el módulo SIM.
   Entre 10 y 30 son valores aceptables para que trabaje normalmente.
*/
String potencia_red_sim() {
  String lecturaDatos = "";
  unsigned long millisPrevio = millis();
  SerialSim.println("AT+CSQ");

  // Esperar respuesta. Ejemplo:  +CSQ: 15,0
  while (millis() - millisPrevio <= tiempoTimeout) {
    lecturaDatos = leer_sim_no_bloqueo();
    if (lecturaDatos.startsWith("+CSQ:")) break;
    else continue;
  }

  String potencia = lecturaDatos.substring(
                      lecturaDatos.indexOf(':') + 2,
                      lecturaDatos.indexOf(',')
                    );
  return potencia;
}


/**
   Setear al módulo SIM en modo SLEEP. En este modo no se reciben ni se envían SMS.
   Este modo consume menos batería que el modo ACTIVE.
*/
void dormir_sim() {
  String lecturaDatos = "";
  int intentos = 5, contador = 0;
  bool simEstaDormida = sim_dormida();
  Serial.println("Activando el modo SLEEP de la SIM800L...");

  if (simEstaDormida) Serial.println("El módulo SIM está en modo SLEEP.");
  else {
    while (contador < intentos) {
      SerialSim.println("AT+CFUN=0");
      esperar(tiempoEspera);
      simEstaDormida = sim_dormida();

      if (simEstaDormida) break;
      else contador++;
    }
    simEstaDormida = sim_dormida();
    if (simEstaDormida) Serial.println("Modo SLEEP activado exitosamente.");
    else Serial.println("No se pudo activar el modo SLEEP.");
  }
}


/**
   Setear al módulo SIM en modo ACTIVE. En este modo se puede enviar y recibir SMS.
   Este modo consume más batería que el modo SLEEP.
*/
void despertar_sim() {
  String lecturaDatos = "";
  int intentos = 5, contador = 0;
  bool simEstaDespierta = !sim_dormida();
  Serial.println("Activando la SIM800L...");

  if (simEstaDespierta) Serial.println("El módulo SIM está en modo ACTIVE.");
  else {
    while (contador < intentos) {
      SerialSim.println("AT+CFUN=1");
      esperar(tiempoEspera);
      simEstaDespierta = !sim_dormida();

      if (simEstaDespierta) break;
      else contador++;
    }

    simEstaDespierta = !sim_dormida();
    if (simEstaDespierta) Serial.println("Módulo SIM activado exitosamente.");
    else Serial.println("No se pudo activar el módulo SIM.");
  }
}


/**
   Retornar TRUE si el módulo SIM está en modo SLEEP, y FALSE si no.
   Esta función se utiliza para controlar el estado actual de conexión a la red.
*/
bool sim_dormida() {
  String lecturaDatos = "";
  unsigned long millisPrevio = millis();
  SerialSim.println("AT+CFUN?");

  while (millis() - millisPrevio <= tiempoTimeout) {
    lecturaDatos = leer_sim_no_bloqueo();
    if (lecturaDatos.startsWith("+CFUN:")) break;
    else continue;
  }

  bool estado = lecturaDatos.endsWith("0");
  return estado;
}
