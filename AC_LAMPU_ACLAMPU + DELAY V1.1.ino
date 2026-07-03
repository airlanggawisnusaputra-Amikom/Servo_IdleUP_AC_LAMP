#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <Preferences.h>


//======================================================
// PIN CONFIGURATION
//======================================================

constexpr uint8_t PIN_SERVO = 25;

constexpr uint8_t PIN_AC = 21;

constexpr uint8_t PIN_LAMP = 19;



//======================================================
// WIFI
//======================================================

const char* AP_NAME = "Servo Idle Up";

const char* AP_PASSWORD = "12345678";



//======================================================
// OBJECT
//======================================================

Servo servo;

WebServer server(80);

Preferences prefs;



//======================================================
// SERVO MODE
//======================================================

enum ServoMode
{
    MODE_HOME = 0,

    MODE_AC,

    MODE_LAMP,

    MODE_AC_LAMP,

    MODE_COUNT
};



//======================================================
// CONFIG
//======================================================

struct Config
{
    int preset[MODE_COUNT];
};


//======================================================
// GLOBAL VARIABLE
//======================================================

Config config;

ServoMode currentMode = MODE_HOME;
// Mode hasil pembacaan GPIO saat ini
ServoMode targetMode = MODE_HOME;

int currentServoAngle = 90;

bool acTrigger = false;

bool lampTrigger = false;

unsigned long acOffTimer = 0;

bool acOffTimerRunning = false;

const unsigned long DEFAULT_AC_OFF_DELAY = 2000;

unsigned long acOffDelay = DEFAULT_AC_OFF_DELAY;



//======================================================
// MODE NAME
//======================================================

const char* modeName[MODE_COUNT] =
{
    "HOME",

    "AC",

    "LAMP",

    "AC + LAMP"
};



//======================================================
// MOVE SERVO
//======================================================

void moveServo(int angle)
{
    angle = constrain(angle,0,180);

    if(angle == currentServoAngle)
        return;

    servo.write(angle);

    currentServoAngle = angle;
}

//======================================================
// READ TARGET MODE
//======================================================

void updateTargetMode()
{
    acTrigger = (digitalRead(PIN_AC) == LOW);
    lampTrigger = (digitalRead(PIN_LAMP) == LOW);

    if (acTrigger && lampTrigger)
    {
        targetMode = MODE_AC_LAMP;
    }
    else if (acTrigger)
    {
        targetMode = MODE_AC;
    }
    else if (lampTrigger)
    {
        targetMode = MODE_LAMP;
    }
    else
    {
        targetMode = MODE_HOME;
    }
}

//======================================================
// LOAD CONFIG
//======================================================

void loadConfig()
{
    prefs.begin("servo", false);

    config.preset[MODE_HOME] =
        prefs.getInt("home",90);

    config.preset[MODE_AC] =
        prefs.getInt("ac",120);

    config.preset[MODE_LAMP] =
        prefs.getInt("lamp",110);

    config.preset[MODE_AC_LAMP] =
        prefs.getInt("aclamp",130);

    acOffDelay = prefs.getULong("acdelay", DEFAULT_AC_OFF_DELAY);
}



//======================================================
// SAVE CONFIG
//======================================================

void savePreset(ServoMode mode)
{
    switch(mode)
    {
        case MODE_HOME:

            prefs.putInt("home",
            config.preset[MODE_HOME]);

            break;

        case MODE_AC:

            prefs.putInt("ac",
            config.preset[MODE_AC]);

            break;

        case MODE_LAMP:

            prefs.putInt("lamp",
            config.preset[MODE_LAMP]);

            break;

        case MODE_AC_LAMP:

            prefs.putInt("aclamp",
            config.preset[MODE_AC_LAMP]);

            break;

        default:

            break;
    }
}

void saveAcOffDelay()
{
    prefs.putULong("acdelay", acOffDelay);
}

//======================================================
// WEB PAGE
//======================================================

const char webpage[] PROGMEM = R"rawliteral(

<!DOCTYPE html>

<html>

<head>

<meta charset="utf-8">

<meta
name="viewport"
content="width=device-width,initial-scale=1">
<title>Servo Idle Up Controller</title>
<style>

body{

font-family:Arial;

background:#f0f0f0;

margin:20px;

}

.card{

background:white;

padding:15px;

margin-bottom:15px;

border-radius:10px;

box-shadow:0 2px 5px rgba(0,0,0,.2);

}

input{

width:70px;

font-size:18px;

text-align:center;

}

button{

padding:8px 18px;

font-size:16px;

margin-left:10px;

}

</style>
</head>
<body>


<h2>

Servo Idle Up Controller

</h2>

<div class="card">

<h3>

Servo Position

</h3>

<div id="servoPos">

0°

</div>

</div>

<div class="card">

<h3>

Current Mode

</h3>

<div id="mode">

HOME

</div>

</div>

<div class="card">

<h3>

Trigger

</h3>

<div>

AC :

<span id="ac">

OFF

</span>

</div>

<div>

Lamp :

<span id="lamp">

OFF

</span>

</div>

</div>

<div class="card">

<h3>

HOME

</h3>

Current

<div id="homeCurrent">

90

</div>

New

<input

id="homeInput"

type="number"

min="0"

max="180">

<button

onclick="saveHome()">

SAVE

</button>

</div>

<div class="card">

<h3>

AC

</h3>

Current

<div id="acCurrent">

90

</div>

New

<input

id="acInput"

type="number"

min="0"

max="180">

<button

onclick="saveAC()">

SAVE

</button>

</div>

<div class="card">

<h3>

LAMPU

</h3>

Current

<div id="lampCurrent">

90

</div>

New

<input

id="lampInput"

type="number"

min="0"

max="180">

<button

onclick="saveLamp()">

SAVE

</button>

</div>


<div class="card">

<h3>

AC dan Lampu

</h3>

Current

<div id="acLampCurrent">

90

</div>

New

<input

id="acLampInput"

type="number"

min="0"

max="180">

<button

onclick="saveAcLamp()">

SAVE

</button>

</div>

<div class="card">

<h3>

AC OFF Delay

</h3>

Current

<div id="acDelayCurrent">

2000

</div>

New

<input

id="acDelayInput"

type="number"

min="0"

max="10000">

<button

onclick="saveAcDelay()">

SAVE

</button>

</div>

<script>

function updateStatus()
{
    fetch('/status')
    .then(r=>r.json())
    .then(data=>{

        document.getElementById("servoPos").innerHTML =
            data.servo + "°";

        document.getElementById("mode").innerHTML =
            data.mode;

        document.getElementById("ac").innerHTML =
            data.ac ? "ON" : "OFF";

        document.getElementById("lamp").innerHTML =
            data.lamp ? "ON" : "OFF";

        document.getElementById("homeCurrent").innerHTML =
            data.home;

        document.getElementById("acCurrent").innerHTML =
            data.acpos;

        document.getElementById("lampCurrent").innerHTML =
            data.lamppos;

        document.getElementById("acLampCurrent").innerHTML =
            data.aclamppos;
        
        document.getElementById("acDelayCurrent").innerHTML =
            data.acdelay + " ms";
        
        

    });
}

function saveHome()
{
    let value =
        document.getElementById("homeInput").value;

    fetch("/saveHome?value="+value)
    .then(()=>updateStatus());
}

function saveAC()
{
    let value =
        document.getElementById("acInput").value;

    fetch("/saveAC?value="+value)
    .then(()=>updateStatus());
}

function saveLamp()
{
    let value =
        document.getElementById("lampInput").value;

    fetch("/saveLamp?value="+value)
    .then(()=>updateStatus());
}

function saveAcLamp()
{
    let value =
        document.getElementById("acLampInput").value;

    fetch("/saveAcLamp?value="+value)
    .then(()=>updateStatus());
}

function saveAcDelay()
{
    let value = parseInt(document.getElementById("acDelayInput").value);

    if(isNaN(value))
    {
        alert("Masukkan nilai delay.");
        return;
    }

    if(value < 0)
        value = 0;

    if(value > 10000)
        value = 10000;

    fetch("/saveAcDelay?value=" + value)
    .then(() =>
    {
        updateStatus();
    });
}

setInterval(updateStatus,500);

window.onload=updateStatus;

</script>

</body>

)rawliteral";

//======================================================
// WEB HANDLER
//======================================================

void handleRoot()
{
    server.send_P(200, "text/html", webpage);
}

void handleStatus()
{
    String json="{";

    json+="\"servo\":"+String(currentServoAngle)+",";
    json += "\"acdelay\":" + String(acOffDelay) + ",";

    json+="\"mode\":\"";
    json+=modeName[currentMode];
    json+="\",";

    json+="\"ac\":";
    json+=acTrigger?"true":"false";
    json+=",";

    json+="\"lamp\":";
    json+=lampTrigger?"true":"false";
    json+=",";

    json+="\"home\":";
    json+=String(config.preset[MODE_HOME]);
    json+=",";

    json+="\"acpos\":";
    json+=String(config.preset[MODE_AC]);
    json+=",";

    json+="\"lamppos\":";
    json+=String(config.preset[MODE_LAMP]);
    json+=",";

    json+="\"aclamppos\":";
    json+=String(config.preset[MODE_AC_LAMP]);

    json+="}";

    server.send(200,"application/json",json);
}

void handleSaveHome()
{
    if(!server.hasArg("value"))
    {
        server.send(400,"text/plain","NO DATA");
        return;
    }

    config.preset[MODE_HOME]=constrain(server.arg("value").toInt(),0,180);

    savePreset(MODE_HOME);

    server.send(200,"text/plain","OK");
}


void handleSaveAC()
{
    if(!server.hasArg("value"))
    {
        server.send(400,"text/plain","NO DATA");
        return;
    }

    config.preset[MODE_AC]=constrain(server.arg("value").toInt(),0,180);

    savePreset(MODE_AC);

    server.send(200,"text/plain","OK");
}


void handleSaveLamp()
{
    if(!server.hasArg("value"))
    {
        server.send(400,"text/plain","NO DATA");
        return;
    }

    config.preset[MODE_LAMP]=constrain(server.arg("value").toInt(),0,180);

    savePreset(MODE_LAMP);

    server.send(200,"text/plain","OK");
}

void handleSaveAcLamp()
{
    if(!server.hasArg("value"))
    {
        server.send(400,"text/plain","NO DATA");
        return;
    }

    config.preset[MODE_AC_LAMP]=constrain(server.arg("value").toInt(),0,180);

    savePreset(MODE_AC_LAMP);

    server.send(200,"text/plain","OK");
}

//======================================================
// SAVE AC OFF DELAY
//======================================================

void handleSaveAcDelay()
{
    if(server.hasArg("value"))
    {
        unsigned long d = server.arg("value").toInt();

        // Batas aman
        if(d > 10000)
            d = 10000;

        acOffDelay = d;

        saveAcOffDelay();

        server.send(200, "text/plain", "OK");
    }
    else
    {
        server.send(400, "text/plain", "NO VALUE");
    }
}

//======================================================
// UPDATE MODE
//======================================================

void updateMode()
{
    //==================================================
    // Kalau target bukan HOME
    //==================================================

    if(targetMode != MODE_HOME)
    {
        // Batalkan timer jika sedang berjalan
        acOffTimerRunning = false;

        // Langsung pindah mode jika berbeda
        if(currentMode != targetMode)
        {
            currentMode = targetMode;
        }

        return;
    }

    //==================================================
// Target = HOME
//==================================================

// Kalau berasal dari LAMP, langsung HOME
if(currentMode == MODE_LAMP)
{
    currentMode = MODE_HOME;
    acOffTimerRunning = false;
    return;
}

// Kalau memang sudah HOME
if(currentMode == MODE_HOME)
{
    acOffTimerRunning = false;
    return;
}

// Selain itu (AC atau AC+LAMP) pakai delay
if(!acOffTimerRunning)
{
    acOffTimerRunning = true;
    acOffTimer = millis();
    return;
}

if(millis() - acOffTimer >= acOffDelay)
{
    currentMode = MODE_HOME;
    acOffTimerRunning = false;
}
}

//======================================================
// DEBUG MODE
//======================================================

void debugMode()
{
    static ServoMode lastMode = MODE_HOME;

    if(lastMode != currentMode)
    {
        Serial.print("MODE -> ");
        Serial.println(modeName[currentMode]);

        lastMode = currentMode;
    }
}

//======================================================
// UPDATE SERVO
//======================================================

void updateServo()
{
    moveServo(config.preset[currentMode]);
}

//======================================================
// SETUP
//======================================================

void setup()
{
    Serial.begin(115200);

    pinMode(PIN_AC, INPUT_PULLUP);
    pinMode(PIN_LAMP, INPUT_PULLUP);

    servo.setPeriodHertz(50);
    servo.attach(PIN_SERVO);

    loadConfig();

    moveServo(config.preset[MODE_HOME]);

    WiFi.mode(WIFI_AP);

    WiFi.softAP(AP_NAME, AP_PASSWORD);

    server.on("/", handleRoot);

    server.on("/status", handleStatus);

    server.on("/saveHome", handleSaveHome);

    server.on("/saveAC", handleSaveAC);

    server.on("/saveLamp", handleSaveLamp);

    server.on("/saveAcLamp", handleSaveAcLamp);

    server.on("/saveAcDelay", handleSaveAcDelay);

    server.begin();

    Serial.println();

    Serial.print("IP : ");

    Serial.println(WiFi.softAPIP());
}

//======================================================
// LOOP
//======================================================

void loop()
{
    server.handleClient();

updateTargetMode();

updateMode();

updateServo();

debugMode();
}
