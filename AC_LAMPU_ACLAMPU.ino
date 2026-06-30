/*
===========================================================
 ESP32 IDLE UP CONTROLLER V3
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
const uint8_t TRIGGER_PIN = 21;

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
    int idleAngle;

    bool enableIdle;

    int triggerPin;

};

ServoConfig config;

//===========================================================
// STATUS
//===========================================================

enum ServoState
{
    STATE_HOME,
    STATE_IDLE
};

ServoState currentState = STATE_HOME;

int currentAngle = 90;

bool lastTriggerState = HIGH;

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

<h3>Servo Position</h3>

<div class="value" id="servoPos">0°</div>

</div>

<div class="card">

<h3>Home Position</h3>

Current

<div class="value" id="homePos">0°</div>

New

<br><br>

<input id="homeInput" type="number">

<br>

<button onclick="saveHome()">SAVE HOME</button>

</div>

<div class="card">

<h3>Idle Position</h3>

Current

<div class="value" id="idlePos">0°</div>

New

<br><br>

<input id="idleInput" type="number">

<br>

<button onclick="saveIdle()">SAVE IDLE</button>

</div>

<script>

function updateStatus(){

fetch("/status")

.then(r=>r.json())

.then(data=>{

document.getElementById("servoPos").innerHTML=data.current+"°";

document.getElementById("homePos").innerHTML=data.home+"°";

document.getElementById("idlePos").innerHTML=data.idle+"°";

let t=document.getElementById("triggerStatus");
t.innerHTML=data.trigger;
if(data.trigger=="LOW"){
 t.className="value green";
}else{
 t.className="value red";
}

});

}

function saveHome(){

let a=parseInt(document.getElementById("homeInput").value);

if(isNaN(a)) return;

if(a<0) a=0;

if(a>180) a=180;

fetch("/saveHome?angle="+a)

.then(()=>updateStatus());

}

function saveIdle(){

let a=document.getElementById("idleInput").value;

fetch("/saveIdle?angle="+a)

.then(()=>updateStatus());

}

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

    String json="{";

    json += "\"enable\":";

    json += config.enableIdle ? "true" : "false";
    json+=",";
    json+="\"current\":"+String(currentAngle)+",";

    json+="\"home\":"+String(config.homeAngle)+",";

    json+="\"idle\":"+String(config.idleAngle)+",";

    json+="\"trigger\":\"";

    json+=(digitalRead(TRIGGER_PIN)==LOW) ? "LOW" : "HIGH";

    json+="\"";

    json+="}";

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

        config.idleAngle=a;

        prefs.putInt("idle",a);

        if(currentState==STATE_IDLE)
{
    moveServo(config.idleAngle);
}

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
    bool now = digitalRead(TRIGGER_PIN);

    if(now != lastRead)
    {
        triggerTimer = millis();
        lastRead = now;
    }

    if(millis() - triggerTimer > 30)
    {
        triggerStable = now;
    }

    if(triggerStable == LOW)
    {
        if(currentState != STATE_IDLE)
        {
            currentState = STATE_IDLE;
            Serial.println("STATE -> IDLE");
            moveServo(config.idleAngle);
        }
    }
    else
    {
        if(currentState != STATE_HOME)
        {
            currentState = STATE_HOME;
            Serial.println("STATE -> HOME");
            moveServo(config.homeAngle);
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

    config.enableIdle = true;

    config.triggerPin = TRIGGER_PIN;

    config.idleAngle = prefs.getInt("idle", 120);

    servo.setPeriodHertz(50);
    servo.attach(SERVO_PIN,500,2500);

    delay(300);

    moveServo(config.homeAngle);

    pinMode(TRIGGER_PIN, INPUT_PULLUP);

    lastTriggerState = digitalRead(TRIGGER_PIN);

    WiFi.softAP(ssid,password);

    server.on("/",handleRoot);

    server.on("/status",handleStatus);

    server.on("/saveHome",handleSaveHome);

    server.on("/saveIdle",handleSaveIdle);

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
