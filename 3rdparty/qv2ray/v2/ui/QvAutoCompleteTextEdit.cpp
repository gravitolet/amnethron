/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "QvAutoCompleteTextEdit.hpp"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QApplication>
#include <QCompleter>
#include <QKeyEvent>
#include <QListView>
#include <QModelIndex>
#include <QScrollBar>
#include <QStyle>
#include <QStringListModel>
#include <QToolTip>
#include <QtDebug>

namespace Qv2ray::ui::widgets {
    AutoCompleteTextEdit::AutoCompleteTextEdit(const QString &prefix, const QStringList &sourceStrings, QWidget *parent) : QPlainTextEdit(parent) {
        this->prefix = prefix;
        this->setLineWrapMode(QPlainTextEdit::NoWrap);
        c = new QCompleter(this);
        c->setModel(new QStringListModel(sourceStrings, c));
        // The rule-set catalog has thousands of one-line entries; without uniform sizes
        // the popup measures every row each time it is shown.
        if (auto *view = qobject_cast<QListView *>(c->popup()))
            view->setUniformItemSizes(true);
        c->setWidget(this);
        c->setCompletionMode(QCompleter::PopupCompletion);
        c->setCaseSensitivity(Qt::CaseInsensitive);
        c->setFilterMode(Qt::MatchContains);
        QObject::connect(c, static_cast<void (QCompleter::*)(const QString &)>(&QCompleter::activated), this, &AutoCompleteTextEdit::insertCompletion);
    }

    AutoCompleteTextEdit::~AutoCompleteTextEdit() {
    }

    void AutoCompleteTextEdit::insertCompletion(const QString &completion) {
        QTextCursor tc = textCursor();
        tc.select(QTextCursor::LineUnderCursor);
        tc.removeSelectedText();
        tc.insertText(completion);
        setTextCursor(tc);
    }

    QString AutoCompleteTextEdit::lineUnderCursor() const {
        QTextCursor tc = textCursor();
        tc.select(QTextCursor::LineUnderCursor);
        return tc.selectedText();
    }

    QString AutoCompleteTextEdit::wordUnderCursor() const {
        QTextCursor tc = textCursor();
        tc.select(QTextCursor::WordUnderCursor);
        return tc.selectedText();
    }

    void AutoCompleteTextEdit::updateCompletionPrefix(const QString &text) {
        if (text == c->completionPrefix())
            return;
        c->setCompletionPrefix(text);
        c->popup()->setCurrentIndex(c->completionModel()->index(0, 0));
    }

    int AutoCompleteTextEdit::completionPopupWidth() {
        // Measured once: QAbstractItemView::sizeHintForColumn walks every row on each call.
        if (popupWidth < 0) {
            const auto *popup = c->popup();
            const auto metrics = popup->fontMetrics();
            int widest = 0;
            for (const auto &item : static_cast<QStringListModel *>(c->model())->stringList())
                widest = qMax(widest, metrics.horizontalAdvance(item));
            const int itemMargin = 2 * (popup->style()->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, popup) + 1);
            popupWidth = widest + itemMargin + 2 * popup->frameWidth();
        }
        return popupWidth + c->popup()->verticalScrollBar()->sizeHint().width();
    }

    void AutoCompleteTextEdit::showCompletionPopup() {
        QRect cr = cursorRect();
        cr.setWidth(completionPopupWidth());
        c->complete(cr); // popup it up!
    }

    void AutoCompleteTextEdit::focusInEvent(QFocusEvent *e) {
        if (c)
            c->setWidget(this);

        // Focus comes back here when the popup closes; reopening it would swallow the next click.
        if (e->reason() != Qt::PopupFocusReason) {
            updateCompletionPrefix(lineUnderCursor());
            showCompletionPopup();
        }

        QPlainTextEdit::focusInEvent(e);
    }

    void AutoCompleteTextEdit::keyPressEvent(QKeyEvent *e) {
        const bool hasCtrlOrShiftModifier = e->modifiers().testFlag(Qt::ControlModifier) || e->modifiers().testFlag(Qt::ShiftModifier);
        const bool hasOtherModifiers = (e->modifiers() != Qt::NoModifier) && !hasCtrlOrShiftModifier; // has other modifiers
        //
        const bool isSpace = (e->modifiers().testFlag(Qt::ShiftModifier) || e->modifiers().testFlag(Qt::NoModifier)) //
                             && e->key() == Qt::Key_Space;
        const bool isTab = (e->modifiers().testFlag(Qt::NoModifier) && e->key() == Qt::Key_Tab);
        const bool isOtherSpace = e->text() == "　";
        //
        if (isSpace && !lineUnderCursor().startsWith("processName:") && !lineUnderCursor().startsWith("processPath:") || isTab || isOtherSpace) {
            QToolTip::showText(this->mapToGlobal(QPoint(0, 0)), tr("You can not input space characters here."), this, QRect{}, 2000);
            return;
        }
        //
        if (c && c->popup()->isVisible()) {
            // The following keys are forwarded by the completer to the widget
            switch (e->key()) {
                case Qt::Key_Enter:
                case Qt::Key_Return:
                case Qt::Key_Escape:
                case Qt::Key_Tab:
                case Qt::Key_Backtab:
                    e->ignore();
                    return; // let the completer do default behavior

                default:
                    break;
            }
        }

        QPlainTextEdit::keyPressEvent(e);

        if (!c)
            return;

        if (lineUnderCursor().isEmpty()) {
            updateCompletionPrefix(QString());
            showCompletionPopup();
            return;
        }

        if (hasCtrlOrShiftModifier && e->text().isEmpty())
            return;

        // if we have other modifiers, or the text is empty, or the line does not start with our prefix.
        if (hasOtherModifiers || e->text().isEmpty() || !lineUnderCursor().startsWith(prefix)) {
            c->popup()->hide();
            return;
        }

        updateCompletionPrefix(lineUnderCursor());

        showCompletionPopup();
    }
} // namespace Qv2ray::ui::widgets
