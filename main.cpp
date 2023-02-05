#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <WiFiManager.h>
#include <ESP8266WebServer.h>

// Replace the hostname you want to access the web interface from.
String newHostname = "kqclipper";

// Replace the password you want to set for the web interface
const char* password = "password";

// Replace with your Twitch Client ID
const char* client_id = "xxxxxxxxxxxxxxxxxx";

// Replace with your Twitch Access Token
const char* access_token = "xxxxxxxxxxxxxxxxxxxxxxxxxxxx";

// Replace with the channel_id of the streamer you want to create a clip for
const char channel_id[50] = "xxxxxxxx";

// Replace with your Discord Webhook URL
const char* webhook_url = "https://discord.com/api/webhooks/xxxxxxxxxxxxxxxxxxxxxxxxxxxx";

// Replace with your the WiFi auto connect ssid and password you want to set. This network will pop up to connect to a new wifi network
const char* AutoConnectSSID = "KQCLIPPER";
const char* AutoConnectPassword = "password";

WiFiClientSecure client;

ESP8266WebServer server(80);

void handleRoot() {
  String html = "<html><body>";
  html += "<h1>Enter password to change value</h1>";
  html += "<form action='/change' method='post'>";
  html += "Password: <input type='password' name='password'>";
  html += "<br><br>";
  html += "channel_id: <input type='text' name='channel_id'>";
  html += "<br><br>";
  html += "<input type='submit' value='Submit'>";
  html += "</form>";
  html += "<br><br>";
  html += "<a href=https://www.streamweasels.com/tools/convert-twitch-username-to-user-id>You can get a channel's ID from here.</a>";
  html += "<br><br>";
  html += "Current value: " + String(channel_id);
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleChange() {
  if (server.arg("password") == password) {
    strcpy((char*)channel_id,server.arg("channel_id").c_str());
  }
  handleRoot();
}

void setup() {
    // WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
    // it is a good practice to make sure your code sets wifi mode how you want it.

    // put your setup code here, to run once:
    Serial.begin(115200);
    
    //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wm;

    // reset settings - wipe stored credentials for testing
    // these are stored by the esp library
    // wm.resetSettings();

    // Automatically connect using saved credentials,
    // if connection fails, it starts an access point with the specified name ( "AutoConnectAP"),
    // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
    // then goes into a blocking loop awaiting configuration and will return success result

    bool res;
    // res = wm.autoConnect(); // auto generated AP name from chipid
    // res = wm.autoConnect("AutoConnectAP"); // anonymous ap
    res = wm.autoConnect(AutoConnectSSID,AutoConnectPassword); // password protected ap

    if(!res) {
        Serial.println("Failed to connect");
        // ESP.restart();
    } 
    else {
        //if you get here you have connected to the WiFi    
        Serial.println("connected...yeey :)");
    }

    client.setInsecure(); // disable certificate validation
    pinMode(14, INPUT_PULLUP);

  //Get Current Hostname
  Serial.printf("Default hostname: %s\n", WiFi.hostname().c_str());

  //Set new hostname
  WiFi.hostname(newHostname.c_str());

  //Get Current Hostname
  Serial.printf("New hostname: %s\n", WiFi.hostname().c_str());

  server.on("/", handleRoot);
  server.on("/change", handleChange);
  server.begin();
  Serial.println("HTTP server started");
}

void postToDiscord(String clip_url) {
  HTTPClient https;
  Serial.println("[HTTP] Connecting to Discord...");
  Serial.println("[HTTP] Message: " + clip_url);
  if (https.begin(client, webhook_url)) {  // HTTPS
    // start connection and send HTTP header
    https.addHeader("Content-Type", "application/json");
    int httpCode = https.POST("{\"content\":\"" + clip_url + "\"}");

    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.print("[HTTP] Status code: ");
      Serial.println(httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        String payload = https.getString();
        Serial.print("[HTTP] Response: ");
        Serial.println(payload);
      }
    } else {
      Serial.print("[HTTP] Post... failed, error: ");
      Serial.println(https.errorToString(httpCode).c_str());
    }

    https.end();
  } else {
    Serial.printf("[HTTP] Unable to connect\n");
  }

  // End extra scoping block
}

void createClip() {
  if (client.connect("api.twitch.tv", 443)) {
    String url = "https://api.twitch.tv/helix/clips?broadcaster_id=" + String(channel_id);
    String payload = "";
    String header = "Authorization: Bearer " + String(access_token) + "\r\n" +
                    "Client-ID: " + String(client_id) + "\r\n" +
                    "Content-Length: " + String(payload.length()) + "\r\n" +
                    "Content-Type: application/json\r\n";
    client.print("POST " + url + " HTTP/1.1\r\n" + header + "\r\n" + payload);
    Serial.println("POST " + url + " HTTP/1.1\r\n" + header + "\r\n" + payload);
    Serial.println("Clip creation requested.");

    // Wait for the response from the Twitch API
    while (client.available() == 0) {
      delay(1);
    }
    
    while (client.connected()) {
      String line = client.readStringUntil('\n');
      if (line == "\r") {
        break;
      }
    }

    String response = client.readString();
    int startIndex = response.indexOf("https://clips.twitch.tv/");
    int endIndex = response.indexOf("/edit");
    if (startIndex != -1 && endIndex != -1) {
      String clip_url = response.substring(startIndex, endIndex);
      postToDiscord(clip_url);
    } else {
      Serial.println("Failed to parse clip URL from Twitch API response.");
    }
  } else {
    Serial.println("Connection to Twitch API failed.");
  }
  client.stop();
}

void loop() {
  static int buttonState = HIGH;
  int reading = digitalRead(14);

  if (reading != buttonState) {
    if (reading == LOW) {
      createClip();
    }
    buttonState = reading;
    delay(45000);
  }

  server.handleClient();

}
