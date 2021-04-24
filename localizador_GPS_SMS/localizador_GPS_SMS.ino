#include <TimeLib.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>

const int PIN_LED =  LED_BUILTIN,         // Pin del Led integrado
          PIN_SIM_RX = 4,                 // Pin de NodeMCU que recibe datos del módulo SIM
          PIN_SIM_TX = 5,                 // Pin de NodeMCU que envía datos al módulo SIM
          PIN_GPS_RX = 14,                // Pin de NodeMCU que recibe datos del módulo GPS
          PIN_GPS_TX = 12;                // Pin de NodeMCU que envía datos al módulo GPS

// Variables globales
String ultimoComandoEnviado = "",         // Se usa para mostrar errores

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

       // Variables para guardar datos GPS
       fechaGps = "",
       diaGps = "",
       mesGps = "",
       anioGps = "",
       horaGps = "",
       horaCompletaGps = "",
       minutosGps = "",
       segundosGps = "",
       longitudGps = "",
       latitudGps = "",
       satelitesGps = "";

unsigned int tiempoEspera = 500,    // Utilizada mayormente en la función esperar()
             tiempoTimeout = 5000;  // Para esperar confirmación de comandos y otros


unsigned long millisGps = 0,        // Variables de control de intervalos de tiempo
              millisFalloGps = 0,
              millisLed = 0;

int husoHorarioGmt = -3,    // Huso horario según ubicación requerida (GMT -3 p/ Argentina)
    estadoLed = HIGH;       // Estado del LED. Para NodeMCU, HIGH es apagado y LOW es encendido


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
  Serial.begin(9600);
  SerialSim.begin(9600);
  SerialGps.begin(9600);

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  esperar(2 * tiempoEspera);

  Serial.println();
  Serial.println("Iniciando...");  Serial.println();

  // Hacer Setup del SIM
  setupSim(); Serial.println();

  Serial.println("Entrando en Loop...");
  digitalWrite(PIN_LED, HIGH);   // El Led se queda prendido durante el Setup
}


void loop() {
  procesar_sim();
  procesar_usb();
  procesar_gps();
  ejecutar_regularmente(cambiar_led, 250, &millisLed);   // El Led titila cada 250ms en Loop
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
   Ejecutar un comando recibido desde el teclado de la PC
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
    varSimNoBloq = quitar_char_en_string(varSimNoBloq, '\r');
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
    varUsbNoBloq = quitar_char_en_string(varUsbNoBloq, '\r');
  }
  return varUsbNoBloq;
}


/**
   Quita del string pasado en el primer argumento,
   el caracter enviado en el segundo argumento, todas las eces que aparezca.
*/
String quitar_char_en_string(String txtCorregir, char caracter) {
  for (int i = 0; i < txtCorregir.length(); i++) {
    if (txtCorregir.charAt(i) == caracter) {
      txtCorregir.remove(i);
      i--;
    }
  }
  return txtCorregir;
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

  } else if (comando == cmdSetup) setupSim(); // Hacer Setup del SIM800L

  else if (comando == cmdLeerMemoriaSms) {    // Acceder a un SMS guardado en la memoria
    digitalWrite(PIN_LED, LOW);
    Serial.println("Seleccione el número de memoria a buscar (nº, o \"todo\")");
    String numMemElegido = leer_usb_bloqueo();
    if (numMemElegido.equalsIgnoreCase("todo")) {
      SerialSim.println("AT+CMGL=\"ALL\"");   // Comando AT para leer todos los SMS en memoria
      ultimoComandoEnviado = "Leer todos los SMS";
      esperar_confirmacion("OK", 10000);       // Se imprime todo por consola durante 10 segundos
    } else {
      Serial.print("Seleccione el dato a leer ");
      Serial.println("(estado, numero, texto, fecha, hora, gmt, todo)");
      String opcionElegida = leer_usb_bloqueo();
      Serial.println("Leyendo " + opcionElegida
                     + " en la memoria " + numMemElegido + ":");
      ultimoComandoEnviado = "Leer " + opcionElegida
                             + " en la memoria " + numMemElegido;

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

  // Se espera hasta 1 minuto la confirm. del envío del SMS
  esperar_confirmacion("+CMGS:", 60000);
  unsigned long finCronometro = millis();

  if (ultimoComandoOk)  {
    Serial.println("MENSAJE ENVIADO CORRECTAMENTE.");
    Serial.println("Tiempo tardado: "
                   + String((finCronometro - iniCronometro) / 1000)
                   + " segundos");

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
*/
void esperar_confirmacion(String comandoEsperado, unsigned long tiempoDeEspera) {
  String lectSim = "";
  unsigned long millisPrevio = millis();
  ultimoComandoOk = false;

  while (true) {
    if (millis() - millisPrevio >= tiempoDeEspera)  {
      ultimoComandoOk = false;
      Serial.println("(Timeout) No se recibió confirmación del siguiente comando: "
                     + ultimoComandoEnviado + '\n');
      break;

    } else {
      lectSim = leer_sim_no_bloqueo();

      if (lectSim.length() > 0) {
        if (lectSim.startsWith(comandoEsperado)) {
          Serial.println("(*)   " + lectSim + '\n');
          ultimoComandoOk = true;
          break;

        } else if (lectSim.indexOf("ERROR") >= 0) {
          Serial.print("Hubo un error al procesar el siguiente comando: "
                       + ultimoComandoEnviado + '\n');
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
void setupSim() {
  String comandos[] = {   // Comandos que se van a ejecutar
    "AT",                 // Chequear comunicación entre SIM y Arduino
    "AT+CFUN=1",          // Modo de funcionalidad
    "AT+CSQ",             // Calidad de señal. 10 a 30 ideal
    "AT+CREG?",           // Registro en la red
    "AT+COPS?",           // Selección de Operador
    "AT+CMGF=1",          // Formato de SMS. 1 = modo texto
    "AT+CNMI=2,1,0,0,0",  // Indicación para nuevo SMS
    "AT+CMEE=2",          // Modo de mostrar los errores
  };
  int cantComandos = sizeof(comandos) / sizeof(comandos[0]);

  Serial.println("Iniciando setup del módulo SIM...");
  digitalWrite(PIN_LED, LOW);
  esperar(tiempoEspera);

  for (int i = 0; i < cantComandos; i++) {
    ultimoComandoEnviado = comandos[i];
    SerialSim.println(ultimoComandoEnviado);
    esperar_confirmacion("OK", tiempoTimeout);
    esperar(tiempoEspera);
  }

  ultimoComandoEnviado = "Hacer Setup";
  Serial.println("Setup del módulo SIM finalizado." + '\n');
  ultimoComandoOk = true;
  digitalWrite(PIN_LED, HIGH);
}


/**

*/
void sms_recibido(String textoSms) {
  /**
      Ejemplo SMS recibido:
      +CMTI: "SM",20
  */
  Serial.println("¡Se ha recibido un nuevo SMS!");

  // Se busca la posición de memoria en la que está el nuevo SMS
  String posMem = "";
  for (int i = 1; isDigit(textoSms.charAt(textoSms.length() - i)); i++) {
    posMem = String(textoSms.charAt(textoSms.length() - i))
             + posMem;
  }

  String num = extraer_info_sms(posMem, "numero"),
         txt = extraer_info_sms(posMem, "texto"),
         fch = extraer_info_sms(posMem, "fecha"),
         hrs = extraer_info_sms(posMem, "hora"),
         gmt = extraer_info_sms(posMem, "gmt"),
         todo = extraer_info_sms(posMem, "todo");

  Serial.println(todo + '\n');   // Se imprime el mensaje con toda la información

  // Se comprueba si el texto contiene algún comando
  if (txt.equalsIgnoreCase(cmdMarco)) {
    Serial.println("Respondiendo el comando...");
    enviar_sms(num, "Polo");

  } else if (txt == cmdSetup) {
    setupSim();
    enviar_sms(num, "Setup iniciado exitosamente");

  } else if (txt == cmdLeerGps) {
    Serial.println("Leyendo el módulo GPS...");
    digitalWrite(PIN_LED, LOW);
    String txtEnviar = mostrar_info_gps();
    enviar_sms(num, txtEnviar);
    digitalWrite(PIN_LED, HIGH);

  } else if (txt == cmdReset) {
    Serial.println("Reiniciando Arduino...");
    enviar_sms(num, "Reiniciando Arduino");
    reset_arduino();

  } else if (txt == cmdBateria) {
    String txtEnviar = leer_bateria();
    enviar_sms(num, txtEnviar);
  }

  esperar(tiempoEspera);
}
/***********************************************************************************
***********************************************************************************/
String extraer_info_sms(String posMem, String datoBuscado) {
  String anio = "",
         confirmSms = "",
         datoEncontrado = "",
         dia = "",
         digitos = "",
         estado = "",
         fecha = "",
         gmt = "",
         hora = "",
         mes = "",
         numero = "",
         signo = "",
         texto = "";

  /*********************************************************
    Comando de lectura de un SMS en la memoria:
      AT+CMGR=X    // X = numero de memoria a leer (1-30)

    Resultado:
      +CMGR: "REC READ","03624164072","","20/10/16,23:34:07-12"
      texto del mensaje
      OK
  *********************************************************/
  // Leer un SMS en la posición de memoria específica
  ultimoComandoEnviado = "AT+CMGR=" + posMem;
  SerialSim.println(ultimoComandoEnviado);

  String primeraLinea = SerialSim.readStringUntil('\n');  //recibe AT+CMGR=...
  String segundaLinea = SerialSim.readStringUntil('\n');
  String terceraLinea = SerialSim.readStringUntil('\n');  //recibe texto del msje
  SerialSim.readStringUntil('\n');                        //recibe salto de linea
  String cuartaLinea = SerialSim.readStringUntil('\n');   //recibe OK

  primeraLinea = quitar_char_en_string(primeraLinea, '\r');
  segundaLinea = quitar_char_en_string(segundaLinea, '\r');
  terceraLinea = quitar_char_en_string(terceraLinea, '\r');
  cuartaLinea = quitar_char_en_string(cuartaLinea, '\r');

  if (segundaLinea.startsWith("OK")) {
    datoEncontrado = "No se encuentra un SMS en esa dirección de memoria.";

  } else {
    segundaLinea.remove(0, 7);

    estado = segundaLinea.substring(
               segundaLinea.indexOf('"'),
               segundaLinea.indexOf(',')
             );
    numero = segundaLinea.substring(
               segundaLinea.indexOf("\",\"") + 3,
               segundaLinea.indexOf("\",\"\",\"")
             );
    anio = segundaLinea.substring(
             segundaLinea.indexOf('/') - 2,
             segundaLinea.indexOf('/')
           );
    mes = segundaLinea.substring(
            segundaLinea.indexOf('/') + 1,
            segundaLinea.indexOf('/') + 3
          );
    dia = segundaLinea.substring(
            segundaLinea.indexOf('/') + 4,
            segundaLinea.indexOf('/') + 6
          );
    hora = segundaLinea.substring(
             segundaLinea.indexOf(':') - 2,
             segundaLinea.indexOf(':') + 6
           );
    signo = segundaLinea.substring(
              segundaLinea.indexOf(':') + 6,
              segundaLinea.indexOf(':') + 7
            );
    digitos = segundaLinea.substring(
                segundaLinea.indexOf(':') + 7,
                segundaLinea.indexOf(':') + 9
              );
    gmt = signo + String(digitos.toInt() / 4);

    fecha = dia + '/' + mes + '/' + anio;
    texto = terceraLinea;
    confirmSms = cuartaLinea;

    estado = quitar_char_en_string(estado, '\r');
    numero = quitar_char_en_string(numero, '\r');
    fecha = quitar_char_en_string(fecha, '\r');
    texto = quitar_char_en_string(texto, '\r');
    gmt = quitar_char_en_string(gmt, '\r');
    confirmSms = quitar_char_en_string(confirmSms, '\r');

    if (confirmSms == "OK")  {
      if (datoBuscado == "numero")  datoEncontrado = numero;
      else if (datoBuscado == "texto")  datoEncontrado = texto;
      else if (datoBuscado == "estado") datoEncontrado = estado;
      else if (datoBuscado == "hora") datoEncontrado = hora;
      else if (datoBuscado == "fecha")  datoEncontrado = fecha;
      else if (datoBuscado == "gmt")  datoEncontrado = gmt;
      else if (datoBuscado == "todo") {
        datoEncontrado = " Número: " + numero + '\n'
                         + "  Fecha: " + fecha + '\n'
                         + "   Hora: " + hora + "  (GMT" + gmt + ")" + '\n'
                         + "Mensaje:" + '\n'
                         + texto;

      } else datoEncontrado = "ERROR DATO BUSCADO";

    } else datoEncontrado = "ERROR LECTURA DE MEMORIA";
  }

  return datoEncontrado;
}
/***********************************************************************************
************************************************************************************
************************************************************************************
***********************************************************************************/
void procesar_gps() {
  // This sketch displays information every time a new sentence is correctly encoded.
  while (SerialGps.available() > 0)
    ObjectGps.encode(SerialGps.read());

  if (millis() - millisGps >= tiempoEspera) {
    millisGps = millis();
    LeerInfoGPS();
  }

  if (millis() - millisFalloGps >= 5000 && ObjectGps.charsProcessed() < 10) {
    millisFalloGps = millis();
    Serial.println("No se detectó el GPS: chequear cableado");
  }
}
/***********************************************************************************
***********************************************************************************/
void LeerInfoGPS() {
  satelitesGps = String(ObjectGps.satellites.value());
  latitudGps = String(ObjectGps.location.lat(), 6);
  longitudGps = String(ObjectGps.location.lng(), 6);

  bool datoUtil = satelitesGps != "0";

  // Poner en hora
  if (ObjectGps.time.isValid() && ObjectGps.date.isValid() && datoUtil) {
    setTime(
      ObjectGps.time.hour(),
      ObjectGps.time.minute(),
      ObjectGps.time.second(),
      ObjectGps.date.day(),
      ObjectGps.date.month(),
      ObjectGps.date.year()
    );
    adjustTime(husoHorarioGmt * 3600);     //Agrega (+) o quita (-) segundos para zona horaria

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
/***********************************************************************************
***********************************************************************************/
String mostrar_info_gps() {
  String resultadoGps = "";

  if (ultimaLecturaGpsOk) {
    resultadoGps = "Coord:  " + latitudGps + ',' + longitudGps + '\n'
                   + "Fecha:  " + fechaGps + '\n'
                   + " Hora:  " + horaCompletaGps + '\n'
                   + "  Sat:  " + satelitesGps + '\n' + '\n'
                   + "https://www.google.com/maps/search/"
                   + latitudGps + ',' + longitudGps;
  }
  else resultadoGps = "El GPS no tiene datos utiles.";

  /**
      Ejemplo de link en Google Maps
      https://www.google.com/maps/search/-27.44849,-58.9963085

      data=!4m2!4m1!3e2     para asignar varis puntos y verlos como trayecto a pie
  */
  return resultadoGps;
}
/***********************************************************************************
************************************************************************************
************************************************************************************
***********************************************************************************/
void ejecutar_regularmente(void (*funcion)(), unsigned long intervalo, unsigned long *refPrevia) {
  if (millis() - *refPrevia >= intervalo) {
    *refPrevia = millis();
    (*funcion)();
  }
}
/***********************************************************************************
***********************************************************************************/
void cambiar_led() {
  estadoLed = !estadoLed;
  digitalWrite(PIN_LED, estadoLed);
}
/***********************************************************************************
***********************************************************************************/
String leer_bateria() {
  SerialSim.println("AT+CBC");
  //SerialSim.readStringUntil('\n');    //Recibe "AT+CBC"
  String lecturaDatos = leer_sim_no_bloqueo();

  // Ejemplo de comando recibido:   +CBC: 0,98,4186
  while (!(lecturaDatos.indexOf("+CBC:") >= 0)) {
    lecturaDatos = leer_sim_no_bloqueo();
  }
  SerialSim.readStringUntil('\n');    //Recibe "OK"
  String porcentaje = lecturaDatos.substring(lecturaDatos.indexOf(',') + 1,
                      lecturaDatos.length() - 5);
  String milivolts = "";

  for (int i = 1; isDigit(lecturaDatos.charAt(lecturaDatos.length() - i)); i++) {
    milivolts = String(lecturaDatos.charAt(lecturaDatos.length() - i))
                + milivolts;
  }

  lecturaDatos = "Carga: " + porcentaje + '%' + '\n'
                 + "Voltaje: " + milivolts + " mV";
  return lecturaDatos;
}
/***********************************************************************************
***********************************************************************************/
String camino_recorrido() {
  /* Ejemplo de link en Google Maps
    https://www.google.com/maps/dir/-27.44849,-58.9963085/-27.51111,-59.00000/data=!4m2!4m1!3e2

    data=!4m2!4m1!3e2     para ver los puntos como trayecto a pie
  */


}
/***********************************************************************************
***********************************************************************************/
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
/***********************************************************************************
***********************************************************************************/
String potencia_red_sim() {
  String lecturaDatos = "";
  unsigned long millisPrevio = millis();
  SerialSim.println("AT+CSQ");

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
/***********************************************************************************
***********************************************************************************/
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
/***********************************************************************************
***********************************************************************************/
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
/***********************************************************************************
***********************************************************************************/
bool sim_dormida() {   // Devuelve TRUE si el módulo SIM está en CFUN = 0 (sleep)
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
