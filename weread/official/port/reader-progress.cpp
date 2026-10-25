// input: server text anchor and rendered cursor; output: stable chapter position; pos: reader sync mapping.
#include "reader.h"
#include <QTextBlock>
#include <QTextCursor>
#include <QRegularExpression>
#include <QFile>
#include <QUrl>
static QString compact(const QString &text,QVector<int> *positions=nullptr) {
    QString out;for(int i=0;i<text.size();++i)if(!text[i].isSpace()&&text[i]!=QChar::ObjectReplacementCharacter){out+=text[i];if(positions)positions->append(i);}return out;
}
bool Reader::restoreProgress(QString summary,int chapterOffset) {
    QVector<int> positions;auto plain=compact(document.toPlainText(),&positions),anchor=compact(summary);
    if(anchor.isEmpty()){if(chapterOffset==0){setPage(0);return true;}return false;}
    auto index=plain.indexOf(anchor);
    if(index<0||plain.indexOf(anchor,index+1)>=0)return false;
    auto block=document.findBlock(positions[index]);setCursor(block.blockNumber(),positions[index]-block.position());return true;
}
QVariantMap Reader::progressPosition(int index,int offset) const {
    auto block=document.findBlockByNumber(index);if(!block.isValid())return {};
    auto text=document.toPlainText();int pos=block.position()+qBound(0,offset,block.length()-1);
    QString summary=compact(text.mid(pos)).left(20);if(summary.isEmpty())return {};
    // The native EPUB protocol uses source XHTML character positions, never Qt block/page numbers.
    if(source.size()!=1)return {};
    auto value=source.first().toMap();QString raw=value.value("richHtml").toString();
    if(!raw.contains(QRegularExpression("<body\\b",QRegularExpression::CaseInsensitiveOption))){
        QFile file(QUrl(value.value("baseUrl").toString()).toLocalFile());
        if(!file.open(QIODevice::ReadOnly))return {};
        QVector<int> positions;auto text=compact(QString::fromUtf8(file.readAll()),&positions);int i=text.indexOf(summary);
        if(i<0||text.indexOf(summary,i+1)>=0)return {};
        return {{"chapterOffset",positions[i]},{"summary",summary}};
    }
    QString visible;QVector<int> rawPositions;bool body=false,inTag=false;QString tag;
    for(int i=0;i<raw.size();++i){auto c=raw[i];
      if(c=='<'){inTag=true;tag.clear();continue;}
      if(inTag){if(c=='>'){inTag=false;if(tag.startsWith("body",Qt::CaseInsensitive))body=true;if(tag.startsWith("/body",Qt::CaseInsensitive))body=false;}else tag+=c;continue;}
      if(!body||c.isSpace())continue;
      if(c=='&'){int end=raw.indexOf(';',i);if(end>i&&end-i<12){QTextDocument entity;entity.setHtml(raw.mid(i,end-i+1));for(auto decoded:entity.toPlainText())if(!decoded.isSpace()){visible+=decoded;rawPositions<<i;}i=end;continue;}}
      visible+=c;rawPositions<<i;
    }
    auto anchor=visible.indexOf(summary);if(anchor<0||visible.indexOf(summary,anchor+1)>=0)return {};
    return {{"chapterOffset",rawPositions[anchor]},{"summary",summary}};
}
