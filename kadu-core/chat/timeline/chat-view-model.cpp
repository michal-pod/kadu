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

#include "chat-style/chat-style-manager.h"
#include "chat/chat-details-room.h"
#include "chat/timeline/chat-timeline-controller.h"
#include "gui/configuration/chat-configuration-holder.h"
#include "message/message.h"
#include "message/sorted-messages.h"
#include "protocols/protocol.h"
#include "protocols/services/protocol-timeline-service.h"
#include "url-handlers/url-handler-manager.h"

#include <QtCore/QBuffer>
#include <QtCore/QByteArray>

ChatViewModel::ChatViewModel(
    Chat chat, ProtocolTimelineService *service, ChatStyleManager *chatStyleManager,
    ChatConfigurationHolder *chatConfigurationHolder, QObject *parent)
        : QObject{parent}, m_chat{chat}, m_chatStyleManager{chatStyleManager},
          m_chatConfigurationHolder{chatConfigurationHolder}
{
    if (auto *protocolTimelineService = timelineService(service))
    {
        m_timelineController = new ChatTimelineController{m_chat, protocolTimelineService, this};
        m_timeline = m_timelineController->timeline();
        connect(
            m_timelineController, &ChatTimelineController::loadingInitialChanged, this,
            &ChatViewModel::timelineStateChangedSlot);
        connect(
            m_timelineController, &ChatTimelineController::loadingOlderChanged, this,
            &ChatViewModel::timelineStateChangedSlot);
        connect(
            m_timelineController, &ChatTimelineController::hasOlderChanged, this,
            &ChatViewModel::timelineStateChangedSlot);
    }
    else
        m_timeline = new ChatTimelineModel{this};

    if (m_chat)
    {
        connect(m_chat, SIGNAL(updated()), this, SLOT(chatUpdated()));
        if (auto *details = qobject_cast<ChatDetailsRoom *>(m_chat.details()))
            connect(details, &ChatDetails::updated, this, &ChatViewModel::refreshRoomDetails);
    }
    if (m_chatStyleManager)
        connect(
            m_chatStyleManager, &ChatStyleManager::chatStyleConfigurationUpdated, this, &ChatViewModel::styleChanged);
    if (m_chatConfigurationHolder)
        connect(
            m_chatConfigurationHolder, &ChatConfigurationHolder::chatConfigurationUpdated, this,
            &ChatViewModel::customColorsChangedSlot);

    refreshRoomDetails();
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

QUrl ChatViewModel::themeSource() const
{
    return m_chatStyleManager ? m_chatStyleManager->styleSource(m_chatStyleManager->currentChatStyle().name())
                              : QUrl{QStringLiteral("qrc:/Kadu/Chat/chat/qml/KaduClassicTimelineStyle.qml")};
}

QString ChatViewModel::themeColorScheme() const
{
    return m_chatStyleManager ? m_chatStyleManager->currentChatStyle().variant() : QStringLiteral("System");
}

QVariantMap ChatViewModel::customColors() const
{
    if (!m_chatConfigurationHolder)
        return {{QStringLiteral("enabled"), false}};

    return {
        {QStringLiteral("enabled"), m_chatConfigurationHolder->customColors()},
        {QStringLiteral("myBackground"), m_chatConfigurationHolder->myBackgroundColor()},
        {QStringLiteral("myText"), m_chatConfigurationHolder->myFontColor()},
        {QStringLiteral("myNick"), m_chatConfigurationHolder->myNickColor()},
        {QStringLiteral("buddyBackground"), m_chatConfigurationHolder->usrBackgroundColor()},
        {QStringLiteral("buddyText"), m_chatConfigurationHolder->usrFontColor()},
        {QStringLiteral("buddyNick"), m_chatConfigurationHolder->usrNickColor()},
        {QStringLiteral("backgroundEnabled"), m_chatConfigurationHolder->chatBgFilled()},
        {QStringLiteral("background"), m_chatConfigurationHolder->chatBgColor().name()}};
}

bool ChatViewModel::roomInfoVisible() const
{
    return m_roomInfoVisible;
}

QString ChatViewModel::roomAvatarSource() const
{
    return m_roomAvatarSource;
}

QString ChatViewModel::roomName() const
{
    return m_roomName;
}

QString ChatViewModel::roomDescription() const
{
    return m_roomDescription;
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

void ChatViewModel::setUrlHandlerManager(UrlHandlerManager *urlHandlerManager)
{
    m_urlHandlerManager = urlHandlerManager;
}

void ChatViewModel::openUrl(const QString &url)
{
    if (m_urlHandlerManager && !url.isEmpty())
        m_urlHandlerManager->openUrl(url.toUtf8());
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
    refreshRoomDetails();
}

void ChatViewModel::styleChanged()
{
    emit themeSourceChanged();
}

void ChatViewModel::customColorsChangedSlot()
{
    emit customColorsChanged();
}

void ChatViewModel::timelineStateChangedSlot()
{
    emit timelineStateChanged();
}

void ChatViewModel::refreshRoomDetails()
{
    const auto *details = m_chat ? qobject_cast<ChatDetailsRoom *>(m_chat.details()) : nullptr;
    if (!details)
    {
        if (!m_roomInfoVisible)
            return;

        m_roomInfoVisible = false;
        m_roomAvatarSource.clear();
        m_roomName.clear();
        m_roomDescription.clear();
        emit roomDetailsChanged();
        return;
    }

    auto roomName = m_chat.display();
    if (roomName.isEmpty())
        roomName = m_chat.name();
    if (roomName.isEmpty())
        roomName = details->name();

    QString avatarSource;
    const auto avatar = details->avatar();
    if (!avatar.isNull())
    {
        QByteArray imageData;
        QBuffer buffer{&imageData};
        buffer.open(QIODevice::WriteOnly);
        if (avatar.save(&buffer, "PNG"))
            avatarSource = QStringLiteral("data:image/png;base64,") + QString::fromLatin1(imageData.toBase64());
    }

    const auto description = details->description();
    if (m_roomInfoVisible && m_roomAvatarSource == avatarSource && m_roomName == roomName &&
        m_roomDescription == description)
        return;

    m_roomInfoVisible = true;
    m_roomAvatarSource = avatarSource;
    m_roomName = roomName;
    m_roomDescription = description;
    emit roomDetailsChanged();
}
