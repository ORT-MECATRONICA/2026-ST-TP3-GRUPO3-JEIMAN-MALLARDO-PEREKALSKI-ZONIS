//Grupo 3: Marco Mallardo, Kenai Jeiman, Ramiro Perekalski, Martin Zonis
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <U8g2lib.h>

#define DHTPIN 23
#define DHTTYPE DHT11
#define LEDPIN 25
#define SW1 35
#define SW2 34
#define BOTtoken "8657893650:AAFE4LEFG1kVS3-XOd7zaRadOI3auWASIjM"
#define CHAT_ID "1487922548"

TaskHandle_t Task1;
TaskHandle_t Task2;

DHT dht(DHTPIN, DHTTYPE);
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

typedef enum
{
  RST,
  P1,
  P1AP2,
  P2,
  P2AP1
} estados_t;
estados_t maquinaPantalla;

const char *ssid = "MECA-IoT-V2";
const char *password = "IoT$2026";

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

float temperatura = 0;
float valorUmbral = 26;

void setup()
{
  Serial.begin(115200);
  pinMode(SW1, INPUT);
  pinMode(SW2, INPUT);
  pinMode(LEDPIN, OUTPUT);
  digitalWrite(LEDPIN, LOW);
  dht.begin();
  u8g2.begin();

  WiFi.begin(ssid, password);
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  bot.sendMessage(CHAT_ID, "Bot Iniciado", "");

  xTaskCreatePinnedToCore(
      Task1code, /* Task function. */
      "Task1",   /* name of task. */
      10000,     /* Stack size of task */
      NULL,      /* parameter of the task */
      1,         /* priority of the task */
      &Task1,    /* Task handle to keep track of created task */
      0);        /* pin task to core 0 */

  xTaskCreatePinnedToCore(
      Task2code, /* Task function. */
      "Task2",   /* name of task. */
      10000,     /* Stack size of task */
      NULL,      /* parameter of the task */
      1,         /* priority of the task */
      &Task2,    /* Task handle to keep track of created task */
      1);        /* pin task to core 1 */
}

void Task1code(void *pvParameters)
{
  bool alertaEnviada = false;
  long int millisUltimoCheck = millis();

  for (;;)
  {
    if (millis() - millisUltimoCheck >= 5000)
    { // Leer el sensor cada 5 segundos
      float temperaturaTest = dht.readTemperature();
      if (isnan(temperaturaTest))
      {
        Serial.println("Lectura DHT fallida");
      }
      else
      {
        temperatura = temperaturaTest;
      }

      if (temperatura > valorUmbral)
      {
        digitalWrite(LEDPIN, HIGH);
        if (!alertaEnviada)
        { // Si es la primera vez que lo supera
          bot.sendMessage(CHAT_ID, "¡Alerta! La temperatura ha superado el umbral: " + String(temperatura) + " °C", "");
          alertaEnviada = true;
        }
      }
      else
      {
        digitalWrite(LEDPIN, LOW);
        alertaEnviada = false;
      }
      millisUltimoCheck = millis();
    }

    // Revisar si hay mensajes nuevos en Telegram
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages)
    {
      for (int i = 0; i < numNewMessages; i++)
      {
        String chat_id = String(bot.messages[i].chat_id);
        String text = bot.messages[i].text;

        // Solo responder si el mensaje viene de Marco (SIN "s")
        if (chat_id == CHAT_ID)
        {

          // Si Marco (SIN "s") envía el comando /temperatura
          if (text == "/temperatura")
          {
            bot.sendMessage(chat_id, "La temperatura actual es: " + String(temperatura) + " °C", "");
          }
          // Si envía /start
          else if (text == "/start")
          {
            bot.sendMessage(chat_id, "¡Hola! Envíame /temperatura para consultar el sensor.", "");
          }
        }
      }
      
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void Task2code(void *pvParameters)
{
  maquinaPantalla = RST;
  bool sw1_flag = false;
  bool sw2_flag = false;

  // Variables para la secuencia del código
  int etapa_secuencia = 0;
  unsigned long tiempo_inicio_codigo = 0;

  for (;;)
  {
    Serial.println(etapa_secuencia);

    switch (maquinaPantalla)
    {
    case RST:
      maquinaPantalla = P1;

    case P1:
      sw1_flag = !digitalRead(SW1);

      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_ncenB08_tr);
      u8g2.setCursor(0, 15);
      u8g2.print("Temp: ");
      u8g2.print(temperatura);
      u8g2.setCursor(0, 35);
      u8g2.print("VU: ");
      u8g2.print(valorUmbral);
      u8g2.sendBuffer();

      if (sw1_flag)
      {
        sw1_flag = false;                // Reseteamos la bandera para evitar múltiples detecciones
        etapa_secuencia = 1;             // Primer paso completado (Presionar SW1)
        tiempo_inicio_codigo = millis(); // Guardamos el tiempo de inicio
        maquinaPantalla = P1AP2;
      }

      break;

    case P1AP2:
      if (!digitalRead(SW1)) sw1_flag = true;
      if (!digitalRead(SW2)) sw2_flag = true;

      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_ncenB08_tr);
      u8g2.setCursor(0, 15);
      u8g2.print("Temp: ");
      u8g2.print(temperatura);
      u8g2.setCursor(0, 35);
      u8g2.print("VU: ");
      u8g2.print(valorUmbral);
      u8g2.sendBuffer();

      if (millis() - tiempo_inicio_codigo > 5000)
      {
        maquinaPantalla = P1; // Tiempo agotado, vuelve al inicio de P1
        etapa_secuencia = 0;
        sw1_flag = false;
        sw2_flag = false;
      }
      else
      {
        // Secuencia
        if (etapa_secuencia == 1 && digitalRead(SW1))
        {
          etapa_secuencia = 2; // SW1 se soltó, avanzar.
          sw1_flag = false;
        }
        else if (etapa_secuencia == 2 && sw2_flag)
        {
          etapa_secuencia = 3; // SW2 se presionó
        }
        else if (etapa_secuencia == 3 && digitalRead(SW2))
        {
          etapa_secuencia = 4; // SW2 se soltó
          sw2_flag = false;
        }
        else if (etapa_secuencia == 4 && sw1_flag)
        {
          etapa_secuencia = 5; // SW1 se presionó
        }
        else if (etapa_secuencia == 5 && digitalRead(SW1))
        {
          maquinaPantalla = P2;
          etapa_secuencia = 0;
          sw1_flag = false;
          sw2_flag = false;
        }

        // Si se comete alguno de estos errores, la secuencia se reinicia
        else if ((etapa_secuencia == 1 && sw2_flag) ||
                 (etapa_secuencia == 2 && sw1_flag) ||
                 (etapa_secuencia == 3 && sw1_flag) ||
                 (etapa_secuencia == 4 && sw2_flag) ||
                 (etapa_secuencia == 5 && sw2_flag))
        {
          maquinaPantalla = P1;
          etapa_secuencia = 0;
          sw1_flag = false;
          sw2_flag = false;
        }
      }
      break;

    case P2:
      if (!digitalRead(SW1))
        sw1_flag = true;
      if (!digitalRead(SW2))
        sw2_flag = true;

      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_ncenB08_tr);
      u8g2.setCursor(0, 15);
      u8g2.print("Seteando VU:");
      u8g2.setCursor(0, 35);
      u8g2.print("VU = ");
      u8g2.print(valorUmbral);
      u8g2.sendBuffer();

      // Aumentar o disminuir verificando que el otro flag no esté activado
      if (sw1_flag && digitalRead(SW1) && !sw2_flag)
      {
        sw1_flag = false;
        valorUmbral++;
      }
      if (sw2_flag && digitalRead(SW2) && !sw1_flag)
      {
        sw2_flag = false;
        valorUmbral--;
      }

      
      if (sw1_flag && sw2_flag)
      {
        sw1_flag = false; 
        sw2_flag = false;
        maquinaPantalla = P1;
      }
      break;
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void loop()
{
}