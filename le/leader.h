#include "IPAddress.h"
#ifndef _LEADER_H_
#define _LEADER_H_

#include<Arduino.h>
#include<ArduinoJson.h>
#include<WiFi.h>
#include<WiFiUdp.h>
#include<WebServer.h>

#include<thread>
#include<string>
#include<set>
#include<mutex>

#define heartbeatInterval 250  //Hearbeat interval 250ms

class LeaderElection{

  String leaderId;
  IPAddress broadcast_address;
  const unsigned int port = 10000;

  std::set<String> nodes;
  std::mutex mtx;

  String nodeId = "";  //It contains the ip address of the current node as its id
  int currentTerm = 0;
  String votedFor = "";  //Storing the ip of the node it voted for, empty means none.
  std::string state = "Follower";

  unsigned long electionTimeout;
  unsigned long lastElectionTime;
  unsigned long lastHeartbeatTime;

  bool isLoad = false;

  WiFiUDP udp;
  // WebServer server(80) ;

  int voteCount = 0;
  int totalNodes = 1;  //Number of total devices I'm using

public:
  void begin(){

  }

  void run(){
      startLoop();
  }

  bool load(){
    startLoad();
    return isLoad;
  }

  std::set<String> allNodes(){
    return nodes;
  }

private:
  //Function to catch udp packets coming from other nodes
  void udp_receive() {
    while (true) {
      // Serial.println("Inside udp_receive");
      // Serial.println(WiFi.localIP());
      // Serial.println(broadcast_address);
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

  void sendRequestVote() {
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

  void processMessage(String msg) {
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

  void startLoad(){
    Serial.println("Inside startLoad");
    if(WiFi.status() != WL_CONNECTED){
      return;      
    }
    Serial.println("wifi is connected");
    nodeId = WiFi.localIP().toString();
    Serial.println("Inside startLoad");
    Serial.println(nodeId.c_str());
    Serial.print("broadcast_ip =");
    Serial.println(WiFi.broadcastIP());
    broadcast_address = WiFi.broadcastIP();
    // broadcast_address = "10.5.20.255";
    Serial.println(broadcast_address);
    nodes.insert(nodeId);
    String brd_msg = "Joined " + nodeId;

    udp.begin(port);

    udp.beginPacket(broadcast_address, port);
    udp.print(brd_msg);
    if (udp.endPacket()) {
      Serial.println(brd_msg.c_str());
    }

    Serial.print("State = ");
    Serial.print(state.c_str());

    resetElectionTimeout();
    lastHeartbeatTime = millis();

    Serial.print("Election time - ");
    Serial.println(electionTimeout);

    Serial.print("heartbeat time - ");
    Serial.println(lastHeartbeatTime);

    std::thread{ &LeaderElection::udp_receive,this }.detach();

    isLoad = true;
  }

  //The loop function to perform operations based on the node's current state
  void startLoop(){
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
};

extern LeaderElection Leader;

#endif

