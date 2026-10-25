// input: local WPA control and touch settings; output: saved station profiles; pos: device-native network controller.
#pragma once
#include <QFile>
#include <QObject>
#include <QTimer>
#include "wpa-control.h"
#include "wifi-data.h"

class Wifi : public QObject {
 Q_OBJECT
 Q_PROPERTY(QVariantList networks READ networks NOTIFY changed)
 Q_PROPERTY(QString status READ status NOTIFY changed)
 Q_PROPERTY(QString connectedSsid READ connectedSsid NOTIFY changed)
 Q_PROPERTY(QString address READ address NOTIFY changed)
 Q_PROPERTY(QString error READ error NOTIFY changed)
 Q_PROPERTY(bool busy READ busy NOTIFY changed)
 Q_PROPERTY(bool scanning READ scanning NOTIFY changed)
 WpaControl control;QTimer poll,scanDeadline;
 QVariantList rows;QString stateText="尚未连接",ssid,ip,problem;
 QList<int> previousProfiles;
 bool active=false,suspended=false,configuring=false,finding=false;
 void statusUpdate(){
  control.request("STATUS",[this](QByteArray value){
   if(value.startsWith("FAIL"))return;
   QMap<QByteArray,QByteArray> fields;
   for(auto line:value.split('\n')){int n=line.indexOf('=');if(n>0)fields[line.left(n)]=line.mid(n+1);}
   const auto phase=fields["wpa_state"];
   QString next=phase=="COMPLETED"?(fields["ip_address"].isEmpty()?"已连接，正在获取地址":"已连接"):
                phase=="SCANNING"?"正在查找网络":phase=="ASSOCIATING"||phase=="4WAY_HANDSHAKE"?"正在连接":"尚未连接";
   QString nextSsid=phase=="COMPLETED"?QString::fromUtf8(WifiData::ssidBytes(fields["ssid"])):QString();
   QString nextIp=QString::fromLatin1(fields["ip_address"]);
   if(stateText!=next||ssid!=nextSsid||ip!=nextIp){stateText=next;ssid=nextSsid;ip=nextIp;emit changed();}
  });
 }
 void scanResults(){
  control.request("SCAN_RESULTS",[this](QByteArray result){
   if(result.startsWith("FAIL"))return;
   rows=WifiData::networks(result);finding=false;scanDeadline.stop();emit changed();
  });
 }
 void configurationFailed(int id){
  problem="连接设置失败，请重试";configuring=false;emit changed();
  if(id>=0)control.request("REMOVE_NETWORK "+QByteArray::number(id));
 }
 void persist(){
  if(!previousProfiles.isEmpty()){
   int id=previousProfiles.takeFirst();
   control.request("REMOVE_NETWORK "+QByteArray::number(id),[this](QByteArray result){
    if(result!="OK"){problem="已启动连接，旧网络更新失败";configuring=false;emit changed();return;}persist();
   });return;
  }
  control.request("SAVE_CONFIG",[this](QByteArray saved){
   configuring=false;if(saved!="OK")problem="本次连接已启动，保存网络失败";
   emit changed();statusUpdate();
  });
 }
 void configure(int id,QByteArray ssidHex,QByteArray key){
  auto prefix="SET_NETWORK "+QByteArray::number(id)+" ";
  control.request(prefix+"ssid "+ssidHex,[this,id,key,prefix](QByteArray result){
   if(result!="OK"){configurationFailed(id);return;}
   control.request(prefix+"key_mgmt "+(key.isEmpty()?"NONE":"WPA-PSK"),[this,id,key,prefix](QByteArray result){
    if(result!="OK"){configurationFailed(id);return;}
    auto select=[this,id]{
     control.request("SELECT_NETWORK "+QByteArray::number(id),[this,id](QByteArray result){
      if(result!="OK"){configurationFailed(id);return;}
      persist();
     });
    };
    if(key.isEmpty()){select();return;}
    control.request(prefix+"psk "+key,[this,id,select](QByteArray result){
     if(result!="OK"){configurationFailed(id);return;}select();
    });
   });
  });
 }
 void updatePolling(){if(active&&!suspended)poll.start();else poll.stop();}
public:
 explicit Wifi(QObject *parent=nullptr):QObject(parent){
  poll.setInterval(5000);connect(&poll,&QTimer::timeout,this,[this]{if(!configuring)statusUpdate();});
  scanDeadline.setSingleShot(true);connect(&scanDeadline,&QTimer::timeout,this,[this]{finding=false;emit changed();scanResults();});
  control.failed=[this]{problem="无线服务未就绪，请稍后刷新";finding=false;scanDeadline.stop();emit changed();};
  control.event=[this](QByteArray event){
   if(event.contains("CTRL-EVENT-SCAN-RESULTS")){if(active)scanResults();}
   else if(event.contains("CTRL-EVENT-CONNECTED")||event.contains("CTRL-EVENT-DISCONNECTED")){if(active)statusUpdate();}
   else if(event.contains("CTRL-EVENT-SSID-TEMP-DISABLED")){problem="连接失败，请检查密码或信号";emit changed();}
  };
 }
 QVariantList networks()const{return rows;}QString status()const{return stateText;}
 QString connectedSsid()const{return ssid;}QString address()const{return ip;}QString error()const{return problem;}
 bool busy()const{return configuring;}bool scanning()const{return finding;}
 Q_INVOKABLE void setActive(bool value){
  active=value;updatePolling();if(value)refresh();else {scanDeadline.stop();finding=false;}
 }
 void setSuspended(bool value){suspended=value;updatePolling();if(!value&&active)refresh();}
 Q_INVOKABLE void refresh(){
  if(configuring||suspended||finding)return;problem.clear();finding=true;emit changed();statusUpdate();
  control.request("SCAN",[this](QByteArray result){
   if(result!="OK"&&result!="FAIL-BUSY"){finding=false;problem="暂时无法查找网络，请重试";emit changed();return;}
   scanDeadline.start(20000);
  });
 }
 Q_INVOKABLE void join(QVariantMap network,QString password){
  if(configuring)return;problem.clear();auto hex=network["ssidHex"].toString().toLatin1();auto bytes=QByteArray::fromHex(hex);
  if(bytes.isEmpty()||bytes.size()>32||bytes.toHex()!=hex.toLower()||!network["supported"].toBool()){problem="此网络暂不支持连接";emit changed();return;}
  QByteArray key;if(network["secured"].toBool()){
   key=WifiData::psk(password.toUtf8(),bytes);
   if(key.isEmpty()){problem="密码需要 8–63 字节，或 64 位十六进制密钥";emit changed();return;}
  }
  configuring=true;finding=false;scanDeadline.stop();stateText="正在连接";emit changed();
  control.request("LIST_NETWORKS",[this,hex,key,bytes](QByteArray result){
   if(result.startsWith("FAIL")){configurationFailed(-1);return;}previousProfiles.clear();
   for(const auto &line:result.split('\n')){
    auto fields=line.split('\t');bool valid=false;int id=fields.value(0).toInt(&valid);
    if(valid&&WifiData::ssidBytes(fields.value(1))==bytes)previousProfiles+=id;
   }
   control.request("ADD_NETWORK",[this,hex,key](QByteArray result){
    bool valid=false;int id=result.toInt(&valid);if(!valid||id<0){configurationFailed(-1);return;}configure(id,hex,key);
   });
  });
 }
signals:void changed();
};
