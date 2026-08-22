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

#include "protocols/protocol.h"

#include <QtCore/QPointer>
#include <injeqt/injeqt.h>

class ChatServiceRepository;
class MatrixChatService;
class PluginInjectedFactory;

namespace Quotient
{
class Connection;
}

class MatrixProtocol final : public Protocol
{
    Q_OBJECT

public:
    explicit MatrixProtocol(Account account, ProtocolFactory *factory);
    virtual ~MatrixProtocol();

    virtual bool contactsListReadOnly() override
    {
        return false;
    }
    virtual QString statusPixmapPath() override
    {
        return "xmpp";
    }

    void joinRoom(const QString &roomIdOrAlias);

private:
    QPointer<ChatServiceRepository> m_chatServiceRepository;
    QPointer<PluginInjectedFactory> m_pluginInjectedFactory;
    QPointer<Quotient::Connection> m_connection;
    MatrixChatService *m_chatService = nullptr;

    void createConnection();
    void handleConnectionError(const QString &message, const QString &details = {});

private slots:
    INJEQT_SET void setChatServiceRepository(ChatServiceRepository *chatServiceRepository);
    INJEQT_SET void setPluginInjectedFactory(PluginInjectedFactory *pluginInjectedFactory);
    INJEQT_INIT void init();

protected:
    virtual void login() override;
    virtual void logout() override;
    virtual void sendStatusToServer() override;
};
