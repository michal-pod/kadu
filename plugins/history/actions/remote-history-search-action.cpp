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

#include "remote-history-search-action.h"
#include "remote-history-search-action.moc"

#include "gui/remote-history-search-dialog.h"
#include "history.h"

#include "actions/action-context.h"
#include "actions/action.h"
#include "chat/chat.h"
#include "core/injected-factory.h"
#include "protocols/protocol.h"
#include "protocols/services/protocol-history-service.h"

RemoteHistorySearchAction::RemoteHistorySearchAction(QObject *parent) : ActionDescription{parent}
{
    setType(ActionDescription::TypeUser);
    setName(QStringLiteral("remoteHistorySearchAction"));
    setIcon(KaduIcon{"edit-find"});
    setText(tr("Search Remote History"));
}

void RemoteHistorySearchAction::setHistory(History *history)
{
    m_history = history;
}

void RemoteHistorySearchAction::actionInstanceCreated(Action *action)
{
    updateActionState(action);
}

void RemoteHistorySearchAction::actionTriggered(QAction *sender, bool toggled)
{
    Q_UNUSED(toggled)

    auto *action = qobject_cast<Action *>(sender);
    if (!action || !m_history)
        return;

    const auto chat = m_history->protocolHistoryChat(action->context()->chat());
    auto *historyService = m_history->historyService(chat);
    if (!historyService)
        return;

    injectedFactory()->makeInjected<RemoteHistorySearchDialog>(historyService, chat, action->parentWidget())->show();
}

void RemoteHistorySearchAction::updateActionState(Action *action)
{
    if (!action || !m_history)
        return;

    const auto chat = m_history->protocolHistoryChat(action->context()->chat());
    auto *protocol = chat ? chat.chatAccount().protocolHandler() : nullptr;
    const auto supportsSearch = protocol && m_history->historyService(chat)
                                && protocol->isRemoteSearchSupported().testFlag(
                                    RemoteHistorySearchCapability::Messages);
    action->setVisible(supportsSearch);
    action->setEnabled(supportsSearch);
}
