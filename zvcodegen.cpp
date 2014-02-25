/****************************************************************************
**                                                                         **
** Copyright (C) 2009 Victor Zinkevich. All rights reserved.               **
** Contact: vicking@yandex.ru                                              **
**                                                                         **
** This file is part of the Algorithm Flowchart Editor project.            **
**                                                                         **
** This file may be used under the terms of the GNU                        **
** General Public License versions 2.0 or 3.0 as published by the Free     **
** Software Foundation and appearing in the file LICENSE.TXT included in   **
** the packaging of this file.                                             **
** You can find license at http://www.gnu.org/licenses/gpl.html            **
**                                                                         **
****************************************************************************/

#include "zvcodegen.h"

QString elementToPascal(const QDomElement & element, const QString & ident, bool addSemicolon = true, bool addBeginEnd = true)
{
  QString code;
  if(element.nodeName() == "algorithm")
  {
    code += ident + "procedure Algorithm;\n";
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
    code += ident + element.attribute("text", QObject::tr("(* empty I/O operation *)")) + ";\n";
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
        code += elementToPascal(child, ident, i!=0);
      }
    }
  }
  else if(element.nodeName() == "pre")
  {
    code += ident + QString("while %1 do\n").arg(element.attribute("cond", QObject::tr("(* no condition *)")));

    QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToPascal(branch, ident);
    }
  }
  else if(element.nodeName() == "for")
  {
    code += ident + QString("for %1 := %2 to %3 do\n").arg(element.attribute("var", QObject::tr("(* no variable *)")), element.attribute("from", QObject::tr("(* no value *)")), element.attribute("to", QObject::tr("(* no value *)")));

    QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToPascal(branch, ident);
    }
  }
  else if(element.nodeName() == "post")
  {
    code += ident + "repeat\n";
    QDomElement branch = element.firstChildElement("branch");
    if (!branch.isNull())
    {
      code += elementToPascal(branch, ident, true, false);
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
    code += "\n" + ident + element.attribute("text", QObject::tr("(* empty I/O operation *)"));
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
    code += ident + "void algorithm()\n";
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
    code += ident + element.attribute("text", QObject::tr("/* empty I/O operation */")) + ";\n";
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
    return xmlToPascal(doc);
  }
  else return QString();
}
