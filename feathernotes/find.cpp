/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2016 <tsujan2000@gmail.com>
 *
 * FeatherNotes is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FeatherNotes is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "fn.h"
#include "ui_fn.h"
#include "dommodel.h"

namespace FeatherNotes {

/************************************************************************
 ***** Qt's backward search has some bugs. Therefore, we do our own *****
 ***** backward search by using the following two static functions. *****
 ************************************************************************/
static bool findBackwardInBlock (const QTextBlock &block, const QString &str, int offset,
                                 QTextCursor &cursor, QTextDocument::FindFlags flags)
{
    Qt::CaseSensitivity cs = !(flags & QTextDocument::FindCaseSensitively)
                             ? Qt::CaseInsensitive : Qt::CaseSensitive;

    QString text = block.text();
    text.replace (QChar::Nbsp, QLatin1Char (' '));

    int idx = -1;
    while (offset >= 0 && offset <= text.length())
    {
        idx = text.lastIndexOf (str, offset, cs);
        if (idx == -1)
            return false;
        if (flags & QTextDocument::FindWholeWords)
        {
            const int start = idx;
            const int end = start + str.length();
            if ((start != 0 && text.at (start - 1).isLetterOrNumber())
                || (end != text.length() && text.at (end).isLetterOrNumber()))
            { // if this is not a whole word, continue the backward search
                offset = idx - 1;
                idx = -1;
                continue;
            }
        }
        cursor.setPosition (block.position() + idx);
        cursor.setPosition (cursor.position() + str.length(), QTextCursor::KeepAnchor);
        return true;
    }
    return false;
}

static bool findBackward (const QTextDocument *txtdoc, const QString str,
                          QTextCursor &cursor, QTextDocument::FindFlags flags)
{
    if (!str.isEmpty() && !cursor.isNull())
    {
        int pos = cursor.anchor()
                  - str.size(); // we don't want a match with the cursor inside it
        if (pos >= 0)
        {
            QTextBlock block = txtdoc->findBlock (pos);
            int blockOffset = pos - block.position();
            while (block.isValid())
            {
                if (findBackwardInBlock (block, str, blockOffset, cursor, flags))
                    return true;
                block = block.previous();
                blockOffset = block.length() - 2; // newline is included in length()
            }
        }
    }
    cursor = QTextCursor();
    return false;
}
/*************************/
// This method extends the searchable strings to those with line breaks.
// It also corrects the behavior of Qt's backward search.
QTextCursor FN::finding (const QString str,
                         const QTextCursor& start,
                         QTextDocument::FindFlags flags) const
{
    /* let's be consistent first */
    if (ui->stackedWidget->currentIndex() == -1 || str.isEmpty())
        return QTextCursor(); // null cursor

    TextEdit *textEdit = qobject_cast< TextEdit *>(ui->stackedWidget->currentWidget());
    QTextDocument *txtdoc = textEdit->document();
    QTextCursor res = QTextCursor (start);
    if (str.contains ('\n'))
    {
        QTextCursor cursor = start;
        QTextCursor found;
        QStringList sl = str.split ("\n");
        int i = 0;
        Qt::CaseSensitivity cs = !(flags & QTextDocument::FindCaseSensitively)
                                 ? Qt::CaseInsensitive : Qt::CaseSensitive;
        QString subStr;
        if (!(flags & QTextDocument::FindBackward))
        {
            /* this loop searches for the consecutive
               occurrences of newline separated strings */
            while (i < sl.count())
            {
                if (i == 0) // the first string
                {
                    subStr = sl.at (0);
                    /* when the first string is empty... */
                    if (subStr.isEmpty())
                    {
                        /* ... search anew from the next block */
                        cursor.movePosition (QTextCursor::EndOfBlock);
                        res.setPosition (cursor.position());
                        if (!cursor.movePosition (QTextCursor::NextBlock))
                            return QTextCursor();
                        ++i;
                    }
                    else
                    {
                        if ((found = txtdoc->find (subStr, cursor, flags)).isNull())
                            return QTextCursor();
                        cursor.setPosition (found.position());
                        /* if the match doesn't end the block... */
                        while (!cursor.atBlockEnd())
                        {
                            /* ... move the cursor to right and search until a match is found */
                            cursor.movePosition (QTextCursor::EndOfBlock);
                            cursor.setPosition (cursor.position() - subStr.length());
                            if ((found = txtdoc->find (subStr, cursor, flags)).isNull())
                                return QTextCursor();
                            cursor.setPosition (found.position());
                        }

                        res.setPosition (found.anchor());
                        if (!cursor.movePosition (QTextCursor::NextBlock))
                            return QTextCursor();
                        ++i;
                    }
                }
                else if (i != sl.count() - 1) // middle strings
                {
                    /* when the next block's test isn't the next string... */
                    if (QString::compare (cursor.block().text(), sl.at (i), cs) != 0)
                    {
                        /* ... reset the loop cautiously */
                        cursor.setPosition (res.position());
                        if (!cursor.movePosition (QTextCursor::NextBlock))
                            return QTextCursor();
                        i = 0;
                        continue;
                    }

                    if (!cursor.movePosition (QTextCursor::NextBlock))
                        return QTextCursor();
                    ++i;
                }
                else // the last string (i == sl.count() - 1)
                {
                    subStr = sl.at (i);
                    if (subStr.isEmpty()) break;
                    if (!(flags & QTextDocument::FindWholeWords))
                    {
                        /* when the last string doesn't start the next block... */
                        if (!cursor.block().text().startsWith (subStr, cs))
                        {
                            /* ... reset the loop cautiously */
                            cursor.setPosition (res.position());
                            if (!cursor.movePosition (QTextCursor::NextBlock))
                                return QTextCursor();
                            i = 0;
                            continue;
                        }
                        cursor.setPosition (cursor.anchor() + subStr.count());
                        break;
                    }
                    else
                    {
                        if ((found = txtdoc->find (subStr, cursor, flags)).isNull()
                            || found.anchor() != cursor.position())
                        {
                            cursor.setPosition (res.position());
                            if (!cursor.movePosition (QTextCursor::NextBlock))
                                return QTextCursor();
                            i = 0;
                            continue;
                        }
                        cursor.setPosition (found.position());
                        break;
                    }
                }
            }
            res.setPosition (cursor.position(), QTextCursor::KeepAnchor);
        }
        else // backward search
        {
            cursor.setPosition (cursor.anchor());
            int endPos = cursor.position();
            while (i < sl.count())
            {
                if (i == 0) // the last string
                {
                    subStr = sl.at (sl.count() - 1);
                    if (subStr.isEmpty())
                    {
                        cursor.movePosition (QTextCursor::StartOfBlock);
                        endPos = cursor.position();
                        if (!cursor.movePosition (QTextCursor::PreviousBlock))
                            return QTextCursor();
                        cursor.movePosition (QTextCursor::EndOfBlock);
                        ++i;
                    }
                    else
                    {
                        if (!findBackward (txtdoc, subStr, cursor, flags))
                            return QTextCursor();
                        /* if the match doesn't start the block... */
                        while (cursor.anchor() > cursor.block().position())
                        {
                            /* ... move the cursor to left and search backward until a match is found */
                            cursor.setPosition (cursor.block().position() + subStr.count());
                            if (!findBackward (txtdoc, subStr, cursor, flags))
                                return QTextCursor();
                        }

                        endPos = cursor.position();
                        if (!cursor.movePosition (QTextCursor::PreviousBlock))
                            return QTextCursor();
                        cursor.movePosition (QTextCursor::EndOfBlock);
                        ++i;
                    }
                }
                else if (i != sl.count() - 1) // the middle strings
                {
                    if (QString::compare (cursor.block().text(), sl.at (sl.count() - i - 1), cs) != 0)
                    { // reset the loop if the block text doesn't match
                        cursor.setPosition (endPos);
                        if (!cursor.movePosition (QTextCursor::PreviousBlock))
                            return QTextCursor();
                        cursor.movePosition (QTextCursor::EndOfBlock);
                        i = 0;
                        continue;
                    }

                    if (!cursor.movePosition (QTextCursor::PreviousBlock))
                        return QTextCursor();
                    cursor.movePosition (QTextCursor::EndOfBlock);
                    ++i;
                }
                else // the first string
                {
                    subStr = sl.at (0);
                    if (subStr.isEmpty()) break;
                    if (!(flags & QTextDocument::FindWholeWords))
                    {
                        /* when the first string doesn't end the previous block... */
                        if (!cursor.block().text().endsWith (subStr, cs))
                        {
                            /* ... reset the loop */
                            cursor.setPosition (endPos);
                            if (!cursor.movePosition (QTextCursor::PreviousBlock))
                                return QTextCursor();
                            cursor.movePosition (QTextCursor::EndOfBlock);
                            i = 0;
                            continue;
                        }
                        cursor.setPosition (cursor.anchor() - subStr.count());
                        break;
                    }
                    else
                    {
                        found = cursor; // block end
                        if (!findBackward (txtdoc, subStr, found, flags)
                            || found.position() != cursor.position())
                        {
                            cursor.setPosition (endPos);
                            if (!cursor.movePosition (QTextCursor::PreviousBlock))
                                return QTextCursor();
                            cursor.movePosition (QTextCursor::EndOfBlock);
                            i = 0;
                            continue;
                        }
                        cursor.setPosition (found.anchor());
                        break;
                    }
                }
            }
            res.setPosition (cursor.anchor());
            res.setPosition (endPos, QTextCursor::KeepAnchor);
        }
    }
    else // there's no line break
    {
        if (!(flags & QTextDocument::FindBackward))
            res = txtdoc->find (str, start, flags);
        else
            findBackward (txtdoc, str, res, flags);
    }

    return res;
}
/*************************/
void FN::find()
{
    QWidget *cw = ui->stackedWidget->currentWidget();
    if (!cw) return;

    if (ui->tagsButton->isChecked())
    {
        findInTags();
        return;
    }
    else
        closeTagsDialog();

    if (ui->namesButton->isChecked())
    {
        findInNames();
        return;
    }

    TextEdit *textEdit = qobject_cast< TextEdit *>(cw);
    disconnect (textEdit->verticalScrollBar(), &QAbstractSlider::valueChanged, this, &FN::scrolled);
    disconnect (textEdit->horizontalScrollBar(), &QAbstractSlider::valueChanged, this, &FN::scrolled);
    disconnect (textEdit, &TextEdit::resized, this, &FN::hlight);
    disconnect (textEdit, &QTextEdit::textChanged, this, &FN::hlight);
    QString txt = ui->lineEdit->text();
    searchEntries_[textEdit] = txt;
    if (txt.isEmpty())
    {
        /* remove all yellow and green highlights */
        QList<QTextEdit::ExtraSelection> extraSelections;
        greenSels_[textEdit] = extraSelections; // not needed
        textEdit->setExtraSelections (extraSelections);
        return;
    }

    bool backwardSearch = false;
    QTextCursor start = textEdit->textCursor();
    if (QObject::sender() == ui->prevButton)
    {
        backwardSearch = true;
        if (searchOtherNode_)
            start.movePosition (QTextCursor::End, QTextCursor::MoveAnchor);
    }
    else // Next button or Enter is pressed
    {
        if (searchOtherNode_)
            start.movePosition (QTextCursor::Start, QTextCursor::MoveAnchor);
    }
    searchOtherNode_ = false;

    reallySetSearchFlags (false);
    QTextDocument::FindFlags newFlags = searchFlags_;
    if (backwardSearch)
        newFlags = searchFlags_ | QTextDocument::FindBackward;

    QTextCursor found = finding (txt, start, newFlags);

    QModelIndex nxtIndx;
    if (found.isNull())
    {
        if (!ui->everywhereButton->isChecked())
        {
            if (backwardSearch)
                start.movePosition (QTextCursor::End, QTextCursor::MoveAnchor);
            else
                start.movePosition (QTextCursor::Start, QTextCursor::MoveAnchor);
            found = finding (txt, start, newFlags);
        }
        else
        {
            /* go to the next node... */
            QModelIndex indx = ui->treeView->currentIndex();
            QString text;
            nxtIndx = indx;
            /* ... but skip nodes that don't contain the search string */
            Qt::CaseSensitivity cs = Qt::CaseInsensitive;
            if (ui->caseButton->isChecked()) cs = Qt::CaseSensitive;
            while (!text.contains (txt, cs))
            {
                nxtIndx = model_->adjacentIndex (nxtIndx, !backwardSearch);
                if (!nxtIndx.isValid()) break;
                DomItem *item = static_cast<DomItem*>(nxtIndx.internalPointer());
                QDomNodeList list = item->node().childNodes();
                text = list.item (0).nodeValue();
            }
        }
    }

    if (!found.isNull())
    {
        start.setPosition (found.anchor());
        start.setPosition (found.position(), QTextCursor::KeepAnchor);
        textEdit->setTextCursor (start);
    }
    /* matches highlights should come here,
       after the text area is scrolled */
    hlight();
    connect (textEdit->verticalScrollBar(), &QAbstractSlider::valueChanged, this, &FN::scrolled);
    connect (textEdit->horizontalScrollBar(), &QAbstractSlider::valueChanged, this, &FN::scrolled);
    connect (textEdit, &TextEdit::resized, this, &FN::hlight);
    connect (textEdit, &QTextEdit::textChanged, this, &FN::hlight);

    if (nxtIndx.isValid())
    {
        searchOtherNode_ = true;
        ui->treeView->setCurrentIndex (nxtIndx);
        textEdit = qobject_cast< TextEdit *>(ui->stackedWidget->currentWidget());
        ui->lineEdit->setText (txt);
        searchEntries_[textEdit] = txt;
        find();
    }
}
/*************************/
// Highlight found matches in the visible part of the text.
void FN::hlight() const
{
    QWidget *cw = ui->stackedWidget->currentWidget();
    if (!cw) return;

    TextEdit *textEdit = qobject_cast< TextEdit *>(cw);
    QString txt = searchEntries_[textEdit];
    if (txt.isEmpty()) return;

    QList<QTextEdit::ExtraSelection> extraSelections;
    /* prepend green highlights */
    extraSelections.append (greenSels_[textEdit]);
    QColor yellow = QColor (Qt::yellow);
    QColor black = QColor (Qt::black);
    QTextCursor found;
    /* first put a start cursor at the top left edge... */
    QPoint Point (0, 0);
    QTextCursor start = textEdit->cursorForPosition (Point);
    /* ... them move it backward by the search text length */
    int startPos = start.position() - txt.length();
    if (startPos >= 0)
        start.setPosition (startPos);
    else
        start.setPosition (0);
    int h = textEdit->geometry().height();
    int w = textEdit->geometry().width();
    /* get the visible text to check if
       the search string is inside it */
    Point = QPoint (h, w);
    QTextCursor end = textEdit->cursorForPosition (Point);
    int endPos = end.position() + txt.length();
    end.movePosition (QTextCursor::End);
    if (endPos <= end.position())
        end.setPosition (endPos);
    QTextCursor visCur = start;
    visCur.setPosition (end.position(), QTextCursor::KeepAnchor);
    QString str = visCur.selection().toPlainText(); // '\n' is included in this way
    Qt::CaseSensitivity cs = Qt::CaseInsensitive;
    if (ui->caseButton->isChecked()) cs = Qt::CaseSensitive;
    while (str.contains (txt, cs) // don't waste time if the search text isn't visible
           && !(found = finding (txt, start, searchFlags_)).isNull())
    {
        QTextEdit::ExtraSelection extra;
        extra.format.setBackground (yellow);
        extra.format.setFontUnderline (true);
        extra.format.setUnderlineStyle (QTextCharFormat::WaveUnderline);
        extra.format.setUnderlineColor (black);
        extra.cursor = found;
        extraSelections.append (extra);
        start.setPosition (found.position());
        if (textEdit->cursorRect (start).top() >= h) break;
    }

    textEdit->setExtraSelections (extraSelections);
}
/*************************/
void FN::rehighlight (TextEdit *textEdit)
{
    if (!searchEntries_[textEdit].isEmpty())
        hlight();
}
/*************************/
void FN::reallySetSearchFlags (bool h)
{
    if (ui->wholeButton->isChecked() && ui->caseButton->isChecked())
        searchFlags_ = QTextDocument::FindWholeWords | QTextDocument::FindCaseSensitively;
    else if (ui->wholeButton->isChecked() && !ui->caseButton->isChecked())
        searchFlags_ = QTextDocument::FindWholeWords;
    else if (!ui->wholeButton->isChecked() && ui->caseButton->isChecked())
        searchFlags_ = QTextDocument::FindCaseSensitively;
    else
        searchFlags_ = 0;

    /* deselect text for consistency */
    if (QObject::sender() == ui->caseButton || (QObject::sender() == ui->wholeButton))
    {
        for (int i = 0; i < ui->stackedWidget->count(); ++i)
        {
            TextEdit *textEdit = qobject_cast< TextEdit *>(ui->stackedWidget->widget (i));
            QTextCursor start = textEdit->textCursor();
            start.setPosition (start.anchor());
            textEdit->setTextCursor (start);
        }

    }

    if (h) hlight();
}
/*************************/
void FN::setSearchFlags()
{
    reallySetSearchFlags (true);
}

}
