// input: synthetic EPUB and signing fixtures; output: assertions; pos: device-side sync regression test.
#include "reader.h"
#include "progress-protocol.h"
#include <QGuiApplication>
#include <QTextDocument>
#include <cstdio>
int main(int argc,char **argv){
    QGuiApplication app(argc,argv);int failures=0;
    auto check=[&](bool ok,const char *name){printf("%s %s\n",ok?"PASS":"FAIL",name);if(!ok)++failures;};
    Reader reader;reader.setWidth(1072);reader.setHeight(800);
    QString html="<html><head><title>not body text</title></head><body><p>甲乙丙丁戊己庚辛壬癸一二三四五六七八九十</p><p>实体&amp;符号与中文以及不同字体排版的同步位置测试</p></body></html>";
    reader.setContent({QVariantMap{{"richHtml",html}}});
    auto first=reader.progressPosition(0,0);
    check(first["chapterOffset"].toInt()==html.indexOf("甲"),"source offset excludes HTML headers");
    check(first["summary"].toString().size()==20,"summary contains twenty characters");
    check(reader.restoreProgress(first["summary"].toString(),first["chapterOffset"].toInt()),"cloud anchor restores");
    auto entity=reader.progressPosition(1,0);
    check(entity["chapterOffset"].toInt()==html.indexOf("实体"),"HTML entity mapping");
    check(reader.restoreProgress(entity["summary"].toString(),entity["chapterOffset"].toInt()),"entity anchor restores");
    reader.setFontSize(54);
    check(reader.restoreProgress(first["summary"].toString(),first["chapterOffset"].toInt()),"font-independent anchor");
    check(!reader.restoreProgress("absent anchor",999),"missing anchor fails explicitly");
    check(reader.progressPosition(999,0).isEmpty(),"invalid cursor cannot upload");
    check(ProgressProtocol::encode("1234567890")=="e80329f0775bcd15g0109e5", "encoded fixture");
    return failures?1:0;
}
