// input: local power socket; output: battery, light and suspend handshake; pos: native hardware client.
#pragma once
#include <QObject>
#include <QLocalSocket>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtMath>
class PowerClient : public QObject {
 Q_OBJECT
 QLocalSocket socket;QTimer retry;QByteArray buffer;int percent=-1,raw=0,maximum=4095;bool asleep=false;
 void receive(){
  buffer+=socket.readAll();if(buffer.size()>8192){buffer.clear();socket.abort();return;}
  int end;while((end=buffer.indexOf('\n'))>=0){
   auto doc=QJsonDocument::fromJson(buffer.left(end));buffer.remove(0,end+1);if(!doc.isObject())continue;
   auto value=doc.object();if(!value.contains("locked"))continue;
   percent=value.value("battery").toInt(-1);raw=value.value("brightness").toInt();maximum=qMax(1,value.value("maxBrightness").toInt(4095));
   bool next=value.value("locked").toBool();bool transition=next!=asleep;asleep=next;
   emit changed();if(transition){qInfo("POWER_STATE locked=%d",asleep);emit lockedChanged(asleep);}
  }
 }
 void open(){if(socket.state()==QLocalSocket::UnconnectedState)socket.connectToServer("/tmp/ocean-power.sock");}
public:
 explicit PowerClient(QObject *parent=nullptr):QObject(parent){
  connect(&socket,&QLocalSocket::readyRead,this,&PowerClient::receive);
  connect(&socket,&QLocalSocket::connected,this,[this]{retry.stop();});
  connect(&socket,&QLocalSocket::disconnected,this,[this]{buffer.clear();retry.start();emit changed();});
  retry.setInterval(10000);connect(&retry,&QTimer::timeout,this,&PowerClient::open);retry.start();open();
 }
 int battery()const{return percent;}
 // A square curve keeps the low end useful for reading in a dark room.
 int light()const{return qRound(24*qSqrt(qMax(0,raw)/double(maximum)));}
 bool locked()const{return asleep;}
 void send(const QByteArray &line){if(socket.state()==QLocalSocket::ConnectedState){socket.write(line+'\n');socket.flush();}}
 void setLight(int value){value=qBound(0,value,24);send("light "+QByteArray::number(qRound(maximum*value*value/576.0)));}
signals:
 void changed();
 void lockedChanged(bool locked);
};
