#include<WiFi.h>
#include<WiFiUdp.h>
#include<Arduino.h>
#include<ArduinoJson.h>
// #include<esp_pthread.h>
#include<WebServer.h>

#include<thread>
#include<string>
#include<set>
#include<mutex>

#define heartbeatInterval 250  //Hearbeat interval 250ms

// const char* ssid = "Me";
// const char* password = "Brow1220";
// const char* broadcast_address = "192.168.95.255";

String leaderId;
const char* ssid = "SMR_LAB";
const char* password = "smrl1991";
const char* broadcast_address = "10.5.20.255";
const unsigned int port = 10000;

IPAddress local_IP(10, 5, 20, 236);  // Replace with your desired IP address
IPAddress gateway(10, 5, 20, 2);     // Replace with your network gateway
IPAddress subnet(255, 255, 255, 0);    // Subnet mask

// enum NodeState { "Follower", "Candidate", "Leader" };

std::set<String> nodes;
std::mutex mtx;

String nodeId = "";  //It contains the ip address of the current node as its id
int currentTerm = 0;
String votedFor = "";  //Storing the ip of the node it voted for, empty means none.
std::string state = "Follower";

unsigned long electionTimeout;
unsigned long lastElectionTime;
unsigned long lastHeartbeatTime;

WiFiUDP udp;
WebServer server(80);

int voteCount = 0;
int totalNodes = 1;  //Number of total devices I'm using

//To be used by leader, not using now
void leader_job() {
  while (true) {
    if (state == "Leader") {
      sendHeartbeat();
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
    continue;
  }
}

void udp_receive() {
  while (true) {
    // Serial.println("Inside udp_receive");
    int packetSize = udp.parsePacket();
    if (packetSize) {
      char packetBuffer[255];
      int len = udp.read(packetBuffer, 255);
      if (len > 0) {
        packetBuffer[len] = '\0';
        Serial.print("Inside udp_receive- ");
        Serial.println(packetBuffer);
      }
      processMessage(String(packetBuffer));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

inline void becomeCandidate() {
  {
    std::lock_guard<std::mutex> lock(mtx);
    state = "Candidate";
    currentTerm++;
    votedFor = nodeId;
    voteCount = 1;
    resetElectionTimeout();
    lastElectionTime = millis();
  }
  sendRequestVote();
}

inline void sendRequestVote() {
  String message = "VoteRequest " + nodeId + " " + String(currentTerm);
  udp.beginPacket(broadcast_address, port);
  udp.print(message);
  if (udp.endPacket()) {
    Serial.print("Sent- ");
    Serial.println(message.c_str());
  }
}

void sendHeartbeat() {
  Serial.println("Inside sendHeartbeat function");
  if (state == "Leader") {
    String message = "Heartbeat " + nodeId + " " + String(currentTerm);
    udp.beginPacket(broadcast_address, port);
    udp.print(message);
    if (udp.endPacket()) {
      std::lock_guard<std::mutex> lock(mtx);
      lastHeartbeatTime = millis();
      Serial.println("Sent Heartbeat");
      Serial.println(message.c_str());
    }
  }
}

inline void processMessage(String msg) {
  Serial.print("Received msg- ");
  Serial.println(msg.c_str());
  if (msg.startsWith("VoteRequest")) {
    String senderId = msg.substring(12, msg.indexOf(' ', 12));
    int senderTerm = msg.substring(msg.indexOf(' ', 12) + 1).toInt();
    if (senderTerm > currentTerm) {
      std::lock_guard<std::mutex> lock(mtx);
      currentTerm = senderTerm;
      votedFor = "";
      state = "Follower";
    }
    if (votedFor.isEmpty()) {
      {
        std::lock_guard<std::mutex> lock(mtx);
        votedFor = senderId;
      }
      String response = "VoteResponse " + nodeId + " 1";
      udp.beginPacket(senderId.c_str(), port);
      udp.print(response);
      if (udp.endPacket()) {
        Serial.print("sending voteresponse- ");
        Serial.println(response.c_str());
        resetElectionTimeout();
        std::lock_guard<std::mutex> lock(mtx);
        lastHeartbeatTime = millis();
      }
    }
  } else if (msg.startsWith("VoteResponse")) {
    Serial.print("Received voteresponse- ");
    Serial.println(msg.c_str());
    String voterId = msg.substring(13, msg.indexOf(' ', 13));
    int voteResponse = msg.substring(msg.indexOf(' ', 13) + 1).toInt();
    if (voteResponse == 1 && state == "Candidate") {
      Serial.printf("Voteresponse is 1.\n");
      voteCount++;
      Serial.printf("Total votecount- %d\n", voteCount);
      Serial.printf("Total nodes- %d\n", totalNodes);
      if (voteCount > (totalNodes / 2)) {
        Serial.printf("Inside leader transition if\n");
        {
          std::lock_guard<std::mutex> lock(mtx);
          state = "Leader";
          leaderId = nodeId;
        }
        String message = "Heartbeat " + nodeId + " " + String(currentTerm);
        udp.beginPacket(broadcast_address, port);
        udp.print(message);
        if (udp.endPacket()) {
          std::lock_guard<std::mutex> lock(mtx);
          lastHeartbeatTime = millis();
          Serial.println("Sent Heartbeat");
          Serial.println(message.c_str());
        }
        Serial.print("Node ");
        Serial.print(nodeId.c_str());
        Serial.println(" became the leader");
      }
    }
  } else if (msg.startsWith("Heartbeat")) {
    String temp_leaderId = msg.substring(10, msg.indexOf(' ', 10));
    int recv_term = msg.substring(msg.indexOf(' ', 10) + 1).toInt();
    if (recv_term >= currentTerm) {
      Serial.println("Recv term is greater, inside if- ");
      Serial.println(recv_term);
      std::lock_guard<std::mutex> lock(mtx);
      currentTerm = recv_term;
      leaderId = temp_leaderId;
      resetElectionTimeout();
      lastHeartbeatTime = millis();
      state = "Follower";
    }
    Serial.print("Received Hearbeat from leader- ");
    Serial.println(temp_leaderId);
    Serial.println(temp_leaderId.c_str());
    Serial.print("Term received = ");
    Serial.println(recv_term);
    Serial.print("My current term = ");
    Serial.println(currentTerm);
  } else if (msg.startsWith("Joined")) {
    Serial.println("Received joined msg- ");
    Serial.println(msg.c_str());
    String newNode = msg.substring(7);
    String ackMsg = "ACK " + nodeId;
    udp.beginPacket(newNode.c_str(), port);
    udp.print(ackMsg);
    if (udp.endPacket()) {
      Serial.print("sent- ");
      Serial.println(ackMsg.c_str());
    }
    nodes.insert(newNode);
    totalNodes = nodes.size();
  } else if (msg.startsWith("ACK")) {
    Serial.println("Received ACK");
    Serial.println(msg.c_str());
    String tempNode = msg.substring(4);
    nodes.insert(tempNode);
    totalNodes = nodes.size();
  }
}

inline void resetElectionTimeout() {
  electionTimeout = random(3000, 5000);
}

void getNodes() {
  DynamicJsonDocument doc(1024);
  JsonArray nodesArray = doc.to<JsonArray>();

  for (const auto& node : nodes) {
    nodesArray.add(node);
  }

  String jsonString;
  serializeJson(doc, jsonString);

  return server.send(200, "application/json", jsonString);
}

void getLeader() {
  return server.send(200, "application/json", leaderId);
}

void setup() {
  Serial.begin(115200);
  Serial.print("Connecting to ");
  Serial.println(ssid);

  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("STA Failed to configure");
  }

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  nodeId = WiFi.localIP().toString();
  nodes.insert(nodeId);
  String brd_msg = "Joined " + nodeId;

  server.on("/nodes", HTTP_GET, getNodes);
  server.on("/leader", HTTP_GET, getLeader);

  udp.begin(port);

  udp.beginPacket(broadcast_address, port);
  udp.print(brd_msg);
  if (udp.endPacket()) {
    Serial.println(brd_msg.c_str());
  }

  resetElectionTimeout();
  lastHeartbeatTime = millis();

  Serial.print("Election time - ");
  Serial.println(electionTimeout);

  Serial.print("heartbeat time - ");
  Serial.println(lastHeartbeatTime);

  // auto cfg = esp_pthread_get_default_config();

  // cfg.stack_size	= 4 * 1024;
  // cfg.inherit_cfg = true;
  // cfg.prio		= 1;

  // esp_pthread_set_cfg( &cfg );
  std::thread{ udp_receive }.detach();

  server.begin();
}

void loop() {
  server.handleClient();
  unsigned long now = millis();
  if (state == "Follower" && now - lastHeartbeatTime > electionTimeout) {
    //If the time limit reached for heartbeattime, then the "Follower" node becomes "Candidate" and starts ""Leader"" election procedure
    Serial.println("Follwer Changing state to candidate");
    becomeCandidate();
  }
  if (state == "Candidate" && now - lastElectionTime > electionTimeout) {
    Serial.println("Candidate again sending vote request");
    sendRequestVote();
    std::lock_guard<std::mutex> lock(mtx);
    resetElectionTimeout();
    lastElectionTime = millis();
  }

  if (state == "Leader" && now - lastHeartbeatTime > heartbeatInterval) {
    sendHeartbeat();
    lastHeartbeatTime = millis();
  }
}
