#ifndef SOURCECODEGENERATOR_H
#define SOURCECODEGENERATOR_H

#include <QObject>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDomElement>
#include <QDomDocument>

class SourceCodeGenerator : public QObject
{
    Q_OBJECT
private:
    QJsonDocument rule;

    QString processElement(const QDomNode &element, int level);
public:
    explicit SourceCodeGenerator(QObject *parent = 0);
    ~SourceCodeGenerator();

    QString applyRule(const QDomDocument &xml);
signals:

public slots:
    void loadRule(const QString &fileName);
    void ruleFromJSON(const QByteArray &json);

};

#endif // SOURCECODEGENERATOR_H
