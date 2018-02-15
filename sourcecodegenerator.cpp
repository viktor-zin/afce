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
    rule = QJsonDocument::fromJson(json);
}

QString SourceCodeGenerator::applyRule(const QDomDocument &xml) {
    QDomElement alg = xml.firstChildElement("algorithm");
    QString code = processElement(alg, 0);
    return code;
}

QString SourceCodeGenerator::processElement(const QDomNode &element, int level) {
    QJsonObject obj = rule.object().value(element.nodeName()).toObject();
    QString sp;
    if (rule.object().contains("additional_settings")) {
        /* if json specified indentation string it will use */
        sp = rule.object().value("additional_settings").toObject()["indentation_preference"].toString();
    }
    else {
        /* otherwise use default */
        sp = QString("  ");
    }
    sp = sp.repeated(level);

    bool else_present = false;
    /* if element is "if" AND second item (number 1) is branch AND it is NOT empty mean "else" is present */
    if (element.nodeName() == "if" && element.childNodes().item(1).nodeName() == "branch") {
        if (element.childNodes().item(1).childNodes().size() > 0) {
            else_present = true;
        }
    }

    QString tpl;
    if (obj.contains("template_shortened") && else_present == false) {
        tpl = sp+obj.value("template_shortened").toString();
    }
    else {
        tpl = sp+obj.value("template").toString();
    }

    for(int i = 0; i < element.attributes().size(); ++i) {
       if(obj.contains("list")) {
           QJsonArray list = obj["list"].toArray();
           if (list.contains(QJsonValue(element.attributes().item(i).nodeName()))) {
                QStringList sl = element.attributes().item(i).nodeValue().split(obj["separator"].toString(), QString::SkipEmptyParts);
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
