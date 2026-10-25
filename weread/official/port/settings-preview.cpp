// input: packaged QML and isolated demonstration objects; output: keyboard render and touch checks; pos: native settings UI test.
#include <QGuiApplication>
#include <QStyleHints>
#include <QFontDatabase>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlComponent>
#include <QQuickWindow>
#include <QMouseEvent>
#include <QTimer>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>
// QtTest's QPA injection hook is exported by the pinned QtGui runtime.
QT_BEGIN_NAMESPACE
Q_GUI_EXPORT void qt_handleMouseEvent(QWindow*,const QPointF&,const QPointF&,
                                      Qt::MouseButtons,Qt::MouseButton,QEvent::Type,
                                      Qt::KeyboardModifiers,int);
QT_END_NAMESPACE
int main(int argc,char **argv){
 QGuiApplication app(argc,argv);if(argc!=3)return 2;app.styleHints()->setCursorFlashTime(0);
 QString root=QString::fromLocal8Bit(argv[1]),output=QString::fromLocal8Bit(argv[2]);
 int font=QFontDatabase::addApplicationFont(root+"/fonts/NotoSansCJKsc-Regular.otf");
 QQmlApplicationEngine engine;engine.addImportPath(root+"/qml-modules");
 QQmlComponent fakeWifi(&engine),fakeDesk(&engine);
 fakeWifi.setData(R"(import QtQuick
 QtObject { property var networks: []; property string status: "尚未连接"; property string connectedSsid: ""; property string address: "";
 property string error: ""; property bool busy: false; property bool scanning: false; signal changed();
 function setActive(active) {} function refresh() {} function join(network,password) {} })",QUrl());
 fakeDesk.setData("import QtQuick; QtObject {property int light: 5; function setLight(value) {light=value} }",QUrl());
 auto wifi=fakeWifi.create(),desk=fakeDesk.create();if(!wifi||!desk)return 3;
 engine.rootContext()->setContextProperty("wifi",wifi);engine.rootContext()->setContextProperty("desk",desk);
 engine.rootContext()->setContextProperty("uiFontFamily",QFontDatabase::applicationFontFamilies(font).value(0));
 engine.loadData(R"(import QtQuick
 import QtQuick.Window
 Window {width:1072;height:1448;visible:true;color:"white"
 NativeSettings {anchors.fill:parent;anchors.bottomMargin:124;selected:({ssid:"界面测试网络",secured:true})}
 Text {x:56;y:28;text:"设置";font.family:uiFontFamily;font.pixelSize:44;font.bold:true}
 Rectangle {x:56;y:122;width:960;height:2;color:"black"}
 })",QUrl::fromLocalFile(root+"/SettingsPreview.qml"));
 if(engine.rootObjects().isEmpty())return 4;
 auto window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());
 QTimer::singleShot(700,&app,[&]{
  int timestamp=0;
  auto tap=[&](int x,int y){
   // Offscreen windows need a rendered frame before positioner hit testing.
   window->grabWindow();window->requestActivate();QCoreApplication::processEvents();
   QPointF p(x,y),global=window->mapToGlobal(QPoint(x,y));timestamp+=1000;
   qt_handleMouseEvent(window,p,global,Qt::LeftButton,Qt::LeftButton,QEvent::MouseButtonPress,Qt::NoModifier,timestamp);
   qt_handleMouseEvent(window,p,global,Qt::NoButton,Qt::LeftButton,QEvent::MouseButtonRelease,Qt::NoModifier,timestamp+90);
   QCoreApplication::processEvents();
  };
  tap(98,720);tap(98,812);tap(584,1088);tap(906,1088);tap(132,1088);tap(98,812);tap(296,1088);tap(98,812);
  auto field=window->findChild<QObject*>("wifiPassword");
  bool typed=field&&field->property("text").toString()=="1qQ!";
  bool saved=window->grabWindow().save(output);
  auto result=QJsonDocument(QJsonObject{{"keyboardTouchPassed",typed},{"renderSaved",saved},
    {"syntheticSequence",field?field->property("text").toString():"field missing"}}).toJson(QJsonDocument::Compact);
  puts(result.constData());
  app.exit(typed&&saved?0:1);
 });
 return app.exec();
}
