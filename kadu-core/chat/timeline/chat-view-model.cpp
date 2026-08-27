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

#include "chat-view-model.h"
#include "chat-view-model.moc"

#include "chat/timeline/chat-timeline-controller.h"
#include "chat-style/chat-style-manager.h"
#include "message/message.h"
#include "message/sorted-messages.h"
#include "protocols/protocol.h"
#include "protocols/services/protocol-timeline-service.h"

ChatViewModel::ChatViewModel(Chat chat, ProtocolTimelineService *service, ChatStyleManager *chatStyleManager,
                             QObject *parent)
        : QObject{parent}, m_chat{chat}, m_chatStyleManager{chatStyleManager}
{
    if (auto *protocolTimelineService = timelineService(service))
    {
        m_timelineController = new ChatTimelineController{m_chat, protocolTimelineService, this};
        m_timeline = m_timelineController->timeline();
        connect(m_timelineController, &ChatTimelineController::loadingInitialChanged, this,
                &ChatViewModel::timelineStateChangedSlot);
        connect(m_timelineController, &ChatTimelineController::loadingOlderChanged, this,
                &ChatViewModel::timelineStateChangedSlot);
        connect(m_timelineController, &ChatTimelineController::hasOlderChanged, this,
                &ChatViewModel::timelineStateChangedSlot);
    }
    else
        m_timeline = new ChatTimelineModel{this};

    if (m_chat)
        connect(m_chat, SIGNAL(updated()), this, SLOT(chatUpdated()));
    if (m_chatStyleManager)
        connect(m_chatStyleManager, &ChatStyleManager::chatStyleConfigurationUpdated, this, &ChatViewModel::styleChanged);
}

ChatViewModel::~ChatViewModel()
{
    close();
}

Chat ChatViewModel::chat() const
{
    return m_chat;
}

ChatTimelineModel *ChatViewModel::timeline() const
{
    return m_timeline;
}

QString ChatViewModel::title() const
{
    return ::title(m_chat);
}

bool ChatViewModel::usesProtocolTimeline() const
{
    return m_timelineController != nullptr;
}

QString ChatViewModel::theme() const
{
    return m_chatStyleManager ? m_chatStyleManager->currentChatStyle().name() : QStringLiteral("KaduClassic");
}

bool ChatViewModel::loadingInitial() const
{
    return m_timelineController && m_timelineController->isLoadingInitial();
}

bool ChatViewModel::loadingOlder() const
{
    return m_timelineController && m_timelineController->isLoadingOlder();
}

bool ChatViewModel::hasOlder() const
{
    return m_timelineController && m_timelineController->hasOlder();
}

void ChatViewModel::addLegacyMessage(const Message &message)
{
    if (m_timelineController || message.messageChat() != m_chat)
        return;

    m_timeline->upsert(m_legacyAdapter.item(message));
}

void ChatViewModel::addLegacyMessages(const SortedMessages &messages)
{
    if (m_timelineController)
        return;

    for (const auto &message : messages.messages())
        addLegacyMessage(message);
}

void ChatViewModel::open()
{
    if (m_open)
        return;

    m_open = true;
    if (!m_timelineController)
        return;

    m_timelineController->setActive(true);
    m_timelineController->setAtNewest(true);
    m_timelineController->loadInitial();
}

void ChatViewModel::close()
{
    if (!m_open)
        return;

    m_open = false;
    if (!m_timelineController)
        return;

    m_timelineController->setActive(false);
    m_timelineController->cancelRequests();
}

void ChatViewModel::loadOlder()
{
    if (m_timelineController)
        m_timelineController->loadOlder();
}

ProtocolTimelineService *ChatViewModel::timelineService(ProtocolTimelineService *service) const
{
    if (service)
        return service;

    const auto account = m_chat.chatAccount();
    auto *protocol = account ? account.protocolHandler() : nullptr;
    return protocol ? protocol->timelineService() : nullptr;
}

void ChatViewModel::chatUpdated()
{
    emit titleChanged();
}

void ChatViewModel::styleChanged()
{
    emit themeChanged();
}

void ChatViewModel::timelineStateChangedSlot()
{
    emit timelineStateChanged();
}
