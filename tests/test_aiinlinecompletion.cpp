#include <QTest>
#include <QPlainTextEdit>
#include "aiinlinecompletion.h"
#include "test_aiinlinecompletion.h"

void TestAiInlineCompletion::testInitialState()
{
    AiInlineCompletion completion;
    QVERIFY(!completion.isEnabled());
    QVERIFY(!completion.hasGhostText());
    QVERIFY(completion.ghostText().isEmpty());
}

void TestAiInlineCompletion::testSetSettings()
{
    AiInlineCompletion completion;
    completion.setSettings("openai", "https://api.openai.com/v1/chat/completions",
                          "gpt-4", true, 300, "test-key");
    QVERIFY(completion.isEnabled());
}

void TestAiInlineCompletion::testSetEditor()
{
    AiInlineCompletion completion;
    QPlainTextEdit editor;
    completion.setEditor(&editor);
    QVERIFY(!completion.isEnabled()); // disabled until setSettings
    completion.setEditor(nullptr);    // clear editor
}
