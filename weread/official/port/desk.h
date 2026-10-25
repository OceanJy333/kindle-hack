// input: JSON agent data, local power and game processes; output: native desktop state; pos: Voyage application controller.
#pragma once
#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QProcess>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantMap>
#include <QCoreApplication>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include "power-client.h"
class Desk : public QObject {
 Q_OBJECT
 Q_PROPERTY(QVariantMap state READ state NOTIFY changed)
 Q_PROPERTY(QVariantMap notice READ notice NOTIFY changed)
 Q_PROPERTY(bool connected READ connected NOTIFY changed)
 Q_PROPERTY(bool gameActive READ gameActive NOTIFY changed)
 Q_PROPERTY(int battery READ battery NOTIFY changed)
 Q_PROPERTY(int light READ light NOTIFY changed)
 Q_PROPERTY(QString error READ error NOTIFY changed)
 QVariantMap data,notification;QTcpSocket socket;QTimer reconnect;QByteArray buffer;
 QProcess game;PowerClient hardware;bool playing=false;QString snoozed,gameId,gameWave,problem;
 QString root=QCoreApplication::applicationDirPath();
 QByteArray read(const QString &path){QFile f(path);return f.open(QIODevice::ReadOnly)?f.readAll().trimmed():QByteArray();}
 void compositor(){emit changed();}
 void selectNotice(){
  const auto list=data.value("notices").toList();QVariantMap next=list.isEmpty()?QVariantMap():list.first().toMap();
  if(next.value("key").toString()==snoozed)next.clear();
  bool opening=notification.isEmpty()&&!next.isEmpty();notification=next;
  if(opening&&playing){::kill(game.processId(),SIGUSR1);QTimer::singleShot(200,this,[this]{compositor();});}
 }
 void receive(){
  buffer+=socket.readAll();if(buffer.size()>2097152){socket.abort();buffer.clear();return;}
  int end;while((end=buffer.indexOf('\n'))>=0){const auto line=buffer.left(end);buffer.remove(0,end+1);auto doc=QJsonDocument::fromJson(line);if(!doc.isObject())continue;
   data=doc.object().toVariantMap();selectNotice();emit changed();
  }
 }
 void connectHost(){if(socket.state()!=QAbstractSocket::UnconnectedState)return;socket.connectToHost(QString::fromUtf8(read("/mnt/us/ocean-install/board-host")),18768);}
public:
 explicit Desk(QObject *parent=nullptr):QObject(parent){
  connect(&socket,&QTcpSocket::connected,this,[this]{socket.write("HELLO "+read("/mnt/us/agent-board-token")+"\n");emit changed();});
  connect(&socket,&QTcpSocket::disconnected,this,[this]{buffer.clear();emit changed();});
  connect(&socket,&QTcpSocket::readyRead,this,&Desk::receive);
  reconnect.setInterval(15000);connect(&reconnect,&QTimer::timeout,this,&Desk::connectHost);reconnect.start();connectHost();
  connect(&hardware,&PowerClient::changed,this,&Desk::changed);
  connect(&hardware,&PowerClient::lockedChanged,this,[this](bool locked){
   if(locked){reconnect.stop();if(playing)::kill(game.processId(),SIGUSR1);}
   else {reconnect.start();connectHost();if(playing&&notification.isEmpty())QTimer::singleShot(300,this,[this]{if(playing)::kill(game.processId(),SIGCONT);});}
  });
  connect(&game,QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),this,[this](int code,QProcess::ExitStatus){playing=false;if(code)problem="游戏退出，代码 "+QString::number(code);compositor();emit changed();});
  connect(&game,&QProcess::errorOccurred,this,[this](QProcess::ProcessError){playing=false;problem="游戏未能启动";compositor();emit changed();});
 }
 QVariantMap state()const{return data;}QVariantMap notice()const{return notification;}
 bool connected()const{return socket.state()==QAbstractSocket::ConnectedState;}bool gameActive()const{return playing;}
 int battery()const{return hardware.battery();}int light()const{return hardware.light();}QString error()const{return problem;}
 PowerClient *power(){return &hardware;}
 Q_INVOKABLE void setLight(int value){hardware.setLight(value);}
 Q_INVOKABLE void sleep(){hardware.send("sleep");}
 Q_INVOKABLE void dismiss(bool later){
  QString key=notification.value("key").toString();if(later)snoozed=key;
  else socket.write(QJsonDocument(QJsonObject{{"action","dismiss"},{"key",key}}).toJson(QJsonDocument::Compact)+"\n");
  notification.clear();if(playing){QTimer::singleShot(300,this,[this]{if(playing)::kill(game.processId(),SIGCONT);});}emit changed();
 }
 // wave "old" replays the refresh path from before the A2 fix, for filming the before/after comparison.
 Q_INVOKABLE void launchGame(QString id,QString wave=QString()){
  if(playing||!(id=="doom"||id=="mslug")||!(wave.isEmpty()||wave=="old"))return;problem.clear();playing=true;gameId=id;gameWave=wave;emit changed();
  QTimer::singleShot(350,this,[this]{game.start("/bin/sh",{root+"/games/native-launch.sh",gameId,gameWave});});
 }
 Q_INVOKABLE void refresh(){if(connected()){socket.abort();}connectHost();}
 Q_INVOKABLE void stopGame(){if(playing){::kill(game.processId(),SIGCONT);game.terminate();}}
signals:void changed();
};
