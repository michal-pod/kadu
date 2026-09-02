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

#include "accounts/account.h"
#include "chat-style/chat-style-manager.h"
#include "chat/chat-details-room.h"
#include "chat/timeline/chat-timeline-controller.h"
#include "configuration/configuration.h"
#include "configuration/deprecated-configuration-api.h"
#include "gui/configuration/chat-configuration-holder.h"
#include "identities/identity.h"
#include "message/message.h"
#include "message/sorted-messages.h"
#include "protocols/protocol.h"
#include "protocols/services/protocol-timeline-service.h"
#include "url-handlers/url-handler-manager.h"

#include <QtCore/QBuffer>
#include <QtCore/QByteArray>
#include <QtCore/QDebug>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonParseError>
#include <QtCore/QStringList>
#include <QtGui/QClipboard>
#include <QtGui/QFont>
#include <QtGui/QGuiApplication>
#include <QtWidgets/QMessageBox>

ChatViewModel::ChatViewModel(
    Chat chat, ProtocolTimelineService *service, ChatStyleManager *chatStyleManager,
    ChatConfigurationHolder *chatConfigurationHolder, QObject *parent, Configuration *configuration)
        : QObject{parent}, m_chat{chat}, m_chatStyleManager{chatStyleManager},
          m_chatConfigurationHolder{chatConfigurationHolder}, m_configuration{configuration}
{
    m_reactionEmojiModel = new EmojiModel{this};

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
            m_timelineController, &ChatTimelineController::loadingNewerChanged, this,
            &ChatViewModel::timelineStateChangedSlot);
        connect(
            m_timelineController, &ChatTimelineController::hasOlderChanged, this,
            &ChatViewModel::timelineStateChangedSlot);
        connect(
            m_timelineController, &ChatTimelineController::hasNewerChanged, this,
            &ChatViewModel::timelineStateChangedSlot);
        connect(
            m_timelineController, &ChatTimelineController::readMarkerIdChanged, this,
            &ChatViewModel::timelineStateChangedSlot);
        connect(
            m_timelineController, &ChatTimelineController::newEventsBelowChanged, this,
            &ChatViewModel::timelineStateChangedSlot);
        connect(m_timelineController, &ChatTimelineController::timelinePositionRequested, this,
                &ChatViewModel::timelinePositionRequested);
        connect(protocolTimelineService, &ProtocolTimelineService::pinnedMessagesChanged, this,
                [this](const Chat &chat) {
                    if (chat == m_chat)
                    {
                        emit pinnedMessagesChanged();
                        emit chatHeaderActionsChanged();
                    }
                });
        connect(protocolTimelineService, &ProtocolTimelineService::availableActionsChanged, this,
                [this](const Chat &chat) {
                    if (chat != m_chat)
                        return;

                    ++m_timelineActionsRevision;
                    emit timelineActionsChanged();
                });
        connect(protocolTimelineService, &ProtocolTimelineService::chatHeaderChanged, this,
                [this](const Chat &chat) {
                    if (chat == m_chat)
                        refreshChatHeader();
                });
        connect(protocolTimelineService, &ProtocolTimelineService::roomInfoChanged, this,
                [this](const Chat &chat) {
                    if (chat == m_chat)
                        emit roomInfoChanged();
                });
    }
    else
        m_timeline = new ChatTimelineModel{this};

    if (m_chat)
    {
        connect(m_chat, SIGNAL(updated()), this, SLOT(chatUpdated()));
        if (auto *details = qobject_cast<ChatDetailsRoom *>(m_chat.details()))
            connect(details, &ChatDetails::updated, this, &ChatViewModel::refreshChatHeader);
    }
    if (m_chatStyleManager)
        connect(
            m_chatStyleManager, &ChatStyleManager::chatStyleConfigurationUpdated, this, &ChatViewModel::styleChanged);
    if (m_chatConfigurationHolder)
        connect(
            m_chatConfigurationHolder, &ChatConfigurationHolder::chatConfigurationUpdated, this,
            &ChatViewModel::customColorsChangedSlot);

    refreshChatHeader();
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

QString ChatViewModel::ownDisplayName() const
{
    const auto account = m_chat.chatAccount();
    const auto identity = account ? account.accountIdentity() : Identity{};
    return identity ? identity.name() : QString{};
}

QVariantMap ChatViewModel::roomInfo() const
{
    if (auto *service = timelineService(nullptr))
        return service->roomInfo(m_chat);
    return {};
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

bool ChatViewModel::chatHeaderVisible() const
{
    return m_chatHeaderVisible;
}

QString ChatViewModel::chatHeaderAvatarSource() const
{
    return m_chatHeaderAvatarSource;
}

QString ChatViewModel::chatHeaderTitle() const
{
    return m_chatHeaderTitle;
}

QString ChatViewModel::chatHeaderDescription() const
{
    return m_chatHeaderDescription;
}

bool ChatViewModel::loadingInitial() const
{
    return m_timelineController && m_timelineController->isLoadingInitial();
}

bool ChatViewModel::loadingOlder() const
{
    return m_timelineController && m_timelineController->isLoadingOlder();
}

bool ChatViewModel::loadingNewer() const
{
    return m_timelineController && m_timelineController->isLoadingNewer();
}

bool ChatViewModel::hasOlder() const
{
    return m_timelineController && m_timelineController->hasOlder();
}

bool ChatViewModel::hasNewer() const
{
    return m_timelineController && m_timelineController->hasNewer();
}

QVariantMap ChatViewModel::chatFont() const
{
    const auto font = m_chatConfigurationHolder ? m_chatConfigurationHolder->chatFont() : QFont{};
    const auto pointSize = font.pointSizeF() > 0.0 ? font.pointSizeF() : 10.0;

    return {{QStringLiteral("family"), font.family()},
            {QStringLiteral("pointSize"), pointSize},
            {QStringLiteral("bold"), font.bold()},
            {QStringLiteral("italic"), font.italic()},
            {QStringLiteral("underline"), font.underline()},
            {QStringLiteral("forced"), m_chatConfigurationHolder && m_chatConfigurationHolder->forceCustomChatFont()}};
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

EmojiModel *ChatViewModel::reactionEmojiModel() const
{
    return m_reactionEmojiModel;
}

QStringList ChatViewModel::recentReactionEmojis() const
{
    if (!m_configuration)
        return {};

    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(
        m_configuration->deprecatedApi()->readEntry(QStringLiteral("ChatTimeline"),
                                                     QStringLiteral("RecentReactionEmojis"))
            .toUtf8(),
        &error);
    if (error.error != QJsonParseError::NoError || !document.isArray())
        return {};

    QStringList result;
    for (const auto &value : document.array())
    {
        const auto emoji = value.toString();
        if (!emoji.isEmpty() && !result.contains(emoji))
            result.append(emoji);
        if (result.size() == 20)
            break;
    }
    return result;
}

QVariantList ChatViewModel::chatHeaderActions() const
{
    if (pinnedMessages().isEmpty())
        return {};

    return {QVariantMap{{QStringLiteral("id"), QStringLiteral("showPinnedMessages")},
                        {QStringLiteral("text"), tr("Pinned messages")},
                        {QStringLiteral("iconName"), QStringLiteral("list-add")}}};
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
                                   {QStringLiteral("text"), tr("Copy message")},
                                   {QStringLiteral("iconName"), QStringLiteral("edit-copy")}});

    auto *service = timelineService(nullptr);
    if (!service)
        return actions;

    const auto available = service->availableActions(m_chat, stableId);
    if (!item.state.redacted && available.testFlag(ChatTimelineAction::React))
        actions.append(QVariantMap{{QStringLiteral("id"), -1}, {QStringLiteral("key"), QStringLiteral("react")},
                                   {QStringLiteral("text"), tr("Add reaction")},
                                   {QStringLiteral("iconName"), QStringLiteral("face-smile")}});
    if (available.testFlag(ChatTimelineAction::Reply))
        actions.append(QVariantMap{{QStringLiteral("id"), static_cast<int>(ChatTimelineAction::Reply)},
                                   {QStringLiteral("key"), QStringLiteral("reply")},
                                   {QStringLiteral("text"), tr("Reply")},
                                   {QStringLiteral("iconName"), QStringLiteral("go-previous")}});
    if (available.testFlag(ChatTimelineAction::Edit))
        actions.append(QVariantMap{{QStringLiteral("id"), static_cast<int>(ChatTimelineAction::Edit)},
                                   {QStringLiteral("key"), QStringLiteral("edit")},
                                   {QStringLiteral("text"), tr("Edit message")},
                                   {QStringLiteral("iconName"), QStringLiteral("document-open")}});
    if (available.testFlag(ChatTimelineAction::SaveAttachment))
        actions.append(QVariantMap{{QStringLiteral("id"), static_cast<int>(ChatTimelineAction::SaveAttachment)},
                                   {QStringLiteral("key"), QStringLiteral("saveAttachment")},
                                   {QStringLiteral("text"), tr("Save attachment")},
                                   {QStringLiteral("iconName"), QStringLiteral("document-open")}});
    if (available.testFlag(ChatTimelineAction::Delete))
        actions.append(QVariantMap{{QStringLiteral("id"), static_cast<int>(ChatTimelineAction::Delete)},
                                   {QStringLiteral("key"), QStringLiteral("delete")},
                                   {QStringLiteral("text"), tr("Delete message")},
                                   {QStringLiteral("iconName"), QStringLiteral("edit-delete")},
                                   {QStringLiteral("destructive"), true}});
    if (available.testFlag(ChatTimelineAction::ShowSource))
        actions.append(QVariantMap{{QStringLiteral("id"), static_cast<int>(ChatTimelineAction::ShowSource)},
                                   {QStringLiteral("key"), QStringLiteral("showSource")},
                                   {QStringLiteral("text"), tr("Show source")},
                                   {QStringLiteral("iconName"), QStringLiteral("help-contents")}});
    if (available.testFlag(ChatTimelineAction::Pin))
        actions.append(QVariantMap{{QStringLiteral("id"), static_cast<int>(ChatTimelineAction::Pin)},
                                   {QStringLiteral("key"), QStringLiteral("pin")},
                                   {QStringLiteral("text"), tr("Pin message")},
                                   {QStringLiteral("iconName"), QStringLiteral("list-add")}});
    if (available.testFlag(ChatTimelineAction::Unpin))
        actions.append(QVariantMap{{QStringLiteral("id"), static_cast<int>(ChatTimelineAction::Unpin)},
                                   {QStringLiteral("key"), QStringLiteral("unpin")},
                                   {QStringLiteral("text"), tr("Unpin message")},
                                   {QStringLiteral("iconName"), QStringLiteral("list-remove")},
                                   {QStringLiteral("destructive"), true}});
    return actions;
}

int ChatViewModel::timelineActionsRevision() const
{
    return m_timelineActionsRevision;
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
    if (action == -1)
    {
        emit reactionSelectorRequested(stableId);
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

void ChatViewModel::addReaction(const QString &stableId, const QString &key)
{
    if (stableId.isEmpty() || key.isEmpty())
        return;

    auto *service = timelineService(nullptr);
    if (!service || !service->availableActions(m_chat, stableId).testFlag(ChatTimelineAction::React))
        return;

    recordReactionEmojiUse(key);
    const auto item = m_timeline->item(stableId);
    for (const auto &reaction : item.content.reactions)
        if (reaction.key == key && reaction.own)
        {
            service->removeOwnReaction(m_chat, stableId, key);
            return;
        }

    service->addReaction(m_chat, stableId, key);
}

void ChatViewModel::removeOwnReaction(const QString &stableId, const QString &key)
{
    if (stableId.isEmpty() || key.isEmpty())
        return;

    if (auto *service = timelineService(nullptr))
        service->removeOwnReaction(m_chat, stableId, key);
}

void ChatViewModel::executeChatHeaderAction(const QString &actionId)
{
    if (actionId == QStringLiteral("showPinnedMessages") && !pinnedMessages().isEmpty())
        emit pinnedMessagesRequested();
}

void ChatViewModel::recordReactionEmojiUse(const QString &key)
{
    if (!m_configuration)
        return;

    auto recent = recentReactionEmojis();
    recent.removeAll(key);
    recent.prepend(key);
    while (recent.size() > 20)
        recent.removeLast();

    QJsonArray serialized;
    for (const auto &emoji : recent)
        serialized.append(emoji);
    m_configuration->deprecatedApi()->writeEntry(
        QStringLiteral("ChatTimeline"), QStringLiteral("RecentReactionEmojis"),
        QString::fromUtf8(QJsonDocument{serialized}.toJson(QJsonDocument::Compact)));
    emit recentReactionEmojisChanged();
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

void ChatViewModel::jumpToTimelineItem(const QString &stableId)
{
    if (m_timelineController)
        m_timelineController->jumpTo(stableId);
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

void ChatViewModel::loadNewer()
{
    if (m_timelineController)
        m_timelineController->loadNewer();
}

void ChatViewModel::loadLatest()
{
    if (m_timelineController)
        m_timelineController->loadLatest();
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
    refreshChatHeader();
}

void ChatViewModel::styleChanged()
{
    emit themeSourceChanged();
}

void ChatViewModel::customColorsChangedSlot()
{
    emit customColorsChanged();
    emit chatFontChanged();
}

void ChatViewModel::timelineStateChangedSlot()
{
    emit timelineStateChanged();
}

void ChatViewModel::refreshChatHeader()
{
    auto *service = timelineService(nullptr);
    auto headerTitle = service ? service->chatHeaderTitle(m_chat) : QString{};
    if (headerTitle.isEmpty())
        headerTitle = ::title(m_chat);

    QString avatarSource;
    QString description;
    if (const auto *details = m_chat ? qobject_cast<ChatDetailsRoom *>(m_chat.details()) : nullptr)
    {
        const auto avatar = details->avatar();
        if (!avatar.isNull())
        {
            QByteArray imageData;
            QBuffer buffer{&imageData};
            buffer.open(QIODevice::WriteOnly);
            if (avatar.save(&buffer, "PNG"))
                avatarSource = QStringLiteral("data:image/png;base64,") + QString::fromLatin1(imageData.toBase64());
        }
        description = details->description();
    }

    const auto visible = !m_chat.isNull();
    if (m_chatHeaderVisible == visible && m_chatHeaderAvatarSource == avatarSource &&
        m_chatHeaderTitle == headerTitle && m_chatHeaderDescription == description)
        return;

    m_chatHeaderVisible = visible;
    m_chatHeaderAvatarSource = avatarSource;
    m_chatHeaderTitle = headerTitle;
    m_chatHeaderDescription = description;
    emit chatHeaderChanged();
}
