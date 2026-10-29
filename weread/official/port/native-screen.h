// input: Qt software frames and evdev; output: EPDC pixels and native input; pos: direct framebuffer backend.
#pragma once
#include <QObject>
#include <QQuickWindow>
#include <QTimer>
#include <QSocketNotifier>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QProcess>
#include <QImage>
#include <QFile>
#include <sys/ioctl.h>
#include <linux/input.h>
#include "desk.h"
extern "C" {
#include "display.h"
}
class NativeScreen : public QObject {
 QQuickWindow *window;Desk *desk;int touch=-1,keys=-1;bool capturing=false,scheduled=false,sleeping=false,force=true,owned=true;
 QImage prior;QRect cleanArea;int large=0;bool panelFrame=false;
 QRect toPanel(QRect r)const{
  if(r.isEmpty())return r;
  double sx=double(display_panel_width())/display_width(),sy=double(display_panel_height())/display_height();
  return QRect(QPoint(int(r.left()*sx),int(r.top()*sy)),QPoint(int(r.right()*sx)+1,int(r.bottom()*sy)+1));
 }
 void capture(){
  scheduled=false;if(capturing||sleeping||(desk&&desk->gameActive()&&desk->notice().isEmpty()))return;
  capturing=true;QImage image=window->grabWindow().convertToFormat(QImage::Format_Grayscale8);capturing=false;
  // QT_SCALE_FACTOR makes Qt render at panel size; otherwise frames are logical-canvas sized and get resampled.
  panelFrame=!image.isNull()&&image.width()==display_panel_width()&&qAbs(image.height()-display_panel_height())<=1;
  if(panelFrame&&image.height()!=display_panel_height())image=image.copy(0,0,image.width(),qMin(image.height(),display_panel_height()));
  if(image.isNull()||(!panelFrame&&(image.width()!=display_width()||image.height()!=display_height())))return;
  int left=image.width(),right=-1,top=image.height(),bottom=-1;
  if(force||prior.size()!=image.size()){left=top=0;right=image.width()-1;bottom=image.height()-1;}
  else for(int y=0;y<image.height();y++){
   const uchar *a=image.constScanLine(y),*b=prior.constScanLine(y);
   for(int x=0;x<image.width();x++)if((a[x]/17)!=(b[x]/17)){left=qMin(left,x);right=qMax(right,x);top=qMin(top,y);bottom=y;}
  }
  QRect requested=(panelFrame?toPanel(cleanArea):cleanArea).intersected(image.rect());
  if(!requested.isEmpty()){left=qMin(left,requested.left());right=qMax(right,requested.right());top=qMin(top,requested.top());bottom=qMax(bottom,requested.bottom());}
  if(right>=left){int w=right-left+1,h=bottom-top+1;bool substantial=w*h>image.width()*image.height()/4;
   if(substantial)large++;int mode=(force||!requested.isEmpty())?DISPLAY_CLEAN:DISPLAY_GRAY;
   if(large>=6){mode=DISPLAY_CLEAN;large=0;}
   if(panelFrame)display_update_panel(0,left,top,w,h,mode,image.constScanLine(top)+left,image.bytesPerLine());
   else display_update_stride(0,left,top,w,h,mode,image.constScanLine(top)+left,image.bytesPerLine());
   prior=image;force=false;cleanArea=QRect();
  }
 }
 void schedule(){if(!scheduled&&!capturing){scheduled=true;QTimer::singleShot(20,this,[this]{capture();});}}
 void lock(){
  sleeping=true;int w=display_width(),h=display_height();QByteArray gray(w*h,char(255));
  for(int y=h/2-110;y<h/2+90;y++)for(int x=w/2-85;x<w/2+85;x++){
   bool body=y>=h/2-10&&(x<w/2-73||x>=w/2+73||y<h/2+2||y>=h/2+78);
   bool arch=y<h/2-10&&x>=w/2-55&&x<w/2+55&&(x<w/2-43||x>=w/2+43||y<h/2-98);
   if(body||arch)gray[y*w+x]=0;
  }
  int submitted=display_update(0,0,0,w,h,DISPLAY_CLEAN,reinterpret_cast<const uint8_t*>(gray.constData()));
  int completed=submitted?submitted:display_wait();
  qInfo("POWER_SCREEN submitted=%d completed=%d errno=%d",submitted,completed,completed?errno:0);
  if(!completed&&desk)desk->power()->send("ready");
 }
 void ownership(){
  if(!desk)return;
  bool wants=!desk->gameActive()||!desk->notice().isEmpty();
  if(!wants&&owned){ioctl(touch,EVIOCGRAB,0);ioctl(keys,EVIOCGRAB,0);owned=false;}
  else if(wants&&!owned){QTimer::singleShot(250,this,[this]{
    if(ioctl(touch,EVIOCGRAB,1)==0){ioctl(keys,EVIOCGRAB,1);owned=true;force=true;schedule();}
  });}
 }
public:
 NativeScreen(QQuickWindow *w,Desk *d=nullptr):QObject(w),window(w),desk(d){
  if(display_open())qFatal("Cannot open e-ink framebuffer");
  touch=touch_open();keys=input_open_pagekeys();
  if(touch<0)qFatal("Cannot open touch device");if(keys>=0)ioctl(keys,EVIOCGRAB,1);
  auto tn=new QSocketNotifier(touch,QSocketNotifier::Read,this);
  connect(tn,&QSocketNotifier::activated,this,[this]{int x,y;while(touch_read(touch,&x,&y)){
   if(!owned||sleeping)continue;double ws=window->width()/double(display_width());QPointF p(x*ws,y*ws);
   QMouseEvent press(QEvent::MouseButtonPress,p,p,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QCoreApplication::sendEvent(window,&press);
   QTimer::singleShot(90,this,[this,p]{QMouseEvent release(QEvent::MouseButtonRelease,p,p,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QCoreApplication::sendEvent(window,&release);});
  }});
  if(keys>=0){auto kn=new QSocketNotifier(keys,QSocketNotifier::Read,this);connect(kn,&QSocketNotifier::activated,this,[this]{struct input_event e;while(::read(keys,&e,sizeof(e))==sizeof(e)){
   if(!owned||sleeping||e.type!=EV_KEY||e.value!=1)continue;int key=e.code==KEY_PAGEUP?Qt::Key_PageUp:e.code==KEY_PAGEDOWN?Qt::Key_PageDown:0;
   if(key){QKeyEvent down(QEvent::KeyPress,key,Qt::NoModifier),up(QEvent::KeyRelease,key,Qt::NoModifier);QCoreApplication::sendEvent(window,&down);QCoreApplication::sendEvent(window,&up);}
  }});}
  connect(window,&QQuickWindow::afterRendering,this,[this]{schedule();},Qt::DirectConnection);
  if(desk){
  connect(desk,&Desk::changed,this,[this]{ownership();});
  connect(desk->power(),&PowerClient::lockedChanged,this,[this](bool locked){
   if(locked)QTimer::singleShot(desk->gameActive()?250:0,this,[this]{if(desk->power()->locked())lock();});
   else {sleeping=false;force=true;schedule();}
  });
  if(desk->power()->locked())lock();
  }
  QTimer::singleShot(500,this,[this]{schedule();});
 }
 void stopUpdates(){sleeping=true;ioctl(touch,EVIOCGRAB,0);if(keys>=0)ioctl(keys,EVIOCGRAB,0);owned=false;}
 void clean(QRect rect=QRect()){if(rect.isEmpty())force=true;else cleanArea=cleanArea.united(rect);schedule();}
 ~NativeScreen(){::close(touch);if(keys>=0)::close(keys);display_close();}
};
