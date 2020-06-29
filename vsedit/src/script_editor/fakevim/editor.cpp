/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

    This file is part of CopyQ.

    CopyQ is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    CopyQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CopyQ.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "editor.h"
#include "fakevimactions.h"
#include "fakevimhandler.h"

#include <QApplication>
#include <QFontMetrics>
#include <QMainWindow>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QTextBlock>
#include <QTextEdit>
#include <QTextStream>
#include <QTemporaryFile>
#include <QStandardPaths>
#include <QDebug>

#define EDITOR(editor, call) \
    if (QPlainTextEdit *ed = qobject_cast<QPlainTextEdit *>(editor)) { \
        (ed->call); \
    } else if (QTextEdit *ed = qobject_cast<QTextEdit *>(editor)) { \
        (ed->call); \
    }

using namespace FakeVim::Internal;

typedef QLatin1String _;

/**
 * Simple editor widget.
 * @tparam TextEdit QTextEdit or QPlainTextEdit as base class
 */
template <typename TextEdit>
class Editor : public TextEdit
{
public:
    explicit Editor(QWidget *parent = nullptr) : TextEdit(parent)
    {
        TextEdit::setCursorWidth(0);
    }

    void paintEvent(QPaintEvent *e)
    {
        TextEdit::paintEvent(e);

        if ( !m_cursorRect.isNull() && e->rect().intersects(m_cursorRect) ) {
            QRect rect = m_cursorRect;
            m_cursorRect = QRect();
            TextEdit::viewport()->update(rect);
        }

        // Draw text cursor.
        QRect rect = TextEdit::cursorRect();
        if ( e->rect().intersects(rect) ) {
            QPainter painter(TextEdit::viewport());

            if ( TextEdit::overwriteMode() ) {
                QFontMetrics fm(TextEdit::font());
                const int position = TextEdit::textCursor().position();
                const QChar c = TextEdit::document()->characterAt(position);
                rect.setWidth(fm.horizontalAdvance(c));
                painter.setPen(Qt::NoPen);
                painter.setBrush(TextEdit::palette().color(QPalette::Base));
                painter.setCompositionMode(QPainter::CompositionMode_Difference);
            } else {
                rect.setWidth(TextEdit::cursorWidth());
                painter.setPen(TextEdit::palette().color(QPalette::Text));
            }

            painter.drawRect(rect);
            m_cursorRect = rect;
        }
    }

private:
    QRect m_cursorRect;
};

QWidget *createEditorWidget(bool usePlainTextEdit)
{
    QWidget *editor = nullptr;
    if (usePlainTextEdit) {
        Editor<QPlainTextEdit> *w = new Editor<QPlainTextEdit>;
        w->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        editor = w;
    } else {
        Editor<QTextEdit> *w = new Editor<QTextEdit>;
        w->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        editor = w;
    }
    editor->setObjectName(_("Editor"));
    editor->setFocus();

    return editor;
}

void initHandler(FakeVimHandler *handler)
{
    handler->handleCommand(_("set nopasskeys"));
    handler->handleCommand(_("set nopasscontrolkey"));

    // Set some Vim options.
    handler->handleCommand(_("set expandtab"));
    handler->handleCommand(_("set shiftwidth=8"));
    handler->handleCommand(_("set tabstop=16"));
    handler->handleCommand(_("set autoindent"));
    handler->handleCommand(_("set smartindent"));

    QString vimrc = QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
#ifdef Q_OS_WIN
        + "/_vimrc";
#else
        + "/.vimrc";
#endif

    if (QFile::exists(vimrc)) {
        qDebug() << "Loading vimrc";
        handler->handleCommand("source " + vimrc);
    }

    handler->installEventFilter();
    handler->setupWidget();
}

void initMainWindow(QMainWindow *mainWindow, QWidget *centralWidget, const QString &title)
{
    mainWindow->setWindowTitle(QString(_("FakeVim (%1)")).arg(title));
    mainWindow->setCentralWidget(centralWidget);
    mainWindow->resize(600, 650);
    mainWindow->move(0, 0);
    mainWindow->show();

    // Set monospace font for editor and status bar.
    QFont font = QApplication::font();
    font.setFamily(_("Monospace"));
    centralWidget->setFont(font);
    mainWindow->statusBar()->setFont(font);
}

void clearUndoRedo(QWidget *editor)
{
    EDITOR(editor, setUndoRedoEnabled(false));
    EDITOR(editor, setUndoRedoEnabled(true));
}

Proxy  *connectSignals(
        FakeVimHandler *handler, QMainWindow *mainWindow, QWidget *editor,
        const QString &fileToEdit)
{
    Proxy *proxy = new Proxy(editor, mainWindow, handler);

    handler->commandBufferChanged
        .connect([proxy](const QString &contents, int cursorPos, int /*anchorPos*/, int /*messageLevel*/) {
        proxy->changeStatusMessage(contents, cursorPos);
    });
    handler->extraInformationChanged.connect([proxy](const QString &text) {
        proxy->changeExtraInformation(text);
    });
    handler->statusDataChanged.connect([proxy](const QString &text) {
        proxy->changeStatusData(text);
    });
    handler->highlightMatches.connect([proxy](const QString &needle) {
        proxy->highlightMatches(needle);
    });
    handler->handleExCommandRequested.connect([proxy](bool *handled, const ExCommand &cmd) {
        proxy->handleExCommand(handled, cmd);
    });
    handler->requestSetBlockSelection.connect([proxy](const QTextCursor &cursor) {
        proxy->requestSetBlockSelection(cursor);
    });
    handler->requestDisableBlockSelection.connect([proxy] {
        proxy->requestDisableBlockSelection();
    });
    handler->requestHasBlockSelection.connect([proxy](bool *on) {
        proxy->requestHasBlockSelection(on);
    });

    handler->indentRegion.connect([proxy](int beginBlock, int endBlock, QChar typedChar) {
        proxy->indentRegion(beginBlock, endBlock, typedChar);
    });
    handler->checkForElectricCharacter.connect([proxy](bool *result, QChar c) {
            proxy->checkForElectricCharacter(result, c);
    });

    QObject::connect(proxy, &Proxy::handleInput,
        handler, [handler] (const QString &text) { handler->handleInput(text); });

    return proxy;
}

Proxy::Proxy(QWidget *widget, QMainWindow *mw, QObject *parent)
    : QObject(parent), m_widget(widget), m_mainWindow(mw)
{
}

void Proxy::changeStatusData(const QString &info)
{
    m_statusData = info;
    updateStatusBar();
}

void Proxy::highlightMatches(const QString &pattern)
{
    QTextEdit *ed = qobject_cast<QTextEdit *>(m_widget);
    if (!ed) {
        return;
    }

    QTextEdit::ExtraSelection selection;
    selection.format.setBackground(Qt::yellow);
    selection.format.setForeground(Qt::black);

    // Highlight matches.
    QTextDocument *doc = ed->document();
    QRegExp re(pattern);
    QTextCursor cur = doc->find(re);

    m_searchSelection.clear();

    int a = cur.position();
    while ( !cur.isNull() ) {
        if ( cur.hasSelection() ) {
            selection.cursor = cur;
            m_searchSelection.append(selection);
        } else {
            cur.movePosition(QTextCursor::NextCharacter);
        }
        cur = doc->find(re, cur);
        int b = cur.position();
        if (a == b) {
            cur.movePosition(QTextCursor::NextCharacter);
            cur = doc->find(re, cur);
            b = cur.position();
            if (a == b) break;
        }
        a = b;
    }

    updateExtraSelections();
}

void Proxy::changeStatusMessage(const QString &contents, int cursorPos)
{
    m_statusMessage = cursorPos == -1 ? contents
                                      : contents.left(cursorPos) + QChar(10073) + contents.mid(cursorPos);
    updateStatusBar();
}

void Proxy::changeExtraInformation(const QString &info)
{
    QMessageBox::information(m_widget, tr("Information"), info);
}

void Proxy::updateStatusBar()
{
    int slack = 80 - m_statusMessage.size() - m_statusData.size();
    QString msg = m_statusMessage + QString(slack, QLatin1Char(' ')) + m_statusData;
    m_mainWindow->statusBar()->showMessage(msg);
}

void Proxy::handleExCommand(bool *handled, const ExCommand &cmd)
{
    if ( wantSaveAndQuit(cmd) ) {
        emit requestSaveAndQuit(); // :wq
    } else if ( wantSave(cmd) ) {
        emit requestSave(); // :w
    } else if ( wantQuit(cmd) ) {
        if (cmd.hasBang) {
            invalidate(); // :q!
        } else {
            emit requestQuit(); // :q
        }
    } else if ( wantRun(cmd) ) {
        emit requestRun();
    } else {
        *handled = false;
        return;
    }

    *handled = true;
}

void Proxy::requestSetBlockSelection(const QTextCursor &tc)
{
    QPlainTextEdit *ed = qobject_cast<QPlainTextEdit *>(m_widget);
    if (!ed)
        return;

    QPalette pal = ed->parentWidget() != nullptr ? ed->parentWidget()->palette()
                                              : QApplication::palette();

    m_blockSelection.clear();
    m_clearSelection.clear();

    QTextCursor cur = tc;

    QTextEdit::ExtraSelection selection;
    selection.format.setBackground( pal.color(QPalette::Base) );
    selection.format.setForeground( pal.color(QPalette::Text) );
    selection.cursor = cur;
    m_clearSelection.append(selection);

    selection.format.setBackground( pal.color(QPalette::Highlight) );
    selection.format.setForeground( pal.color(QPalette::HighlightedText) );

    int from = cur.positionInBlock();
    int to = cur.anchor() - cur.document()->findBlock(cur.anchor()).position();
    const int min = qMin(cur.position(), cur.anchor());
    const int max = qMax(cur.position(), cur.anchor());
    for ( QTextBlock block = cur.document()->findBlock(min);
          block.isValid() && block.position() < max; block = block.next() ) {
        cur.setPosition( block.position() + qMin(from, block.length()) );
        cur.setPosition( block.position() + qMin(to, block.length()), QTextCursor::KeepAnchor );
        selection.cursor = cur;
        m_blockSelection.append(selection);
    }

    disconnect( ed, &QPlainTextEdit::selectionChanged,
                this, &Proxy::updateBlockSelection );
    ed->setTextCursor(tc);
    connect( ed, &QPlainTextEdit::selectionChanged,
             this, &Proxy::updateBlockSelection );

    QPalette pal2 = ed->palette();
    pal2.setColor(QPalette::Highlight, Qt::transparent);
    pal2.setColor(QPalette::HighlightedText, Qt::transparent);
    ed->setPalette(pal2);

    updateExtraSelections();
}

void Proxy::requestDisableBlockSelection()
{
    QPlainTextEdit *ed = qobject_cast<QPlainTextEdit *>(m_widget);
    if (!ed)
        return;

    QPalette pal = ed->parentWidget() != NULL ? ed->parentWidget()->palette()
                                              : QApplication::palette();

    m_blockSelection.clear();
    m_clearSelection.clear();

    ed->setPalette(pal);

    disconnect( ed, &QPlainTextEdit::selectionChanged,
                this, &Proxy::updateBlockSelection );

    updateExtraSelections();
}

void Proxy::updateBlockSelection()
{
    QPlainTextEdit *ed = qobject_cast<QPlainTextEdit *>(m_widget);
    if (!ed)
        return;

    requestSetBlockSelection(ed->textCursor());
}

void Proxy::requestHasBlockSelection(bool *on)
{
    *on = !m_blockSelection.isEmpty();
}

void Proxy::indentRegion(int beginBlock, int endBlock, QChar typedChar)
{
    QPlainTextEdit *ed = qobject_cast<QPlainTextEdit *>(m_widget);
    if (!ed)
        return;

    const auto indentSize = theFakeVimSetting(ConfigShiftWidth)->value().toInt();

    QTextDocument *doc = ed->document();
    QTextBlock startBlock = doc->findBlockByNumber(beginBlock);

    // Record line lenghts for mark adjustments
    QVector<int> lineLengths(endBlock - beginBlock + 1);
    QTextBlock block = startBlock;

    for (int i = beginBlock; i <= endBlock; ++i) {
        const auto line = block.text();
        lineLengths[i - beginBlock] = line.length();
        if (typedChar.unicode() == 0 && line.simplified().isEmpty()) {
            // clear empty lines
            QTextCursor cursor(block);
            while (!cursor.atBlockEnd())
                cursor.deleteChar();
        } else {
            const auto previousBlock = block.previous();
            const auto previousLine = previousBlock.isValid() ? previousBlock.text() : QString();

            int indent = firstNonSpace(previousLine);
            if (typedChar == '}')
                indent = std::max(0, indent - indentSize);
            else if ( previousLine.endsWith("{") )
                indent += indentSize;
            const auto indentString = QString(" ").repeated(indent);

            QTextCursor cursor(block);
            cursor.beginEditBlock();
            cursor.movePosition(QTextCursor::StartOfBlock);
            cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, firstNonSpace(line));
            cursor.removeSelectedText();
            cursor.insertText(indentString);
            cursor.endEditBlock();
        }
        block = block.next();
    }
}

void Proxy::checkForElectricCharacter(bool *result, QChar c)
{
    *result = c == '{' || c == '}';
}

int Proxy::firstNonSpace(const QString &text)
{
    int indent = 0;
    while ( indent < text.length() && text.at(indent) == ' ' )
        ++indent;
    return indent;
}

void Proxy::updateExtraSelections()
{
    QPlainTextEdit *ed = qobject_cast<QPlainTextEdit *>(m_widget);
    if (ed)
        ed->setExtraSelections(m_clearSelection + m_searchSelection + m_blockSelection);
}

bool Proxy::wantSaveAndQuit(const ExCommand &cmd)
{
    return cmd.cmd == "wq";
}

bool Proxy::wantSave(const ExCommand &cmd)
{
    return cmd.matches("w", "write") || cmd.matches("wa", "wall");
}

bool Proxy::wantQuit(const ExCommand &cmd)
{
    return cmd.matches("q", "quit") || cmd.matches("qa", "qall");
}

bool Proxy::wantRun(const ExCommand &cmd)
{
    return cmd.matches("r", "run") || cmd.matches("m", "make");
}

void Proxy::invalidate()
{
    QApplication::quit();
}

QTextDocument *Proxy::document() const
{
    QTextDocument *doc = NULL;
    if (QPlainTextEdit *ed = qobject_cast<QPlainTextEdit *>(m_widget))
        doc = ed->document();
    else if (QTextEdit *ed = qobject_cast<QTextEdit *>(m_widget))
        doc = ed->document();
    return doc;
}

QString Proxy::content() const
{
    return document()->toPlainText();
}
