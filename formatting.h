#ifndef FORMATTING_H
#define FORMATTING_H

#include <Arduino.h>
#include <map>

// LoRa and WiFi timesharing
#define PROTOCOL_TIMESLOT 6000 // Given this amount of time per protocol to send msg and receive ACK before next send

// Retransmission related thresholds
#define MAX_RETRANSMISSIONS 3
#define MAX_FORWARDING 2
#define RETRANSMIT_BACKOFF 3600 // Retransmit only if this amount of time has passed
#define FWD_CACHE_EXPIRY 18000 // for msgIDs older than this, remove from the forward list
#define DUPLICATE_RX_PERIOD 18000 // 3* of protocol_timeslot

// Define the GPIO pin for the button to trigger message sending manually
#define BUTTON_PIN 38
#define DEBOUNCE_PERIOD 300

// For tracking forwarded messages
struct msgFwdCountTime {
    int count;
    unsigned long startTime;
    unsigned long wifiFwdTime;
    unsigned long loraFwdTime;
    int wifiTransmitCount;
    int loraTransmitCount;
};

// For message cache to retransmit
struct msgCacheItem {
    String dest;
    String fullMsg; // contains 4 char msgID, 2 char dest, 2 char source
    unsigned long lastSent;
    int wifiTransmitCount;
    int loraTransmitCount;
    bool initiatedOnWifi;
};

// Function prototypes
String generateMsgID();

String formatMsgString(String msg, String dest, String myNodeName);

String formatResponseMsgString(String msgID, String returnAddr, String myNodeName, String msg);

String formatForwardedMsgString(String msgID, String dest, String source, String myNodeName, String msg);

std::map<String, msgCacheItem> insertMsgCache(std::map<String, msgCacheItem> &map, String msg, unsigned long timeNow, bool initOnWifi=true);

std::map<String, msgCacheItem> updateMsgCache(std::map<String, msgCacheItem> &map, String msgID, unsigned long timeNow, bool incrementWifi=true);

std::map<String, msgCacheItem> removeFromRetransmitCache(std::map<String, msgCacheItem> &map, String msgID);

std::map<String, String> getMsgDetails(String msg);

std::map<String, msgFwdCountTime> updateFwdList(std::map<String, msgFwdCountTime> &map, String key, unsigned long newTime, bool fwdOnWifi);

std::map<String, msgFwdCountTime> cleanUpFwdList(std::map<String, msgFwdCountTime> &map);

int getForwardedCount(std::map<String, msgFwdCountTime> &map, String msgID, bool forWifi);

std::map<String, unsigned long> cleanUpDuplicateTracker(std::map<String, unsigned long> &map);

bool _isDuplicate(std::map<String, unsigned long> &map, String msgID);

#endif // FORMATTING_H