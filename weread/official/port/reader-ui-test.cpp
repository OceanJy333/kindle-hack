// input: standalone QML and synthetic controller; output: rendered screens and integration assertions; pos: host Qt UI test.
#include "reader.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickWindow>
#include <QDir>
#include <QTimer>
#include <QImage>
#include <QMouseEvent>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>
// Qt 6's QPA hook supplies the host mouse device to offscreen Quick windows.
QT_BEGIN_NAMESPACE
Q_GUI_EXPORT void qt_handleMouseEvent(QWindow*,const QPointF&,const QPointF&,
    Qt::MouseButtons,Qt::MouseButton,QEvent::Type,Qt::KeyboardModifiers,int);
QT_END_NAMESPACE
static QVariant invoke(QObject *object, const char *name, QVariant a = {}, QVariant b = {}) {
    QVariant result;
    bool ok;
    if (b.isValid()) ok=QMetaObject::invokeMethod(object,name,Q_RETURN_ARG(QVariant,result),Q_ARG(QVariant,a),Q_ARG(QVariant,b));
    else if (a.isValid()) ok=QMetaObject::invokeMethod(object,name,Q_RETURN_ARG(QVariant,result),Q_ARG(QVariant,a));
    else ok=QMetaObject::invokeMethod(object,name,Q_RETURN_ARG(QVariant,result));
    if(!ok) qFatal("Could not invoke %s",name);
    return result;
}
int main(int argc,char **argv) {
    QGuiApplication app(argc,argv);
    if(argc!=3)return 2;
    const QString source=QString::fromLocal8Bit(argv[1]),output=QString::fromLocal8Bit(argv[2]);
    QDir().mkpath(output);
    int font=QFontDatabase::addApplicationFont(source+"/../../../build/fonts/NotoSansCJKsc-Regular.otf");
    if(font<0)qFatal("Test CJK font missing");
    const QString family=QFontDatabase::applicationFontFamilies(font).value(0);
    app.setFont(QFont(family));
    qmlRegisterType<Reader>("remarkable_weread",1,0,"ReaderPageItem");
    QQmlApplicationEngine engine;
    bool warnings=false;
    QObject::connect(&engine,&QQmlEngine::warnings,&app,[&](const QList<QQmlError> &errors){for(auto e:errors)qWarning()<<e;warnings=true;});
    QQmlComponent component(&engine,QUrl::fromLocalFile(source+"/../tests/reader-fixture.qml"));
    auto controller=component.create();
    if(!controller){qWarning()<<component.errors();return 3;}
    engine.rootContext()->setContextProperty("appController",controller);
    engine.rootContext()->setContextProperty("bookNotes",controller);
    engine.rootContext()->setContextProperty("uiFontFamily",family);
    QQmlComponent inkComponent(&engine);
    inkComponent.setData("import QtQuick; QtObject { function clearGhosting() {} }",QUrl());
    auto eink=inkComponent.create();engine.rootContext()->setContextProperty("eink",eink);
    engine.load(QUrl::fromLocalFile(source+"/ReaderApp.qml"));
    if(engine.rootObjects().isEmpty())return 4;
    auto window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    QTimer::singleShot(500,&app,[&] {
        QJsonObject results;
        auto check=[&](const char *name,bool passed){results[name]=passed;if(!passed)qWarning("FAILED %s",name);};
        auto shot=[&](const char *name){QCoreApplication::processEvents();return window->grabWindow().save(output+"/"+name+".png");};
        check("loginRendered",shot("login"));
        controller->setProperty("currentPage","bookshelf");check("shelfRendered",shot("shelf"));
        auto reading=window->findChild<QObject*>("readingView");
        auto reader=window->findChild<Reader*>("readerPage");
        check("nativeReaderExists",reading&&reader);
        if(!reading||!reader){app.exit(1);return;}
        invoke(controller,"beginReading");check("localCursorRestored",reader->page()>0);
        check("replacementDidNotSaveCloud",controller->property("savedPositionCount").toInt()==0);
        check("readingRendered",reader->pages()>2&&shot("reading"));
        invoke(reading,"turn",1);check("pageTurnSavedPosition",controller->property("savedPositionCount").toInt()>0&&!controller->property("lastPosition").toMap().isEmpty());
        controller->setProperty("cloudProgress",QVariantMap{{"summary","可恢复的云端位置"},{"chapterOffset",100}});
        int saves=controller->property("savedPositionCount").toInt();invoke(controller,"loadChapter",1);
        check("cloudAnchorRestored",controller->property("restored").toBool()&&reader->page()>0);
        check("cloudRestoreDidNotOverwrite",controller->property("savedPositionCount").toInt()==saves);
        reader->setPage(reader->pages()-1);invoke(reading,"turn",1);
        check("nextChapterAtStart",controller->property("uid").toInt()==2&&reader->page()==0);
        invoke(reading,"turn",-1);
        check("previousChapterAtEnd",controller->property("uid").toInt()==1&&reader->page()==reader->pages()-1&&!controller->property("atEnd").toBool());
        reading->setProperty("toolsOpen",true);reading->setProperty("panel","toc");check("tocRendered",shot("directory"));
        reading->setProperty("panel","layout");invoke(reading,"changeLayout","fontSize",36);
        check("layoutPersisted",reader->property("fontSize").toInt()==36&&controller->property("savedLayoutCount").toInt()==1&&shot("layout"));
        reading->setProperty("toolsOpen",false);reading->setProperty("panel","");reader->setPage(0);
        shot("marked-page");QPointF markPoint(-1,-1);
        for(int y=0;y<150&&markPoint.x()<0;y+=4)for(int x=48;x<1024;x+=8)
            if(reader->hitTestAt(x,y).value("type").toInt()==Reader::NOTE){markPoint=QPointF(x,y+86);break;}
        check("nativeMarkHitTest",markPoint.x()>=0);
        if(markPoint.x()>=0){
            window->requestActivate();QCoreApplication::processEvents();
            qt_handleMouseEvent(window,markPoint,markPoint,Qt::LeftButton,Qt::LeftButton,QEvent::MouseButtonPress,Qt::NoModifier,1000);
            qt_handleMouseEvent(window,markPoint,markPoint,Qt::NoButton,Qt::LeftButton,QEvent::MouseButtonRelease,Qt::NoModifier,1090);
        }
        QCoreApplication::processEvents();QCoreApplication::processEvents();
        check("notesRangeOpened",controller->property("reviewRange").toString()=="12-24");check("notesRendered",shot("notes"));
        invoke(reading,"back");check("backToShelf",controller->property("currentPage").toString()=="bookshelf");
        window->setProperty("offlineOpen",true);check("offlineRendered",shot("offline"));
        invoke(window,"exitReader",false);check("exitInvoked",controller->property("exited").toBool());
        check("noQmlWarnings",!warnings);
        bool passed=true;for(auto value:results)passed=passed&&value.toBool();
        puts(QJsonDocument(results).toJson(QJsonDocument::Compact).constData());
        app.exit(passed?0:1);
    });
    return app.exec();
}
