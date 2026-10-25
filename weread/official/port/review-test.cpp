// input: nested API fixtures; output: sorting and cache normalization assertions; pos: paragraph review regression test.
#include "review-data.h"
#include <cstdio>
int main(){
 int failures=0;auto check=[&](bool ok,const char *name){printf("%s %s\n",ok?"PASS":"FAIL",name);if(!ok)++failures;};
 QJsonObject a{{"reviewId","a"},{"likesCount",35},{"commentsCount",2},{"review",QJsonObject{{"content","first"},{"createTime",10},{"author",QJsonObject{{"name","A"}}}}}};
 auto compact=ReviewData::compactRow(a);check(compact["likesCount"].toInt()==35,"retain outer likes");
 check(compact["reviewId"]=="a","retain outer ID");check(!compact.contains("review"),"cache contains compact fields");
 auto twice=ReviewData::compactRow(compact);check(twice==compact,"cache normalization is idempotent");
 QJsonObject b{{"reviewId","b"},{"likesCount",6},{"content","second"},{"createTime",30}};
 QJsonObject c{{"reviewId","c"},{"likesCount",35},{"content","third"},{"createTime",20}};
 QVariantList rows{ReviewData::model(b),ReviewData::model(a),ReviewData::model(c)};
 ReviewData::sort(rows,true);check(rows[0].toMap()["id"]=="c"&&rows[1].toMap()["id"]=="a"&&rows[2].toMap()["id"]=="b","likes descending and time tie break");
 ReviewData::sort(rows,false);check(rows[0].toMap()["id"]=="b"&&rows[2].toMap()["id"]=="a","latest descending");
 check(ReviewData::model(QJsonObject{{"review",QJsonObject{{"content","zero"}}}})["likes"].toInt()==0,"omitted count is zero");
 return failures?1:0;
}
