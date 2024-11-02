#include"leader.h"
#include<WebServer.h>

const char* ssid = "SMR_LAB";
const char* password = "smrl1991";

LeaderElection Leader;
WebServer server(80);

void getNodes() {
  std::set<String> nodes = Leader.allNodes();
  DynamicJsonDocument doc(1024);
  JsonArray nodesArray = doc.to<JsonArray>();

  for (const auto& node : nodes) {
    nodesArray.add(node);
  }

  String jsonString;
  serializeJson(doc, jsonString);

  return server.send(200, "application/json", jsonString);
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/nodes",HTTP_GET,getNodes);

  if(Leader.load()){
    Serial.println("Loaded successfully.");
  }

  server.begin();
}

void loop() {
  // put your main code here, to run repeatedly:
  server.handleClient();
  Leader.run();
}
