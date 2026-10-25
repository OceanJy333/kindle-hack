// input: local WPA datagrams; output: serialized replies and events; pos: native Wi-Fi transport.
#pragma once
#include <QObject>
#include <QFile>
#include <QQueue>
#include <QSocketNotifier>
#include <QTemporaryDir>
#include <QTimer>
#include <functional>
#include <cerrno>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

class WpaControl : public QObject {
 using Reply=std::function<void(QByteArray)>;
 struct Request {QByteArray command;Reply reply;};
 QQueue<Request> pending;QTemporaryDir temporary{"/tmp/ocean-wifi-XXXXXX"};
 QSocketNotifier *notifier=nullptr;QTimer deadline;int fd=-1;bool waiting=false;
 void closeSocket(){
  deadline.stop();waiting=false;
  if(notifier){notifier->setEnabled(false);notifier->deleteLater();notifier=nullptr;}
  if(fd>=0){::close(fd);fd=-1;}
  ::unlink(QFile::encodeName(temporary.path()+"/control").constData());
 }
 void fail(){
  auto requests=std::move(pending);pending.clear();closeSocket();
  if(failed)failed();
  // Mutating requests are never replayed after an unknown outcome.
  for(auto &request:requests)if(request.reply)request.reply("FAIL-TRANSPORT");
 }
 bool openSocket(){
  if(!temporary.isValid())return false;
  fd=::socket(AF_UNIX,SOCK_DGRAM|SOCK_NONBLOCK|SOCK_CLOEXEC,0);
  if(fd<0)return false;
  sockaddr_un local{},remote{};local.sun_family=remote.sun_family=AF_UNIX;
  qstrncpy(local.sun_path,QFile::encodeName(temporary.path()+"/control").constData(),sizeof(local.sun_path));
  qstrncpy(remote.sun_path,"/run/wpa_supplicant/wlan0",sizeof(remote.sun_path));
  ::unlink(local.sun_path);
  if(::bind(fd,reinterpret_cast<sockaddr*>(&local),sizeof(local))||
     ::connect(fd,reinterpret_cast<sockaddr*>(&remote),sizeof(remote))){closeSocket();return false;}
  notifier=new QSocketNotifier(fd,QSocketNotifier::Read,this);
  connect(notifier,&QSocketNotifier::activated,this,[this]{receive();});
  pending.enqueue({"ATTACH",[this](QByteArray result){if(result!="OK")fail();}});
  return true;
 }
 void pump(){
  if(waiting||pending.isEmpty()||fd<0)return;
  waiting=true;const auto &line=pending.head().command;
  if(::send(fd,line.constData(),line.size(),MSG_NOSIGNAL)!=line.size()){fail();return;}
  deadline.start(4000);
 }
 void receive(){
  while(fd>=0){
   char bytes[65536];auto n=::recv(fd,bytes,sizeof(bytes),0);
   if(n<0){if(errno!=EAGAIN&&errno!=EWOULDBLOCK)fail();return;}
   QByteArray value(bytes,n);while(value.endsWith('\n')||value.endsWith('\r'))value.chop(1);
   if(value.startsWith('<')){if(event)event(value);continue;}
   if(!waiting||pending.isEmpty())continue;
   deadline.stop();waiting=false;auto reply=pending.dequeue().reply;
   if(reply)reply(value);pump();
  }
 }
public:
 std::function<void(QByteArray)> event;std::function<void()> failed;
 explicit WpaControl(QObject *parent=nullptr):QObject(parent){
  deadline.setSingleShot(true);connect(&deadline,&QTimer::timeout,this,[this]{fail();});
 }
 ~WpaControl(){closeSocket();}
 void request(QByteArray line,Reply reply={}){
  if(fd<0&&!openSocket()){if(failed)failed();if(reply)reply("FAIL-TRANSPORT");return;}
  pending.enqueue({std::move(line),std::move(reply)});pump();
 }
};
