#include "formatting.h"
#include <Arduino.h>
#include <map>

String generateMsgID(){
    char buffer[5];
    int num1 = rand() % 10000; // 0 - 9999
    sprintf(buffer, "%04d", num1); // make it 4 digits
    return String(buffer);
}

String formatMsgString(String msg, String dest, String myNodeName){
  // Format: msgID, dest, src, forwarder, content
    return (generateMsgID() + dest + myNodeName + myNodeName + msg);
}

String formatResponseMsgString(String msgID, String returnAddr, String myNodeName, String msg){
  // Format: msgID, dest, src, forwarder, content
    return (msgID + returnAddr + myNodeName + myNodeName + msg);
}

String formatForwardedMsgString(String msgID, String dest, String source, String myNodeName, String msg){
  // Replace forwarder address with myNodeName
  return (msgID + dest + source + myNodeName + msg);
}

std::map<String, String> getMsgDetails(String msg){
    // Serial.println("MSG: " + msg);
    std::map<String, String> msgData;
    msgData["msgID"] = msg.substring(0,4);
    msgData["dest"] = msg.substring(4,6);
    msgData["sender"] = msg.substring(6,8);
    msgData["forwarder"] = msg.substring(8,10);
    msgData["content"] = msg.substring(10);
    return msgData;
}

// Inserts new msg into cache or updates transmit count and time if exists already
std::map<String, msgCacheItem> insertMsgCache(std::map<String, msgCacheItem> &map, String msg, unsigned long timeNow, bool initOnWifi){
  auto it = map.find(msg.substring(0,4));
  if(it != map.end()){
    return updateMsgCache(map, msg.substring(0,4), timeNow, initOnWifi);
  }
  map[msg.substring(0,4)] = {msg.substring(4,6), msg, timeNow, 0, 0, initOnWifi};
  return map;
}

// Updates time of last sent on the msgID on each retransmission
std::map<String, msgCacheItem> updateMsgCache(std::map<String, msgCacheItem> &map, String msgID, unsigned long timeNow, bool incrementWifi){
    auto it = map.find(msgID);
    if (it != map.end()) {
        if(incrementWifi){
            it->second.wifiTransmitCount = it->second.wifiTransmitCount + 1;
        } else {
            it->second.loraTransmitCount = it->second.loraTransmitCount + 1;
        }
        it->second.lastSent = timeNow;
    }
    return map;
}

std::map<String, msgCacheItem> removeFromRetransmitCache(std::map<String, msgCacheItem> &map, String msgID){
    auto it = map.find(msgID);
    if (it != map.end()) {
        it = map.erase(it); // Remove that message ID
    }
    Serial.println("[RETRANSMIT CACHE] Dropped msg #" + msgID + " from list");
    return map;
}

// Function to update the value only if the key exists
std::map<String, msgFwdCountTime> updateFwdList(std::map<String, msgFwdCountTime> &map, String key, unsigned long newTime, bool fwdOnWifi) {
    auto it = map.find(key);
    if (it != map.end()) {
        it->second.count = it->second.count +1;
      if(fwdOnWifi){
        it->second.wifiTransmitCount = (it->second.wifiTransmitCount + 1);
        it->second.wifiFwdTime = newTime;
      } else {
        it->second.loraTransmitCount = (it->second.loraTransmitCount + 1);
        it->second.loraFwdTime = newTime;
      }
    } else {    
      if(fwdOnWifi){
        map[key] = {1, newTime, newTime, 0, 1, 0};
      } else {
        map[key] = {1, newTime, 0, newTime, 0, 1};
      }
    }
    return map;
}

// Remove old messages from here
std::map<String, msgFwdCountTime> cleanUpFwdList(std::map<String, msgFwdCountTime> &map){
    for (auto it = map.begin(); it != map.end();) {

        // remove if old
        if (millis() - it->second.startTime > FWD_CACHE_EXPIRY) {
            Serial.println("[FORWARD LIST] Removed " + it->first);
            it = map.erase(it);

        // remove if forwarded enough times
        } else if (it->second.wifiTransmitCount == MAX_FORWARDING && it->second.loraTransmitCount == MAX_FORWARDING){
          it = map.erase(it);
        } 
        else {
            ++it;
        }
    }
    return map;
}


int getForwardedCount(std::map<String, msgFwdCountTime> &map, String msgID, bool forWifi){
    auto it = map.find(msgID);
    if (it != map.end()){
      if(forWifi){
        return it->second.wifiTransmitCount;
      }
        return it->second.loraTransmitCount;
    }
    return 0;
}

std::map<String, unsigned long> cleanUpDuplicateTracker(std::map<String, unsigned long> &map){
  for (auto it = map.begin(); it != map.end();) {
    if (millis() - it->second > DUPLICATE_RX_PERIOD) {
      it = map.erase(it); // Remove that message ID
    } else {
      ++it;
    }
  }
  return map;
}

bool _isDuplicate(std::map<String, unsigned long> &map, String msgID){
  auto it = map.find(msgID);
  if (it != map.end()){
    return (millis() - it->second < DUPLICATE_RX_PERIOD);
  }
  return false;
}
