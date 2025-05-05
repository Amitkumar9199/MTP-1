#include<mutex>
#include<set>
#include <LittleFS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include<atomic>
#include<thread>
#include<Arduino.h>
#include<ArduinoJson.h>
#include<esp_pthread.h>
#include<string>

#define FORMAT_LITTLEFS_IF_FAILED true

struct CompareSecond {
    bool operator()(const std::pair<std::string, int>& lhs, const std::pair<std::string, int>& rhs) const {
        if (lhs.second != rhs.second) {
            return lhs.second < rhs.second; // Compare second item (integer part)
            
        }
        return lhs.first < rhs.first; // Compare first item (string part)
    }
};

std::mutex set_mutex{};
std::set<std::pair<std::string,int>,CompareSecond> main_set{};
std::set<std::pair<std::string,int>> add_set{};
std::set<std::pair<std::string,int>> rem_set{};
std::vector<String> ips{};
std::atomic<bool> new_data(false);

// const char * ssid	  = "Me";
// const char * password = "Brow1220";

// const char *udp_address = "192.168.163.255";
// const int udp_port = 10000; 

const char * ssid	  = "TP-Kita_24";
const char * password = "nikalLodean123";

const char * udp_address = "192.168.0.255";
// const char * udp_address = "10.5.20.255";
const int	 udp_port	= 10000;

WiFiUDP udp;

WebServer server(80);

void setup() {
  	Serial.begin( 115200 );
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.println("Connecting to WiFi...");
    }

  Serial.print(WiFi.localIP());

  // randomSeed(esp_random());
// 172.20.10.9
  server.on("/add",HTTP_POST,add_item);
  server.on("/rem",HTTP_POST,rem_item);
  server.on("/print",HTTP_GET,print_set);
  server.on("/participants",HTTP_GET,get_ip);

  udp.begin(udp_port);

  auto cfg = esp_pthread_get_default_config();

	cfg.stack_size	= 4 * 1024;
	cfg.inherit_cfg = true;
	cfg.prio		= 1;

	esp_pthread_set_cfg( &cfg );

  std::thread { udp_broadcast }.detach();
  std::thread { udp_receive }.detach();

  String brd_msg = WiFi.localIP().toString();
  ips.push_back(brd_msg);
  {
    std::lock_guard<std::mutex> lock(set_mutex);
    udp.beginPacket(udp_address,udp_port);
    udp.print(brd_msg);
    udp.endPacket();
  }

  server.begin();
}

std::string generateUniqueId(){
  std::string uniqueID = std::to_string((millis())) + std::to_string((random(1000, 9999)));
  return uniqueID;
}

void get_ip(){
  JsonDocument doc;
  // JsonArray ipArray = doc.createNestedArray();
  String data = "";
  for(auto &it:ips){
    Serial.println(it.c_str());
    data+=it+";";
    // doc.add();
  }
  // data+="\r";
  // JsonString str;
  // serializeJson(doc,str);
  server.send(200,"text/plain",data);      
}

void add_item(){
  if(server.hasArg("val")){
    int number = server.arg("val").toInt();
    Serial.print("Received add-");
    Serial.println(number);
    {
      std::lock_guard< std::mutex > lock( set_mutex );
      auto it=main_set.lower_bound(std::make_pair("",number));
      if(it==main_set.end() || it->second!=number ){
        std::string uniqueID = generateUniqueId();
        main_set.insert(std::make_pair(uniqueID,number));
        add_set.insert(std::make_pair(uniqueID,number));
        // new_data.store(true);
      }
      else{
        return server.send(409, "application/json", "{\"error\":\"Element already present in the set\"}");
      }
    }
    print_set();
  }
  server.send(200,"application/json",{});
}

void rem_item(){
  if(server.hasArg("val")){
    int number = server.arg("val").toInt();
    Serial.print("Received rem-");
    Serial.println(number);
    {
      std::lock_guard<std::mutex> lock(set_mutex);
      int cnt=0;
      while(1){
        auto it = main_set.lower_bound(std::make_pair("",number));
        if(it!=main_set.end() && it->second==number){
          rem_set.insert(*it);
          main_set.erase(it); 
          cnt++;
          // new_data.store(true);       
        }
        else {
          if(!cnt)
            return server.send(400,"application/json", "{\"error\":\"Element not found in the set\"}");
          else
            break;
        }        
      }    
    }
    print_set();
  }
  server.send(200,"application/json",{});
}

void udp_broadcast(){
  while(true){
      String jsonString;
      {
        std::lock_guard<std::mutex> lock(set_mutex);       
        const size_t capacity = JSON_ARRAY_SIZE(add_set.size());
        DynamicJsonDocument doc(capacity);
        JsonArray addArray = doc.createNestedArray("add_set");
        for(const auto& item:add_set){
          JsonArray pairArray = addArray.createNestedArray();
          pairArray.add(item.first);
          pairArray.add(item.second);
        }

        JsonArray remArray = doc.createNestedArray("rem_set");
        for(const auto& item:rem_set){
          JsonArray pairArray = remArray.createNestedArray();
          pairArray.add(item.first);
          pairArray.add(item.second);        
        }
      serializeJson(doc, jsonString); 
      Serial.print("Broadcasting data- ");
      Serial.println(jsonString);

      udp.beginPacket(udp_address, udp_port);
      serializeJson(doc, udp);
      udp.endPacket();
    }
      std::this_thread::sleep_for(std::chrono::seconds(1));
      continue;
  }
}

void udp_receive(){
  while(true){
    int sz = udp.parsePacket();
    if(not sz){
      std::this_thread::sleep_for(std::chrono::seconds(1));
      continue;
    }

    uint8_t buffer[1024] = "";
    if(udp.read(buffer,sizeof(buffer)) > 0){
      Serial.printf("Received msg: %s\n\n",buffer);

      JsonDocument doc;
      deserializeJson(doc,buffer);

      if(doc.containsKey("add_set") && doc.containsKey("rem_set")){
        JsonArray addArray = doc["add_set"];
        JsonArray remArray = doc["rem_set"];

        std::set<std::pair<std::string, int>> temp_add_set;
        std::set<std::pair<std::string, int>> temp_rem_set;
        
        for (JsonVariant variant : addArray) {
          std::pair<std::string, int> item{variant[0].as<std::string>(), variant[1].as<int>()};
          temp_add_set.insert(item);
        }

        for (JsonVariant variant : remArray) {
          std::pair<std::string, int> item{variant[0].as<std::string>(), variant[1].as<int>()};
          temp_rem_set.insert(item);
        }
        
        std::lock_guard<std::mutex> lock(set_mutex);
        for(auto &it:temp_add_set){
          if(temp_rem_set.find(it)==temp_rem_set.end()){
            if(add_set.find(it)==add_set.end()){
              main_set.insert(it);
              add_set.insert(it);
            }
          }
          else{
            auto el = main_set.lower_bound(it);
            if(el!=main_set.end() && el->first == it.first){
              rem_set.insert(*el);
              main_set.erase(el);
            }         
            temp_rem_set.erase(it);
          }
        }

        for(auto &it:temp_rem_set){
          if(rem_set.find(it)==rem_set.end()){
            rem_set.insert(it);
            main_set.erase(it);
          }
        }
      }
      else{
        Serial.println(doc.as<String>().c_str());
        ips.push_back((const char *)buffer);
      }
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
    continue;  
  }
}

void print_set(){
    std::lock_guard<std::mutex> lock(set_mutex);       
    const size_t capacity = JSON_ARRAY_SIZE(main_set.size());
    DynamicJsonDocument doc(capacity);
    // Convert the set to a JSON array
    JsonArray mainArray = doc.createNestedArray("main_set");
    for(const auto& item:main_set){
      JsonArray pairArray = mainArray.createNestedArray();
      pairArray.add(item.first);
      pairArray.add(item.second);
    }
    server.send(200,"application/json",doc.as<String>());
}

void loop() {
  server.handleClient();
}
