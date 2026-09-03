/*
 * %kadu copyright begin%
 * Copyright 2013 Rafał Przemysław Malinowski (rafal.przemyslaw.malinowski@gmail.com)
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

#include "accounts/account.h"
#include "core/injected-factory.h"
#include "protocols/protocol.h"
#include "windows/chat-data-window.h"

#include "chat-data-window-repository.h"
#include "chat-data-window-repository.moc"

ChatDataWindowRepository::ChatDataWindowRepository(QObject *parent) : QObject(parent)
{
}

ChatDataWindowRepository::~ChatDataWindowRepository()
{
}

void ChatDataWindowRepository::setInjectedFactory(InjectedFactory *injectedFactory)
{
    m_injectedFactory = injectedFactory;
}

QWidget *ChatDataWindowRepository::windowForChat(const Chat &chat)
{
    if (Windows.contains(chat))
        return Windows.value(chat);

    QWidget *result = nullptr;
    const auto account = chat.chatAccount();
    if (account && account.protocolHandler())
        result = account.protocolHandler()->createChatSettingsWindow(chat, nullptr);
    if (!result)
        result = m_injectedFactory->makeInjected<ChatDataWindow>(chat);

    result->setAttribute(Qt::WA_DeleteOnClose);
    connect(result, &QObject::destroyed, this, [this, chat] { Windows.remove(chat); });
    Windows.insert(chat, result);

    return result;
}

const QMap<Chat, QWidget *> &ChatDataWindowRepository::windows() const
{
    return Windows;
}

void ChatDataWindowRepository::showChatWindow(const Chat &chat)
{
    QWidget *window = windowForChat(chat);
    if (window)
    {
        window->show();
        window->raise();
    }
}
