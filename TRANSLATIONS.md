How to translate AFCE?
======================

Everyone can translate AFCE to another language. Before translating you should to define the name of the locale. For example, for German it will be "de_DE". Now you can copy afce_en_US.ts and rename it to afce_de_DE.ts. This file has text format (XML). You can open it in any plain text editor and translate the strings to other language. Instead of direct editing you can use Qt Linguist tool for more convinient work.

To translate help files you should to translate all files from the help directory. As you can see there are a subfolders in the help folder. Each subfolder is for certain language.

There are another files to translate. These are:
* afce.desktop
* afce.xml
* generators/*.json
* afce.nsi

Generators
----------------
Each json file in the "generators" folder has a special object called "name". The object contains a string keys that are named according to a locale. If the translation will contain a non-ASCII chars you should to use \uXXXX notation. Please, do not translate every language name. Most of people know how the programming language is called in English. 
To translate non-ASCII symbols you can use command:

`$ echo Алгоритмический язык Ершова | recode UTF-8..JAVA`

You can send me your translations by means of GitHub or Email <vicking@yandex.ru>.