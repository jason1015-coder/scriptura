#ifndef TEST_DATAFORMATTER_H
#define TEST_DATAFORMATTER_H

#include <QObject>

class TestDataFormatter : public QObject
{
    Q_OBJECT
private slots:
    void testFormatJson();
    void testFormatJsonInvalid();
    void testMinifyJson();
    void testValidateJsonValid();
    void testValidateJsonInvalid();
    void testFormatYaml();
    void testMinifyYaml();
    void testValidateYaml();
    void testFormatXml();
    void testMinifyXml();
    void testValidateXmlValid();
    void testValidateXmlInvalid();
    void testClear();
    void testFormatEmptyInput();
    void testFormatChangedClearsOutput();
    void testJsonRoundTripViaButtons();
};

#endif // TEST_DATAFORMATTER_H
