#include <QTest>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include "dataformatter.h"
#include "test_dataformatter.h"

namespace {

QPlainTextEdit* inputEdit(DataFormatter &w)
{
    auto edits = w.findChildren<QPlainTextEdit*>();
    for (auto *e : edits) {
        if (!e->isReadOnly()) return e;
    }
    return edits.isEmpty() ? nullptr : edits.first();
}

QPlainTextEdit* outputEdit(DataFormatter &w)
{
    auto edits = w.findChildren<QPlainTextEdit*>();
    for (auto *e : edits) {
        if (e->isReadOnly()) return e;
    }
    return nullptr;
}

QComboBox* formatCombo(DataFormatter &w)
{
    return w.findChild<QComboBox*>();
}

QPushButton* button(DataFormatter &w, const QString &text)
{
    auto buttons = w.findChildren<QPushButton*>();
    for (auto *b : buttons) {
        if (b->text() == text) return b;
    }
    return nullptr;
}

QLabel* statusLabel(DataFormatter &w)
{
    // m_statusLabel is created last in setupUI, so it is the last QLabel child
    auto labels = w.findChildren<QLabel*>();
    return labels.isEmpty() ? nullptr : labels.last();
}

void setFormat(DataFormatter &w, const QString &name)
{
    QComboBox *combo = formatCombo(w);
    QVERIFY2(combo, "format combo must exist");
    int idx = combo->findText(name);
    QVERIFY2(idx >= 0, qPrintable("combo must contain " + name));
    combo->setCurrentIndex(idx);
}

}

void TestDataFormatter::testFormatJson()
{
    DataFormatter w;
    inputEdit(w)->setPlainText("{\"a\":1,\"b\":[2,3]}");
    setFormat(w, "JSON");
    QTest::mouseClick(button(w, "Format"), Qt::LeftButton);
    QString out = outputEdit(w)->toPlainText();
    QVERIFY(out.contains("\"a\": 1"));
    QVERIFY(!out.contains("{\"a\":1"));
}

void TestDataFormatter::testFormatJsonInvalid()
{
    DataFormatter w;
    inputEdit(w)->setPlainText("{invalid json");
    setFormat(w, "JSON");
    QTest::mouseClick(button(w, "Format"), Qt::LeftButton);
    QVERIFY(outputEdit(w)->toPlainText().isEmpty());
    QVERIFY(statusLabel(w)->text().contains("Error"));
}

void TestDataFormatter::testMinifyJson()
{
    DataFormatter w;
    inputEdit(w)->setPlainText("{\n  \"a\": 1\n}");
    setFormat(w, "JSON");
    QTest::mouseClick(button(w, "Minify"), Qt::LeftButton);
    QString out = outputEdit(w)->toPlainText();
    QVERIFY(out.contains("{\"a\":1}"));
}

void TestDataFormatter::testValidateJsonValid()
{
    DataFormatter w;
    inputEdit(w)->setPlainText("{\"ok\":true}");
    setFormat(w, "JSON");
    QTest::mouseClick(button(w, "Validate"), Qt::LeftButton);
    QVERIFY(statusLabel(w)->text().contains("Valid"));
}

void TestDataFormatter::testValidateJsonInvalid()
{
    DataFormatter w;
    inputEdit(w)->setPlainText("{broken");
    setFormat(w, "JSON");
    QTest::mouseClick(button(w, "Validate"), Qt::LeftButton);
    QVERIFY(statusLabel(w)->text().contains("Invalid"));
}

void TestDataFormatter::testFormatYaml()
{
    DataFormatter w;
    inputEdit(w)->setPlainText("name: test\n  nested: value");
    setFormat(w, "YAML");
    QTest::mouseClick(button(w, "Format"), Qt::LeftButton);
    QString out = outputEdit(w)->toPlainText();
    QVERIFY(!out.isEmpty());
}

void TestDataFormatter::testMinifyYaml()
{
    DataFormatter w;
    inputEdit(w)->setPlainText("# comment\nkey: value\n\nother: 2");
    setFormat(w, "YAML");
    QTest::mouseClick(button(w, "Minify"), Qt::LeftButton);
    QString out = outputEdit(w)->toPlainText();
    QVERIFY(!out.contains("#"));
    QVERIFY(!out.contains("comment"));
}

void TestDataFormatter::testValidateYaml()
{
    DataFormatter w;
    inputEdit(w)->setPlainText("a: 1");
    setFormat(w, "YAML");
    QTest::mouseClick(button(w, "Validate"), Qt::LeftButton);
    QVERIFY(statusLabel(w)->text().contains("Valid"));
}

void TestDataFormatter::testFormatXml()
{
    DataFormatter w;
    inputEdit(w)->setPlainText("<a><b/></a>");
    setFormat(w, "XML");
    QTest::mouseClick(button(w, "Format"), Qt::LeftButton);
    QString out = outputEdit(w)->toPlainText();
    QVERIFY(out.contains("<a>"));
    QVERIFY(out.contains("</a>"));
}

void TestDataFormatter::testMinifyXml()
{
    DataFormatter w;
    inputEdit(w)->setPlainText("<a>\n  <b/>\n</a>");
    setFormat(w, "XML");
    QTest::mouseClick(button(w, "Minify"), Qt::LeftButton);
    QString out = outputEdit(w)->toPlainText();
    QVERIFY(!out.contains('\n'));
    QVERIFY(out.contains("<a><b/></a>"));
}

void TestDataFormatter::testValidateXmlValid()
{
    DataFormatter w;
    inputEdit(w)->setPlainText("<a><b></b></a>");
    setFormat(w, "XML");
    QTest::mouseClick(button(w, "Validate"), Qt::LeftButton);
    QVERIFY(statusLabel(w)->text().contains("Valid"));
}

void TestDataFormatter::testValidateXmlInvalid()
{
    DataFormatter w;
    inputEdit(w)->setPlainText("<a><b></a>");
    setFormat(w, "XML");
    QTest::mouseClick(button(w, "Validate"), Qt::LeftButton);
    QVERIFY(statusLabel(w)->text().contains("Invalid"));
}

void TestDataFormatter::testClear()
{
    DataFormatter w;
    inputEdit(w)->setPlainText("{\"x\":1}");
    setFormat(w, "JSON");
    QTest::mouseClick(button(w, "Format"), Qt::LeftButton);
    QVERIFY(!outputEdit(w)->toPlainText().isEmpty());
    QTest::mouseClick(button(w, "Clear"), Qt::LeftButton);
    QVERIFY(inputEdit(w)->toPlainText().isEmpty());
    QVERIFY(outputEdit(w)->toPlainText().isEmpty());
}

void TestDataFormatter::testFormatEmptyInput()
{
    DataFormatter w;
    setFormat(w, "JSON");
    QTest::mouseClick(button(w, "Format"), Qt::LeftButton);
    QVERIFY(outputEdit(w)->toPlainText().isEmpty());
}

void TestDataFormatter::testFormatChangedClearsOutput()
{
    DataFormatter w;
    inputEdit(w)->setPlainText("{\"x\":1}");
    setFormat(w, "JSON");
    QTest::mouseClick(button(w, "Format"), Qt::LeftButton);
    QVERIFY(!outputEdit(w)->toPlainText().isEmpty());
    setFormat(w, "YAML");
    QVERIFY(outputEdit(w)->toPlainText().isEmpty());
}

void TestDataFormatter::testJsonRoundTripViaButtons()
{
    DataFormatter w;
    inputEdit(w)->setPlainText("{\"a\":1}");
    setFormat(w, "JSON");
    QTest::mouseClick(button(w, "Minify"), Qt::LeftButton);
    QString minified = outputEdit(w)->toPlainText();
    QVERIFY(minified.contains("{\"a\":1}"));

    // Feed the minified output back in and format it
    inputEdit(w)->setPlainText(minified);
    QTest::mouseClick(button(w, "Format"), Qt::LeftButton);
    QVERIFY(outputEdit(w)->toPlainText().contains("\"a\": 1"));
}
