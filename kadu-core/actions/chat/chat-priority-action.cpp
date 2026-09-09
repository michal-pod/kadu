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

#include "chat-priority-action.h"
#include "chat-priority-action.moc"

#include "actions/action-context.h"
#include "actions/action.h"
#include "chat/chat-service-repository.h"
#include "icons/kadu-icon.h"
#include "protocols/services/chat-service.h"

#include <QtGui/QActionGroup>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QMenu>

ChatPriorityAction::ChatPriorityAction(QObject *parent) : ActionDescription{parent}
{
    setType(ActionDescription::TypeUser);
    setName(QStringLiteral("chatPriorityAction"));
    setIcon(KaduIcon{QStringLiteral("emblem-favorite")});
    setText(tr("Priority"));
}

void ChatPriorityAction::setChatServiceRepository(ChatServiceRepository *chatServiceRepository)
{
    m_chatServiceRepository = chatServiceRepository;
}

QMenu *ChatPriorityAction::menuForAction(Action *action)
{
    auto menu = new QMenu{};
    connect(menu, &QMenu::aboutToShow, this, [this, menu, action] { populateMenu(menu, action); });
    connect(menu, &QMenu::triggered, this, [this, action](QAction *selectedAction) {
        if (!selectedAction || !selectedAction->data().isValid())
            return;
        setPriority(action, static_cast<ChatPriority>(selectedAction->data().toInt()));
    });
    return menu;
}

void ChatPriorityAction::populateMenu(QMenu *menu, Action *action)
{
    menu->clear();
    const auto currentPriority = action->context()->chat().priority();

    auto favorite = menu->addAction(tr("Favorite"));
    auto group = new QActionGroup{favorite};
    group->setExclusive(true);

    const auto addPriority = [group, currentPriority](QAction *item, ChatPriority priority) {
        group->addAction(item);
        item->setCheckable(true);
        item->setChecked(currentPriority == priority);
        item->setData(static_cast<qint32>(priority));
    };

    addPriority(favorite, ChatPriority::Favorite);
    addPriority(menu->addAction(tr("Default")), ChatPriority::Default);
    addPriority(menu->addAction(tr("Low priority")), ChatPriority::LowPriority);
}

void ChatPriorityAction::setPriority(Action *action, ChatPriority priority)
{
    const auto chat = action->context()->chat();
    auto *chatService = chat && m_chatServiceRepository
                            ? m_chatServiceRepository->chatService(chat.chatAccount())
                            : nullptr;
    if (!chatService || chat.priority() == priority)
        return;

    if (!chatService->setChatPriority(chat, priority))
        QMessageBox::warning(action->parentWidget(), tr("Priority"),
                             tr("The chat priority could not be changed."));
}

void ChatPriorityAction::updateActionState(Action *action)
{
    const auto chat = action->context()->chat();
    action->setEnabled(chat && m_chatServiceRepository &&
                       m_chatServiceRepository->chatService(chat.chatAccount()));
}
