// OTP 
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <NTPClient.h>
#include <TOTP.h>
#include <ESP_Mail_Client.h>
#include <PubSubClient.h>

// Wifi network station credentials
#define WIFI_SSID "" // YOUR WIFI SSID NAME
#define WIFI_PASSWORD "" // YOUR WIFI PASSWORD

// Telegram BOT Token (Get from Botfather)
#define BOT_TOKEN "" // YOUR BOT TOKEN 

const char* mqtt_server = "mqtt-dashboard.com";

WiFiClient espClient;
PubSubClient clienty (espClient);

const unsigned long BOT_MTBS = 1000; // mean time between scan messages

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime; // last time messages' scan has been done

uint8_t hmacKey[] = {0x73, 0x65, 0x63, 0x72, 0x65, 0x74, 0x6b, 0x65, 0x79, 0x30};

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
TOTP totp = TOTP(hmacKey, 10);
bool signupOK = false;

#define SMTP_HOST "smtp.gmail.com"

#define SMTP_PORT esp_mail_smtp_port_587 // port 465 is not available for Outlook.com

/* The log in credentials */
#define AUTHOR_EMAIL "" // YOUR EMAIL ADDRESS WILL BE USED TO SEND THE OTP CODE
#define AUTHOR_PASSWORD "zusvjnhyksioigpn" //YOUR AUTHOR PASSWORD OF ACCOUNT GOOGLE, THIS PASSWORD IS DIFFERENT FROM YOUR GOOGLE ACCOUNT PASSWORD. THIS PASSWORD IS OBTAINED WHEN YOU CREATE APP PASSWORD(https://support.google.com/mail/answer/185833?hl=en) 

#define RECIPIENT_EMAIL "" // YOUR EMAIL ADDRESS WILL BE USED TO RECEIVE THE OTP CODE
String newCode ;
SMTPSession smtp;

void handleNewMessages(int numNewMessages)
{
  for (int i = 0; i < numNewMessages; i++)
  {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;
    String from_name = bot.messages[i].from_name;

    if (text == "/start") {
      sendemail();
    }
      if (text == newCode) {
        bot.sendMessage(chat_id, "Hello..." + from_name, "");
      }
      if (text != newCode) {
        bot.sendMessage(chat_id, "Masukan Dengan Benar yaa...", "");
      }
    
  }
}

void sendemail() {
  newCode = String(totp.getCode(timeClient.getEpochTime()));

  MailClient.networkReconnect(true);

  smtp.debug(1);

  Session_Config configor;

  /* Set the session config */
  configor.server.host_name = SMTP_HOST;
  configor.server.port = SMTP_PORT;
  configor.login.email = AUTHOR_EMAIL;
  configor.login.password = AUTHOR_PASSWORD;
  configor.login.user_domain = F("127.0.0.1");
  configor.time.ntp_server = F("pool.ntp.org,time.nist.gov");
  configor.time.gmt_offset = 3;
  configor.time.day_light_offset = 0;

  SMTP_Message message;

  /* Set the message headers */
  message.sender.name = F("ESP32");
  message.sender.email = AUTHOR_EMAIL;

  message.subject = F("KODE OTP");
  message.addRecipient(F("Someone"), RECIPIENT_EMAIL);

  char code[newCode.length() + 1];
  newCode.toCharArray(code, sizeof(code));
  String textMsg = newCode;
  message.text.content = textMsg;
  message.text.charSet = F("us-ascii");

  message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

  message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_low;

  message.addHeader(F("Message-ID: <abcde.fghij@gmail.com>"));

  if (!smtp.connect(&configor))
  {
    MailClient.printf("Connection error, Status Code: %d, Error Code: %d, Reason: %s\n", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
    return;
  }
  if (!smtp.isLoggedIn())
  {
    Serial.println("Not yet logged in.");
  }
  else
  {
    if (smtp.isAuthenticated()) {
      Serial.println("Successfully logged in.");
    } else {
      Serial.println("Connected with no Auth.");
    }
  }

  /* Start sending Email and close the session */
  if (!MailClient.sendMail(&smtp, &message)) {
    MailClient.printf("Error, Status Code: %d, Error Code: %d, Reason: %s\n", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
  }
  clienty.publish("bed/OTP", code);
  Serial.println(newCode);
}

void callback (String topic, byte* message, unsigned int length) {
  Serial.print("Message Arrived on topic : ");
  Serial.print(topic);
  Serial.print(". Message");
  String messageTemp;

  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
}

void reconnect() {
  while (!clienty.connected()) {
    Serial.println("Attempting MQTT connection...");

    if (clienty.connect("bed")) {
      Serial.println("Connected");
      clienty.subscribe("bed/text");
      clienty.subscribe("bed/OTP");
      clienty.subscribe("bed/waktu");
    } else {
      Serial.println("failed");
    }
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println();

  // attempt to connect to Wifi network:
  Serial.print("Connecting to Wifi SSID ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());
  timeClient.update();
  Serial.print("Retrieving time: ");
  configTime(0, 0, "pool.ntp.org"); // get UTC time via NTP
  time_t now = time(nullptr);
  while (now < 24 * 3600)
  {
    Serial.print(".");
    delay(100);
    now = time(nullptr);
  }
  Serial.println(now);

  timeClient.begin();

  clienty.setServer(mqtt_server, 1883);
  clienty.setCallback(callback);
}

void loop()
{
  if (millis() - bot_lasttime > BOT_MTBS)
  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages)
    {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    bot_lasttime = millis();
  }
  if (!clienty.connected()) {
    reconnect();
  }
  clienty.loop();
}
