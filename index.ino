/*
 * Checks temperature through a DHT sensor
 * and sends it to a Telegram bot who shows
 * two options: Turn AC on or off, the code
 * checks for updates from Telegram every X
 * seconds and answers the callback query based
 * on what the user clicked, the AC is simulated
 * with a RGB LED turning blue.
 * Author: Marcos; Higor Librelato
 */

#include <DHT.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <set>

#define DHTPIN 2
#define DHTTYPE DHT11
#define LED_PIN 5

int previousId = 0;
std::set<String> callbackIds;

/* 
 * Replace with the 
 * WiFi settings
 */
const char* ssid = "";
const char* password = "";

/* 
 * Replace with your CHAT_ID
 * and API_TOKEN
 */
const char* CHAT_ID = "";
const char* API_TOKEN = "";
const char* server = "api.telegram.org";

DHT dht(DHTPIN, DHTTYPE);

/*
   The root certificate will be valid for years:
   *.telegram.org ->
     Go Daddy Secure Certificate Authority - G2 ->
       Go Daddy Root Certificate Authority - G2
*/

// Go Daddy Root Certificate Authority - G2
const char TELEGRAM_CERTIFICATE_ROOT[] = R"=EOF=(
-----BEGIN CERTIFICATE-----
MIIDxTCCAq2gAwIBAgIBADANBgkqhkiG9w0BAQsFADCBgzELMAkGA1UEBhMCVVMx
EDAOBgNVBAgTB0FyaXpvbmExEzARBgNVBAcTClNjb3R0c2RhbGUxGjAYBgNVBAoT
EUdvRGFkZHkuY29tLCBJbmMuMTEwLwYDVQQDEyhHbyBEYWRkeSBSb290IENlcnRp
ZmljYXRlIEF1dGhvcml0eSAtIEcyMB4XDTA5MDkwMTAwMDAwMFoXDTM3MTIzMTIz
NTk1OVowgYMxCzAJBgNVBAYTAlVTMRAwDgYDVQQIEwdBcml6b25hMRMwEQYDVQQH
EwpTY290dHNkYWxlMRowGAYDVQQKExFHb0RhZGR5LmNvbSwgSW5jLjExMC8GA1UE
AxMoR28gRGFkZHkgUm9vdCBDZXJ0aWZpY2F0ZSBBdXRob3JpdHkgLSBHMjCCASIw
DQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAL9xYgjx+lk09xvJGKP3gElY6SKD
E6bFIEMBO4Tx5oVJnyfq9oQbTqC023CYxzIBsQU+B07u9PpPL1kwIuerGVZr4oAH
/PMWdYA5UXvl+TW2dE6pjYIT5LY/qQOD+qK+ihVqf94Lw7YZFAXK6sOoBJQ7Rnwy
DfMAZiLIjWltNowRGLfTshxgtDj6AozO091GB94KPutdfMh8+7ArU6SSYmlRJQVh
GkSBjCypQ5Yj36w6gZoOKcUcqeldHraenjAKOc7xiID7S13MMuyFYkMlNAJWJwGR
tDtwKj9useiciAF9n9T521NtYJ2/LOdYq7hfRvzOxBsDPAnrSTFcaUaz4EcCAwEA
AaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAQYwHQYDVR0OBBYE
FDqahQcQZyi27/a9BUFuIMGU2g/eMA0GCSqGSIb3DQEBCwUAA4IBAQCZ21151fmX
WWcDYfF+OwYxdS2hII5PZYe096acvNjpL9DbWu7PdIxztDhC2gV7+AJ1uP2lsdeu
9tfeE8tTEH6KRtGX+rcuKxGrkLAngPnon1rpN5+r5N9ss4UXnT3ZJE95kTXWXwTr
gIOrmgIttRD02JDHBHNA7XIloKmf7J6raBKZV8aPEjoJpL1E/QYVN8Gb5DKj7Tjo
2GTzLH4U/ALqn83/B2gX2yKQOC16jdFU8WnjXzPKej17CuPKf1855eJ1usV2GDPO
LPAvTK33sefOT6jEm0pUBsV/fdUID+Ic/n4XuKxe9tQWskMJDE32p2u0mYRlynqI
4uJEvlz36hz1
-----END CERTIFICATE-----
)=EOF=";

String generateMessageJSON(float temperature);
String generateEditMessageJSON(int id);
void getUpdates();
void answerCallbackQuery(String query_id, String message);
void sendMessage(float temperature);
void editPreviousMessage(int messageId);
void handleLongPolling(void *parameter);
void checkTemperature(void *parameter);
bool isIdProcessed(const String& id);
void setColor(int red, int green, int blue);

void setup() {
  Serial.begin(115200);
  dht.begin();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  pinMode(LED_PIN, OUTPUT);

  xTaskCreatePinnedToCore(
    checkTemperature,    // Task function
    "CheckTemperature",  // Task name
    4096,                // Stack size (bytes)
    NULL,                // Task parameters
    1,                   // Priority (0 or 1)
    NULL,                // Task handle
    1                    // Core to run the task (0 or 1)
  );

  xTaskCreatePinnedToCore(
    handleLongPolling,   // Task function
    "LongPolling",       // Task name
    4096,                // Stack size (bytes)
    NULL,                // Task parameters
    1,                   // Priority (0 or 1)
    NULL,                // Task handle
    0                    // Core to run the task (0 or 1)
  );
}

void loop() {}

/*
 * Generate the required JSON message
 * for the sendMessage request which is:
 * {
 *  "chat_id": "id",
 *  "text": "id",
 *  "reply_markup": [
 *    [{ "text": "Turn AC on", "callback_data": "ac_on" }],
 *    [{ "text": "Turn AC off", "callback_data": "ac_off" }]
 *  ]
 * }
 */
String generateMessageJSON(float temperature) {
  StaticJsonDocument<256> jsonDocument;
  jsonDocument["chat_id"] = CHAT_ID;
  String textToSend = "The temperature in the ambient is ";
  textToSend += String(temperature);
  textToSend += "°C";
  jsonDocument["text"] = textToSend;
  JsonArray inlineKeyboard = jsonDocument.createNestedObject("reply_markup")["inline_keyboard"].to<JsonArray>();

  JsonArray row1 = inlineKeyboard.createNestedArray();
  JsonObject acOnButton = row1.createNestedObject();
  acOnButton["text"] = "Turn AC on";
  acOnButton["callback_data"] = "ac_on";

  JsonArray row2 = inlineKeyboard.createNestedArray();
  JsonObject acOffButton = row2.createNestedObject();
  acOffButton["text"] = "Turn AC off";
  acOffButton["callback_data"] = "ac_off";

  String data;
  serializeJson(jsonDocument, data);
  return data;
}

/*
 * Generate the required JSON message
 * for the editMessageReplyMarkup request
 * which is:
 * {
 *  "chat_id": "id",
 *  "message_id": "id"
 * }
 * reply_markup is purposedly left empty
 * as we're just clearing the buttons in 
 * the message
 */
String generateEditMessageJSON(int id) {
  StaticJsonDocument<256> jsonDocument;
  jsonDocument["chat_id"] = CHAT_ID;
  jsonDocument["message_id"] = id;
  String data;
  serializeJson(jsonDocument, data);
  return data;
}

/*
 * Request for the getUpdates endpoint
 * here we're receiving Telegram updates
 * through long polling (https://en.wikipedia.org/wiki/Push_technology#Long_polling)
 * returns an array of Update objects, from
 * which we only extract the ones from callback_query
 * meaning the user clicked a button, and therefore
 * act on that input (turning AC on or OFF)
 */
void getUpdates() {
  WiFiClientSecure client;
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  String url = "/bot";
  url += API_TOKEN;
  url += "/getUpdates";

  if (!client.connect(server, 443)) {
    Serial.println("Connection failed");
    return;
  }

  String requestBody = "GET ";
  requestBody += url; 
  requestBody += " HTTP/1.1\r\n";
  requestBody += "Host: api.telegram.org\r\n";
  requestBody += "Connection: close\r\n";
  requestBody += "User-Agent: Arduino/1.0\r\n";
  requestBody += "Accept: /\r\n";
  requestBody += "Cache-Control: no-cache\r\n";
  requestBody += "\r\n";
  
  client.print(requestBody);

  String response = "";
  bool headersReceived = false;

  while (client.connected()) {
    while (client.available()) {
      char c = client.read();
      response += c;

      if (c == '\n' && response.endsWith("\r\n\r\n")) {
        headersReceived = true;
        Serial.println("\n=== HEADERS /getUpdates ===");
        Serial.println(response);
        response = "";
      } else if (headersReceived) {
        if (c == '\n') {
          break;
        }
      }
    }

    if (!client.connected()) {
      break;
    }
  }

  client.stop();

  // The filter: it contains "true" for each value we want to keep
  StaticJsonDocument<200> filter;
  filter["result"][0]["callback_query"]["id"] = true;
  filter["result"][0]["callback_query"]["data"] = true;

  DynamicJsonDocument jsonDocument(1024);
  DeserializationError error = deserializeJson(jsonDocument, response, DeserializationOption::Filter(filter));
  Serial.println("=== JSON /getUpdates ===\n");
  serializeJsonPretty(jsonDocument, Serial);

  if (!error) {
    JsonArray resultArray = jsonDocument["result"].as<JsonArray>();

    if (resultArray.size() > 0) {
      JsonObject lastUpdate = resultArray[resultArray.size() - 1].as<JsonObject>();
      if (lastUpdate.containsKey("callback_query")) {
        JsonObject callbackQuery = lastUpdate["callback_query"].as<JsonObject>();
        String data = callbackQuery["data"].as<String>();
        String id = callbackQuery["id"].as<String>();
        if (!isIdProcessed(id)) {
          if (data == "ac_on") {
            answerCallbackQuery(id, "AC turned ON successfully");
            setColor(0, 0, 255);
          } else if (data == "ac_off") {
            answerCallbackQuery(id, "AC turned OFF successfully");
            setColor(0, 0, 0);
          }
        }
      }
    }
  } else {
    Serial.println(error.c_str());
    Serial.println("Failed to parse JSON");
  }
}

/*
 * Request to post an answer to a
 * callback query, in case the user clicked
 * a button, this endpoint will offer visual
 * feedback with a message
 */
void answerCallbackQuery(String query_id, String message) {
  WiFiClientSecure client;
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  String url = "/bot";
  url += API_TOKEN;
  url += "/answerCallbackQuery";

  if (!client.connect(server, 443)) {
    Serial.println("Connection failed");
    return;
  }

  StaticJsonDocument<256> jsonDocument;
  jsonDocument["callback_query_id"] = query_id;
  jsonDocument["text"] = message;
  String data;
  serializeJson(jsonDocument, data);

  String requestBody = "POST ";
  requestBody += url;
  requestBody += " HTTP/1.1\r\n";
  requestBody += "Host: ";
  requestBody += server;
  requestBody += "\r\n";
  requestBody += "Content-Type: application/json\r\n";
  requestBody += "Content-Length: ";
  requestBody += String(data.length());
  requestBody += "\r\n";
  requestBody += "Connection: close\r\n";
  requestBody += "User-Agent: Arduino/1.0\r\n";
  requestBody += "Accept: /\r\n";
  requestBody += "Cache-Control: no-cache\r\n";
  requestBody += "Connection: close\r\n";
  requestBody += "\r\n";
  requestBody += data;
  
  client.print(requestBody);

  String response = "";
  bool headersReceived = false;

  while (client.connected()) {
    while (client.available()) {
      char c = client.read();
      response += c;

      if (c == '\n' && response.endsWith("\r\n\r\n")) {
        headersReceived = true;
        Serial.println("\n=== HEADERS /answerCallbackQuery ===");
        Serial.println(response);
        response = "";
      } else if (headersReceived) {
        if (c == '\n') {
          break;
        }
      }
    }

    if (!client.connected()) {
      break;
    }
  }

  Serial.println("=== JSON /answerCallbackQuery ===");
  Serial.println(response);
  Serial.println("\n");

  callbackIds.insert(query_id);
  client.stop();
}

/*
 * Request to programatically send a message,
 * in this case we're sending the temperature,
 * it returns the message object, we're storing it
 * so we can edit the previous message when the next
 * is being sent, so the buttons will only be available
 * in the latest temperature update
 */
void sendMessage(float temperature) {
  WiFiClientSecure client;
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  String url = "/bot";
  url += API_TOKEN;
  url += "/sendMessage";

  if (!client.connect(server, 443)) {
    Serial.println("Connection failed");
    return;
  }

  if (previousId) {
    editPreviousMessage(previousId);
  }

  String data = generateMessageJSON(temperature);
  String requestBody = "POST ";
  requestBody += url; 
  requestBody += " HTTP/1.1\r\n";
  requestBody += "Host: api.telegram.org\r\n";
  requestBody += "Connection: close\r\n";
  requestBody += "User-Agent: Arduino/1.0\r\n";
  requestBody += "Content-Type: application/json\r\n";
  requestBody += "Content-Length: ";
  requestBody += String(data.length());
  requestBody += "\r\n";
  requestBody += "\r\n";
  requestBody += data;

  client.print(requestBody);

  String response = "";
  bool headersReceived = false;

  while (client.connected()) {
    while (client.available()) {
      char c = client.read();
      response += c;

      if (c == '\n' && response.endsWith("\r\n\r\n")) {
        headersReceived = true;
        Serial.println("\n=== HEADERS /sendMessage ===");
        Serial.println(response);
        response = "";
      } else if (headersReceived) {
        if (c == '\n') {
          break;
        }
      }
    }

    if (!client.connected()) {
      break;
    }
  }

  client.stop();

  // The filter: it contains "true" for each value we want to keep
  StaticJsonDocument<200> filter;
  filter["result"]["message_id"] = true;

  DynamicJsonDocument jsonDocument(1024);
  DeserializationError error = deserializeJson(jsonDocument, response, DeserializationOption::Filter(filter));
  Serial.println("=== JSON /sendMessage ===\n");
  serializeJsonPretty(jsonDocument, Serial);

  if (!error) {
    previousId = jsonDocument["result"]["message_id"];
  } else {
    Serial.println(error.c_str());
    Serial.println("Failed to parse JSON");
  }
}

/*
 * Request to programatically edit a message
 * reply markup, in this case we're only clearing
 * the reply markup so the buttons disappear when
 * a new update comes with the temperature
 */
void editPreviousMessage(int messageId) {
  WiFiClientSecure client;
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  String url = "/bot";
  url += API_TOKEN;
  url += "/editMessageReplyMarkup";

  if (!client.connect(server, 443)) {
    Serial.println("Connection failed");
    return;
  }

  String data = generateEditMessageJSON(previousId);

  String requestBody = "POST ";
  requestBody += url; 
  requestBody += " HTTP/1.1\r\n";
  requestBody += "Host: api.telegram.org\r\n";
  requestBody += "Connection: close\r\n";
  requestBody += "User-Agent: Arduino/1.0\r\n";
  requestBody += "Content-Type: application/json\r\n";
  requestBody += "Content-Length: ";
  requestBody += String(data.length());
  requestBody += "\r\n";
  requestBody += "\r\n";
  requestBody += data;

  client.print(requestBody);

  String response = "";
  bool headersReceived = false;

  while (client.connected()) {
    while (client.available()) {
      char c = client.read();
      response += c;

      if (c == '\n' && response.endsWith("\r\n\r\n")) {
        headersReceived = true;
        Serial.println("\n=== HEADERS /editMessageReplyMarkup ===");
        Serial.println(response);
        response = "";
      } else if (headersReceived) {
        if (c == '\n') {
          break;
        }
      }
    }

    if (!client.connected()) {
      break;
    }
  }

  Serial.println("=== JSON /editMessageReplyMarkup ===");
  Serial.println(response);
  Serial.println("\n");

  client.stop();
}

/*
 * Handles the long polling, calls getUpdates
 * every 5 seconds to check for new updates
 * from Telegram
 */
void handleLongPolling(void *parameter) {
  while (1) {
    getUpdates();
    vTaskDelay(pdMS_TO_TICKS(5 * 1000));
  }
}

/*
 * Checks the temperature every 5 minutes
 * and then sends the updated temperature
 * to Telegram
 */
void checkTemperature(void *parameter) {
  while (1) {
    float temperature = dht.readTemperature();

    if (isnan(temperature)) {
      Serial.println("Failed to read temperature");
      return;
    }

    Serial.print("Temperature: ");
    Serial.println(temperature);
    sendMessage(temperature);

    vTaskDelay(pdMS_TO_TICKS(5 * 60 * 1000));
  }
}

/*
 * Util for checking whether the id is present
 * in the std::set or not
 */
bool isIdProcessed(const String& id) {
  return callbackIds.count(id) > 0;
}

/*
 * Util for changing the RGB LED color
 */
void setColor(int red, int green, int blue) {
  analogWrite(LED_PIN, blue);
}