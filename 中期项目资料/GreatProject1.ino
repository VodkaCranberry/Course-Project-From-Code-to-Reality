/**
 * 项目：复古光学摩斯密码机 V5.0 (按键加速版)
 * 逻辑：
 * 1. 按键模式：短按 = 点(.), 长按(>2s) = 划(-)
 * 2. 光感模式：2-4s = 点(.), >4s = 划(-)
 */

#include <WiFi.h>
#include <WebServer.h>

const int LDR_PIN = 4;       
const int BUTTON_PIN = 6;    
const int LED_PIN = 15;      

const char *ssid = "Imaginal Disk";
const char *password = "20060426";

// --- 判定参数 (毫秒) ---
const int LIGHT_THRESHOLD = 7500;  
const unsigned long BTN_DASH_TIME = 2000; // 按键模式下，2秒就变划
const unsigned long LDR_DOT_MIN = 2000;   // 光感模式下，2秒才算点
const unsigned long LDR_DASH_MIN = 4000;  // 光感模式下，4秒才算划

WebServer server(80);
String morseBuffer = "";    
String decodedMessage = ""; 
unsigned long startTime = 0;
bool isActive = false;      
bool buttonMode = false;    

char decodeMorse(String s) {
  if (s == ".-") return 'A';   if (s == "-...") return 'B'; if (s == "-.-.") return 'C';
  if (s == "-..") return 'D';  if (s == ".") return 'E';    if (s == "..-.") return 'F';
  if (s == "--.") return 'G';  if (s == "....") return 'H'; if (s == "..") return 'I';
  if (s == ".---") return 'J'; if (s == "-.-") return 'K';  if (s == ".-..") return 'L';
  if (s == "--") return 'M';   if (s == "-.") return 'N';   if (s == "---") return 'O';
  if (s == ".--.") return 'P'; if (s == "--.-") return 'Q'; if (s == ".-.") return 'R';
  if (s == "...") return 'S';  if (s == "-") return 'T';    if (s == "..-") return 'U';
  if (s == "...-") return 'V'; if (s == ".--") return 'W';  if (s == "-..-") return 'X';
  if (s == "-.--") return 'Y'; if (s == "--..") return 'Z';
  return '?';
}

const char* HTML_BODY = R"rawliteral(
<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>
<style>
  body { background: #000; color: #0f0; font-family: monospace; text-align: center; padding: 20px; }
  .box { border: 2px solid #0f0; padding: 20px; margin: 20px auto; max-width: 400px; background: #111; }
  .btn { background: #0f0; color: #000; padding: 15px; border: none; margin: 10px; cursor: pointer; font-size: 18px; font-weight: bold; width: 85%; }
  .input-bar { font-size: 40px; color: #fff; letter-spacing: 10px; min-height: 50px; margin: 20px 0; }
  .result-bar { font-size: 60px; font-weight: bold; color: #0f0; }
  .info { color: #888; font-size: 12px; line-height: 1.5; }
</style></head>
<body>
  <h1>MORSE 控制台 V5.0</h1>
  <div style='color:#ff0'>模式: <span id='m'>光敏电阻</span></div>
  <div class='box'><div id='buf' class='input-bar'></div><div id='res' class='result-bar'></div></div>
  <button class='btn' onclick="f('/decode')">破译当前字母并清空</button>
  <button class='btn' onclick="f('/toggle')">切换输入模式</button>
  <button class='btn' style='background:#555' onclick="f('/clear')">全部重置</button>
  <div class='info'>
    按键模式：按下即触发，>2s为划<br>
    光感模式：2-4s为点，>4s为划
  </div>
<script>
  function f(u){ fetch(u); }
  setInterval(() => {
    fetch('/data').then(r=>r.json()).then(d=>{
      document.getElementById('buf').innerText = d.b;
      document.getElementById('res').innerText = d.d;
      document.getElementById('m').innerText = d.m;
    });
  }, 500);
</script></body></html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  pinMode(LDR_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nIP: " + WiFi.localIP().toString());

  server.on("/", [](){ server.send(200, "text/html", HTML_BODY); });
  server.on("/data", [](){
    String json = "{\"b\":\"" + morseBuffer + "\", \"d\":\"" + decodedMessage + "\", \"m\":\"" + (buttonMode?"实体按键":"光敏电阻") + "\"}";
    server.send(200, "application/json", json);
  });
  server.on("/decode", [](){
    if(morseBuffer.length() > 0){ decodedMessage += decodeMorse(morseBuffer); morseBuffer = ""; }
    server.send(200, "text/plain", "OK");
  });
  server.on("/clear", [](){ morseBuffer = ""; decodedMessage = ""; server.send(200, "text/plain", "OK"); });
  server.on("/toggle", [](){ buttonMode = !buttonMode; morseBuffer = ""; server.send(200, "text/plain", "OK"); });
  server.begin();
}

void loop() {
  server.handleClient();
  bool triggered = buttonMode ? (digitalRead(BUTTON_PIN) == LOW) : (analogRead(LDR_PIN) >= LIGHT_THRESHOLD);

  if (triggered && !isActive) {
    startTime = millis();
    isActive = true;
    digitalWrite(LED_PIN, HIGH);
  }

  if (!triggered && isActive) {
    unsigned long dur = millis() - startTime;
    isActive = false;
    digitalWrite(LED_PIN, LOW);

    // --- 核心改动：分支判定逻辑 ---
    if (buttonMode) {
      // 1. 实体按键逻辑：快，且不设下限
      if (dur >= BTN_DASH_TIME) {
        morseBuffer += "-"; // >2s
      } else if (dur > 50) {
        morseBuffer += "."; // 消抖后只要小于2s都是点
      }
    } else {
      // 2. 光感逻辑：严谨，有 2s 过滤阈值
      if (dur >= LDR_DASH_MIN) {
        morseBuffer += "-"; // >4s
      } else if (dur >= LDR_DOT_MIN) {
        morseBuffer += "."; // 2s-4s
      }
    }
    Serial.println("当前缓冲区: " + morseBuffer);
  }
}