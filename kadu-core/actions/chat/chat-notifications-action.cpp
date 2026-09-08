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

#include "chat-notifications-action.h"
#include "chat-notifications-action.moc"

#include "actions/action-context.h"
#include "actions/action.h"
#include "chat/chat-service-repository.h"
#include "icons/kadu-icon.h"
#include "protocols/services/chat-service.h"

#include <QtGui/QActionGroup>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QMenu>

ChatNotificationsAction::ChatNotificationsAction(QObject *parent) : ActionDescription{parent}
{
    setType(ActionDescription::TypeUser);
    setName(QStringLiteral("chatNotificationsAction"));
    setIcon(KaduIcon{QStringLiteral("preferences-desktop-notification")});
    setText(tr("Notifications"));
}

void ChatNotificationsAction::setChatServiceRepository(ChatServiceRepository *chatServiceRepository)
{
    m_chatServiceRepository = chatServiceRepository;
    connect(m_chatServiceRepository, &ChatServiceRepository::chatServiceAdded, this,
            &ChatNotificationsAction::watchChatService);
    for (auto *chatService : chatServiceRepository)
        watchChatService(chatService);
}

void ChatNotificationsAction::watchChatService(ChatService *chatService)
{
    if (!chatService)
        return;

    connect(chatService, &ChatService::chatNotificationModeChanged, this,
            &ChatNotificationsAction::chatNotificationModeChanged, Qt::UniqueConnection);
    connect(chatService, &ChatService::chatNotificationModeChangeFailed, this,
            &ChatNotificationsAction::chatNotificationModeChangeFailed, Qt::UniqueConnection);
}

QMenu *ChatNotificationsAction::menuForAction(Action *action)
{
    auto menu = new QMenu{};
    connect(menu, &QMenu::aboutToShow, this, [this, menu, action] { populateMenu(menu, action); });
    connect(menu, &QMenu::triggered, this, [this, action](QAction *selectedAction) {
        if (!selectedAction || !selectedAction->data().isValid())
            return;
        setMode(action, static_cast<ChatNotificationMode>(selectedAction->data().toInt()));
    });
    return menu;
}

void ChatNotificationsAction::populateMenu(QMenu *menu, Action *action)
{
    menu->clear();
    const auto chat = action->context()->chat();
    const auto currentMode = chat.notificationMode();

    auto defaultMode = menu->addAction(tr("Default"));
    auto group = new QActionGroup{defaultMode};
    group->setExclusive(true);

    const auto addMode = [group, currentMode](QAction *item, ChatNotificationMode mode) {
        group->addAction(item);
        item->setCheckable(true);
        item->setChecked(currentMode == mode);
        item->setData(static_cast<int>(mode));
    };

    addMode(defaultMode, ChatNotificationMode::Default);
    addMode(menu->addAction(tr("All messages")), ChatNotificationMode::AllMessages);
    addMode(menu->addAction(tr("Mentions only")), ChatNotificationMode::MentionsOnly);
    addMode(menu->addAction(tr("No notifications")), ChatNotificationMode::NoNotifications);
}

void ChatNotificationsAction::setMode(Action *action, ChatNotificationMode mode)
{
    const auto chat = action->context()->chat();
    auto *chatService = chat && m_chatServiceRepository
                            ? m_chatServiceRepository->chatService(chat.chatAccount())
                            : nullptr;
    if (!chatService || chat.notificationMode() == mode)
        return;

    m_pendingChanges.insert(chat.uuid(), PendingChange{mode, action->parentWidget()});
    updateActionStates();
    if (!chatService->setChatNotificationMode(chat, mode))
    {
        m_pendingChanges.remove(chat.uuid());
        updateActionStates();
        QMessageBox::warning(action->parentWidget(), tr("Notifications"),
                             tr("The notification setting could not be changed."));
    }
}

void ChatNotificationsAction::updateActionState(Action *action)
{
    const auto chat = action->context()->chat();
    auto *chatService = chat && m_chatServiceRepository
                            ? m_chatServiceRepository->chatService(chat.chatAccount())
                            : nullptr;
    action->setEnabled(chatService && !m_pendingChanges.contains(chat.uuid()));
}

void ChatNotificationsAction::chatNotificationModeChanged(const Chat &chat, ChatNotificationMode mode)
{
    const auto pending = m_pendingChanges.constFind(chat.uuid());
    if (pending == m_pendingChanges.cend() || pending->mode != mode)
        return;

    m_pendingChanges.remove(chat.uuid());
    updateActionStates();
}

void ChatNotificationsAction::chatNotificationModeChangeFailed(const Chat &chat, const QString &error)
{
    const auto pending = m_pendingChanges.take(chat.uuid());
    if (!pending.parent)
        return;

    QMessageBox::warning(pending.parent, tr("Notifications"),
                         error.isEmpty() ? tr("The notification setting could not be changed.") : error);
    updateActionStates();
}
