// input: WPA scan fixtures and published WPA PSK vector; output: ARM data checks; pos: Wi-Fi parser regression.
#include "wifi-data.h"
#include <cstdio>
int main(){
 auto rows=WifiData::networks("bssid / frequency / signal level / flags / ssid\n"
  "02:00:00:00:00:01\t2412\t-70\t[WPA2-PSK-CCMP][ESS]\tOffice\\x20WiFi\n"
  "02:00:00:00:00:02\t2412\t-40\t[WPA2-PSK-CCMP][ESS]\tOffice WiFi\n"
  "02:00:00:00:00:03\t2412\t-60\t[WPA2-EAP-CCMP][ESS]\tEnterprise\n"
  "02:00:00:00:00:04\t2412\t-80\t[ESS]\t\n");
 if(rows.size()!=2||rows[0].toMap()["ssid"]!="Office WiFi"||rows[0].toMap()["signal"]!=-40)return 1;
 if(rows[1].toMap()["supported"].toBool())return 2;
 if(WifiData::psk("password","IEEE")!="f42c6fc52df0ebef9ebb4b90b38a5f902e83fe1b135a70e23aed762e9710a12e")return 3;
 if(!WifiData::psk("short","IEEE").isEmpty()||!WifiData::psk(QByteArray(64,'z'),"IEEE").isEmpty())return 4;
 if(WifiData::psk(QByteArray(64,'A'),"IEEE")!=QByteArray(64,'a'))return 5;
 if(WifiData::ssidBytes("a\\tb\\\\c\\x20d")!=QByteArray("a\tb\\c d"))return 6;
 puts("Wi-Fi data checks passed: strongest BSS, hidden SSID, enterprise flags, PSK vector, invalid passwords, raw key, SSID escapes.");
}
