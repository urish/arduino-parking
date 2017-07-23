#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include <Adafruit_SSD1306.h>
#include <U8x8lib.h>

SoftwareSerial serialSIM800(D5, D6);

U8X8_SSD1306_64X48_ER_HW_I2C u8x8(U8X8_PIN_NONE, D1, D2);

#define APN "uinternet"
#define TARGET_URL "parking-sensor-app.firebaseio.com/s.json"

const int trigPin = D4;
const int echoPin = D7;

StaticJsonBuffer<200> jsonBuffer;
JsonObject& report = jsonBuffer.createObject();

bool readResponse(String expected = "OK", String ignore = "") {
  int timeout = 10000;
  String line = "";
  char ch;
  while (timeout >= 0) {
    if (serialSIM800.available()) {
      ch = serialSIM800.read();
      if (ch == '\r') {
        continue;
      }
      if (ch == '\n') {
        if (line == "") {
          continue;
        }
        if (line[0] == '+' || line == "OVER-VOLTAGE WARNNING") {
          Serial.println("<<< " + line);
          line = "";
          Serial.println(":: " + expected);
          Serial.println(line.startsWith(expected));
          if (line.startsWith(expected)) {
            return true;
          }
          continue;
        }
        Serial.println("< " + line);
        if (ignore.length() > 0 && line == ignore) {
          line = "";
          continue;
        }
        if (line != expected) {
          Serial.println("!!! Unexpected Response");
          u8x8.println("ERROR!");
          u8x8.println(line);
          return false;
        } else {
          return true;
        }
      } else {
        line += ch;
      }
    } else {
      delay(1);
      timeout--;
    }
  }
}

void sendCommand(String cmd) {
  while (serialSIM800.available()) {
    Serial.write(serialSIM800.read());
  }

  Serial.println("> " + cmd);
  serialSIM800.println(cmd);
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  // Initialize the sensor
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  Serial.begin(115200);
  Serial.println("Start");

  u8x8.begin();
  u8x8.setFont(u8x8_font_amstrad_cpc_extended_r);
  u8x8.println("Start!");

  serialSIM800.begin(9600);
  delay(3000);
  digitalWrite(LED_BUILTIN, HIGH);

  // JSON report we will send to Firebase
  JsonObject &timestamp = report.createNestedObject("t");
  timestamp[".sv"] = "timestamp";

  // Initialize the modem
  do {
    sendCommand("AT");
    delay(100);
  } while (!readResponse("OK", "AT"));
  sendCommand("ATE0");
  readResponse("OK", "ATE0");

  Serial.println("Modem ready, initializing GSM");
  u8x8.println("Modem");

  do {
    sendCommand("AT+SAPBR=3,1,\"Contype\",\"GPRS\"");
    delay(100);
  } while (!readResponse());
  sendCommand("AT+SAPBR=3,1,\"APN\",\"" APN "\"");
  readResponse();
  sendCommand("AT+SAPBR=1,1");
  readResponse();
  sendCommand("AT+SAPBR=2,1");
  readResponse();
}

long readDistance() {
  long duration;
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH);
  return duration * 0.034 / 2;
}

// the loop function runs over and over again forever
void loop() {
  u8x8.clearDisplay();
  u8x8.home();
  Serial.println("Measuring...");
  long distance = readDistance();
  Serial.println(distance);
  u8x8.print("D:");
  u8x8.println(distance);
  sendCommand("AT+SAPBR=2,1");
  readResponse();
  sendCommand("AT+CGATT?");
  readResponse();
  sendCommand("AT+HTTPINIT");
  readResponse();
  sendCommand("AT+HTTPPARA=\"CID\",1");
  readResponse();
  sendCommand("AT+HTTPPARA=\"URL\",\"" TARGET_URL "\"");
  readResponse();
  sendCommand("AT+HTTPPARA=\"CONTENT\",\"application/json\"");
  readResponse();
  sendCommand("AT+HTTPSSL=1");
  readResponse();

  String jsonData;
  report["v"] = distance;
  int len = report.printTo(jsonData);

  String cmd = "AT+HTTPDATA=";
  cmd += len;
  cmd += ",10000";

  sendCommand(cmd);
  readResponse("DOWNLOAD");
  sendCommand(jsonData);
  readResponse();
  sendCommand("AT+HTTPACTION=1");
  readResponse();
  sendCommand("AT+HTTPREAD");
  readResponse();
  u8x8.println("Send");
  readResponse("+HTTPACTION");
  delay(100);
  u8x8.println("Done");
  sendCommand("AT+HTTPTERM");
  readResponse();
  delay(10000);
}

