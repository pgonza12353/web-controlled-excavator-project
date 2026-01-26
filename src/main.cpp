#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include "MotorController.h"
#include "secrets.h"

// WiFi credentials
const char* ssid     = ACTIVE_WIFI_SSID;
const char* password = ACTIVE_WIFI_PASSWORD;

// Servers
WebServer server(80);             // HTTP server on port 80. Simple requests, check if the board is alive
WebSocketsServer webSocket(81);   // WebSocket server on port 81. Real-time, 2-way communication

// HTTP request handler
void handleRoot() {
  server.send(200, "text/plain", "Hello from ESP32 HTTP Server!");
}
// MOTOR PIN CONFIGURATION
// Left track
const int LEFT_LPWM = 22;
const int LEFT_RPWM = 23;

// Right track
const int RIGHT_LPWM = 33;
const int RIGHT_RPWM = 32;

// Base rotation
const int BASE_LPWM = 26;
const int BASE_RPWM = 25;

// Left track
const int LEFT_LEN = 14;  // Left enable
const int LEFT_REN = 13;  // Right enable

// Right track
const int RIGHT_LEN = 19; // Left enable
const int RIGHT_REN = 18; // Right enable

// Base rotation
const int BASE_LEN = 16;  // Left enable
const int BASE_REN = 17;  // Right enable

// PWM channels
const int LEFT_L_CH = 0;
const int LEFT_R_CH = 1;
const int RIGHT_L_CH = 2;
const int RIGHT_R_CH = 3;
const int BASE_L_CH = 4;
const int BASE_R_CH = 5;

// PWM config
const int PWM_FREQ = 20000;       // 20 kHz 
const int PWM_RES = 8;            // 8-bit resolution (0-255) 

// Component speed variables
volatile int leftTrackSpeed = 0;
volatile int rightTrackSpeed = 0;
volatile int baseSpeed = 0;

int leftTrackApplied = 0;
int rightTrackApplied = 0;
int baseApplied = 0;

// Reference previous speeds
// int prevLeftTrackSpeed = 0;
// int prevRightTrackSpeed = 0;
// int prevBaseSpeed = 0;

// Command separator
String SEPARATOR = "===================================================================================";
String SEPARATOR2 = " - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -";

// Motor Control Functions
void driveBTS7960 (
  int lpwmCh, int rpwmCh, int speed
) {
  speed = constrain(speed, -255, 255);
  if(speed > 0) {
    // Forward
    ledcWrite(lpwmCh, speed);
    ledcWrite(rpwmCh, 0);
  } else if(speed < 0) {
    // Backward
    ledcWrite(lpwmCh, 0);
    ledcWrite(rpwmCh, -speed);
  } else {
    // Stop
    ledcWrite(lpwmCh, 0);
    ledcWrite(rpwmCh, 0);
  }
}

int normalizeSpeed(int s) {
  if (s == 0) return 0;

  // Deadzone compensation (static friction)
  if (abs(s) < 60) {
    return (s > 0) ? 60 : -60;
  }

  return s;
}

void rampMotor(
  int &currentSpeed,
  int targetSpeed,
  void (*moveFn)(int)
) {

  const int STEP = 3;

  if (currentSpeed < targetSpeed) {
    currentSpeed += STEP;
    if(currentSpeed > targetSpeed) currentSpeed = targetSpeed;
  }
  else if (currentSpeed > targetSpeed) {
    currentSpeed -= STEP;
    if(currentSpeed < targetSpeed) currentSpeed = targetSpeed;
  }

  moveFn(currentSpeed);
}

void moveLeftTrack(int speed) {
  driveBTS7960(LEFT_L_CH, LEFT_R_CH, speed);
}

void moveRightTrack(int speed) {
  driveBTS7960(RIGHT_L_CH, RIGHT_R_CH, speed);
}

void rotateBase(int speed) {
  driveBTS7960(BASE_L_CH, BASE_R_CH, speed);
}

// ESP32 WIFI + WEBSOCKET SKELETON CODE
// num = which client, type = what just happened, payload = data sent by client, length = length of data
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("WebSocket[%u] Disconnected!\n", num);
      moveLeftTrack(0);
      moveRightTrack(0);
      rotateBase(0);
      break;
    case WStype_CONNECTED: {
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("WebSocket[%u] Connected from %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
      Serial.println(SEPARATOR);
      webSocket.sendTXT(num, "Hello from ESP32 WebSocket Server!");
      break;
    }
    // This is where most of the real firmware logic will go
    case WStype_TEXT: {
      String msg = String((char*)payload, length);

      // TRACK RAW MSG ----------------------------------------------
      Serial.println("{RAW MSG: [" + msg + "]}");     

      int sepIndex = msg.indexOf(':');
      bool validCommand = false;

      if(msg == "stop") {
          leftTrackSpeed = 0;
          rightTrackSpeed = 0;
          baseSpeed = 0;
          webSocket.sendTXT(num, "VALID STOP COMMAND --- " + msg);
          Serial.println("> Base speed updated: " + String(baseSpeed));
          Serial.println("> Right Track speed updated: " + String(rightTrackSpeed));
          Serial.println("> Left Track speed updated: " + String(leftTrackSpeed));
          Serial.println(SEPARATOR2);
          validCommand = true;
      } else if(msg == "stopTracks") {
          leftTrackSpeed = 0;
          rightTrackSpeed = 0;
          webSocket.sendTXT(num, "VALID STOP COMMAND --- " + msg);
          Serial.println("> Right Track speed updated: " + String(rightTrackSpeed));
          Serial.println("> Left Track speed updated: " + String(leftTrackSpeed));
          Serial.println(SEPARATOR2);
          validCommand = true;
      } else if(msg == "stopBase") {
          baseSpeed = 0;
          webSocket.sendTXT(num, "VALID STOP COMMAND --- " + msg);
          Serial.println("> Base speed updated: " + String(baseSpeed));
          Serial.println(SEPARATOR2);
          validCommand = true;
      } else if(sepIndex != -1) {
        String component = msg.substring(0, sepIndex);
        String speedStr = msg.substring(sepIndex + 1);
        speedStr.trim();                // remove any leading/trailing whitespace

        int speed = speedStr.toInt();       // Convert speed to integer. Non-numeric speeds are converted to 0
        bool speedValid = (speedStr.length() > 0 && speed >= -255 && speed <= 255);

        // DEBUG PARSED COMMAND (COMPONENT + SPEED) -----------------
        Serial.println("{COMPONENT: [" + component + "]" + " ----- " + "SPEED: " + String(speed) + "}");

        // Call the appropriate motor control function
        if(component == "leftTrack" && speedValid) {
          leftTrackSpeed = speed;
          webSocket.sendTXT(num, "VALID LEFT TRACK COMMAND --- " + msg);
          Serial.println("> Left Track speed updated: " + String(leftTrackSpeed));
          Serial.println(SEPARATOR2);
          validCommand = true;
        } else if(component == "rightTrack" && speedValid) {
          rightTrackSpeed = speed;
          webSocket.sendTXT(num, "VALID RIGHT TRACK COMMAND --- " + msg);
          Serial.println("> Right Track speed updated: " + String(rightTrackSpeed));
          Serial.println(SEPARATOR2);
          validCommand = true;
        } else if(component == "base" && speedValid ) {
          baseSpeed = speed;
          webSocket.sendTXT(num, "VALID BASE COMMAND --- " + msg);
          Serial.println("> Base speed updated: " + String(baseSpeed));
          Serial.println(SEPARATOR2);
          validCommand = true;
        } else if(component == "forward" && speedValid) {
          leftTrackSpeed = speed;
          rightTrackSpeed = speed;
          webSocket.sendTXT(num, "VALID FORWARD COMMAND --- " + msg);
          Serial.println("> Right Track speed updated: " + String(rightTrackSpeed));
          Serial.println("> Left Track speed updated: " + String(leftTrackSpeed));
          Serial.println(SEPARATOR2);
          validCommand = true;
        } else if(component == "turnRight" && speedValid) {
          leftTrackSpeed = speed;
          rightTrackSpeed = 0;
          webSocket.sendTXT(num, "VALID TURN COMMAND --- " + msg);
          Serial.println("> Right Track speed updated: " + String(rightTrackSpeed));
          Serial.println("> Left Track speed updated: " + String(leftTrackSpeed));
          Serial.println(SEPARATOR2);
          validCommand = true;
        } else if(component == "turnLeft" && speedValid) {
          leftTrackSpeed = 0;
          rightTrackSpeed = speed;
          webSocket.sendTXT(num, "VALID TURN COMMAND --- " + msg);
          Serial.println("> Right Track speed updated: " + String(rightTrackSpeed));
          Serial.println("> Left Track speed updated: " + String(leftTrackSpeed));
          Serial.println(SEPARATOR2);
          validCommand = true;
        }
        
        if(!validCommand) {
          Serial.printf("WebSocket[%u] INVALID COMMAND: [%s]\n", num, msg.c_str());
          webSocket.sendTXT(num, "INVALID COMMAND --- " + msg);
          Serial.println(SEPARATOR);
        }
      } else {
          Serial.printf("WebSocket[%u] MALFORMED COMMAND; NO \":\" GIVEN: %s\n", num, msg.c_str());
          webSocket.sendTXT(num, "MALFORMED COMMAND --- NO \":\" GIVEN");
          Serial.println(SEPARATOR);
      }
      break;
    }
    case WStype_BIN:
      Serial.printf("WebSocket[%u] Received Binary Data of length: %u\n", num, length);
      break;
    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("BOOK OK");

  
  pinMode(LEFT_LEN, OUTPUT);
  pinMode(LEFT_REN, OUTPUT);
  digitalWrite(LEFT_LEN, HIGH);
  digitalWrite(LEFT_REN, HIGH);

  pinMode(RIGHT_LEN, OUTPUT);
  pinMode(RIGHT_REN, OUTPUT);
  digitalWrite(RIGHT_LEN, HIGH);
  digitalWrite(RIGHT_REN, HIGH);

  pinMode(BASE_LEN, OUTPUT);
  pinMode(BASE_REN, OUTPUT);
  digitalWrite(BASE_LEN, HIGH);
  digitalWrite(BASE_REN, HIGH);

  Serial.println("ESP32 WiFi + WebSocket skeleton");
  // Set up PWM channels
  ledcSetup(LEFT_L_CH, PWM_FREQ, PWM_RES);
  ledcSetup(LEFT_R_CH, PWM_FREQ, PWM_RES);
  ledcSetup(RIGHT_L_CH, PWM_FREQ, PWM_RES);
  ledcSetup(RIGHT_R_CH, PWM_FREQ, PWM_RES);
  ledcSetup(BASE_L_CH, PWM_FREQ, PWM_RES);
  ledcSetup(BASE_R_CH, PWM_FREQ, PWM_RES);

  // Attach PWM pins to channels
  ledcAttachPin(LEFT_LPWM, LEFT_L_CH);
  ledcAttachPin(LEFT_RPWM, LEFT_R_CH);
  ledcAttachPin(RIGHT_LPWM, RIGHT_L_CH);
  ledcAttachPin(RIGHT_RPWM, RIGHT_R_CH);
  ledcAttachPin(BASE_LPWM, BASE_L_CH);
  ledcAttachPin(BASE_RPWM, BASE_R_CH);

  moveLeftTrack(0);
  moveRightTrack(0);
  rotateBase(0);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println();
  Serial.print("Connected! IP: "); 
  Serial.println(WiFi.localIP());

  // HTTP server setup
  server.on("/", handleRoot);
  server.begin();
  Serial.println("HTTP server started");

  // WebSocket server setup
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("WebSocket server started");

  // Boot up tests for each motor
  delay(10000);

  moveLeftTrack(150);
  Serial.println("Left track moving forward for 3 seconds...");
  delay(3000);
  moveLeftTrack(0);
  Serial.println("Left track stopped.");

  moveRightTrack(150);
  Serial.println("Right track moving forward for 3 seconds...");
  delay(3000);
  moveRightTrack(0);
  Serial.println("Right track stopped.");

  rotateBase(150);
  Serial.println("Base rotating forward for 3 seconds...");
  delay(3000);
  rotateBase(0);
  Serial.println("Base stopped.");

  Serial.println("SETUP COMPLETE — READY");
}

void loop() {
  // Handle incoming HTTP requests
  server.handleClient();

  // Handle WebSocket connections & messages
  webSocket.loop();

  // Normalize targets (deadzone fix)
  int leftTarget = normalizeSpeed(leftTrackSpeed);
  int rightTarget = normalizeSpeed(rightTrackSpeed);
  int baseTarget = normalizeSpeed(baseSpeed);

  // Ramp motors safely
  rampMotor(leftTrackApplied, leftTarget, moveLeftTrack);
  rampMotor(rightTrackApplied, rightTarget, moveRightTrack);
  rampMotor(baseApplied, baseTarget, rotateBase);
}