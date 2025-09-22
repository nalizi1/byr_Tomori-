#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

const char* AP_SSID = "起名困难灯";
const char* AP_PASS = "nalizi11";
ESP8266WebServer server(80);

IPAddress AP_IP(192, 168, 4, 1);
IPAddress AP_GW(192, 168, 4, 1);
IPAddress AP_MASK(255, 255, 255, 0);

// =============== 网页 ===============
const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html><html lang="zh-CN"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP8266 起名困难灯</title>
<style>
 body{font-family:sans-serif;margin:20px}
 textarea{width:100%;height:80px}
 button{margin-top:10px;padding:8px 14px}
 pre{background:#f5f5f5;padding:10px;max-height:300px;overflow:auto}
 .ok{color:#0a0}.err{color:#c00}
</style>
</head><body>
<h2>ESP8266 串口发送</h2>
<textarea id="msg" placeholder="输入命令..."></textarea><br>
<button onclick="send()">发送</button>
<span id="status"></span>

<h3>串口回传</h3>
<pre id="log"></pre>

<script>
function send(){
  let msg=document.getElementById("msg").value;
  if(!msg.trim()){return;}
  fetch("/send",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:"msg="+encodeURIComponent(msg)})
  .then(r=>r.json()).then(j=>{
    document.getElementById("status").innerText=j.ok?"已发送":"失败";
    document.getElementById("status").className = j.ok ? "ok" : "err";
  });
}

// 建立SSE监听串口回传（每行期望是 JSON）
let log=document.getElementById("log");
let evtSource=new EventSource("/events");
evtSource.onmessage=function(e){
  try{
    const obj = JSON.parse(e.data);
    log.textContent = JSON.stringify(obj, null, 2) + "\n" + log.textContent;
  }catch(_){
    // 不是 JSON 时就原样显示
    log.textContent = e.data + "\n" + log.textContent;
  }
};
</script>
</body></html>
)HTML";

// =============== SSE 事件源 ===============
WiFiClient sseClient;
bool clientConnected = false;
// SSE keepalive
unsigned long sseLastKeepalive = 0;
const unsigned long SSE_KEEPALIVE_MS = 15000; // 15 seconds

// CORS 辅助
void addCORS() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void handleRoot() {
  addCORS();
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void handleSend() {
  addCORS();
  if (!server.hasArg("msg")) {
    server.send(400, "application/json", "{\"ok\":false}");
    return;
  }
  String msg = server.arg("msg");
  Serial.println(msg);
  server.send(200, "application/json", "{\"ok\":true,\"len\":" + String(msg.length()) + "}");
}

// 事件流：浏览器访问 /events 就保持长连接
void handleEvents() {
  WiFiClient client = server.client();
  if (client.connected()) {
    sseClient = client;
    clientConnected = true;

    Serial.println("set_mode on");  //连接指令

    // 记录连接时间，用于 keepalive
    sseLastKeepalive = millis();

    // 设置HTTP头
    client.print("HTTP/1.1 200 OK\r\n");
    client.print("Content-Type: text/event-stream\r\n");
    client.print("Cache-Control: no-cache\r\n");
    client.print("Connection: keep-alive\r\n\r\n");

    // 建议：设置自动重连间隔
    sseClient.print("retry: 5000\n\n");

    // 可选：告知前端已连接
    sseClient.print("data: {\"evt\":\"connected\"}\n\n");
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_AP);                         // 仅 AP；如需有效 RSSI，可改为 WIFI_AP_STA 并连接路由
  WiFi.softAPConfig(AP_IP, AP_GW, AP_MASK);
  WiFi.softAP(AP_SSID, AP_PASS);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/send", HTTP_POST, handleSend);
  server.on("/events", HTTP_GET, handleEvents);
  server.begin();
}

void loop() {
  server.handleClient();

  // 检查串口是否有数据 → 转发给网页
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');     // 期望下位机每条 JSON 以 \n 结尾
    if (line.endsWith("\r")) line.remove(line.length()-1);
    line.trim();

    if (sseClient && sseClient.connected()) {
      String out;

      // 如果是 JSON 对象，直接转发
      if (line.length() && line[0] == '{' && line.endsWith("}")) {
        int last = line.lastIndexOf('}');
        // 直接将原始 JSON 输出（保留原样）
        out = line;
      } else {
        // 非 JSON：包装为一个 JSON
        out.reserve(line.length() + 40);
        out = "{\"evt\":\"raw\",\"data\":\"";
        // 简单转义双引号和反斜杠
        for (unsigned int i=0;i<line.length();++i){
          char c=line[i];
          if (c=='\\' || c=='\"') out += '\\';
          if (c=='\r' || c=='\n') continue;
          out += c;
        }
        out += "\"}";
      }

      sseClient.print("data: ");
      sseClient.print(out);
      sseClient.print("\n\n");
    }
  }

  // 发送 SSE 注释行心跳，避免代理/NAT 在空闲时关闭连接
  if (clientConnected && sseClient && sseClient.connected()) {
    if (millis() - sseLastKeepalive >= SSE_KEEPALIVE_MS) {
      sseClient.print(": ping\n\n");
      sseLastKeepalive = millis();
    }
  }

  // 检查 SSE 客户端是否断开
  if (clientConnected && (!sseClient.connected())) {
    clientConnected = false;
    Serial.println("set_mode breathe 1500");
  }
}
