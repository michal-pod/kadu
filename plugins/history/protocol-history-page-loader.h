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

#include "protocols/services/protocol-history-request.h"

#include <QtCore/QFuture>
#include <QtCore/QObject>
#include <QtCore/QPointer>

class ProtocolHistoryService;
class WebkitMessagesView;

class ProtocolHistoryPageLoader : public QObject
{
    Q_OBJECT

public:
    ProtocolHistoryPageLoader(ProtocolHistoryService *historyService, ProtocolHistoryRequest request,
                              WebkitMessagesView *messagesView, QObject *parent = nullptr);

private:
    QPointer<ProtocolHistoryService> m_historyService;
    QPointer<WebkitMessagesView> m_messagesView;
    ProtocolHistoryRequest m_request;
    QFuture<ProtocolHistoryPage> m_page;
    bool m_loading = false;
    bool m_hasMore = true;

    void loadPage();
    void loadPreviousPageIfAtTop();
    void restoreScrollPosition(const QString &messageId) const;

private slots:
    void loadPreviousPage();
    void pageAvailable();
};
