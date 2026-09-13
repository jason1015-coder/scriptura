#ifndef LANGUAGE_DEFINITION_H
#define LANGUAGE_DEFINITION_H

#include <QString>
#include <QStringList>
#include <QVector>

struct LanguageDefinition
{
    QString name;
    QStringList extensions;
    QStringList keywords;
    QStringList builtins;
    QString blockCommentStart;
    QString blockCommentEnd;
    QString lineComment;
    bool hasCStyleComments = false;
    bool hasHtmlComments = false;
    bool hasPythonTripleStrings = false;
    bool hasBracketMatching = true;
    QStringList stringDelimiters = {"\"", "'"};
    QStringList multiLineStringDelimiters;
    QString templateStringDelimiter;
};

#endif // LANGUAGE_DEFINITION_H
