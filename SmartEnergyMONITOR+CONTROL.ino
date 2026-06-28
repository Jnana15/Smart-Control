#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <LiquidCrystal_I2C.h>
#include <PZEM004Tv30.h>

// WIFI
const char* ssid = "          ";
const char* password = "       ";
bool overloadSent = false;
// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// PZEM
HardwareSerial pzemSerial(2);
WebServer server(80);
//code for sendind data to telegram
PZEM004Tv30 pzem(pzemSerial, 16, 17);
const char* server1 = "https://script.google.com/macros/s/AKfycbwGlFnldKyOKr07aPT6djXkWVQ5SeYL0kycFwLBEPCtY_mfOO9HnVcIjUe_wwAInxA/exec";
String urlEncode(String str) {
  String encoded = "";
  char c;
  char code0;
  char code1;

  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);

    if (isalnum(c)) {
      encoded += c;
    } else {
      encoded += "%";
      code0 = (c >> 4) & 0xF;
      code1 = c & 0xF;
      encoded += String(code0, HEX);
      encoded += String(code1, HEX);
    }
  }
  return encoded;
}

// function for send data to telegram
void sendMsg(String msg) {

  HTTPClient http;

  String fullUrl = String(server1) + "?msg=" + urlEncode(msg);

  Serial.println(fullUrl);

  http.begin(fullUrl);

  int code = http.GET();

  Serial.print("HTTP Code: ");
  Serial.println(code);

  String payload = http.getString();
  Serial.println(payload);

  http.end();
}
// WEB


// RELAYS
#define RELAY1 26   //LAMP
#define RELAY2 27   //SOCKET

// DATA
float voltage = 0, current = 0, power = 0, energy = 0;
String statusMsg = "INIT";

// STATES
bool relay1State = LOW;
bool relay2State = LOW;
bool state=false;

// TIMERS
unsigned long tSensor = 0, tLCD = 0, tWiFi = 0 , tload = 0;

// INTERVALS
const int sensorInterval = 1000;
const int lcdInterval = 1000;
const int wifiInterval = 5000;
const int loadoffinterval = 120000;

void handleRoot() {

  String html = "<html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";

  html += "<style>";
  html += "body{font-family:Arial;background:#0f172a;color:white;text-align:center;}";
  html += ".card{background:#1e293b;margin:10px;padding:20px;border-radius:15px;}";
  html += ".value{font-size:24px;font-weight:bold;}";
  html += ".green{color:#22c55e;}";
  html += ".red{color:#ef4444;}";
  html += "button{padding:15px;border:none;border-radius:10px;margin:5px;font-size:16px;}";
  html += ".blue{background:#3b82f6;color:white;}";
  html += ".greenBtn{background:#22c55e;color:white;}";
  html += "</style>";

  html += "<script>";
  html += "function fetchData(){";
  html += "fetch('/data').then(res=>res.json()).then(d=>{";
  html += "document.getElementById('v').innerText=d.voltage;";
  html += "document.getElementById('c').innerText=d.current;";
  html += "document.getElementById('p').innerText=d.power;";
  html += "document.getElementById('e').innerText=d.energy;";
  html += "document.getElementById('s').innerText=d.status;";

  html += "if(d.status=='OVERLOAD!'){document.getElementById('s').className='red';alarm();}";
  html += "else{document.getElementById('s').className='green';}";

  html += "});}";
  html += "setInterval(fetchData,1000);";

  html += "function alarm(){var a=new Audio('https://actions.google.com/sounds/v1/alarms/alarm_clock.ogg');a.play();}";
  html += "</script></head><body onload='fetchData()'>";

  html += "<h2>⚡ Smart Energy Dashboard</h2>";

  html += "<div class='card'>";
  html += "Voltage:<div id='v' class='value'>0</div>";
  html += "Current:<div id='c' class='value'>0</div>";
  html += "Power:<div id='p' class='value'>0</div>";
  html += "Energy(kWh):<div id='e' class='value'>0</div>";
  html += "Status:<div id='s' class='value'>INIT</div>";
  html += "</div>";
 html += "<div class='card'>";
  html += "<a href='/r1_toggle'><button class='blue'>LAMP</button></a>";
  html += "<a href='/r2_toggle'><button class='blue'>SOCKET</button></a><br>";
  html += "<a href='/r1_on_r2_off'><button class='greenBtn'>MINIMUM</button></a>";
  html += "<a href='/r1_off_r2_on'><button class='greenBtn'>MAXIMUM</button></a>";
  html += "</div>";

  html += "</body></html>";

  server.send(200, "text/html", html);
}
void handleData() {

  String json = "{";
  json += "\"voltage\":" + String(voltage) + ",";
  json += "\"current\":" + String(current) + ",";
  json += "\"power\":" + String(power) + ",";
  json += "\"energy\":" + String(energy) + ",";
  json += "\"status\":\"" + statusMsg + "\"";
  json += "}";

  server.send(200, "application/json", json);
}
oid setup() {
  // put your setup code here, to run once:

}

void loop() {
  // put your main code here, to run repeatedly:

}
// ================= REDIRECT =================
void redirect() {
  server.sendHeader("Location", "/");
  server.send(303);
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);

  lcd.init();
  lcd.backlight();

  // WIFI CONNECT
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    lcd.setCursor(0, 1);
    lcd.print("Please wait...");
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connected");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());
  delay(9000);
  lcd.clear();

  // ROUTES
  server.on("/", handleRoot);
  server.on("/data", handleData);

  server.on("/r1_toggle", []() {
    relay1State = !relay1State;
    redirect();
  });

  server.on("/r2_toggle", []() {
    relay2State = !relay2State;
    redirect();
  });

  server.on("/r1_on_r2_off", []() {
    relay1State = HIGH;
    relay2State = LOW;
    redirect();
  });

  server.on("/r1_off_r2_on", []() {
    relay1State = LOW;
    relay2State = HIGH;
    redirect();
  });

  server.begin();
}

// ================= LOOP =================
void loop() {

  server.handleClient();

  // WIFI RECONNECT
  if (millis() - tWiFi > wifiInterval) {
    tWiFi = millis();
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.begin(ssid, password);
    }
  }

  // SENSOR
  if (millis() - tSensor > sensorInterval) {
    tSensor = millis();

    voltage = pzem.voltage();
    current = pzem.current();
    power   = pzem.power();
    energy  = pzem.energy();

    if (isnan(voltage)) voltage = 0;
    if (isnan(current)) current = 0;
    if (isnan(power)) power = 0;
    if (isnan(energy)) energy = 0;
  }
   
if (current >2)
{
   statusMsg = "OVERLOAD!";
  lcd.clear();
   lcd.setCursor(4, 1);
    lcd.print("OVERLOAD!");
    lcd.print("      ");
  // statusMsg = "";
  digitalWrite(RELAY2,HIGH);
  delay(60000);

 
    if (!overloadSent)
    {
        sendMsg("Hi! OVERLOAD! TURN LOAD OFF");
        overloadSent = true;
    }
}
    else
    {
    statusMsg = "NORMAL";
    digitalWrite(RELAY1, relay1State);
    digitalWrite(RELAY2, relay2State);
     }

  // LCD
  if (millis() - tLCD > lcdInterval) {
    tLCD = millis();

    lcd.setCursor(0, 0);
    lcd.print("V:");
    lcd.print(voltage, 1);
    lcd.print(" P:");
    lcd.print(power, 0);
    lcd.print("W   ");

    lcd.setCursor(4, 1);
    lcd.print(statusMsg);
    lcd.print("        ");
  }
}
