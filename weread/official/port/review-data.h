// input: nested WeRead review rows; output: compact rows and deterministic sorting; pos: social data normalization.
#pragma once
#include <QJsonObject>
#include <QJsonArray>
#include <QVariantList>
#include <algorithm>
namespace ReviewData {
inline QJsonObject compactRow(QJsonObject row) {
    auto outer=row;
    while(row["review"].isObject())row=row["review"].toObject();
    QJsonObject out;
    for(auto key:{"reviewId","content","abstract","range","chapterUid","newRatingLevel","createTime"})
        if(row.contains(key))out[key]=row[key];
    if(!out.contains("reviewId"))out["reviewId"]=outer["reviewId"];
    for(auto key:{"likesCount","commentsCount"})out[key]=outer.contains(key)?outer[key].toInt():row.value(key).toInt();
    out["author"]=QJsonObject{{"name",row["author"].toObject()["name"]}};
    return out;
}
inline QVariantMap model(QJsonObject row){
    auto r=compactRow(row);
    return {{"id",r["reviewId"].toString()},{"author",r["author"].toObject()["name"].toString()},
        {"content",r["content"].toString()},{"quote",r["abstract"].toString()},{"range",r["range"].toString()},
        {"likes",r["likesCount"].toInt()},{"created",r["createTime"].toDouble()},{"comments",r["commentsCount"].toInt()}};
}
inline void sort(QVariantList &rows,bool likes){
    std::stable_sort(rows.begin(),rows.end(),[likes](const QVariant &a,const QVariant &b){
        auto x=a.toMap(),y=b.toMap();
        if(likes&&x["likes"].toInt()!=y["likes"].toInt())return x["likes"].toInt()>y["likes"].toInt();
        if(x["created"].toDouble()!=y["created"].toDouble())return x["created"].toDouble()>y["created"].toDouble();
        return x["id"].toString()<y["id"].toString();
    });
}
}
