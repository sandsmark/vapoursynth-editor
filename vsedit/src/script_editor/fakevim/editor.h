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

#pragma once

#include <QObject>
#include <QTextEdit> // just for ExtraSelection...

class QMainWindow;
class QTextDocument;
class QString;
class QWidget;

namespace FakeVim {
namespace Internal {
class FakeVimHandler;
struct ExCommand;
} // namespace Internal
} // namespace FakeVim

class Proxy : public QObject
{
    Q_OBJECT

public:
    Proxy(QWidget *widget, QMainWindow *mw, QObject *parent = 0);

    void openFile(const QString &fileName);

signals:
    void handleInput(const QString &keys);
    void requestSave();
    void requestSaveAndQuit();

public slots:
    void changeStatusData(const QString &info);

    void highlightMatches(const QString &pattern);

    void changeStatusMessage(const QString &contents, int cursorPos);

    void changeExtraInformation(const QString &info);

    void updateStatusBar();

    void handleExCommand(bool *handled, const FakeVim::Internal::ExCommand &cmd);

    void requestSetBlockSelection(const QTextCursor &tc);

    void requestDisableBlockSelection();

    void updateBlockSelection();

    void requestHasBlockSelection(bool *on);

    void indentRegion(int beginBlock, int endBlock, QChar typedChar);

    void checkForElectricCharacter(bool *result, QChar c);

private:
    static int firstNonSpace(const QString &text);

    void updateExtraSelections();

    bool wantSaveAndQuit(const FakeVim::Internal::ExCommand &cmd);

    bool wantSave(const FakeVim::Internal::ExCommand &cmd);

    bool wantQuit(const FakeVim::Internal::ExCommand &cmd);

    bool save();

    void cancel();

    void invalidate();

    bool hasChanges();

    QTextDocument *document() const;

    QString content() const;

    QWidget *m_widget;
    QMainWindow *m_mainWindow;
    QString m_statusMessage;
    QString m_statusData;
    QString m_fileName;

    QList<QTextEdit::ExtraSelection> m_searchSelection;
    QList<QTextEdit::ExtraSelection> m_clearSelection;
    QList<QTextEdit::ExtraSelection> m_blockSelection;
};

QWidget *createEditorWidget(bool usePlainTextEdit);
void initHandler(FakeVim::Internal::FakeVimHandler *handler);
void initMainWindow(QMainWindow *mainWindow, QWidget *centralWidget, const QString &title);
void clearUndoRedo(QWidget *editor);
Proxy *connectSignals(
        FakeVim::Internal::FakeVimHandler *handler, QMainWindow *mainWindow,
        QWidget *editor, const QString &fileToEdit);
