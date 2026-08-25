/*
 * %kadu copyright begin%
 * Copyright 2026 Kadu Qt6 port
 * %kadu copyright end%
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "chat/chat.h"
#include "misc/memory.h"

#include <QtCore/QPointer>
#include <QtWidgets/QDialog>
#include <injeqt/injeqt.h>

class QLabel;
class QLineEdit;
class ProtocolHistoryPageLoader;
class ProtocolHistoryService;
class QPushButton;
class WebkitMessagesView;
class WebkitMessagesViewFactory;

class RemoteHistorySearchDialog final : public QDialog
{
    Q_OBJECT

public:
    Q_INVOKABLE explicit RemoteHistorySearchDialog(ProtocolHistoryService *historyService, Chat chat,
                                                   QWidget *parent = nullptr);
    virtual ~RemoteHistorySearchDialog();

private:
    QPointer<ProtocolHistoryService> m_historyService;
    QPointer<WebkitMessagesViewFactory> m_webkitMessagesViewFactory;
    QPointer<ProtocolHistoryPageLoader> m_pageLoader;
    Chat m_chat;
    QLineEdit *m_query = nullptr;
    QLabel *m_error = nullptr;
    QPushButton *m_search = nullptr;
    owned_qptr<WebkitMessagesView> m_messagesView;

private slots:
    INJEQT_SET void setWebkitMessagesViewFactory(WebkitMessagesViewFactory *webkitMessagesViewFactory);
    INJEQT_INIT void init();

    void search();
    void updateSearchEnabled(const QString &text);
    void showError(const QString &error);
};
