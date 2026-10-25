// input: selected chapter underline and authenticated API; output: paginated paragraph reviews; pos: reader comment flow.
#include "notes.h"
#include "library.h"
#include "review-data.h"
#include <QRegularExpression>
#include <QTimer>
QString Notes::rangeKey() const{return "r:"+reviewChapter+":"+reviewRange;}
void Notes::loadReviews(const QString &range){
    if(!QRegularExpression("^[0-9]+-[0-9]+$").match(range).hasMatch())return;
    ++reviewGeneration;reviewRange=range;reviewChapter=chapterKey();rangeLoading=false;
    auto cached=raw[rangeKey()].toObject();rangeRows=cached["reviews"].toArray();
    reviewTotal=cached["total"].toInt();reviewOffset=cached["offset"].toInt();rangeHasMore=cached["hasMore"].toBool();reviewStatus.clear();
    emit changed();fetchRange(false);
}
void Notes::setReviewSort(const QString &value){
    if((value!="likes"&&value!="latest")||value==reviewSort)return;
    reviewSort=value;emit changed();
    if(value=="likes"&&rangeHasMore&&!rangeLoading)fetchRange(true);
}
void Notes::loadMoreReviews(){if(rangeHasMore)fetchRange(true);}
QVariantList Notes::reviews() const{
    QVariantList out;for(auto row:rangeRows){auto m=ReviewData::model(row.toObject());if(!m["content"].toString().isEmpty())out<<m;}
    ReviewData::sort(out,reviewSort=="likes");return out;
}
void Notes::fetchRange(bool more){
    if(bookId.isEmpty()||reviewRange.isEmpty()||rangeLoading)return;
    if(library->isOfflineMode()||!library->loggedIn()){
        reviewStatus=rangeRows.isEmpty()?"离线，暂无这段评论的缓存":"离线 · 显示已缓存评论";emit changed();return;
    }
    const int gen=++reviewGeneration,bookGeneration=generation,offset=more?reviewOffset:0;
    const auto key=rangeKey(),range=reviewRange;
    rangeLoading=true;reviewStatus.clear();emit changed();
    QJsonObject body{{"bookId",bookId},{"chapterUid",reviewChapter.toInt()},
        {"reviews",QJsonArray{QJsonObject{{"range",range},{"maxIdx",offset},{"count",100},{"synckey",0}}}}};
    library->webPost("/web/book/readReviews",body,[this,gen,bookGeneration,key,range,offset,more](QJsonObject data,QString error){
        if(gen!=reviewGeneration||bookGeneration!=generation)return;
        rangeLoading=false;QJsonObject group;
        for(auto value:data["reviews"].toArray())if(value.toObject()["range"].toString()==range)group=value.toObject();
        if(!error.isEmpty()||group.isEmpty()){reviewStatus=error.contains("过期")?(rangeRows.isEmpty()?"登录过期，请重新扫码":"登录过期 · 显示缓存"):"评论加载失败，点此重试";emit changed();return;}
        auto incoming=group["pageReviews"].toArray();if(!more)rangeRows={};
        QSet<QString> ids;for(auto row:rangeRows)ids.insert(row.toObject()["reviewId"].toString());
        int added=0;
        for(auto row:incoming){auto r=ReviewData::compactRow(row.toObject());auto id=r["reviewId"].toString();
            if(!id.isEmpty()&&!ids.contains(id)){rangeRows.append(r);ids.insert(id);++added;}}
        reviewTotal=group["totalCount"].toInt();reviewOffset=offset+incoming.size();rangeHasMore=group["hasMore"].toVariant().toBool();
        if(more&&!added&&rangeHasMore){reviewStatus="未取得更多评论，点此重试";rangeHasMore=false;}
        raw[key]=QJsonObject{{"reviews",rangeRows},{"total",reviewTotal},{"offset",reviewOffset},{"hasMore",rangeHasMore}};save();
        int highest=0;for(auto row:rangeRows)highest=qMax(highest,row.toObject()["likesCount"].toInt());
        qInfo("PARAGRAPH_REVIEWS loaded=%d total=%d highestLikes=%d more=%d",int(rangeRows.size()),reviewTotal,highest,rangeHasMore);
        emit changed();
        if(rangeHasMore&&reviewSort=="likes")QTimer::singleShot(30,this,[this,gen]{if(gen==reviewGeneration)fetchRange(true);});
    });
}
