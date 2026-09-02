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

#include "attach-file-action.h"
#include "attach-file-action.moc"

#include "actions/action.h"
#include "protocols/protocol.h"
#include "widgets/chat-edit-box.h"

AttachFileAction::AttachFileAction(QObject *parent) : ActionDescription(parent)
{
    setIcon(KaduIcon{"document-send"});
    setName(QStringLiteral("attachFileAction"));
    setText(tr("Attach File"));
    setType(ActionDescription::TypeChat);
}

void AttachFileAction::actionTriggered(QAction *sender, bool)
{
    auto *chatEditBox = qobject_cast<ChatEditBox *>(sender->parent());
    if (chatEditBox)
        chatEditBox->openAttachFileDialog();
}

void AttachFileAction::updateActionState(Action *action)
{
    const auto *chatEditBox = qobject_cast<ChatEditBox *>(action->parent());
    const auto account = action->context()->chat().chatAccount();
    const auto *protocol = account ? account.protocolHandler() : nullptr;
    action->setEnabled(chatEditBox && chatEditBox->attachmentsEnabled() && protocol && protocol->isAttachmentsSupported());
}
