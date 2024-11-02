#include<Arduino.h>
#include<ArduinoJson.h>
#include<AsyncUDP.h>
#include<WiFi.h>
#include<string>
#include<set>
#include<mutex>
#include<vector>
#include<WebServer.h>

String leaderId;
const char* ssid = "SMR_LAB";
const char* password = "smrl1991";
const unsigned int port = 10000;
const char* broadcast_address = "10.5.20.255";

AsyncUDP udp;
std::set<String> nodes;
std::mutex mtx;

enum NodeState { FOLLOWER, CANDIDATE, LEADER };

String nodeId = ""; //It contains the ip address of the current node as its id
int currentTerm = 0;
String votedFor = "";//Storing the ip of the node it voted for, empty means none.
NodeState state = FOLLOWER;
int voteCount = 0; //Stores the votecount received when in candidate state and sent votereqeust
int totalNodes = 1;//Storing the number of active nodes, initially one for self

unsigned long electionTimeout;
unsigned long lastElectionTime;
unsigned long lastHeartbeatTime;

WebServer server(80);

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
  nodeId = WiFi.localIP().toString();
  nodes.insert(nodeId);
  String joinedMsg = "Joined " + nodeId;
  std::vector<uint8_t> tmpVector(joinedMsg.begin(),joinedMsg.end());
  uint8_t *p = &tmpVector[0];
  AsyncUDPMessage tmpMsg;
  tmpMsg.write(p,joinedMsg.length());
  // udp.sendTo(tmpMsg,join(broadcast_address),port);
  // udp.broadcast(joinedMsg.c_str());
  if(udp.listen(port)){
    udp.onPacket(processUdpPacket);
  }  
  udp.broadcastTo(joinedMsg.c_str(),port);
  Serial.println(joinedMsg.c_str());
  server.on("/nodes",HTTP_GET,getNodes);
  resetElectionTimeout();
  lastHeartbeatTime = millis();
  server.begin();
}


inline IPAddress stringToIP(String str) {
  IPAddress ip;
  ip.fromString(str);
  return ip;
}

void processUdpPacket(AsyncUDPPacket packet){
  size_t len = packet.length();
  const uint8_t* receivedData = packet.data();
  processMsg(String((char *)receivedData));
  // String msg = new char[len + 1];
  // memcpy(msg, receivedData, len);
  // std::string message(receivedData, len);
}

void sendHeartbeat(){
    String message = "Heartbeat " + nodeId + " " + String(currentTerm) + "\0";
    std::vector<uint8_t> myVector(message.begin(), message.end());
    uint8_t *p = &myVector[0];
    AsyncUDPMessage hbMsg;
    hbMsg.write(p,message.length());
    // udp.beginPacket(broadcast_address, port);
    // udp.print(message);
    // if(udp.endPacket()){
    //   std:std::lock_guard<std::mutex> lock(mtx);
    //   lastHeartbeatTime = millis();
    //   Serial.println("Sent Heartbeat");
    //   Serial.println(message.c_str());
    // }
}

void processMsg(String msg){
  Serial.print("Received msg- ");
  Serial.println(msg.c_str());
  std:std::lock_guard<std::mutex> lock(mtx);
  if (msg.startsWith("VoteRequest")) {
      String senderId = msg.substring(12, msg.indexOf(' ', 12)); 
      int senderTerm = msg.substring(msg.indexOf(' ', 12) + 1).toInt();
      if (senderTerm > currentTerm) {
          currentTerm = senderTerm;
          votedFor = "";
          state = FOLLOWER;
      }
      if (votedFor.isEmpty()) {
          votedFor = senderId;
          String response = "VoteResponse " + nodeId + " 1\0";
          Serial.print("sending voteresponse- ");
          Serial.println(response.c_str());
          // udp.beginPacket(senderId.c_str(), port);
          // udp.print(response);
          // udp.endPacket();
        // AsyncUDPPacket ackPacket;
        // ackPacket.write(response);
        // udp.sendTo(packet, broadcastIp, 1234);
        std::vector<uint8_t> myVector(response.begin(), response.end());
        uint8_t *p = &myVector[0];
        AsyncUDPMessage respMsg;
        respMsg.write(p,response.length());
        udp.sendTo(respMsg,stringToIP(senderId), port);
      }
  } else if (msg.startsWith("VoteResponse")) {
      Serial.print("Received voteresponse- ");
      Serial.println(msg.c_str());
      String voterId = msg.substring(13, msg.indexOf(' ', 13));
      int voteResponse = msg.substring(msg.indexOf(' ', 13) + 1).toInt();
      if (voteResponse == 1) {
          voteCount++;
          if (voteCount > (totalNodes / 2)) {
                  state = LEADER;
                  sendHeartbeat();
                  Serial.print("Node ");
                  Serial.print(nodeId.c_str());
                  Serial.println(" became the leader");
          }
      }
  } else if (msg.startsWith("Heartbeat")) {
      // int leaderId = msg.substring(10).toInt();
      String temp_leaderId = msg.substring(10,msg.indexOf(' ',10));
      int recv_term = msg.substring(msg.indexOf(10,msg.indexOf(' '))+1).toInt();
      if(recv_term >= currentTerm){
        currentTerm = recv_term;
        leaderId = temp_leaderId;
        lastHeartbeatTime = millis();
        state = FOLLOWER;
      }
      Serial.print("Received Hearbeat from leader- ");
      Serial.println(temp_leaderId);
      Serial.println(temp_leaderId.c_str());
      Serial.print("Term received = ");
      Serial.println(recv_term);
      Serial.print("My current term = ");
      Serial.println(currentTerm);
  }
  else if (msg.startsWith("Joined")){
    Serial.println("");
    Serial.println(msg.c_str());
    String newNode = msg.substring(7);
    String response = "ACK " + nodeId;
    std::vector<uint8_t> respVector(response.begin(),response.end());
    uint8_t* p = &respVector[0];    
    Serial.println(response.c_str());
    AsyncUDPMessage respMsg;
    respMsg.write(p,response.length());
    udp.sendTo(respMsg,stringToIP(newNode), port);
    // udp.beginPacket(newNode.c_str(),port);
    // udp.print(ackMsg);
    // udp.endPacket();
    nodes.insert(newNode);
    totalNodes = nodes.size()+1;
  }
  else if (msg.startsWith("ACK")){
    Serial.println("");
    Serial.println(msg.c_str());
    String tempNode = msg.substring(4);
    nodes.insert(tempNode);    
  }
}

void getNodes(){
    std::lock_guard<std::mutex> lock(mtx);
    DynamicJsonDocument doc(1024);
    JsonArray nodesArray = doc.to<JsonArray>();

    for (const auto& node : nodes) {
        nodesArray.add(node);
    }

    String jsonString;
    serializeJson(doc, jsonString);

    return server.send(200, "application/json", jsonString);
}

inline void resetElectionTimeout() {
    electionTimeout = random(1000, 2000);
}

void becomeCandidate(){
  std::lock_guard<std::mutex> lock(mtx);
  state = CANDIDATE;
  currentTerm++;
  votedFor = nodeId;
  voteCount = 1;
  sendRequestVote();
}

void sendRequestVote() {
  String message = "VoteRequest " + nodeId + " " + String(currentTerm) + "\0";
  Serial.print("Sending ");
  Serial.println(message.c_str());
  // std::vector<uint8_t> respVector(message.begin(),message.end());
  // uint8_t *p = &respVector[0];
  // AsyncUDPMessage respMsg;
  // respMsg.write(p,message.length());
  udp.broadcast(message.c_str());
}

void loop() {
  server.handleClient();
  // put your main code here, to run repeatedly:
    unsigned long now = millis();
    if (state == FOLLOWER && now - lastHeartbeatTime > electionTimeout) {
      //If the time limit reached for heartbeattime, then the follower node becomes candidate and starts leader election procedure
      becomeCandidate();
    }

    if (state == CANDIDATE && now - lastElectionTime > electionTimeout) {
        sendRequestVote();
        lastElectionTime = now;
    }

    if (state == LEADER && now - lastHeartbeatTime > 1000) {
        sendHeartbeat();
        lastHeartbeatTime = now;
    }
}
