#include "sourcecodegenerator.h"

#include <QFile>
#include <QDomDocument>
#include <QDomElement>
#include <QDebug>

SourceCodeGenerator::SourceCodeGenerator(QObject *parent) :
    QObject(parent)
{
}


SourceCodeGenerator::~SourceCodeGenerator() {
}

void SourceCodeGenerator::loadRule(const QString &fileName) {
    QFile f(fileName);

    if (f.open(QIODevice::ReadOnly)) {
        QByteArray data = f.readAll();
        f.close();
        ruleFromJSON(data);
    }
    else qDebug() << "Error: Unable to load rules from file " << fileName;
}

void SourceCodeGenerator::ruleFromJSON(const QByteArray &json) {
    QJsonParseError err;
    rule = QJsonDocument::fromJson(json, &err);
}

QString SourceCodeGenerator::applyRule(const QDomDocument &xml) {
    QDomElement alg = xml.firstChildElement("algorithm");
    QString code = processElement(alg, 0);
    return code;
}

QString SourceCodeGenerator::processElement(const QDomNode &element, int level) {
    QString sp = QString("  ").repeated(level);
    QJsonObject obj = rule.object().value(element.nodeName()).toObject();
    QString tpl = sp+obj.value("template").toString();
    for(int i = 0; i < element.attributes().size(); ++i) {
       if(obj.contains("list")) {
           QJsonArray list = obj["list"].toArray();
           if (list.contains(QJsonValue(element.attributes().item(i).nodeName()))) {
                QStringList sl = element.attributes().item(i).nodeValue().split(obj["separator"].toString());
                QString prefix = obj["prefix"].toString();
                QString suffix = obj["suffix"].toString();
                for(int k=0; k < sl.size(); ++k) {
                    QString s = sl[k];
                    s = prefix + s + suffix;
                    s.replace("%$%", sl[k]);
                    sl[k] = s;
                }
                tpl.replace("%"+element.attributes().item(i).nodeName() + "%", sl.join(obj["glue"].toString()));
           }
           tpl.replace("%"+element.attributes().item(i).nodeName() + "%", element.attributes().item(i).nodeValue());
       }
       else
           tpl.replace("%"+element.attributes().item(i).nodeName() + "%", element.attributes().item(i).nodeValue());

    }
    tpl.replace("\n","\n" + sp);
    tpl.replace("\t", sp);

    for(int i = 0; i < element.childNodes().size(); ++i) {
        if (element.childNodes().item(i).nodeName() == "branch") {
            QDomNode branch = element.childNodes().item(i);
            QString bt = QString("%branch%1%").arg(i + 1);
            QStringList body;
            for(int j = 0; j < branch.childNodes().size(); ++j) {

                body << processElement(branch.childNodes().item(j), level + 1);
            }
            tpl.replace(bt, "\n" + body.join("\n"));
        }
    }
    return tpl;

}
