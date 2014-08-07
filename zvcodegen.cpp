/****************************************************************************
**                                                                         **
** Copyright (C) 2009-2014 Victor Zinkevich. All rights reserved.          **
** Contact: vicking@yandex.ru                                              **
**                                                                         **
** This file is part of the Algorithm Flowchart Editor project.            **
**                                                                         **
** This file may be used under the terms of the GNU                        **
** General Public License versions 2.0 or 3.0 as published by the Free     **
** Software Foundation and appearing in the file LICENSE included in       **
** the packaging of this file.                                             **
** You can find license at http://www.gnu.org/licenses/gpl.html            **
**                                                                         **
****************************************************************************/

#include "zvcodegen.h"

//------------------------------------------------------------------------------
QString elementToI(const QDomElement & element, const QString & ident)
{
  QString code;
  if(element.nodeName() == "algorithm")
  {
    code += ident + "void algorithm()\n";
    QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToI(branch, ident);
    }
    code += "\n";
  }
  else if(element.nodeName() == "branch")
  {
    code += ident + "{\n";
    QDomNodeList items = element.childNodes();
    for(int i = 0;  i < items.size(); ++i)
    {
      QDomNode node = items.at(i);
      if (node.isElement())
      {
        QDomElement child = node.toElement();
        code += elementToI(child, ident + "  ");
      }
    }
    code += ident + "}\n";

  }
    else if(element.nodeName() == "process")
    {
      code += ident + element.attribute("text", QObject::tr("/* empty process */")) + ";\n";
    }
  else if(element.nodeName() == "io")
  {
      if (QString("%1").arg(element.attribute("t1"))!="")
      code += ident + QString("%1").arg(element.attribute("t1")).remove(QChar(','), Qt::CaseInsensitive)+ " = prompt('', ''));\n";
       if (QString("%2").arg(element.attribute("t2"))!="")
      code += ident + QString("%2").arg(element.attribute("t2")).remove(QChar(','), Qt::CaseInsensitive)+ " = prompt('', ''));\n";
       if (QString("%3").arg(element.attribute("t3"))!="")
      code += ident + QString("%3").arg(element.attribute("t3")).remove(QChar(','), Qt::CaseInsensitive)+ " = prompt('', ''));\n";
       if (QString("%4").arg(element.attribute("t4"))!="")
      code += ident + QString("%4").arg(element.attribute("t4")).remove(QChar(','), Qt::CaseInsensitive)+ " = prompt('', ''));\n";
       if (QString("%5").arg(element.attribute("t5"))!="")
      code += ident + QString("%5").arg(element.attribute("t5")).remove(QChar(','), Qt::CaseInsensitive)+ " = prompt('', ''));\n";
       if (QString("%6").arg(element.attribute("t6"))!="")
      code += ident + QString("%6").arg(element.attribute("t6")).remove(QChar(','), Qt::CaseInsensitive)+ " = prompt('', ''));\n";
       if (QString("%7").arg(element.attribute("t7"))!="")
      code += ident + QString("%7").arg(element.attribute("t7")).remove(QChar(','), Qt::CaseInsensitive)+ " = prompt('', ''));\n";
       if (QString("%8").arg(element.attribute("t8"))!="")
      code += ident + QString("%8").arg(element.attribute("t8")).remove(QChar(','), Qt::CaseInsensitive)+ " = prompt('', ''));\n";
   // code += ident + element.attribute("text", QObject::tr("(* empty input operation *)")) + " = prompt('', ''));\n";
  }
  else if(element.nodeName() == "ou")
  {
      if (QString("%1").arg(element.attribute("t1"))!="")
      code += ident + "document.write(" + QString("%1").arg(element.attribute("t1")).remove(QChar(','), Qt::CaseInsensitive)+ ");\n";
       if (QString("%2").arg(element.attribute("t2"))!="")
      code += ident + "document.write(" + QString("%2").arg(element.attribute("t2")).remove(QChar(','), Qt::CaseInsensitive)+ ");\n";
       if (QString("%3").arg(element.attribute("t3"))!="")
      code += ident + "document.write(" + QString("%3").arg(element.attribute("t3")).remove(QChar(','), Qt::CaseInsensitive)+ ");\n";
       if (QString("%4").arg(element.attribute("t4"))!="")
      code += ident + "document.write(" + QString("%4").arg(element.attribute("t4")).remove(QChar(','), Qt::CaseInsensitive)+ ");\n";
       if (QString("%5").arg(element.attribute("t5"))!="")
      code += ident + "document.write(" + QString("%5").arg(element.attribute("t5")).remove(QChar(','), Qt::CaseInsensitive)+ ");\n";
       if (QString("%6").arg(element.attribute("t6"))!="")
      code += ident + "document.write(" + QString("%6").arg(element.attribute("t6")).remove(QChar(','), Qt::CaseInsensitive)+ ");\n";
       if (QString("%7").arg(element.attribute("t7"))!="")
      code += ident + "document.write(" + QString("%7").arg(element.attribute("t7")).remove(QChar(','), Qt::CaseInsensitive)+ ");\n";
       if (QString("%8").arg(element.attribute("t8"))!="")
      code += ident + "document.write(" + QString("%8").arg(element.attribute("t8")).remove(QChar(','), Qt::CaseInsensitive)+ ");\n";
    //code += ident + "document.write(" + element.attribute("text", QObject::tr("(* empty output operation *)")) + ");\n";
  }
    else if(element.nodeName() == "assign")
    {
      code += ident + element.attribute("dest", QObject::tr("/* no destination */")) + " = " + element.attribute("src", QObject::tr("/* no source */")) + ";\n";
    }

    else if(element.nodeName() == "if")
    {
      code += ident + "if ("+element.attribute("cond", QObject::tr("/* no condition */")) + ")\n";
      QDomNodeList items = element.childNodes();
      for(int i = 0;  i < items.size(); ++i)
      {
        QDomNode node = items.at(i);
        if (node.isElement())
        {
          QDomElement child = node.toElement();
          if (i != 0) code += ident + "else\n";
          code += elementToI(child, ident);
        }
      }
    }
    else if(element.nodeName() == "pre")
    {
      code += ident + QString("while (%1)\n").arg(element.attribute("cond", QObject::tr("/* no condition */")));

      QDomElement branch = element.firstChildElement("branch");
      if (!branch.isNull())
      {
        code += elementToI(branch, ident);
      }
    }
    else if(element.nodeName() == "for")
    {
      code += ident + QString("for(int %1 = %2; i <= %3; ++i)\n").arg(element.attribute("var", QObject::tr("/* no variable */")), element.attribute("from", QObject::tr("/* no value */")), element.attribute("to", QObject::tr("/* no value */")));

      QDomElement branch = element.firstChildElement("branch");
      if (!branch.isNull())
      {
        code += elementToI(branch, ident);
      }
    }
    else if(element.nodeName() == "post")
    {
      code += ident + "do\n";
      QDomElement branch = element.firstChildElement("branch");
      if (!branch.isNull())
      {
        code += elementToI(branch, ident);
      }
      code += ident + QString("while (%1);\n").arg(element.attribute("cond", QObject::tr("/* no condition */")));
    }

   return code;
}

QString xmlToI(const QDomDocument & xml)
{
  QDomElement alg = xml.firstChildElement("algorithm");
  if(!alg.isNull())
  {
    return elementToI(alg, "");
  }
  else return QString();
}

QString xmlToI(const QString & xml)
{
  QDomDocument doc;
  if(doc.setContent(xml, false))
  {
    return xmlToI(doc);
  }
  else return QString();
}

//------------------------------------------------------------------------------
QString elementToPHPi(const QDomElement & element, const QString & ident, bool addSemicolon = true, bool addBeginEnd = true)
{
  QString code;
  if(element.nodeName() == "algorithm")
  {
    code += ident + "function algorithm()\n";
    QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToPHPi(branch, ident);
    }
    code += "\n";
  }
  else if(element.nodeName() == "branch")
  {
    if (addBeginEnd)
      code += ident + "{\n";
    QDomNodeList items = element.childNodes();
    for(int i = 0;  i < items.size(); ++i)
    {
      QDomNode node = items.at(i);
      if (node.isElement())
      {
        QDomElement child = node.toElement();
        code += elementToPHPi(child, ident + "  ");
      }
    }
    if (addBeginEnd)
    {
      code += ident + "";
      if (addSemicolon)
        code += "}";
    }
    code += "\n";
  }

  else if(element.nodeName() == "process")
  {
      code += ident + "$" + element.attribute("text", QObject::tr("/* empty process */")) + ";\n";
  }
  else if(element.nodeName() == "io")
  {
      if (QString("%1").arg(element.attribute("t1"))!="")
      code += ident + "$" + QString("%1").arg(element.attribute("t1")).remove(QChar(','), Qt::CaseInsensitive)+ " = " + "$_POST['" + QString("%1").arg(element.attribute("t1")).remove(QChar(','), Qt::CaseInsensitive) + "'];\n";
       if (QString("%2").arg(element.attribute("t2"))!="")
      code += ident + "$" + QString("%2").arg(element.attribute("t2")).remove(QChar(','), Qt::CaseInsensitive)+ " = " + "$_POST['" + QString("%2").arg(element.attribute("t2")).remove(QChar(','), Qt::CaseInsensitive) + "'];\n";
       if (QString("%3").arg(element.attribute("t3"))!="")
      code += ident + "$" + QString("%3").arg(element.attribute("t3")).remove(QChar(','), Qt::CaseInsensitive)+ " = " + "$_POST['" + QString("%3").arg(element.attribute("t3")).remove(QChar(','), Qt::CaseInsensitive) + "'];\n";
       if (QString("%4").arg(element.attribute("t4"))!="")
      code += ident + "$" + QString("%4").arg(element.attribute("t4")).remove(QChar(','), Qt::CaseInsensitive)+ " = " + "$_POST['" + QString("%4").arg(element.attribute("t4")).remove(QChar(','), Qt::CaseInsensitive) + "'];\n";
       if (QString("%5").arg(element.attribute("t5"))!="")
      code += ident + "$" + QString("%5").arg(element.attribute("t5")).remove(QChar(','), Qt::CaseInsensitive)+ " = " + "$_POST['" + QString("%5").arg(element.attribute("t5")).remove(QChar(','), Qt::CaseInsensitive) + "'];\n";
       if (QString("%6").arg(element.attribute("t6"))!="")
      code += ident + "$" + QString("%6").arg(element.attribute("t6")).remove(QChar(','), Qt::CaseInsensitive)+ " = " + "$_POST['" + QString("%6").arg(element.attribute("t6")).remove(QChar(','), Qt::CaseInsensitive) + "'];\n";
       if (QString("%7").arg(element.attribute("t7"))!="")
      code += ident + "$" + QString("%7").arg(element.attribute("t7")).remove(QChar(','), Qt::CaseInsensitive)+ " = " + "$_POST['" + QString("%7").arg(element.attribute("t7")).remove(QChar(','), Qt::CaseInsensitive) + "'];\n";
       if (QString("%8").arg(element.attribute("t8"))!="")
      code += ident + "$" + QString("%8").arg(element.attribute("t8")).remove(QChar(','), Qt::CaseInsensitive)+ " = " + "$_POST['" + QString("%8").arg(element.attribute("t8")).remove(QChar(','), Qt::CaseInsensitive) + "'];\n";
  //  code += ident + "$" + element.attribute("text", QObject::tr("(* empty input operation *)")) + " = " + "$_POST['" + element.attribute("text", QObject::tr("(* empty input operation *)")) + "'];\n";
  }
  else if(element.nodeName() == "ou")
  {
      if (QString("%1").arg(element.attribute("t1"))!="")
      code += ident + "echo $" + QString("%1").arg(element.attribute("t1")).remove(QChar(','), Qt::CaseInsensitive)+ ";\n";
       if (QString("%2").arg(element.attribute("t2"))!="")
      code += ident + "echo $" + QString("%2").arg(element.attribute("t2")).remove(QChar(','), Qt::CaseInsensitive)+ ";\n";
       if (QString("%3").arg(element.attribute("t3"))!="")
      code += ident + "echo $" + QString("%3").arg(element.attribute("t3")).remove(QChar(','), Qt::CaseInsensitive)+ ";\n";
       if (QString("%4").arg(element.attribute("t4"))!="")
      code += ident + "echo $" + QString("%4").arg(element.attribute("t4")).remove(QChar(','), Qt::CaseInsensitive)+ ";\n";
       if (QString("%5").arg(element.attribute("t5"))!="")
      code += ident + "echo $" + QString("%5").arg(element.attribute("t5")).remove(QChar(','), Qt::CaseInsensitive)+ ";\n";
       if (QString("%6").arg(element.attribute("t6"))!="")
      code += ident + "echo $" + QString("%6").arg(element.attribute("t6")).remove(QChar(','), Qt::CaseInsensitive)+ ";\n";
       if (QString("%7").arg(element.attribute("t7"))!="")
      code += ident + "echo $" + QString("%7").arg(element.attribute("t7")).remove(QChar(','), Qt::CaseInsensitive)+ ";\n";
       if (QString("%8").arg(element.attribute("t8"))!="")
      code += ident + "echo $" + QString("%8").arg(element.attribute("t8")).remove(QChar(','), Qt::CaseInsensitive)+ ";\n";
   // code += ident + "echo $" + element.attribute("text", QObject::tr("(* empty output operation *)")) + ";\n";
  }
  else if(element.nodeName() == "assign")
  {
    code += ident + element.attribute("dest", QObject::tr("/* no destination */")) + " = " + element.attribute("src", QObject::tr("/* no source */")) + ";\n";
  }

  else if(element.nodeName() == "if")
  {
    code += ident + "if ($"+element.attribute("cond", QObject::tr("/* no condition */")) + ")\n";
    QDomNodeList items = element.childNodes();
    for(int i = 0;  i < items.size(); ++i)
    {
      QDomNode node = items.at(i);
      if (node.isElement())
      {
        QDomElement child = node.toElement();
        if (i != 0) code += ident + "else\n";
        code += elementToPHPi(child, ident);

      }
    }
  }
  else if(element.nodeName() == "pre")
  {
    code += ident + QString("while ($%1)\n").arg(element.attribute("cond", QObject::tr("/* no condition */")));

    QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToPHPi(branch, ident);
    }
  }
  else if(element.nodeName() == "for")
  {
    code += ident + QString("for($%1 = %2; $%1 <= $%3; ++$%1)\n").arg(element.attribute("var", QObject::tr("/* no variable */")), element.attribute("from", QObject::tr("/* no value */")), element.attribute("to", QObject::tr("/* no value */")));

    QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToPHPi(branch, ident);
    }
  }
  else if(element.nodeName() == "post")
  {
    code += ident + "do\n";
    QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToPHPi(branch, ident);
    }
    code += ident + QString("while ($%1);\n").arg(element.attribute("cond", QObject::tr("/* no condition */")));
  }

  return code;
}

QString xmlToPHPi(const QDomDocument & xml)
{
  QDomElement alg = xml.firstChildElement("algorithm");
  if(!alg.isNull())
  {
    return elementToPHPi(alg, "");
  }
  else return QString();
}

QString xmlToPHPi(const QString & xml)
{
  QDomDocument doc;
  if(doc.setContent(xml, false))
  {
    return xmlToPHPi(doc);
  }
  else return QString();
}

//------------------------------------------------------------------------------

QString elementToPas(const QDomElement & element, const QString & ident, bool addSemicolon = true, bool addBeginEnd = true)
{
  QString code;
  if(element.nodeName() == "algorithm")
  {
    code += ident + "procedure Algorithm;\n";
    QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToPas(branch, ident);
    }
    code += "\n";
  }
  else if(element.nodeName() == "branch")
  {
    if (addBeginEnd)
      code += ident + "begin\n";
    QDomNodeList items = element.childNodes();
    for(int i = 0;  i < items.size(); ++i)
    {
      QDomNode node = items.at(i);
      if (node.isElement())
      {
        QDomElement child = node.toElement();
        code += elementToPas(child, ident + "  ");
      }
    }
    if (addBeginEnd)
    {
      code += ident + "end";
      if (addSemicolon)
        code += ";";
    }
    code += "\n";
  }
  else if(element.nodeName() == "process")
  {
    code += ident + element.attribute("text", QObject::tr("(* empty process *)")) + ";\n";
  }
  else if(element.nodeName() == "io")
  {
      code += ident + "readln(";
       if (QString("%1").arg(element.attribute("t1"))!="")
      code += QString("%1").arg(element.attribute("t1"));
       if (QString("%2").arg(element.attribute("t2"))!="")
      code +=" ";
      code += QString("%2").arg(element.attribute("t2"));
       if (QString("%3").arg(element.attribute("t3"))!="")
      code +=" ";
      code += QString("%3").arg(element.attribute("t3"));
       if (QString("%4").arg(element.attribute("t4"))!="")
      code +=" ";
      code += QString("%4").arg(element.attribute("t4"));
       if (QString("%5").arg(element.attribute("t5"))!="")
      code +=" ";
      code += QString("%5").arg(element.attribute("t5"));
       if (QString("%6").arg(element.attribute("t6"))!="")
      code +=" ";
      code += QString("%6").arg(element.attribute("t6"));
       if (QString("%7").arg(element.attribute("t7"))!="")
      code +=" ";
      code += QString("%7").arg(element.attribute("t7"));
       if (QString("%8").arg(element.attribute("t8"))!="")
      code +=" ";
      code += QString("%8").arg(element.attribute("t8"));
      code += ");\n";
    //code += ident + "readln(" + element.attribute("text", QObject::tr("(* empty input operation *)")) + ");\n";
  }
  else if(element.nodeName() == "ou")
  {
      code += ident + "writeln(";
       if (QString("%1").arg(element.attribute("t1"))!="")
      code += QString("%1").arg(element.attribute("t1"));
       if (QString("%2").arg(element.attribute("t2"))!="")
      code +=" ";
      code += QString("%2").arg(element.attribute("t2"));
       if (QString("%3").arg(element.attribute("t3"))!="")
      code +=" ";
      code += QString("%3").arg(element.attribute("t3"));
       if (QString("%4").arg(element.attribute("t4"))!="")
      code +=" ";
      code += QString("%4").arg(element.attribute("t4"));
       if (QString("%5").arg(element.attribute("t5"))!="")
      code +=" ";
      code += QString("%5").arg(element.attribute("t5"));
       if (QString("%6").arg(element.attribute("t6"))!="")
      code +=" ";
      code += QString("%6").arg(element.attribute("t6"));
       if (QString("%7").arg(element.attribute("t7"))!="")
      code +=" ";
      code += QString("%7").arg(element.attribute("t7"));
       if (QString("%8").arg(element.attribute("t8"))!="")
      code +=" ";
      code += QString("%8").arg(element.attribute("t8"));
      code += ");\n";
   // code += ident + "writeln(" + element.attribute("text", QObject::tr("(* empty output operation *)")) + ");\n";
  }
  else if(element.nodeName() == "assign")
  {
    code += ident + element.attribute("dest", QObject::tr("(* no destination *)")) + " := " + element.attribute("src", QObject::tr("(* no source *)")) + ";\n";
  }

  else if(element.nodeName() == "if")
  {
    code += ident + "if "+element.attribute("cond", QObject::tr("(* no condition *)")) + " then\n";
    QDomNodeList items = element.childNodes();
    for(int i = 0;  i < items.size(); ++i)
    {
      QDomNode node = items.at(i);

      if (node.isElement())
      {
        QDomElement child = node.toElement();
        if (i != 0) code += ident + "else\n";
        code += elementToPas(child, ident, i!=0);
      }
    }
  }
  else if(element.nodeName() == "pre")
  {
    code += ident + QString("while %1 do\n").arg(element.attribute("cond", QObject::tr("(* no condition *)")));

    QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToPas(branch, ident);
    }
  }
  else if(element.nodeName() == "for")
  {
    code += ident + QString("for %1 := %2 to %3 do\n").arg(element.attribute("var", QObject::tr("(* no variable *)")), element.attribute("from", QObject::tr("(* no value *)")), element.attribute("to", QObject::tr("(* no value *)")));

    QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToPas(branch, ident);
    }
  }
  else if(element.nodeName() == "post")
  {
    code += ident + "repeat\n";
    QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToPas(branch, ident, true, false);
    }
    code += ident + QString("until not (%1);\n").arg(element.attribute("cond", QObject::tr("(* no condition *)")));
  }

  return code;
}

QString xmlToPas(const QDomDocument & xml)
{
  QDomElement alg = xml.firstChildElement("algorithm");
  if(!alg.isNull())
  {
    return elementToPas(alg, "");
  }
  else return QString();
}

QString xmlToPas(const QString & xml)
{
  QDomDocument doc;
  if(doc.setContent(xml, false))
  {
    return xmlToPas(doc);
  }
  else return QString();
}

//------------------------------------------------------------------------------


QString elementToPascal(const QDomElement & element, const QString & ident, bool addSemicolon = true, bool addBeginEnd = true)
{
  QString code;
  if(element.nodeName() == "algorithm")
  {
    code += ident + "program algorithm;\n";
    QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToPascal(branch, ident);
    }
    code += "\n";
  }
  else if(element.nodeName() == "branch")
  {
    if (addBeginEnd)
      code += ident + "begin\n";
    QDomNodeList items = element.childNodes();
    for(int i = 0;  i < items.size(); ++i)
    {
      QDomNode node = items.at(i);
      if (node.isElement())
      {
        QDomElement child = node.toElement();
        code += elementToPascal(child, ident + "  ");
      }
    }
    if (addBeginEnd)
    {
      code += ident + "end";
      if (addSemicolon)
        code += ".";
    }
    code += "\n";
  }
  else if(element.nodeName() == "process")
  {
    code += ident + element.attribute("text", QObject::tr("(* empty process *)")) + ";\n";
  }
  else if(element.nodeName() == "io")
  {
      code += ident + "readln(";
       if (QString("%1").arg(element.attribute("t1"))!="")
      code += QString("%1").arg(element.attribute("t1"));
       if (QString("%2").arg(element.attribute("t2"))!="")
      code +=" ";
      code += QString("%2").arg(element.attribute("t2"));
       if (QString("%3").arg(element.attribute("t3"))!="")
      code +=" ";
      code += QString("%3").arg(element.attribute("t3"));
       if (QString("%4").arg(element.attribute("t4"))!="")
      code +=" ";
      code += QString("%4").arg(element.attribute("t4"));
       if (QString("%5").arg(element.attribute("t5"))!="")
      code +=" ";
      code += QString("%5").arg(element.attribute("t5"));
       if (QString("%6").arg(element.attribute("t6"))!="")
      code +=" ";
      code += QString("%6").arg(element.attribute("t6"));
       if (QString("%7").arg(element.attribute("t7"))!="")
      code +=" ";
      code += QString("%7").arg(element.attribute("t7"));
       if (QString("%8").arg(element.attribute("t8"))!="")
      code +=" ";
      code += QString("%8").arg(element.attribute("t8"));
      code += ");\n";
    //code += ident + "readln(" + element.attribute("text", QObject::tr("(* empty input operation *)")) + ");\n";
  }
  else if(element.nodeName() == "ou")
  {
      code += ident + "writeln(";
       if (QString("%1").arg(element.attribute("t1"))!="")
      code += QString("%1").arg(element.attribute("t1"));
       if (QString("%2").arg(element.attribute("t2"))!="")
      code +=" ";
      code += QString("%2").arg(element.attribute("t2"));
       if (QString("%3").arg(element.attribute("t3"))!="")
      code +=" ";
      code += QString("%3").arg(element.attribute("t3"));
       if (QString("%4").arg(element.attribute("t4"))!="")
      code +=" ";
      code += QString("%4").arg(element.attribute("t4"));
       if (QString("%5").arg(element.attribute("t5"))!="")
      code +=" ";
      code += QString("%5").arg(element.attribute("t5"));
       if (QString("%6").arg(element.attribute("t6"))!="")
      code +=" ";
      code += QString("%6").arg(element.attribute("t6"));
       if (QString("%7").arg(element.attribute("t7"))!="")
      code +=" ";
      code += QString("%7").arg(element.attribute("t7"));
       if (QString("%8").arg(element.attribute("t8"))!="")
      code +=" ";
      code += QString("%8").arg(element.attribute("t8"));
      code += ");\n";
   // code += ident + "writeln(" + element.attribute("text", QObject::tr("(* empty output operation *)")) + ");\n";
  }
  else if(element.nodeName() == "assign")
  {
    code += ident + element.attribute("dest", QObject::tr("(* no destination *)")) + " := " + element.attribute("src", QObject::tr("(* no source *)")) + ";\n";
  }

  else if(element.nodeName() == "if")
  {
    code += ident + "if "+element.attribute("cond", QObject::tr("(* no condition *)")) + " then\n";
    QDomNodeList items = element.childNodes();
    for(int i = 0;  i < items.size(); ++i)
    {
      QDomNode node = items.at(i);
      if (node.isElement())
      {
        QDomElement child = node.toElement();
        if (i != 0) code += ident + "else\n";
        code += elementToPas(child, ident, i!=0);

      }
    }
  }
  else if(element.nodeName() == "pre")
  {
    code += ident + QString("while %1 do\n").arg(element.attribute("cond", QObject::tr("(* no condition *)")));

    QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToPas(branch, ident);
    }
  }
  else if(element.nodeName() == "for")
  {
     code += ident + QString("for %1 := %2 to %3 do\n").arg(element.attribute("var", QObject::tr("(* no variable *)")), element.attribute("from", QObject::tr("(* no value *)")), element.attribute("to", QObject::tr("(* no value *)")));
     QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToPas(branch, ident);
    }
  }
  else if(element.nodeName() == "post")
  {
    code += ident + "repeat\n";
    QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToPas(branch, ident, true, false);
    }
    code += ident + QString("until not (%1);\n").arg(element.attribute("cond", QObject::tr("(* no condition *)")));
  }

  return code;
}

QString xmlToPascal(const QDomDocument & xml)
{
  QDomElement alg = xml.firstChildElement("algorithm");
  if(!alg.isNull())
  {
    return elementToPascal(alg, "");
  }
  else return QString();
}

QString xmlToPascal(const QString & xml)
{
  QDomDocument doc;
  if(doc.setContent(xml, false))
  {
    return xmlToPascal(doc);
  }
  else return QString();
}

//------------------------------------------------------------------------------

QString elementToE87(const QDomElement & element, const QString & ident, bool addIdent = true, bool addVLine = true)
{
  QString code;
  if(element.nodeName() == "algorithm")
  {
    code += ident + "алг Алгоритм\n"+ident+"нач";
    QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToE87(branch, ident);
    }
    code += "\n" + ident + "кон\n";
  }
  else if(element.nodeName() == "branch")
  {
//    if (addBeginEnd)
//      code += ident + "\n";
    QDomNodeList items = element.childNodes();
    for(int i = 0;  i < items.size(); ++i)
    {
      QDomNode node = items.at(i);
      if (node.isElement())
      {
        QDomElement child = node.toElement();
        if (addIdent)
        {
          if (addVLine)
            code += elementToE87(child, ident + "| ");
          else
            code += elementToE87(child, ident + "  ");
        }
        else
        {
          code += elementToE87(child, ident);
        }
      }
    }
//    if (addBeginEnd)
//    {
//      code += ident + "";
//      if (addSemicolon)
//        code += ";";
//    }
//    if (!code.isEmpty())
//      code += "\n";
  }
  else if(element.nodeName() == "process")
  {
    code += "\n" + ident + element.attribute("text", QObject::tr("(* empty process *)"));
  }
  else if(element.nodeName() == "io")
  {
      code += "\n" + ident + "ввод ";
       if (QString("%1").arg(element.attribute("t1"))!="")
      code += QString("%1").arg(element.attribute("t1")) + " ";
       if (QString("%2").arg(element.attribute("t2"))!="")
      code += QString("%2").arg(element.attribute("t2")) + " ";
       if (QString("%3").arg(element.attribute("t3"))!="")
      code += QString("%3").arg(element.attribute("t3")) + " ";
       if (QString("%4").arg(element.attribute("t4"))!="")
      code += QString("%4").arg(element.attribute("t4")) + " ";
       if (QString("%5").arg(element.attribute("t5"))!="")
      code += QString("%5").arg(element.attribute("t5")) + " ";
       if (QString("%6").arg(element.attribute("t6"))!="")
      code += QString("%6").arg(element.attribute("t6")) + " ";
       if (QString("%7").arg(element.attribute("t7"))!="")
      code += QString("%7").arg(element.attribute("t7")) + " ";
       if (QString("%8").arg(element.attribute("t8"))!="")
      code += QString("%8").arg(element.attribute("t8")) + " ";
      //code += ident + "\n";
   // code += "\n" + ident + "ввод " + element.attribute("text", QObject::tr("(* empty I/O operation *)"));
  }
  else if(element.nodeName() == "ou")
  {
      code += "\n" + ident + "вывод ";
       if (QString("%1").arg(element.attribute("t1"))!="")
      code += QString("%1").arg(element.attribute("t1")) + " ";
       if (QString("%2").arg(element.attribute("t2"))!="")
      code += QString("%2").arg(element.attribute("t2")) + " ";
       if (QString("%3").arg(element.attribute("t3"))!="")
      code += QString("%3").arg(element.attribute("t3")) + " ";
       if (QString("%4").arg(element.attribute("t4"))!="")
      code += QString("%4").arg(element.attribute("t4")) + " ";
       if (QString("%5").arg(element.attribute("t5"))!="")
      code += QString("%5").arg(element.attribute("t5")) + " ";
       if (QString("%6").arg(element.attribute("t6"))!="")
      code += QString("%6").arg(element.attribute("t6")) + " ";
       if (QString("%7").arg(element.attribute("t7"))!="")
      code += QString("%7").arg(element.attribute("t7")) + " ";
       if (QString("%8").arg(element.attribute("t8"))!="")
      code += QString("%8").arg(element.attribute("t8")) + " ";
    //  code += ident + "\n";
   // code += "\n" + ident + "вывод " + element.attribute("text", QObject::tr("(* empty I/O operation *)"));
  }
  else if(element.nodeName() == "assign")
  {
    code += "\n" + ident + element.attribute("dest", QObject::tr("(* no destination *)")) + " := " + element.attribute("src", QObject::tr("(* no source *)"));
  }

  else if(element.nodeName() == "if")
  {
    code += "\n" + ident + "если "+element.attribute("cond", QObject::tr("(* no condition *)")) + "\n"+ident+"| то";
    QDomNodeList items = element.childNodes();
    for(int i = 0;  i < items.size(); ++i)
    {
      QDomNode node = items.at(i);
      if (node.isElement())
      {
        QDomElement child = node.toElement();
        if (i != 0) code += "\n" + ident + "| иначе";
        code += elementToE87(child, ident+"| ",  true, false);
      }
    }
    code += "\n" + ident + "все";
  }
  else if(element.nodeName() == "pre")
  {
    code += "\n" + ident + QString("нц пока %1").arg(element.attribute("cond", QObject::tr("(* no condition *)")));

    QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToE87(branch, ident);
    }
    code += "\n" + ident + "кц";
  }
  else if(element.nodeName() == "for")
  {
    code += "\n" + ident + QString("нц для %1 от %2 до %3").arg(element.attribute("var", QObject::tr("(* no variable *)")), element.attribute("from", QObject::tr("(* no value *)")), element.attribute("to", QObject::tr("(* no value *)")));

    QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToE87(branch, ident);
    }
    code += "\n" + ident + "кц";
  }
  else if(element.nodeName() == "post")
  {
    QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToE87(branch, ident, false);
    code += "\n" + ident + QString("нц пока %1").arg(element.attribute("cond", QObject::tr("(* no condition *)")));
      code += elementToE87(branch, ident, false);

    code += "\n" + ident + "кц";
    }
  }

  return code;
}


QString xmlToE87(const QDomDocument & xml)
{
  QDomElement alg = xml.firstChildElement("algorithm");
  if(!alg.isNull())
  {
    return elementToE87(alg, "");
  }
  else return QString();
}

QString xmlToE87(const QString & xml)
{
  QDomDocument doc;
  if(doc.setContent(xml, false))
  {
    return xmlToE87(doc);
  }
  else return QString();
}

//------------------------------------------------------------------------------
QString elementToC(const QDomElement & element, const QString & ident)
{
  QString code;
  if(element.nodeName() == "algorithm")
  {
    code += ident + "int main()\n";
    QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToC(branch, ident);
    }
    code += "\n";
  }
  else if(element.nodeName() == "branch")
  {
    code += ident + "{\n";
    QDomNodeList items = element.childNodes();
    for(int i = 0;  i < items.size(); ++i)
    {
      QDomNode node = items.at(i);
      if (node.isElement())
      {
        QDomElement child = node.toElement();
        code += elementToC(child, ident + "  ");
      }
    }
    code += ident + "}\n";

  }
  else if(element.nodeName() == "process")
  {
    code += ident + element.attribute("text", QObject::tr("/* empty process */")) + ";\n";
  }
  else if(element.nodeName() == "io")
  {
      if (QString("%1").arg(element.attribute("t1"))!="")
      code += ident + "std::cin >> " + QString("%1").arg(element.attribute("t1")).remove(QChar(','), Qt::CaseInsensitive)+ ";\n";
       if (QString("%2").arg(element.attribute("t2"))!="")
      code += ident + "std::cin >> " + QString("%2").arg(element.attribute("t2")).remove(QChar(','), Qt::CaseInsensitive)+ ";\n";
       if (QString("%3").arg(element.attribute("t3"))!="")
      code += ident + "std::cin >> " + QString("%3").arg(element.attribute("t3")).remove(QChar(','), Qt::CaseInsensitive)+ ";\n";
       if (QString("%4").arg(element.attribute("t4"))!="")
      code += ident + "std::cin >> " + QString("%4").arg(element.attribute("t4")).remove(QChar(','), Qt::CaseInsensitive)+ ";\n";
       if (QString("%5").arg(element.attribute("t5"))!="")
      code += ident + "std::cin >> " + QString("%5").arg(element.attribute("t5")).remove(QChar(','), Qt::CaseInsensitive)+ ";\n";
       if (QString("%6").arg(element.attribute("t6"))!="")
      code += ident + "std::cin >> " + QString("%6").arg(element.attribute("t6")).remove(QChar(','), Qt::CaseInsensitive)+ ";\n";
       if (QString("%7").arg(element.attribute("t7"))!="")
      code += ident + "std::cin >> " + QString("%7").arg(element.attribute("t7")).remove(QChar(','), Qt::CaseInsensitive)+ ";\n";
       if (QString("%8").arg(element.attribute("t8"))!="")
      code += ident + "std::cin >> " + QString("%8").arg(element.attribute("t8")).remove(QChar(','), Qt::CaseInsensitive)+ ";\n";
    //code += ident + "std::cin >> " + element.attribute("text", QObject::tr("(* empty input operation *)")) + ";\n";
  }
  else if(element.nodeName() == "ou")
  {
      if (QString("%1").arg(element.attribute("t1"))!="")
      code += ident + "std::cout << " + QString("%1").arg(element.attribute("t1")).remove(QChar(','), Qt::CaseInsensitive)+ ";\n";
       if (QString("%2").arg(element.attribute("t2"))!="")
      code += ident + "std::cout << " + QString("%2").arg(element.attribute("t2")).remove(QChar(','), Qt::CaseInsensitive)+ ";\n";
       if (QString("%3").arg(element.attribute("t3"))!="")
      code += ident + "std::cout << " + QString("%3").arg(element.attribute("t3")).remove(QChar(','), Qt::CaseInsensitive)+ ";\n";
       if (QString("%4").arg(element.attribute("t4"))!="")
      code += ident + "std::cout << " + QString("%4").arg(element.attribute("t4")).remove(QChar(','), Qt::CaseInsensitive)+ ";\n";
       if (QString("%5").arg(element.attribute("t5"))!="")
      code += ident + "std::cout << " + QString("%5").arg(element.attribute("t5")).remove(QChar(','), Qt::CaseInsensitive)+ ";\n";
       if (QString("%6").arg(element.attribute("t6"))!="")
      code += ident + "std::cout << " + QString("%6").arg(element.attribute("t6")).remove(QChar(','), Qt::CaseInsensitive)+ ";\n";
       if (QString("%7").arg(element.attribute("t7"))!="")
      code += ident + "std::cout << " + QString("%7").arg(element.attribute("t7")).remove(QChar(','), Qt::CaseInsensitive)+ ";\n";
       if (QString("%8").arg(element.attribute("t8"))!="")
      code += ident + "std::cout << " + QString("%8").arg(element.attribute("t8")).remove(QChar(','), Qt::CaseInsensitive)+ ";\n";
   // code += ident + "std::cout << " + element.attribute("text", QObject::tr("(* empty output operation *)")) + ";\n";
  }
  else if(element.nodeName() == "assign")
  {
    code += ident + element.attribute("dest", QObject::tr("/* no destination */")) + " = " + element.attribute("src", QObject::tr("/* no source */")) + ";\n";
  }

  else if(element.nodeName() == "if")
  {
    code += ident + "if ("+element.attribute("cond", QObject::tr("/* no condition */")) + ")\n";
    QDomNodeList items = element.childNodes();
    for(int i = 0;  i < items.size(); ++i)
    {
      QDomNode node = items.at(i);
      if (node.isElement())
      {
        QDomElement child = node.toElement();
        if (i != 0) code += ident + "else\n";
        code += elementToC(child, ident);
      }
    }
  }
  else if(element.nodeName() == "pre")
  {
    code += ident + QString("while (%1)\n").arg(element.attribute("cond", QObject::tr("/* no condition */")));

    QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToC(branch, ident);
    }
  }
  else if(element.nodeName() == "for")
  {
    code += ident + QString("for(int %1 = %2; i <= %3; ++i)\n").arg(element.attribute("var", QObject::tr("/* no variable */")), element.attribute("from", QObject::tr("/* no value */")), element.attribute("to", QObject::tr("/* no value */")));

    QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToC(branch, ident);
    }
  }
  else if(element.nodeName() == "post")
  {
    code += ident + "do\n";
    QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToC(branch, ident);
    }
    code += ident + QString("while (%1);\n").arg(element.attribute("cond", QObject::tr("/* no condition */")));
  }

  return code;
}

QString xmlToC(const QDomDocument & xml)
{
  QDomElement alg = xml.firstChildElement("algorithm");
  if(!alg.isNull())
  {
    return elementToC(alg, "");
  }
  else return QString();
}

QString xmlToC(const QString & xml)
{
  QDomDocument doc;
  if(doc.setContent(xml, false))
  {
    return xmlToC(doc);
  }
  else return QString();
}

//------------------------------------------------------------------------------
QString elementToPython(const QDomElement & element, const QString & ident)
{
  QString code;
  if(element.nodeName() == "algorithm")
  {
    code += ident + "def algorithm():\n";
    QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToPython(branch, ident);
    }
  //  code += "";
  }
  else if(element.nodeName() == "branch")
  {
   // code += ident + " ";
    QDomNodeList items = element.childNodes();
    for(int i = 0;  i < items.size(); ++i)
    {
      QDomNode node = items.at(i);
      if (node.isElement())
      {
        QDomElement child = node.toElement();
        code += elementToPython(child, ident + "   ");
      }
    }

  //  code += ident + " ";

  }
  else if(element.nodeName() == "process")
  {
    code += ident + element.attribute("text", QObject::tr("/* empty process */")) + "\n";
  }
  else if(element.nodeName() == "io")
  {
      if (QString("%1").arg(element.attribute("t1"))!="")
      code += ident + QString("%1").arg(element.attribute("t1")).remove(QChar(','), Qt::CaseInsensitive)+ " = input()\n";
       if (QString("%2").arg(element.attribute("t2"))!="")
      code += ident + QString("%2").arg(element.attribute("t2")).remove(QChar(','), Qt::CaseInsensitive)+ " = input()\n";
       if (QString("%3").arg(element.attribute("t3"))!="")
      code += ident + QString("%3").arg(element.attribute("t3")).remove(QChar(','), Qt::CaseInsensitive)+ " = input()\n";
       if (QString("%4").arg(element.attribute("t4"))!="")
      code += ident + QString("%4").arg(element.attribute("t4")).remove(QChar(','), Qt::CaseInsensitive)+ " = input()\n";
       if (QString("%5").arg(element.attribute("t5"))!="")
      code += ident + QString("%5").arg(element.attribute("t5")).remove(QChar(','), Qt::CaseInsensitive)+ " = input()\n";
       if (QString("%6").arg(element.attribute("t6"))!="")
      code += ident + QString("%6").arg(element.attribute("t6")).remove(QChar(','), Qt::CaseInsensitive)+ " = input()\n";
       if (QString("%7").arg(element.attribute("t7"))!="")
      code += ident + QString("%7").arg(element.attribute("t7")).remove(QChar(','), Qt::CaseInsensitive)+ " = input()\n";
       if (QString("%8").arg(element.attribute("t8"))!="")
      code += ident + QString("%8").arg(element.attribute("t8")).remove(QChar(','), Qt::CaseInsensitive)+ " = input()\n";
    //code += ident + element.attribute("text", QObject::tr("(* empty input operation *)")) + " = input()\n";
  }
  else if(element.nodeName() == "ou")
  {
     code += ident + "print ";
      if (QString("%1").arg(element.attribute("t1"))!="")
     code += QString("%1").arg(element.attribute("t1")) + " ";
      if (QString("%2").arg(element.attribute("t2"))!="")
     code += QString("%2").arg(element.attribute("t2")) + " ";
      if (QString("%3").arg(element.attribute("t3"))!="")
     code += QString("%3").arg(element.attribute("t3")) + " ";
      if (QString("%4").arg(element.attribute("t4"))!="")
     code += QString("%4").arg(element.attribute("t4")) + " ";
      if (QString("%5").arg(element.attribute("t5"))!="")
     code += QString("%5").arg(element.attribute("t5")) + " ";
      if (QString("%6").arg(element.attribute("t6"))!="")
     code += QString("%6").arg(element.attribute("t6")) + " ";
      if (QString("%7").arg(element.attribute("t7"))!="")
     code += QString("%7").arg(element.attribute("t7")) + " ";
      if (QString("%8").arg(element.attribute("t8"))!="")
     code += QString("%8").arg(element.attribute("t8")) + " ";
     code += ident + "\n";
    //code += ident + "print " + QString("%1 %2 %3 %4 %5 %6 %7 %8\n").arg(element.attribute("t1"),element.attribute("t2"),element.attribute("t3"),element.attribute("t4"),element.attribute("t5"),element.attribute("t6"),element.attribute("t7"),element.attribute("t8"));
  }
  else if(element.nodeName() == "assign")
  {
    code += ident + element.attribute("dest", QObject::tr("/* no destination */")) + " = " + element.attribute("src", QObject::tr("/* no source */")) + "\n";
  }

  else if(element.nodeName() == "if")
  {
    code += ident + "if "+element.attribute("cond", QObject::tr("/* no condition */")) + ":\n";
    QDomNodeList items = element.childNodes();
    for(int i = 0;  i < items.size(); ++i)
    {
      QDomNode node = items.at(i);
      if (node.isElement())
      {
        QDomElement child = node.toElement();
        if (i != 0) code += ident + "else:\n";
        code += elementToPython(child, ident);
      }
    }
  }
  else if(element.nodeName() == "pre")
  {
    code += ident + QString("while %1:\n").arg(element.attribute("cond", QObject::tr("/* no condition */")));

    QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToPython(branch, ident);
    }
  }
  else if(element.nodeName() == "for")
  {
   // code += ident + QString("for(int %1 = %2; i <= %3; ++i)\n").arg(element.attribute("var", QObject::tr("/* no variable */")), element.attribute("from", QObject::tr("/* no value */")), element.attribute("to", QObject::tr("/* no value */")));
      code += ident + QString("%1 = %2\n   while %1 <= %3:\n      %1 = %1 + 1\n").arg(element.attribute("var", QObject::tr("/* no variable */")), element.attribute("from", QObject::tr("/* no value */")), element.attribute("to", QObject::tr("/* no value */")));
    QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToPython(branch, ident);
    }
  }
  else if(element.nodeName() == "post")
  {
      code += ident + QString("while %1:\n").arg(element.attribute("cond", QObject::tr("/* no condition */")));

      QDomElement branch = element.firstChildElement("branch");
      if (!branch.isNull())
      {
        code += elementToPython(branch, ident);
      }
  }

  return code;
}

QString xmlToPython(const QDomDocument & xml)
{
  QDomElement alg = xml.firstChildElement("algorithm");
  if(!alg.isNull())
  {
    return elementToPython(alg, "");
  }
  else return QString();
}

QString xmlToPython(const QString & xml)
{
  QDomDocument doc;
  if(doc.setContent(xml, false))
  {
    return xmlToPython(doc);
  }
  else return QString();
}

//------------------------------------------------------------------------------
QString elementToPHP(const QDomElement & element, const QString & ident, bool addSemicolon = true, bool addBeginEnd = true)
{
  QString code;
  if(element.nodeName() == "algorithm")
  {
    code += ident + "<?php\n";
    QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToPHP(branch, ident);
    }
  //  code += "\n";
  }
  else if(element.nodeName() == "branch")
  {
    if (addBeginEnd)
      code += ident + "";
    QDomNodeList items = element.childNodes();
    for(int i = 0;  i < items.size(); ++i)
    {
      QDomNode node = items.at(i);
      if (node.isElement())
      {
        QDomElement child = node.toElement();
        code += elementToPHP(child, ident + "  ");
      }
    }
    if (addBeginEnd)
    {
      code += ident + "";
      if (addSemicolon)
        code += "?>\n";
    }
    code += "\n";
  }

  else if(element.nodeName() == "process")
  {
    code += ident + "$" + element.attribute("text", QObject::tr("/* empty process */")) + ";\n";
  }
  else if(element.nodeName() == "io")
  {
      if (QString("%1").arg(element.attribute("t1"))!="")
      code += ident + "$" + QString("%1").arg(element.attribute("t1")).remove(QChar(','), Qt::CaseInsensitive)+ " = " + "$_POST['" + QString("%1").arg(element.attribute("t1")).remove(QChar(','), Qt::CaseInsensitive) + "'];\n";
       if (QString("%2").arg(element.attribute("t2"))!="")
      code += ident + "$" + QString("%2").arg(element.attribute("t2")).remove(QChar(','), Qt::CaseInsensitive)+ " = " + "$_POST['" + QString("%2").arg(element.attribute("t2")).remove(QChar(','), Qt::CaseInsensitive) + "'];\n";
       if (QString("%3").arg(element.attribute("t3"))!="")
      code += ident + "$" + QString("%3").arg(element.attribute("t3")).remove(QChar(','), Qt::CaseInsensitive)+ " = " + "$_POST['" + QString("%3").arg(element.attribute("t3")).remove(QChar(','), Qt::CaseInsensitive) + "'];\n";
       if (QString("%4").arg(element.attribute("t4"))!="")
      code += ident + "$" + QString("%4").arg(element.attribute("t4")).remove(QChar(','), Qt::CaseInsensitive)+ " = " + "$_POST['" + QString("%4").arg(element.attribute("t4")).remove(QChar(','), Qt::CaseInsensitive) + "'];\n";
       if (QString("%5").arg(element.attribute("t5"))!="")
      code += ident + "$" + QString("%5").arg(element.attribute("t5")).remove(QChar(','), Qt::CaseInsensitive)+ " = " + "$_POST['" + QString("%5").arg(element.attribute("t5")).remove(QChar(','), Qt::CaseInsensitive) + "'];\n";
       if (QString("%6").arg(element.attribute("t6"))!="")
      code += ident + "$" + QString("%6").arg(element.attribute("t6")).remove(QChar(','), Qt::CaseInsensitive)+ " = " + "$_POST['" + QString("%6").arg(element.attribute("t6")).remove(QChar(','), Qt::CaseInsensitive) + "'];\n";
       if (QString("%7").arg(element.attribute("t7"))!="")
      code += ident + "$" + QString("%7").arg(element.attribute("t7")).remove(QChar(','), Qt::CaseInsensitive)+ " = " + "$_POST['" + QString("%7").arg(element.attribute("t7")).remove(QChar(','), Qt::CaseInsensitive) + "'];\n";
       if (QString("%8").arg(element.attribute("t8"))!="")
      code += ident + "$" + QString("%8").arg(element.attribute("t8")).remove(QChar(','), Qt::CaseInsensitive)+ " = " + "$_POST['" + QString("%8").arg(element.attribute("t8")).remove(QChar(','), Qt::CaseInsensitive) + "'];\n";
  //  code += ident + "$" + element.attribute("text", QObject::tr("(* empty input operation *)")) + " = " + "$_POST['" + element.attribute("text", QObject::tr("(* empty input operation *)")) + "'];\n";
  }
  else if(element.nodeName() == "ou")
  {
      if (QString("%1").arg(element.attribute("t1"))!="")
      code += ident + "echo $" + QString("%1").arg(element.attribute("t1")).remove(QChar(','), Qt::CaseInsensitive)+ ";\n";
       if (QString("%2").arg(element.attribute("t2"))!="")
      code += ident + "echo $" + QString("%2").arg(element.attribute("t2")).remove(QChar(','), Qt::CaseInsensitive)+ ";\n";
       if (QString("%3").arg(element.attribute("t3"))!="")
      code += ident + "echo $" + QString("%3").arg(element.attribute("t3")).remove(QChar(','), Qt::CaseInsensitive)+ ";\n";
       if (QString("%4").arg(element.attribute("t4"))!="")
      code += ident + "echo $" + QString("%4").arg(element.attribute("t4")).remove(QChar(','), Qt::CaseInsensitive)+ ";\n";
       if (QString("%5").arg(element.attribute("t5"))!="")
      code += ident + "echo $" + QString("%5").arg(element.attribute("t5")).remove(QChar(','), Qt::CaseInsensitive)+ ";\n";
       if (QString("%6").arg(element.attribute("t6"))!="")
      code += ident + "echo $" + QString("%6").arg(element.attribute("t6")).remove(QChar(','), Qt::CaseInsensitive)+ ";\n";
       if (QString("%7").arg(element.attribute("t7"))!="")
      code += ident + "echo $" + QString("%7").arg(element.attribute("t7")).remove(QChar(','), Qt::CaseInsensitive)+ ";\n";
       if (QString("%8").arg(element.attribute("t8"))!="")
      code += ident + "echo $" + QString("%8").arg(element.attribute("t8")).remove(QChar(','), Qt::CaseInsensitive)+ ";\n";
   // code += ident + "echo $" + element.attribute("text", QObject::tr("(* empty output operation *)")) + ";\n";
  }
  else if(element.nodeName() == "assign")
  {
    code += ident + element.attribute("dest", QObject::tr("/* no destination */")) + " = " + element.attribute("src", QObject::tr("/* no source */")) + ";\n";
  }

  else if(element.nodeName() == "if")
  {
    code += ident + "if ($"+element.attribute("cond", QObject::tr("/* no condition */")) + ")\n";
    QDomNodeList items = element.childNodes();
    for(int i = 0;  i < items.size(); ++i)
    {
      QDomNode node = items.at(i);
      if (node.isElement())
      {
        QDomElement child = node.toElement();
        if (i != 0) code += ident + "else\n";
        code += elementToPHPi(child, ident);

      }
    }
  }
  else if(element.nodeName() == "pre")
  {
    code += ident + QString("while ($%1)\n").arg(element.attribute("cond", QObject::tr("/* no condition */")));

    QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToPHPi(branch, ident);
    }
  }
  else if(element.nodeName() == "for")
  {
    code += ident + QString("for($%1 = %2; $%1 <= $%3; ++$%1)\n").arg(element.attribute("var", QObject::tr("/* no variable */")), element.attribute("from", QObject::tr("/* no value */")), element.attribute("to", QObject::tr("/* no value */")));

    QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToPHPi(branch, ident);
    }
  }
  else if(element.nodeName() == "post")
  {
    code += ident + "do\n";
    QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToPHPi(branch, ident);
    }
    code += ident + QString("while ($%1);\n").arg(element.attribute("cond", QObject::tr("/* no condition */")));
  }

  return code;
}

QString xmlToPHP(const QDomDocument & xml)
{
  QDomElement alg = xml.firstChildElement("algorithm");
  if(!alg.isNull())
  {
    return elementToPHP(alg, "");
  }
  else return QString();
}

QString xmlToPHP(const QString & xml)
{
  QDomDocument doc;
  if(doc.setContent(xml, false))
  {
    return xmlToPHP(doc);
  }
  else return QString();
}
//------------------------------------------------------------------------------
QString elementToJavaScript(const QDomElement & element, const QString & ident)
{
  QString code;
  if(element.nodeName() == "algorithm")
  {
      code += ident + "<scriрt " + "language = \"JavaScript\">" + "\n";
      QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToJavaScript(branch, ident);
    }
    code += "\n";
  }
  else if(element.nodeName() == "branch")
  {
   // code += ident + "\n";
    QDomNodeList items = element.childNodes();
    for(int i = 0;  i < items.size(); ++i)
    {
      QDomNode node = items.at(i);
      if (node.isElement())
      {
        QDomElement child = node.toElement();
        code += elementToJavaScript(child, ident + "  ");
      }
    }
    code += ident + "</script>\n";

  }
  else if(element.nodeName() == "process")
  {
    code += ident + element.attribute("text", QObject::tr("/* empty process */")) + ";\n";
  }
  else if(element.nodeName() == "io")
  {
      if (QString("%1").arg(element.attribute("t1"))!="")
      code += ident + QString("%1").arg(element.attribute("t1")).remove(QChar(','), Qt::CaseInsensitive)+ " = prompt('', ''));\n";
       if (QString("%2").arg(element.attribute("t2"))!="")
      code += ident + QString("%2").arg(element.attribute("t2")).remove(QChar(','), Qt::CaseInsensitive)+ " = prompt('', ''));\n";
       if (QString("%3").arg(element.attribute("t3"))!="")
      code += ident + QString("%3").arg(element.attribute("t3")).remove(QChar(','), Qt::CaseInsensitive)+ " = prompt('', ''));\n";
       if (QString("%4").arg(element.attribute("t4"))!="")
      code += ident + QString("%4").arg(element.attribute("t4")).remove(QChar(','), Qt::CaseInsensitive)+ " = prompt('', ''));\n";
       if (QString("%5").arg(element.attribute("t5"))!="")
      code += ident + QString("%5").arg(element.attribute("t5")).remove(QChar(','), Qt::CaseInsensitive)+ " = prompt('', ''));\n";
       if (QString("%6").arg(element.attribute("t6"))!="")
      code += ident + QString("%6").arg(element.attribute("t6")).remove(QChar(','), Qt::CaseInsensitive)+ " = prompt('', ''));\n";
       if (QString("%7").arg(element.attribute("t7"))!="")
      code += ident + QString("%7").arg(element.attribute("t7")).remove(QChar(','), Qt::CaseInsensitive)+ " = prompt('', ''));\n";
       if (QString("%8").arg(element.attribute("t8"))!="")
      code += ident + QString("%8").arg(element.attribute("t8")).remove(QChar(','), Qt::CaseInsensitive)+ " = prompt('', ''));\n";
   // code += ident + element.attribute("text", QObject::tr("(* empty input operation *)")) + " = prompt('', ''));\n";
  }
  else if(element.nodeName() == "ou")
  {
      if (QString("%1").arg(element.attribute("t1"))!="")
      code += ident + "document.write(" + QString("%1").arg(element.attribute("t1")).remove(QChar(','), Qt::CaseInsensitive)+ ");\n";
       if (QString("%2").arg(element.attribute("t2"))!="")
      code += ident + "document.write(" + QString("%2").arg(element.attribute("t2")).remove(QChar(','), Qt::CaseInsensitive)+ ");\n";
       if (QString("%3").arg(element.attribute("t3"))!="")
      code += ident + "document.write(" + QString("%3").arg(element.attribute("t3")).remove(QChar(','), Qt::CaseInsensitive)+ ");\n";
       if (QString("%4").arg(element.attribute("t4"))!="")
      code += ident + "document.write(" + QString("%4").arg(element.attribute("t4")).remove(QChar(','), Qt::CaseInsensitive)+ ");\n";
       if (QString("%5").arg(element.attribute("t5"))!="")
      code += ident + "document.write(" + QString("%5").arg(element.attribute("t5")).remove(QChar(','), Qt::CaseInsensitive)+ ");\n";
       if (QString("%6").arg(element.attribute("t6"))!="")
      code += ident + "document.write(" + QString("%6").arg(element.attribute("t6")).remove(QChar(','), Qt::CaseInsensitive)+ ");\n";
       if (QString("%7").arg(element.attribute("t7"))!="")
      code += ident + "document.write(" + QString("%7").arg(element.attribute("t7")).remove(QChar(','), Qt::CaseInsensitive)+ ");\n";
       if (QString("%8").arg(element.attribute("t8"))!="")
      code += ident + "document.write(" + QString("%8").arg(element.attribute("t8")).remove(QChar(','), Qt::CaseInsensitive)+ ");\n";
    //code += ident + "document.write(" + element.attribute("text", QObject::tr("(* empty output operation *)")) + ");\n";
  }
  else if(element.nodeName() == "assign")
  {
    code += ident + element.attribute("dest", QObject::tr("/* no destination */")) + " = " + element.attribute("src", QObject::tr("/* no source */")) + ";\n";
  }

  else if(element.nodeName() == "if")
  {
    code += ident + "if ("+element.attribute("cond", QObject::tr("/* no condition */")) + ")\n";
    QDomNodeList items = element.childNodes();
    for(int i = 0;  i < items.size(); ++i)
    {
      QDomNode node = items.at(i);
      if (node.isElement())
      {
        QDomElement child = node.toElement();
        if (i != 0) code += ident + "else\n";
        code += elementToI(child, ident);

      }
    }
  }
  else if(element.nodeName() == "pre")
  {
    code += ident + QString("while (%1)\n").arg(element.attribute("cond", QObject::tr("/* no condition */")));

    QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToI(branch, ident);
    }
  }
  else if(element.nodeName() == "for")
  {
    code += ident + QString("for(%1 = %2; %1 <= %3; ++%1)\n").arg(element.attribute("var", QObject::tr("/* no variable */")), element.attribute("from", QObject::tr("/* no value */")), element.attribute("to", QObject::tr("/* no value */")));

    QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToI(branch, ident);
    }
  }
  else if(element.nodeName() == "post")
  {
    code += ident + "do\n";
    QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToI(branch, ident);
    }
    code += ident + QString("while (%1);\n").arg(element.attribute("cond", QObject::tr("/* no condition */")));
  }

  return code;
}

QString xmlToJavaScript(const QDomDocument & xml)
{
  QDomElement alg = xml.firstChildElement("algorithm");
  if(!alg.isNull())
  {
    return elementToJavaScript(alg, "");
  }
  else return QString();
}

QString xmlToJavaScript(const QString & xml)
{
  QDomDocument doc;
  if(doc.setContent(xml, false))
  {
    return xmlToJavaScript(doc);
  }
  else return QString();
}



//------------------------------------------------------------------------------
QString elementToCdef(const QDomElement & element, const QString & ident)
{
  QString code;
  if(element.nodeName() == "algorithm")
  {
    code += ident + "int main()\n";
    QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToCdef(branch, ident);
    }
    code += "\n";
  }
  else if(element.nodeName() == "branch")
  {
    code += ident + "{\n";
    QDomNodeList items = element.childNodes();
    for(int i = 0;  i < items.size(); ++i)
    {
      QDomNode node = items.at(i);
      if (node.isElement())
      {
        QDomElement child = node.toElement();
        code += elementToCdef(child, ident + "  ");
      }
    }
    code += ident + "}\n";

  }
  else if(element.nodeName() == "process")
  {
    code += ident + element.attribute("text", QObject::tr("/* empty process */")) + ";\n";
  }
  else if(element.nodeName() == "io")
  {
    if (QString("%1").arg(element.attribute("t1"))!="")
    code += ident + "scanf(" + QString("%1").arg(element.attribute("t1")).remove(QChar(','), Qt::CaseInsensitive)+ ");\n";
     if (QString("%2").arg(element.attribute("t2"))!="")
    code += ident + "scanf(" + QString("%2").arg(element.attribute("t2")).remove(QChar(','), Qt::CaseInsensitive)+ ");\n";
     if (QString("%3").arg(element.attribute("t3"))!="")
    code += ident + "scanf(" + QString("%3").arg(element.attribute("t3")).remove(QChar(','), Qt::CaseInsensitive)+ ");\n";
     if (QString("%4").arg(element.attribute("t4"))!="")
    code += ident + "scanf(" + QString("%4").arg(element.attribute("t4")).remove(QChar(','), Qt::CaseInsensitive)+ ");\n";
     if (QString("%5").arg(element.attribute("t5"))!="")
    code += ident + "scanf(" + QString("%5").arg(element.attribute("t5")).remove(QChar(','), Qt::CaseInsensitive)+ ");\n";
     if (QString("%6").arg(element.attribute("t6"))!="")
    code += ident + "scanf(" + QString("%6").arg(element.attribute("t6")).remove(QChar(','), Qt::CaseInsensitive)+ ");\n";
     if (QString("%7").arg(element.attribute("t7"))!="")
    code += ident + "scanf(" + QString("%7").arg(element.attribute("t7")).remove(QChar(','), Qt::CaseInsensitive)+ ");\n";
     if (QString("%8").arg(element.attribute("t8"))!="")
    code += ident + "scanf(" + QString("%8").arg(element.attribute("t8")).remove(QChar(','), Qt::CaseInsensitive)+ ");\n";
   }
  else if(element.nodeName() == "ou")
  {
     if (QString("%1").arg(element.attribute("t1"))!="")
    code += ident + "printf(" + QString("%1").arg(element.attribute("t1")).remove(QChar(','), Qt::CaseInsensitive)+ ");\n";
     if (QString("%2").arg(element.attribute("t2"))!="")
    code += ident + "printf(" + QString("%2").arg(element.attribute("t2")).remove(QChar(','), Qt::CaseInsensitive)+ ");\n";
     if (QString("%3").arg(element.attribute("t3"))!="")
    code += ident + "printf(" + QString("%3").arg(element.attribute("t3")).remove(QChar(','), Qt::CaseInsensitive)+ ");\n";
     if (QString("%4").arg(element.attribute("t4"))!="")
    code += ident + "printf(" + QString("%4").arg(element.attribute("t4")).remove(QChar(','), Qt::CaseInsensitive)+ ");\n";
     if (QString("%5").arg(element.attribute("t5"))!="")
    code += ident + "printf(" + QString("%5").arg(element.attribute("t5")).remove(QChar(','), Qt::CaseInsensitive)+ ");\n";
     if (QString("%6").arg(element.attribute("t6"))!="")
    code += ident + "printf(" + QString("%6").arg(element.attribute("t6")).remove(QChar(','), Qt::CaseInsensitive)+ ");\n";
     if (QString("%7").arg(element.attribute("t7"))!="")
    code += ident + "printf(" + QString("%7").arg(element.attribute("t7")).remove(QChar(','), Qt::CaseInsensitive)+ ");\n";
     if (QString("%8").arg(element.attribute("t8"))!="")
    code += ident + "printf(" + QString("%8").arg(element.attribute("t8")).remove(QChar(','), Qt::CaseInsensitive)+ ");\n";
    //code += ident + "printf(" + element.attribute("text", QObject::tr("(* empty output operation *)")) + ");\n";
  }
  else if(element.nodeName() == "assign")
  {
    code += ident + element.attribute("dest", QObject::tr("/* no destination */")) + " = " + element.attribute("src", QObject::tr("/* no source */")) + ";\n";
  }

  else if(element.nodeName() == "if")
  {
    code += ident + "if ("+element.attribute("cond", QObject::tr("/* no condition */")) + ")\n";
    QDomNodeList items = element.childNodes();
    for(int i = 0;  i < items.size(); ++i)
    {
      QDomNode node = items.at(i);
      if (node.isElement())
      {
        QDomElement child = node.toElement();
        if (i != 0) code += ident + "else\n";
        code += elementToCdef(child, ident);
      }
    }
  }
  else if(element.nodeName() == "pre")
  {
    code += ident + QString("while (%1)\n").arg(element.attribute("cond", QObject::tr("/* no condition */")));

    QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToCdef(branch, ident);
    }
  }
  else if(element.nodeName() == "for")
  {
    code += ident + QString("for(int %1 = %2; i <= %3; ++i)\n").arg(element.attribute("var", QObject::tr("/* no variable */")), element.attribute("from", QObject::tr("/* no value */")), element.attribute("to", QObject::tr("/* no value */")));

    QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToCdef(branch, ident);
    }
  }
  else if(element.nodeName() == "post")
  {
    code += ident + "do\n";
    QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToCdef(branch, ident);
    }
    code += ident + QString("while (%1);\n").arg(element.attribute("cond", QObject::tr("/* no condition */")));
  }

  return code;
}

QString xmlToCdef(const QDomDocument & xml)
{
  QDomElement alg = xml.firstChildElement("algorithm");
  if(!alg.isNull())
  {
    return elementToCdef(alg, "");
  }
  else return QString();
}

QString xmlToCdef(const QString & xml)
{
  QDomDocument doc;
  if(doc.setContent(xml, false))
  {
    return xmlToCdef(doc);
  }
  else return QString();
}

//------------------------------------------------------------------------------
