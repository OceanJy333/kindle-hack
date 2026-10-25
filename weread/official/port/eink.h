// input: official QML refresh hints; output: nonblocking compositor requests; pos: renderer hardware boundary.
#pragma once
#include <QObject>
#include <QRect>
#include <QVariantList>
#include <fcntl.h>
#include <QtGlobal>
#include <unistd.h>
class Eink : public QObject {
    Q_OBJECT
public:
    explicit Eink(QObject *parent=nullptr) : QObject(parent) {}
    Q_INVOKABLE bool clearGhosting() { emit refreshRequested(QRect());return true; }
    Q_INVOKABLE bool openTab(int) { return false; }
    Q_INVOKABLE bool clearRegion(int x,int y,int w,int h,int=1) { if(w<=0||h<=0)return false;emit refreshRequested(QRect(x,y,w,h));return true; }
    Q_INVOKABLE bool clearRegions(const QVariantList &regions) {
        bool ok=true;
        for(const auto &item:regions){QRect r=item.toRect();ok=clearRegion(r.x(),r.y(),r.width(),r.height())&&ok;}
        return ok;
    }
signals:
    void refreshRequested(QRect rect);
    void frameCommitted(QRect rect);
};
