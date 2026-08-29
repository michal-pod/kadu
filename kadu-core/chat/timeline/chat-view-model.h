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
#include "chat/timeline/chat-timeline-model.h"
#include "chat/timeline/legacy-message-timeline-adapter.h"
#include "exports.h"

#include <QtCore/QPointer>
#include <QtCore/QUrl>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>

class ChatTimelineController;
class ChatConfigurationHolder;
class ChatStyleManager;
class Message;
class ProtocolTimelineService;
class SortedMessages;
class UrlHandlerManager;

/**
 * @short QML-facing state of one chat card.
 *
 * The view model chooses a protocol-native timeline when it is available. A
 * legacy protocol still receives a neutral timeline through
 * LegacyMessageTimelineAdapter, so QML never has to inspect Message objects.
 */
class KADUAPI ChatViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(ChatTimelineModel *timeline READ timeline CONSTANT)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(QUrl themeSource READ themeSource NOTIFY themeSourceChanged)
    Q_PROPERTY(QString themeColorScheme READ themeColorScheme NOTIFY themeSourceChanged)
    Q_PROPERTY(QVariantMap customColors READ customColors NOTIFY customColorsChanged)
    Q_PROPERTY(bool roomInfoVisible READ roomInfoVisible NOTIFY roomDetailsChanged)
    Q_PROPERTY(QString roomAvatarSource READ roomAvatarSource NOTIFY roomDetailsChanged)
    Q_PROPERTY(QString roomName READ roomName NOTIFY roomDetailsChanged)
    Q_PROPERTY(QString roomDescription READ roomDescription NOTIFY roomDetailsChanged)
    Q_PROPERTY(bool usesProtocolTimeline READ usesProtocolTimeline CONSTANT)
    Q_PROPERTY(bool loadingInitial READ loadingInitial NOTIFY timelineStateChanged)
    Q_PROPERTY(bool loadingOlder READ loadingOlder NOTIFY timelineStateChanged)
    Q_PROPERTY(bool hasOlder READ hasOlder NOTIFY timelineStateChanged)
    Q_PROPERTY(QString readMarkerId READ readMarkerId NOTIFY timelineStateChanged)
    Q_PROPERTY(int newEventsBelow READ newEventsBelow NOTIFY timelineStateChanged)

public:
    explicit ChatViewModel(Chat chat, ProtocolTimelineService *timelineService = nullptr,
                           ChatStyleManager *chatStyleManager = nullptr,
                           ChatConfigurationHolder *chatConfigurationHolder = nullptr, QObject *parent = nullptr);
    virtual ~ChatViewModel();

    Chat chat() const;
    ChatTimelineModel *timeline() const;
    QString title() const;
    QUrl themeSource() const;
    QString themeColorScheme() const;
    QVariantMap customColors() const;
    bool roomInfoVisible() const;
    QString roomAvatarSource() const;
    QString roomName() const;
    QString roomDescription() const;
    bool usesProtocolTimeline() const;
    bool loadingInitial() const;
    bool loadingOlder() const;
    bool hasOlder() const;
    QString readMarkerId() const;
    int newEventsBelow() const;

    void addLegacyMessage(const Message &message);
    void addLegacyMessages(const SortedMessages &messages);
    void setUrlHandlerManager(UrlHandlerManager *urlHandlerManager);

    Q_INVOKABLE void openUrl(const QString &url);
    Q_INVOKABLE void copyText(const QString &text);
    Q_INVOKABLE QVariantList timelineActions(const QString &stableId) const;
    Q_INVOKABLE void executeTimelineAction(const QString &stableId, int action);
    Q_INVOKABLE void setTimelineAtNewest(bool atNewest);
    Q_INVOKABLE void markTimelineItemVisible(const QString &stableId);

public slots:
    void open();
    void close();
    void loadOlder();

signals:
    void titleChanged();
    void themeSourceChanged();
    void customColorsChanged();
    void roomDetailsChanged();
    void timelineStateChanged();

private:
    Chat m_chat;
    ChatTimelineController *m_timelineController = nullptr;
    ChatTimelineModel *m_timeline = nullptr;
    LegacyMessageTimelineAdapter m_legacyAdapter;
    ChatStyleManager *m_chatStyleManager = nullptr;
    QPointer<ChatConfigurationHolder> m_chatConfigurationHolder;
    QPointer<UrlHandlerManager> m_urlHandlerManager;
    bool m_roomInfoVisible = false;
    QString m_roomAvatarSource;
    QString m_roomName;
    QString m_roomDescription;
    bool m_open = false;

    ProtocolTimelineService *timelineService(ProtocolTimelineService *service) const;
    void refreshRoomDetails();

private slots:
    void chatUpdated();
    void styleChanged();
    void customColorsChangedSlot();
    void timelineStateChangedSlot();
};
