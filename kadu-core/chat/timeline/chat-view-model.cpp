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
#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>
#include <QtWidgets/QMessageBox>

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
        connect(
            m_timelineController, &ChatTimelineController::readMarkerIdChanged, this,
            &ChatViewModel::timelineStateChangedSlot);
        connect(
            m_timelineController, &ChatTimelineController::newEventsBelowChanged, this,
            &ChatViewModel::timelineStateChangedSlot);
        connect(protocolTimelineService, &ProtocolTimelineService::pinnedMessagesChanged, this,
                [this](const Chat &chat) {
                    if (chat == m_chat)
                        emit pinnedMessagesChanged();
                });
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
                              : QUrl{QStringLiteral("qrc:/Kadu/Chat/chat/qml/styles/KaduClassic/KaduClassicChatStyle.qml")};
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

QString ChatViewModel::readMarkerId() const
{
    return m_timelineController ? m_timelineController->readMarkerId() : QString{};
}

int ChatViewModel::newEventsBelow() const
{
    return m_timelineController ? m_timelineController->newEventsBelow() : 0;
}

bool ChatViewModel::composerActive() const
{
    return m_composerMode != ComposerMode::None && !m_composerTarget.stableId.isEmpty();
}

QVariantMap ChatViewModel::composerContext() const
{
    if (!composerActive())
        return {};

    const auto &sender = m_composerTarget.sender;
    const auto &content = m_composerTarget.content;
    return {
        {QStringLiteral("mode"), m_composerMode == ComposerMode::Reply ? QStringLiteral("reply")
                                                                          : QStringLiteral("edit")},
        {QStringLiteral("target"),
         QVariantMap{{QStringLiteral("id"), m_composerTarget.stableId},
                     {QStringLiteral("protocolEventType"), m_composerTarget.protocolEventType},
                     {QStringLiteral("kind"), static_cast<int>(m_composerTarget.kind)},
                     {QStringLiteral("senderDisplayName"), sender.displayName},
                     {QStringLiteral("senderAvatarSource"), QVariant::fromValue(sender.avatarSource)},
                     {QStringLiteral("senderColor"), QVariant::fromValue(sender.color)},
                     {QStringLiteral("plainText"), content.plainText},
                     {QStringLiteral("formattedText"), content.formattedText},
                     {QStringLiteral("edited"), m_composerTarget.state.edited},
                     {QStringLiteral("redacted"), m_composerTarget.state.redacted}}}};
}

QVariantList ChatViewModel::pinnedMessages() const
{
    if (auto *service = timelineService(nullptr))
        return service->pinnedMessages(m_chat);
    return {};
}

ChatViewModel::ComposerMode ChatViewModel::composerMode() const
{
    return m_composerMode;
}

QString ChatViewModel::composerTargetId() const
{
    return composerActive() ? m_composerTarget.stableId : QString{};
}

QString ChatViewModel::composerTargetPlainText() const
{
    return composerActive() ? m_composerTarget.content.plainText : QString{};
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

void ChatViewModel::clearComposerContext()
{
    if (!composerActive())
        return;

    m_composerMode = ComposerMode::None;
    m_composerTarget = {};
    emit composerContextChanged();
}

void ChatViewModel::openUrl(const QString &url)
{
    if (m_urlHandlerManager && !url.isEmpty())
        m_urlHandlerManager->openUrl(url.toUtf8());
}

void ChatViewModel::copyText(const QString &text)
{
    if (auto *clipboard = QGuiApplication::clipboard(); clipboard && !text.isEmpty())
        clipboard->setText(text);
}

QVariantList ChatViewModel::timelineActions(const QString &stableId) const
{
    if (stableId.isEmpty())
        return {};

    QVariantList actions;
    const auto item = m_timeline->item(stableId);
    if (!item.stableId.isEmpty() && !item.state.redacted && !item.content.plainText.isEmpty())
        actions.append(QVariantMap{{QStringLiteral("id"), 0}, {QStringLiteral("key"), QStringLiteral("copy")},
                                   {QStringLiteral("text"), tr("Copy message")}});

    auto *service = timelineService(nullptr);
    if (!service)
        return actions;

    const auto available = service->availableActions(m_chat, stableId);
    if (available.testFlag(ChatTimelineAction::Reply))
        actions.append(QVariantMap{{QStringLiteral("id"), static_cast<int>(ChatTimelineAction::Reply)},
                                   {QStringLiteral("key"), QStringLiteral("reply")},
                                   {QStringLiteral("text"), tr("Reply")}});
    if (available.testFlag(ChatTimelineAction::Edit))
        actions.append(QVariantMap{{QStringLiteral("id"), static_cast<int>(ChatTimelineAction::Edit)},
                                   {QStringLiteral("key"), QStringLiteral("edit")},
                                   {QStringLiteral("text"), tr("Edit message")}});
    if (available.testFlag(ChatTimelineAction::SaveAttachment))
        actions.append(QVariantMap{{QStringLiteral("id"), static_cast<int>(ChatTimelineAction::SaveAttachment)},
                                   {QStringLiteral("key"), QStringLiteral("saveAttachment")},
                                   {QStringLiteral("text"), tr("Save attachment")}});
    if (available.testFlag(ChatTimelineAction::Delete))
        actions.append(QVariantMap{{QStringLiteral("id"), static_cast<int>(ChatTimelineAction::Delete)},
                                   {QStringLiteral("key"), QStringLiteral("delete")},
                                   {QStringLiteral("text"), tr("Delete message")},
                                   {QStringLiteral("destructive"), true}});
    if (available.testFlag(ChatTimelineAction::ShowSource))
        actions.append(QVariantMap{{QStringLiteral("id"), static_cast<int>(ChatTimelineAction::ShowSource)},
                                   {QStringLiteral("key"), QStringLiteral("showSource")},
                                   {QStringLiteral("text"), tr("Show source")}});
    if (available.testFlag(ChatTimelineAction::Unpin))
        actions.append(QVariantMap{{QStringLiteral("id"), static_cast<int>(ChatTimelineAction::Unpin)},
                                   {QStringLiteral("key"), QStringLiteral("unpin")},
                                   {QStringLiteral("text"), tr("Unpin message")},
                                   {QStringLiteral("destructive"), true}});
    return actions;
}

void ChatViewModel::executeTimelineAction(const QString &stableId, int action)
{
    if (stableId.isEmpty())
        return;

    if (action == 0)
    {
        copyText(m_timeline->item(stableId).content.plainText);
        return;
    }

    const auto timelineAction = static_cast<ChatTimelineAction>(action);
    auto *service = timelineService(nullptr);
    if (!service || !service->availableActions(m_chat, stableId).testFlag(timelineAction))
        return;

    if (timelineAction == ChatTimelineAction::Reply)
    {
        setComposerContext(ComposerMode::Reply, m_timeline->item(stableId));
        return;
    }
    if (timelineAction == ChatTimelineAction::Edit)
    {
        setComposerContext(ComposerMode::Edit, m_timeline->item(stableId));
        return;
    }
    if (timelineAction == ChatTimelineAction::Delete &&
        QMessageBox::question(nullptr, tr("Delete message"), tr("Do you want to delete this message?"),
                              QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
        return;
    if (timelineAction == ChatTimelineAction::Unpin &&
        QMessageBox::question(nullptr, tr("Unpin message"), tr("Do you want to unpin this message?"),
                              QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
        return;

    service->executeAction(m_chat, stableId, timelineAction);
}

void ChatViewModel::setTimelineAtNewest(bool atNewest)
{
    if (m_timelineController)
        m_timelineController->setAtNewest(atNewest);
}

void ChatViewModel::markTimelineItemVisible(const QString &stableId)
{
    if (m_timelineController && !stableId.isEmpty())
        m_timelineController->markVisible(stableId);
}

void ChatViewModel::cancelComposerContext()
{
    if (!composerActive())
        return;

    clearComposerContext();
    emit composerContextCancelled();
}

void ChatViewModel::setComposerContext(ComposerMode mode, const ChatTimelineItem &item)
{
    if (mode == ComposerMode::None || item.stableId.isEmpty() || item.state.redacted)
        return;

    m_composerMode = mode;
    m_composerTarget = item;
    emit composerContextChanged();
    emit composerContextActivated(mode);
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
