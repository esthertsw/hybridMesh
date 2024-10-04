/*
  RadioLib SX126x Compatible
*/

// include the library
#include <RadioLib.h>
#include "formatting.h"
#include "namedMesh.h"
#include <map>

#define   MESH_SSID       "wifiMesh"
#define   MESH_PASSWORD   "wifiloramesh"
#define   MESH_PORT       5678

namedMesh  wifiMesh;

String myNodeName = "0A"; //"0B" "0C" "0D"
String destNode = "0C";

SX1262 radio = new Module(18, 33, 23, 32);
int stateTX = 1;
int stateRX = 1;
unsigned long wifiStart = 0;
unsigned long loraStart = 0;

// Variable to store the button state
int buttonState = 0;
unsigned long buttonLastPressed = 0;

// Keep track of transmitted/forwarded packets
int count = 0;
int responseCount = 0;
std::map<String, msgFwdCountTime> forwardedMsgList;
std::map<String, msgCacheItem> msgRetransmitCache;

std::map<String, unsigned long> duplicateTracker;
std::map<uint32_t, String> nodeIdNameMap;

void setup() {
  Serial.begin(115200);

  // Set the button pin as an input
  pinMode(BUTTON_PIN, INPUT); 

  // initialize SX1262 with default settings
  Serial.print(F("[SX1262] Initializing ... "));
  int state = radio.begin();
  delay(1000); // must have this after radio begin

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("Setup success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true) { delay(10); }
  }
  // automatically control external RF switch
  radio.setRfSwitchPins(3, 4);

  wifiMesh.setDebugMsgTypes(DEBUG | ERROR | CONNECTION);  // set before init() so that you can see startup messages

  wifiMesh.init(MESH_SSID, MESH_PASSWORD, MESH_PORT);

  wifiMesh.setName(myNodeName); // This needs to be an unique name! 

  wifiMesh.onNewConnection([](auto nodeId) {
    // To allow the device to associate nodeId with node name
    String initMsg = "AAAA" + myNodeName; 
    wifiMesh.sendSingle(nodeId, initMsg);
    Serial.println("New Wi-Fi connection established with node #" + String(nodeId) + "!");
  });

  // Add nodeId association to node name
  wifiMesh.onReceive([](uint32_t nodeId, String &msg) {
    if(msg.substring(0,4) == "AAAA"){
      nodeIdNameMap[nodeId] = msg.substring(4);
      Serial.println("Wi-Fi nodeId-nodeName map updated:");
      for (auto it: nodeIdNameMap){
        Serial.print(it.first);
        Serial.println("\t" + it.second);
      }
    }
  });

  // Remove node name association from nodeId
  wifiMesh.onDroppedConnection([](auto nodeId) {
    nodeIdNameMap.erase(nodeId);
    Serial.println("Wi-Fi nodeId-nodeName map updated (removed items):");
    for (auto it: nodeIdNameMap){
      Serial.print(it.first);
      Serial.println("\t" + it.second);
    }
  });

  wifiMesh.onReceive([](String &from, String &msg) {
    std::map<String, String> msgDetails = getMsgDetails(msg);
    // if not the intended recipient, forward
    if(msgDetails["dest"] != myNodeName && msgDetails["msgID"] != "AAAA"){
      if(getForwardedCount(forwardedMsgList, msgDetails["msgID"], true) < MAX_FORWARDING && sufficientTimeSincePreviousFwd(msgDetails["msgID"], true)){
        // fwd and increment
        wifiMesh.sendBroadcast(msg);
        Serial.printf("[WiFi] Forwarded msg #%s for %s\n", msgDetails["msgID"].c_str(), msgDetails["dest"].c_str());
        forwardedMsgList = updateFwdList(forwardedMsgList, msgDetails["msgID"], millis(), true);
      }
      
      forwardedMsgList = cleanUpFwdList(forwardedMsgList); 
    }
    else {
      // Print only if it's the first time receiving the response on that msgID
      // Check for msgID in msg cache, if in there, pring msg and remove from cache.
      auto it = msgRetransmitCache.find(msgDetails["msgID"]);

      if(it != msgRetransmitCache.end()) { // msgID originated from self or was previously sent
        if(!isDuplicate(msgDetails["msgID"])){ // is an ACK to that msg
          Serial.printf("[Wi-Fi] Received ACK from: %s on msg #%s, %s\n", from.c_str(), msg.substring(0,4).c_str(), msg.substring(8).c_str());
        }
        msgRetransmitCache = removeFromRetransmitCache(msgRetransmitCache, msgDetails["msgID"]); // Received ACK for this message so no need to retransmit anymore
      } else { // msg originated elsewhere
        if(!isDuplicate(msgDetails["msgID"])){ // unseen msg, send ACK
          Serial.printf("[Wi-Fi] Received message from: %s, msg #%s, %s\n", from.c_str(), msg.substring(0,4).c_str(), msg.substring(8).c_str());
          String strTX = formatResponseMsgString(msgDetails["msgID"], msgDetails["sender"], myNodeName, "Response to msg #" + msgDetails["msgID"] + "! #" + String(responseCount++));
          wifiMesh.sendBroadcast( strTX); //msgDetails["sender"],
          msgRetransmitCache = insertMsgCache(msgRetransmitCache, strTX, millis(), true);
        }
      }
      trackForDuplication(msgDetails["msgID"]);
    }
  });


  // ------------------------------------------------
  /* --- COMMENT FOR ALL NODES EXCEPT NODE 0A --- */
  // [Choice of LoRa or Wi-Fi as the initial message]

  // LoRa:

  // loraStart = millis();
  // Serial.print(F("[LoRa] Transmitting packet ... "));
  // String strTX = formatMsgString("Hello World! #" + String(count++), destNode, myNodeName);
  // stateTX = radio.transmit(strTX);
  // msgRetransmitCache = insertMsgCache(msgRetransmitCache, strTX, millis(), false);

  // Wi-Fi:

  wifiStart = millis();
  Serial.print(F("[Wi-Fi] Transmitting packet ... "));
  String strTX = formatMsgString("Hello World! #" + String(count++), destNode, myNodeName);
  wifiMesh.sendBroadcast(strTX);
  msgRetransmitCache = insertMsgCache(msgRetransmitCache, strTX, millis(), true);

  /* ------------------- [END] --------------------*/
  // ------------------------------------------------

  // you can also transmit byte array up to 256 bytes long
  /*
    byte byteArr[] = {0x01, 0x23, 0x45, 0x56, 0x78, 0xAB, 0xCD, 0xEF};
    int stateTX = radio.transmit(byteArr, 8);
  */
}

void onButtonPress(){
    if (millis() - buttonLastPressed < DEBOUNCE_PERIOD){
      return;
    }
    buttonLastPressed = millis();
    Serial.println(F("[BUTTON PRESS]"));

	  /* Send message */
    // If on LoRa time
    if(loraStart != 0){
      Serial.print(F("[LoRa] Transmitting packet #"));
      String strTX = formatMsgString("Hello World! (button press) #" + String(count++) + " from " + myNodeName + " initiated on LoRa", destNode, myNodeName);
      stateTX = radio.transmit(strTX);
      Serial.println(strTX.substring(0,4));
      msgRetransmitCache = insertMsgCache(msgRetransmitCache, strTX, millis(), false);
    } else { // On Wi-Fi time
      Serial.print(F("[Wi-Fi] Transmitting packet #"));
      String strTX = generateMsgID() + destNode + myNodeName + "Hello World! (button press) #" + String(count++) + " from " + myNodeName + " initiated on Wi-Fi";
      Serial.println(strTX.substring(0,4));
      wifiMesh.sendBroadcast(strTX); // destNode
      msgRetransmitCache = insertMsgCache(msgRetransmitCache, strTX, millis());
    }
}

void loop() {
  // Check if the button is pressed
  buttonState = digitalRead(BUTTON_PIN);
  if (buttonState == LOW) {
    onButtonPress();
  }

  // LoRa has sent a message, so expect an ACK
  if (loraStart != 0){
    String str;
    stateRX = radio.receive(str);

    // LoRa has held CPU for long enough, switch to WiFi now
    if(millis()- loraStart > PROTOCOL_TIMESLOT){
      loraStart = 0;
      wifiStart = millis();
      Serial.println("\n[PROTOCOL SWITCH]: ------------- WIFI ------------"); // run wifiMesh update
      if(stateRX != RADIOLIB_ERR_NONE){
        stateRX = 1; // ignore current because of interrupted receive
      }
    // LoRa continues to hold CPU time (but Wi-Fi receives in the background via interrupts)
    } else { 
      // carry on with LoRa tasks
      Serial.println(".");

      /* LoRa Receiver's Checks */
      if (stateRX == RADIOLIB_ERR_NONE) {

        // check for message's recipient address
        std::map<String, String> msgDetails = getMsgDetails(str);

        // FORWARD if it's not intended for this node
        if (msgDetails["dest"] != myNodeName){
          // check how many times it was forwrded already
          if(getForwardedCount(forwardedMsgList, msgDetails["msgID"], false) < MAX_FORWARDING && sufficientTimeSincePreviousFwd(msgDetails["msgID"], false)){
            // fwd and increment
            radio.transmit(str);
            Serial.printf("[LoRa] Forwarded msg #%s for %s\n", msgDetails["msgID"].c_str(), msgDetails["dest"].c_str());
            forwardedMsgList = updateFwdList(forwardedMsgList, msgDetails["msgID"], millis(), false);
          } // else will be removed when WiFi forwarding also hits max, or timeout

        }
        // RESPOND if it's the intended recipient
        else {
          // reset
          stateRX = 1;

          // Respond only if it's not a packet originating/previously send from itself

          auto it = msgRetransmitCache.find(msgDetails["msgID"]);

          // MsgID originated from self and was last retransmitted recently, therefore this is a response to it.
          if (it != msgRetransmitCache.end()){
            if(!isDuplicate(msgDetails["msgID"])){
              // print received message, do not respond to ACK.
              Serial.printf("[LoRa] Received ACK from: %s on msg #%s, %s\n", msgDetails["sender"].c_str(), msgDetails["msgID"].c_str(), msgDetails["content"].c_str());
            }
            trackForDuplication(msgDetails["msgID"]);
            msgRetransmitCache = removeFromRetransmitCache(msgRetransmitCache, msgDetails["msgID"]); // This message received is the ACK, so no need to retransmit anymore
          } else { 
            if(!isDuplicate(msgDetails["msgID"])){
              // print received message and send response
              Serial.printf("[LoRa] Received message from %s, msg #%s, %s\n", msgDetails["sender"].c_str(), msgDetails["msgID"].c_str(), msgDetails["content"].c_str());
              String strTX = formatResponseMsgString(msgDetails["msgID"], msgDetails["sender"], myNodeName, "Response to msg #" + msgDetails["msgID"] + "! #" + String(responseCount++));
              stateTX = radio.transmit(strTX);
              msgRetransmitCache = insertMsgCache(msgRetransmitCache, strTX, millis(), false);
            }
            trackForDuplication(msgDetails["msgID"]);
          }
        }
        forwardedMsgList = cleanUpFwdList(forwardedMsgList); // Remove msgs forwarded max times already

      } else if (stateRX == RADIOLIB_ERR_CRC_MISMATCH) {
        // packet was received, but is malformed
        Serial.println(F("CRC error!"));
      }

      /* LoRa Transmitter's Checks */
      if (stateTX == RADIOLIB_ERR_NONE) {
        // the packet was successfully transmitted
        // reset
        stateTX = 1;
      } else if (stateTX == RADIOLIB_ERR_PACKET_TOO_LONG) {
        // the supplied packet was longer than 256 bytes
        Serial.println(F("TX message too long!"));

      } else if (stateTX == RADIOLIB_ERR_TX_TIMEOUT) {
        // timeout occured while transmitting packet
        Serial.println(F("TX timeout!"));
      }

      // Loop through cache and retransmit whichever is still there
      bool quit = false;
      for (auto it = msgRetransmitCache.begin(); it != msgRetransmitCache.end();) {
        if ((millis() - it->second.lastSent) > RETRANSMIT_BACKOFF) {
          if (it->second.initiatedOnWifi){
            if (it->second.wifiTransmitCount < MAX_RETRANSMISSIONS){
              // CASE 1: let WiFi retransmit
            } else {
              if (it->second.loraTransmitCount < MAX_RETRANSMISSIONS) {
                // CASE 2: LoRa retransmit
                if(millis() - it->second.lastSent >= RETRANSMIT_BACKOFF){
                  stateTX = loraRetransmit(it);
                  quit=true; // equivalent to "break" 
                }
              } else {
                // CASE 3: remove
                trackForDuplication(it->first);
                msgRetransmitCache = removeFromRetransmitCache(msgRetransmitCache, it->first);
              }
            }
          } else {
            if (it->second.loraTransmitCount < MAX_RETRANSMISSIONS){
              // CASE 4: Send on LoRa
              if(millis() - it->second.lastSent >= RETRANSMIT_BACKOFF){
                stateTX = loraRetransmit(it);
                quit=true; // equivalent to "break" 
              }
            } else {
              if (it->second.wifiTransmitCount < MAX_RETRANSMISSIONS){
                // CASE 5: let WiFi retransmit
              } else {
                // CASE 6: remove
                trackForDuplication(it->first);
                msgRetransmitCache = removeFromRetransmitCache(msgRetransmitCache, it->first);
              }
            }
          }
        }
        ++it;
      }
      
    }
  }
  // WiFi holding CPU time
  else if(wifiStart != 0){
    if(millis() - wifiStart > PROTOCOL_TIMESLOT){
        wifiStart = 0; // switch to LoRa
        loraStart = millis();
        Serial.println("\n[PROTOCOL SWITCH]: ------------- LORA -------------");
    } else {
      // carry on WiFi tasks
      wifiMesh.update();

      // clear WiFi retransmission cache
      bool quit = false;
      for (auto it = msgRetransmitCache.begin(); it != msgRetransmitCache.end();) {
        if ((millis() - it->second.lastSent) > RETRANSMIT_BACKOFF && !quit) {
          if (it->second.initiatedOnWifi){
            if (it->second.wifiTransmitCount < MAX_RETRANSMISSIONS){
              // CASE 1: WiFi retransmit
              if(millis() - it->second.lastSent >= RETRANSMIT_BACKOFF){
                wifiRetransmit(it);
                quit=true; // equivalent to "break" 
              }
            } else {
              if (it->second.loraTransmitCount < MAX_RETRANSMISSIONS) {
                // CASE 2: let LoRa retransmit
              } else {
                // CASE 3: remove
                trackForDuplication(it->first);
                msgRetransmitCache = removeFromRetransmitCache(msgRetransmitCache, it->first);
              }
            }
          } else {
            if (it->second.loraTransmitCount < MAX_RETRANSMISSIONS){
              // CASE 4: let LoRa retransmit
            } else {
              if (it->second.wifiTransmitCount < MAX_RETRANSMISSIONS){
                // CASE 5: WiFi retransmit
                if(millis() - it->second.lastSent >= RETRANSMIT_BACKOFF){
                  wifiRetransmit(it);
                  quit=true; // equivalent to "break" 
                }
              } else { 
                // CASE 6: remove
                trackForDuplication(it->first);
                msgRetransmitCache = removeFromRetransmitCache(msgRetransmitCache, it->first);
              }
            }
          }
        }
        ++it;
      }
    }
  }
}

// Only if it was not initiated on Wi-Fi, or if Wi-Fi retransmits have hit the max. LoRa retransmits must not be max
int loraRetransmit(auto it){
  Serial.println("[LoRa] Retransmitting msg #" + it->first + ", try " + String(it->second.loraTransmitCount+1) + " of " + String(MAX_RETRANSMISSIONS));
  stateTX = radio.transmit(it->second.fullMsg);
  msgRetransmitCache = updateMsgCache(msgRetransmitCache, it->first, millis(), false);
  return stateTX;
}

void wifiRetransmit(auto it){
  Serial.println("\n[Wi-Fi] Retransmitting msg #" + it->first + ", try " + String(it->second.wifiTransmitCount+1) + " of " + String(MAX_RETRANSMISSIONS));
  wifiMesh.sendBroadcast(it->second.fullMsg); //it->second.dest, 
  msgRetransmitCache = updateMsgCache(msgRetransmitCache, it->first, millis());
}

bool isDuplicate(String msgID){
  duplicateTracker = cleanUpDuplicateTracker(duplicateTracker);
  return _isDuplicate(duplicateTracker, msgID);
}

void trackForDuplication(String msgID){
  duplicateTracker[msgID] = millis();
}

bool sufficientTimeSincePreviousFwd(String msgID, bool wifi){
  auto it = forwardedMsgList.find(msgID);
  if (it!= forwardedMsgList.end()){
    if(wifi){
      return millis() - it->second.wifiTransmitCount >= RETRANSMIT_BACKOFF;
    }
    return  millis() - it->second.loraTransmitCount >= RETRANSMIT_BACKOFF;
  }
  return true; // for unseen msg
}
