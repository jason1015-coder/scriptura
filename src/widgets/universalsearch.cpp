#include "universalsearch.h"

#include <QVBoxLayout>
#include <QKeyEvent>
#include <QApplication>
#include <QPainter>
#include <QStyledItemDelegate>
#include <algorithm>

// -----------------------------------------------------------------------
// Custom delegate to render search results with category badges
// -----------------------------------------------------------------------

class SearchResultDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        painter->save();

        if (opt.state & QStyle::State_Selected)
            painter->fillRect(opt.rect, opt.palette.color(QPalette::Highlight));
        else if (opt.state & QStyle::State_MouseOver)
            painter->fillRect(opt.rect, opt.palette.color(QPalette::AlternateBase));

        const int x = opt.rect.x() + 8;
        const int y = opt.rect.y();
        const int cat = index.data(Qt::UserRole).toInt();

        QString cl;
        QColor bc;
        switch (cat) {
        case 0: cl = "CMD";  bc = QColor(96, 165, 250, 180);  break;
        case 1: cl = "FILE"; bc = QColor(52, 211, 153, 180);  break;
        case 2: cl = "SET";  bc = QColor(251, 191, 36, 180);  break;
        case 3: cl = "THM";  bc = QColor(192, 132, 252, 180); break;
        }

        QRect br(x, y + 8, 36, 16);
        painter->setPen(Qt::NoPen);
        painter->setBrush(bc);
        painter->drawRoundedRect(br, 3, 3);
        painter->setPen(Qt::white);
        QFont bf = opt.font; bf.setPixelSize(9); bf.setBold(true);
        painter->setFont(bf);
        painter->drawText(br, Qt::AlignCenter, cl);

        QFont mf = opt.font; mf.setPixelSize(13);
        painter->setFont(mf);
        painter->setPen(opt.palette.color(QPalette::Text));
        painter->drawText(x + 52, y + 4, opt.rect.width() - 110, 20,
                          Qt::AlignLeft | Qt::AlignVCenter,
                          index.data(Qt::DisplayRole).toString());

        QString sub = index.data(Qt::UserRole + 1).toString();
        if (!sub.isEmpty()) {
            QFont sf = opt.font; sf.setPixelSize(10);
            painter->setFont(sf);
            painter->setPen(opt.palette.color(QPalette::Midlight));
            painter->drawText(x + 52, y + 22, opt.rect.width() - 110, 16,
                              Qt::AlignLeft | Qt::AlignVCenter, sub);
        }

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        return QSize(0, 42);
    }
};

// -----------------------------------------------------------------------
// UniversalSearchPopup implementation
// -----------------------------------------------------------------------

UniversalSearchPopup::UniversalSearchPopup(QLineEdit *searchField, QWidget *parent)
    : QFrame(parent)
    , m_searchField(searchField)
    , m_listWidget(new QListWidget(this))
{
    setObjectName("universalSearchPopup");
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_ShowWithoutActivating, false);

    m_listWidget->setItemDelegate(new SearchResultDelegate(this));
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_listWidget->setSpacing(1);
    m_listWidget->setMouseTracking(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_listWidget);

    setFixedWidth(480);
    setMaximumHeight(400);

    setStyleSheet(R"(
        QFrame#universalSearchPopup {
            background-color: palette(base);
            border: 1px solid palette(mid);
            border-radius: 8px;
        }
        QListWidget {
            background-color: transparent;
            border: none;
            border-radius: 8px;
            padding: 4px;
            outline: none;
        }
        QListWidget::item { border-radius: 8px; padding: 2px 4px; }
        QListWidget::item:selected {
            background-color: palette(highlight);
            color: palette(highlighted-text);
        }
    )");

    connect(m_searchField, &QLineEdit::textChanged, this, &UniversalSearchPopup::onSearchTextChanged);
    connect(m_listWidget, &QListWidget::itemActivated, this, &UniversalSearchPopup::activateItem);
    connect(m_listWidget, &QListWidget::itemClicked, this, &UniversalSearchPopup::activateItem);

    m_searchField->installEventFilter(this);
}

void UniversalSearchPopup::registerResult(const SearchResult &result)
{
    m_permanentResults.append(result);
}

void UniversalSearchPopup::registerResults(const QVector<SearchResult> &results)
{
    m_permanentResults.append(results);
}

void UniversalSearchPopup::clearResults()
{
    m_permanentResults.clear();
    m_allResults.clear();
    m_listWidget->clear();
}

void UniversalSearchPopup::setFileModel(QFileSystemModel *model, const QString &rootPath)
{
    m_fileModel = model;
    m_rootPath = rootPath;
}

void UniversalSearchPopup::showPopup()
{
    QPoint pos = m_searchField->mapToGlobal(QPoint(0, m_searchField->height() + 4));
    move(pos);
    show();
    raise();
}

void UniversalSearchPopup::hidePopup()
{
    QFrame::hide();
    emit dismissed();
}

bool UniversalSearchPopup::eventFilter(QObject *obj, QEvent *event)
{
    if (obj != m_searchField)
        return QFrame::eventFilter(obj, event);

    if (event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent*>(event);
        switch (ke->key()) {
        case Qt::Key_Down:
        case Qt::Key_Up: {
            int dir = (ke->key() == Qt::Key_Down) ? 1 : -1;
            int cur = m_listWidget->currentRow();
            int nxt = qBound(0, cur + dir, m_listWidget->count() - 1);
            m_listWidget->setCurrentRow(nxt);
            return true;
        }
        case Qt::Key_Escape:
            hidePopup();
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (m_listWidget->currentItem()) {
                activateItem(m_listWidget->currentItem());
                return true;
            }
            break;
        default:
            break;
        }
    }

    if (event->type() == QEvent::FocusIn) {
        if (!m_searchField->text().trimmed().isEmpty() && m_listWidget->count() > 0)
            showPopup();
    }

    return QFrame::eventFilter(obj, event);
}

void UniversalSearchPopup::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        hidePopup();
        m_searchField->setFocus();
        return;
    case Qt::Key_Down: case Qt::Key_Up: {
        int dir = (event->key() == Qt::Key_Down) ? 1 : -1;
        int cur = m_listWidget->currentRow();
        int nxt = qBound(0, cur + dir, m_listWidget->count() - 1);
        m_listWidget->setCurrentRow(nxt);
        return;
    }
    case Qt::Key_Return: case Qt::Key_Enter:
        if (m_listWidget->currentItem())
            activateItem(m_listWidget->currentItem());
        return;
    default: break;
    }
    QFrame::keyPressEvent(event);
}

void UniversalSearchPopup::onSearchTextChanged(const QString &text)
{
    filterResults(text.trimmed());

    if (m_listWidget->count() > 0 && !text.trimmed().isEmpty())
        showPopup();
    else
        hidePopup();
}

void UniversalSearchPopup::filterResults(const QString &query)
{
    m_listWidget->clear();

    // Start with permanent results, add file matches on top
    m_allResults = m_permanentResults;

    struct Scored { int index; int score; };
    QVector<Scored> scored;

    // Score all permanent results
    for (int i = 0; i < m_permanentResults.size(); ++i) {
        int s = query.isEmpty() ? 1 : fuzzyScore(query.toLower(), m_permanentResults[i].label.toLower());
        if (s > 0)
            scored.append({i, s});
    }

    // Collect file matches from the file model (only when query is non-empty)
    if (!query.isEmpty() && m_fileModel && !m_rootPath.isEmpty()) {
        QModelIndex rootIdx = m_fileModel->index(m_rootPath);
        if (rootIdx.isValid()) {
            struct FMatch { QString name; QString path; int score; };
            QVector<FMatch> files;
            int rootRows = m_fileModel->rowCount(rootIdx);
            for (int r = 0; r < rootRows; ++r) {
                QModelIndex child = m_fileModel->index(r, 0, rootIdx);
                QString name = m_fileModel->fileName(child);
                int s = fuzzyScore(query.toLower(), name.toLower());
                if (s > 0)
                    files.append({name, m_fileModel->filePath(child), s});

                // One level deep for subdirectories
                if (m_fileModel->isDir(child)) {
                    int subRows = m_fileModel->rowCount(child);
                    for (int s2 = 0; s2 < subRows && s2 < 10; ++s2) {
                        QModelIndex sub = m_fileModel->index(s2, 0, child);
                        QString sn = m_fileModel->fileName(sub);
                        int ss = fuzzyScore(query.toLower(), sn.toLower());
                        if (ss > 0)
                            files.append({sn, m_fileModel->filePath(sub), ss});
                    }
                }
            }

            // Sort files by score
            std::sort(files.begin(), files.end(), [](const FMatch &a, const FMatch &b) {
                return a.score > b.score;
            });

            // Append top file matches to allResults and scored
            int fileBase = m_allResults.size();
            for (int fi = 0; fi < qMin(files.size(), 20); ++fi) {
                SearchResult sr;
                sr.category = SearchResult::File;
                sr.label = files[fi].name;
                sr.sublabel = files[fi].path;
                sr.action = [this, path = files[fi].path]() { emit fileOpenRequested(path); };
                m_allResults.append(sr);
                scored.append({fileBase + fi, files[fi].score});
            }
        }
    }

    // Sort by score descending
    std::sort(scored.begin(), scored.end(), [](const Scored &a, const Scored &b) {
        return a.score > b.score;
    });

    // Populate the list widget
    for (const Scored &s : scored) {
        if (s.index >= m_allResults.size()) continue;
        const auto &r = m_allResults[s.index];
        auto *item = new QListWidgetItem(r.label);
        item->setData(Qt::UserRole, static_cast<int>(r.category));
        item->setData(Qt::UserRole + 1, r.sublabel);
        item->setData(Qt::UserRole + 2, s.index);
        item->setSizeHint(QSize(0, 42));
        m_listWidget->addItem(item);
    }

    if (m_listWidget->count() > 0)
        m_listWidget->setCurrentRow(0);

    int vc = qMin(m_listWidget->count(), 8);
    setMaximumHeight(vc * 42 + 16);
}

void UniversalSearchPopup::activateItem(QListWidgetItem *item)
{
    if (!item) return;

    int idx = item->data(Qt::UserRole + 2).toInt();
    if (idx >= 0 && idx < m_allResults.size()) {
        const SearchResult &r = m_allResults[idx];
        hidePopup();
        m_searchField->clear();
        if (r.action)
            r.action();
    }
}

int UniversalSearchPopup::fuzzyScore(const QString &pattern, const QString &text) const
{
    int pi = 0, score = 0, last = -1;
    for (int ti = 0; ti < text.size() && pi < pattern.size(); ++ti) {
        if (text[ti] == pattern[pi]) {
            score += (last == ti - 1) ? 3 : 1;
            last = ti;
            ++pi;
        }
    }
    return (pi == pattern.size()) ? score : 0;
}
