#include "debuggergutter.h"
#include "codeeditor.h"
#include <QPainter>
#include <QMouseEvent>
#include <QPainterPath>

DebuggerGutter::DebuggerGutter(CodeEditor *editor, QWidget *parent)
    : QWidget(parent)
    , m_editor(editor)
{
    setObjectName("debuggerGutter");
}

void DebuggerGutter::setBreakpoints(const QSet<int> &lines)
{
    m_breakpoints = lines;
    update();
}

void DebuggerGutter::toggleBreakpoint(int line)
{
    if (m_breakpoints.contains(line)) {
        m_breakpoints.remove(line);
        emit breakpointToggled(line, false);
    } else {
        m_breakpoints.insert(line);
        emit breakpointToggled(line, true);
    }
    update();
}

void DebuggerGutter::clearBreakpoints()
{
    QSet<int> oldBreakpoints = m_breakpoints;
    m_breakpoints.clear();
    for (int line : oldBreakpoints) {
        emit breakpointToggled(line, false);
    }
    update();
}

void DebuggerGutter::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.fillRect(event->rect(), palette().color(QPalette::AlternateBase));

    if (!m_editor) return;

    const int lineHeight = fontMetrics().height();

    for (int line : m_breakpoints) {
        const int y = (line - 1) * lineHeight + lineHeight / 2;

        QPainterPath path;
        path.addEllipse(4, y - 5, 10, 10);
        painter.setPen(QPen(Qt::red, 2));
        painter.setBrush(Qt::red);
        painter.drawPath(path);
    }
}

void DebuggerGutter::mousePressEvent(QMouseEvent *event)
{
    if (!m_editor) return;

    const int lineHeight = fontMetrics().height();
    const int line = static_cast<int>(event->position().y() / lineHeight) + 1;
    toggleBreakpoint(line);
}
