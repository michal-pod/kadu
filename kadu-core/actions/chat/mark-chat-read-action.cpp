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

#include "mark-chat-read-action.h"
#include "mark-chat-read-action.moc"

#include "actions/action-context.h"
#include "actions/action.h"
#include "chat/chat-manager.h"
#include "chat/chat-service-repository.h"
#include "icons/kadu-icon.h"
#include "message/sorted-messages.h"
#include "message/unread-message-repository.h"
#include "protocols/services/chat-service.h"

MarkChatReadAction::MarkChatReadAction(QObject *parent) : ActionDescription{parent}
{
    setType(ActionDescription::TypeChat);
    setName(QStringLiteral("markChatReadAction"));
    setIcon(KaduIcon{QStringLiteral("mail-mark-read")});
    setText(tr("Mark as read"));
}

void MarkChatReadAction::setChatManager(ChatManager *chatManager)
{
    m_chatManager = chatManager;
    connect(m_chatManager, &ChatManager::unreadMessagesCountChanged, this,
            [this](quint64) { updateActionStates(); });
}

void MarkChatReadAction::setChatServiceRepository(ChatServiceRepository *chatServiceRepository)
{
    m_chatServiceRepository = chatServiceRepository;
}

void MarkChatReadAction::setUnreadMessageRepository(UnreadMessageRepository *unreadMessageRepository)
{
    m_unreadMessageRepository = unreadMessageRepository;
}

void MarkChatReadAction::triggered(QWidget *, ActionContext *context, bool toggled)
{
    Q_UNUSED(toggled)

    const auto chat = context->chat();
    auto *chatService = chat && m_chatServiceRepository
                            ? m_chatServiceRepository->chatService(chat.chatAccount())
                            : nullptr;
    if (!chatService || !chatService->markChatRead(chat))
        return;

    if (m_unreadMessageRepository)
        m_unreadMessageRepository->markMessagesAsRead(m_unreadMessageRepository->unreadMessagesForChat(chat));
}

void MarkChatReadAction::updateActionState(Action *action)
{
    const auto chat = action->context()->chat();
    const auto *chatService = chat && m_chatServiceRepository
                                  ? m_chatServiceRepository->chatService(chat.chatAccount())
                                  : nullptr;
    action->setEnabled(chatService && chat.unreadMessagesCount() > 0);
}
