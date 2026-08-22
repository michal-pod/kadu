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

#include "matrix-protocol.h"
#include "matrix-protocol.moc"

#include "chat/chat-service-repository.h"
#include "plugin/plugin-injected-factory.h"

#include "matrix-account-data.h"
#include "matrix-chat-service.h"

#include <Quotient/connection.h>

#include <QtCore/QUrl>

MatrixProtocol::MatrixProtocol(Account account, ProtocolFactory *factory) : Protocol{account, factory}
{
}

MatrixProtocol::~MatrixProtocol()
{
    if (m_chatServiceRepository && m_chatService)
        m_chatServiceRepository->removeChatService(m_chatService);
}

void MatrixProtocol::setChatServiceRepository(ChatServiceRepository *chatServiceRepository)
{
    m_chatServiceRepository = chatServiceRepository;
}

void MatrixProtocol::setPluginInjectedFactory(PluginInjectedFactory *pluginInjectedFactory)
{
    m_pluginInjectedFactory = pluginInjectedFactory;
}

void MatrixProtocol::init()
{
    createConnection();
    m_chatService = m_pluginInjectedFactory->makeInjected<MatrixChatService>(account(), this);
    m_chatService->setConnection(m_connection);
    m_chatServiceRepository->addChatService(m_chatService);
}

void MatrixProtocol::createConnection()
{
    m_connection = new Quotient::Connection{QUrl{MatrixAccountData{account()}.homeserver()}, this};
    // The first messaging step deliberately supports only unencrypted direct chats.
    m_connection->enableDirectChatEncryption(false);
    if (m_chatService)
        m_chatService->setConnection(m_connection);

    connect(m_connection, &Quotient::Connection::connected, this, [this] {
        if (!m_connection)
            return;

        m_connection->syncLoop();
        loggedIn();
    });
    connect(m_connection, &Quotient::Connection::loggedOut, this, [this] {
        if (m_chatService)
            m_chatService->setConnection(nullptr);
        loggedOut();
    });
    connect(
        m_connection, &Quotient::Connection::loginError, this,
        [this](const QString &message, const QString &details) { handleConnectionError(message, details); });
    connect(
        m_connection, &Quotient::Connection::resolveError, this,
        [this](const QString &message) { handleConnectionError(message); });
}

void MatrixProtocol::handleConnectionError(const QString &message, const QString &details)
{
    const auto reason = details.isEmpty() ? message : QStringLiteral("%1: %2").arg(message, details);
    emit connectionError(account(), MatrixAccountData{account()}.homeserver(), reason);
    connectionError();
}

void MatrixProtocol::login()
{
    if (!m_connection)
        createConnection();

    m_connection->loginWithPassword(account().id(), account().password(), QStringLiteral("Kadu"));
}

void MatrixProtocol::logout()
{
    if (m_connection && m_connection->isLoggedIn())
    {
        m_connection->logout();
        return;
    }

    if (m_connection)
    {
        disconnect(m_connection, nullptr, this, nullptr);
        if (m_chatService)
            m_chatService->setConnection(nullptr);
        m_connection->deleteLater();
        m_connection = nullptr;
    }
    loggedOut();
}

void MatrixProtocol::sendStatusToServer()
{
    // Presence export belongs to the future libQuotient-backed connection.
}
