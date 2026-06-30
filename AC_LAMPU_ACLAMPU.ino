/*
===========================================================
 ESP32 IDLE UP CONTROLLER V3
 Part 1 / 3
 Author : Airlangga + ChatGPT

 Hardware

 GPIO25 -> Servo
 GPIO18 -> Trigger Switch (INPUT_PULLUP)

===========================================================
*/

#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <Preferences.h>

WebServer server(80);
Preferences prefs;

//===========================================================
// WiFi
//===========================================================

const char* ssid = "ESP32_IDLE_UP";
const char* password = "12345678";

//===========================================================
// PIN
//===========================================================

const uint8_t SERVO_PIN = 25;

const uint8_t AC_TRIGGER_PIN = 21;
const uint8_t LAMP_TRIGGER_PIN = 18;

//===========================================================
// SERVO
//===========================================================

Servo servo;

//===========================================================
// CONFIG STRUCT
//===========================================================

struct ServoConfig
{
    int homeAngle;

    int acAngle;

    int lampAngle;

    int acLampAngle;

    bool enableIdle;
};

ServoConfig config;

//===========================================================
// STATUS
//===========================================================

enum ServoState
{
    STATE_HOME,
    STATE_AC,
    STATE_LAMP,
    STATE_AC_LAMP
};

ServoState currentState = STATE_HOME;

int currentAngle = 90;

bool lastACTriggerState = HIGH;
bool lastLampTriggerState = HIGH;

//===========================================================
// HTML
//===========================================================

String webpage = R"rawliteral(
<!DOCTYPE html>
<html>

<head>

<meta name="viewport" content="width=device-width, initial-scale=1">

<title>ESP32 Idle Up Controller</title>

<style>

body{

background:#202124;
font-family:Arial;
color:white;
text-align:center;
padding:20px;

}

.card{

background:#303134;
padding:20px;
margin:auto;
margin-top:20px;
max-width:420px;
border-radius:10px;

}

.value{

font-size:28px;
margin:15px;

}

input{

width:120px;
padding:10px;
font-size:22px;
text-align:center;

}

button{

padding:10px 25px;
font-size:18px;
margin-top:15px;
cursor:pointer;

}

.green{

color:#00ff00;

}

.red{

color:#ff4444;

}

</style>

</head>

<body>

<h2>ESP32 Idle Up Controller</h2>

<div class="card">

<h3>Trigger Status</h3>

<div class="value" id="triggerStatus">

HIGH

</div>

</div>

<div class="card">

<h3>Trigger AC</h3>

<div class="value red" id="triggerAC">
HIGH
</div>

<hr style="margin:20px 0;">

<h3>Trigger Lamp</h3>

<div class="value red" id="triggerLamp">
HIGH
</div>

<hr style="margin:20px 0;">

<h3>Servo Mode</h3>

<div class="value" id="servoMode">
HOME
</div>

</div>

Current

<div class="value" id="homePos">0°</div>

New

<br><br>

<input id="homeInput" type="number">

<br>

<button onclick="saveHome()">SAVE HOME</button>

</div>

<div class="card">

<h3>AC Position</h3>

Current

<div class="value" id="idlePos">0°</div>

New

<br><br>

<input id="idleInput" type="number">

<br>

<button onclick="saveIdle()">SAVE AC</button>

</div>

<div class="card">

<h3>Lamp Position</h3>

Current

<div class="value" id="lampPos">0°</div>

New

<br><br>

<input id="lampInput" type="number">

<br>

<button onclick="saveLamp()">SAVE LAMP</button>

</div>

<div class="card">

<h3>AC + Lamp Position</h3>

Current

<div class="value" id="acLampPos">0°</div>

New

<br><br>

<input id="acLampInput" type="number">

<br>

<button onclick="saveAcLamp()">SAVE AC + LAMP</button>

</div>

<script>

function updateStatus(){

fetch("/status")

.then(r=>r.json())

.then(data=>{

document.getElementById("servoPos").innerHTML=data.current+"°";

document.getElementById("homePos").innerHTML=data.home+"°";

document.getElementById("idlePos").innerHTML=data.ac+"°";

document.getElementById("lampPos").innerHTML=data.lamp+"°";

document.getElementById("acLampPos").innerHTML=data.aclamp+"°";

document.getElementById("servoMode").innerHTML=data.mode;



let ac=document.getElementById("triggerAC");

ac.innerHTML=data.triggerAC;

if(data.triggerAC=="LOW")
{

ac.className="value green";

}
else
{

ac.className="value red";

}



let lamp=document.getElementById("triggerLamp");

lamp.innerHTML=data.triggerLamp;

if(data.triggerLamp=="LOW")
{

lamp.className="value green";

}
else
{

lamp.className="value red";

}

});

setInterval(updateStatus,500);

updateStatus();

</script>

</body>

</html>
)rawliteral";

//===========================================================
// SERVO FUNCTION
//===========================================================

void moveServo(int angle)
{

    angle = constrain(angle,0,180);

    if(angle==currentAngle)
        return;

    currentAngle = angle;

    Serial.print("Servo -> ");

    Serial.println(currentAngle);

    servo.write(currentAngle);

}


void handleRoot()
{

    server.send(200,"text/html",webpage);

}

void handleStatus()
{
    String mode = "HOME";

    switch(currentState)
    {
        case STATE_HOME:
            mode = "HOME";
            break;

        case STATE_AC:
            mode = "AC";
            break;

        case STATE_LAMP:
            mode = "LAMP";
            break;

        case STATE_AC_LAMP:
            mode = "AC + LAMP";
            break;
    }

    String json="{";

    json += "\"current\":" + String(currentAngle) + ",";
    json += "\"home\":" + String(config.homeAngle) + ",";
    json += "\"ac\":" + String(config.acAngle) + ",";
    json += "\"lamp\":" + String(config.lampAngle) + ",";
    json += "\"aclamp\":" + String(config.acLampAngle) + ",";

    json += "\"triggerAC\":\"";
    json += (digitalRead(AC_TRIGGER_PIN)==LOW) ? "LOW" : "HIGH";
    json += "\",";

    json += "\"triggerLamp\":\"";
    json += (digitalRead(LAMP_TRIGGER_PIN)==LOW) ? "LOW" : "HIGH";
    json += "\",";

    json += "\"mode\":\"";
    json += mode;
    json += "\"";

    json += "}";

    server.send(200,"application/json",json);
}

void handleEnable()
{

config.enableIdle=true;

server.send(200,"text/plain","OK");

}

void handleDisable()
{

config.enableIdle=false;

moveServo(config.homeAngle);

server.send(200,"text/plain","OK");

}

void handleSaveHome()
{

    if(server.hasArg("angle"))
    {

        int a=server.arg("angle").toInt();

        a=constrain(a,0,180);

        config.homeAngle=a;

        prefs.putInt("home",a);

        if(currentState==STATE_HOME)
{
    moveServo(config.homeAngle);
}

        server.send(200,"text/plain","OK");

        return;

    }

    server.send(400,"text/plain","ERROR");

}

void handleSaveIdle()
{

    if(server.hasArg("angle"))
    {

        int a=server.arg("angle").toInt();

        a=constrain(a,0,180);

        config.acAngle=a;

        prefs.putInt("ac",a);

        if(currentState==STATE_AC)
      {
    moveServo(config.acAngle);
      }

        server.send(200,"text/plain","OK");

        return;

    }

    server.send(400,"text/plain","ERROR");

}

void handleSaveLamp()
{

    if(server.hasArg("angle"))
    {

        int a = server.arg("angle").toInt();

        a = constrain(a,0,180);

        config.lampAngle = a;

        prefs.putInt("lamp",a);

        server.send(200,"text/plain","OK");

        return;

    }

    server.send(400,"text/plain","ERROR");

}

void handleSaveAcLamp()
{

    if(server.hasArg("angle"))
    {

        int a = server.arg("angle").toInt();

        a = constrain(a,0,180);

        config.acLampAngle = a;

        prefs.putInt("aclamp",a);

        server.send(200,"text/plain","OK");

        return;

    }

    server.send(400,"text/plain","ERROR");

}

//===========================================================
// Global debounce variables
unsigned long triggerTimer = 0;
bool triggerStable = HIGH;
bool lastRead = HIGH;

void updateState()
{

    bool ac = digitalRead(AC_TRIGGER_PIN)==LOW;

    bool lamp = digitalRead(LAMP_TRIGGER_PIN)==LOW;

    if(!ac && !lamp)
    {
        if(currentState != STATE_HOME)
        {
            currentState = STATE_HOME;

            moveServo(config.homeAngle);
        }
    }

    else if(ac && !lamp)
    {
        if(currentState != STATE_AC)
        {
            currentState = STATE_AC;

            moveServo(config.acAngle);
        }
    }

    else if(!ac && lamp)
    {
        if(currentState != STATE_LAMP)
        {
            currentState = STATE_LAMP;

            moveServo(config.lampAngle);
        }
    }

    else
    {
        if(currentState != STATE_AC_LAMP)
        {
            currentState = STATE_AC_LAMP;

            moveServo(config.acLampAngle);
        }
    }

}

//===========================================================
// SETUP
//===========================================================

void setup()
{

    Serial.begin(115200);

    prefs.begin("servo", false);

    config.homeAngle = prefs.getInt("home", 90);

    config.acAngle = prefs.getInt("ac", 120);

    config.lampAngle = prefs.getInt("lamp", 5);

    config.acLampAngle = prefs.getInt("aclamp", 125);

    config.enableIdle = true;

    config.acAngle = prefs.getInt("idle", 120);

    servo.setPeriodHertz(50);
    servo.attach(SERVO_PIN,500,2500);

    delay(300);

    moveServo(config.homeAngle);

    pinMode(AC_TRIGGER_PIN, INPUT_PULLUP);
    pinMode(LAMP_TRIGGER_PIN, INPUT_PULLUP);

    lastACTriggerState = digitalRead(AC_TRIGGER_PIN);
    lastLampTriggerState = digitalRead(LAMP_TRIGGER_PIN);

    WiFi.softAP(ssid,password);

    server.on("/",handleRoot);

    server.on("/status",handleStatus);

    server.on("/saveHome",handleSaveHome);

    server.on("/saveIdle",handleSaveIdle);

    server.on("/saveLamp",handleSaveLamp);

    server.on("/saveAcLamp",handleSaveAcLamp);

    server.begin();

    Serial.println();
    Serial.println("================================");
    Serial.println("ESP32 Idle Up Controller");
    Serial.println("AP Started");
    Serial.print("IP : ");
    Serial.println(WiFi.softAPIP());
    Serial.println("================================");

}

//===========================================================
// LOOP
//===========================================================

void loop()
{

    server.handleClient();

    if(config.enableIdle)
      {

        updateState();

      }

}