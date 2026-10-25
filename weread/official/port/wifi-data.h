// input: WPA scan text and passwords; output: network rows and PSK; pos: Wi-Fi data conversion.
#pragma once
#include <QVariantList>
#include <QVariantMap>
#include <QMap>
#include <QPasswordDigestor>
#include <QCryptographicHash>
#include <algorithm>

namespace WifiData {
inline QByteArray ssidBytes(QByteArray text){
 QByteArray result;
 for(int i=0;i<text.size();++i){
  if(text[i]!='\\'||i+1==text.size()){result+=text[i];continue;}
  char next=text[++i];
  if(next=='x'&&i+2<text.size()){
   bool valid=false;int value=text.mid(i+1,2).toInt(&valid,16);
   if(valid){result+=char(value);i+=2;continue;}
  }
  if(next=='n')result+='\n';else if(next=='r')result+='\r';
  else if(next=='t')result+='\t';else if(next=='e')result+=char(27);else result+=next;
 }
 return result;
}
inline QVariantList networks(QByteArray reply){
 QMap<QString,QVariantMap> unique;
 for(const auto &line:reply.split('\n')){
  auto fields=line.split('\t');if(fields.size()<5||!fields[0].contains(':'))continue;
  const auto ssid=ssidBytes(fields[4]);if(ssid.isEmpty()||ssid.size()>32)continue;
  const auto flags=fields[3];bool secured=flags.contains("PSK");
  bool supported=secured||!(flags.contains("WPA")||flags.contains("RSN")||flags.contains("WEP")||flags.contains("SAE")||flags.contains("OWE")||flags.contains("EAP"));
  QString security=secured?"密码网络":supported?"开放网络":"暂不支持此认证方式";
  auto hex=QString::fromLatin1(ssid.toHex());auto key=hex+security;
  QVariantMap row{{"ssid",QString::fromUtf8(ssid)},{"ssidHex",hex},{"signal",fields[2].toInt()},
                  {"secured",secured},{"supported",supported},{"security",security}};
  if(!unique.contains(key)||unique[key]["signal"].toInt()<row["signal"].toInt())unique[key]=row;
 }
 QVariantList result;for(const auto &row:unique)result+=row;
 std::sort(result.begin(),result.end(),[](const QVariant &a,const QVariant &b){return a.toMap()["signal"].toInt()>b.toMap()["signal"].toInt();});
 return result;
}
inline QByteArray psk(QByteArray password,QByteArray ssid){
 if(password.size()==64&&QByteArray::fromHex(password).size()==32){
  for(char ch:password)if(!((ch>='0'&&ch<='9')||(ch>='a'&&ch<='f')||(ch>='A'&&ch<='F')))return {};
  return password.toLower();
 }
 if(password.size()<8||password.size()>63)return {};
 return QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha1,password,ssid,4096,32).toHex();
}
}
