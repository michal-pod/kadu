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

#include "matrix-protocol-factory.h"
#include "matrix-protocol-factory.moc"

#include "icons/kadu-icon.h"
#include "plugin/plugin-injected-factory.h"
#include "status/status-type.h"

#include "gui/matrix-add-account-widget.h"
#include "gui/matrix-edit-account-widget.h"
#include "matrix-id-validator.h"
#include "matrix-protocol.h"

MatrixProtocolFactory::MatrixProtocolFactory(QObject *parent) : ProtocolFactory{}
{
    Q_UNUSED(parent);

    m_supportedStatusTypes.append(StatusType::Online);
    m_supportedStatusTypes.append(StatusType::Offline);
}

void MatrixProtocolFactory::setPluginInjectedFactory(PluginInjectedFactory *pluginInjectedFactory)
{
    m_pluginInjectedFactory = pluginInjectedFactory;
}

Protocol *MatrixProtocolFactory::createProtocolHandler(Account account)
{
    return m_pluginInjectedFactory->makeInjected<MatrixProtocol>(account, this);
}

AccountAddWidget *MatrixProtocolFactory::newAddAccountWidget(bool showButtons, QWidget *parent)
{
    auto result = m_pluginInjectedFactory->makeInjected<MatrixAddAccountWidget>(showButtons, parent);
    connect(this, SIGNAL(destroyed()), result, SLOT(deleteLater()));
    return result;
}

AccountCreateWidget *MatrixProtocolFactory::newCreateAccountWidget(bool, QWidget *)
{
    return nullptr;
}

AccountEditWidget *MatrixProtocolFactory::newEditAccountWidget(Account account, QWidget *parent)
{
    auto result = m_pluginInjectedFactory->makeInjected<MatrixEditAccountWidget>(account, parent);
    connect(this, SIGNAL(destroyed()), result, SLOT(deleteLater()));
    return result;
}

QList<StatusType> MatrixProtocolFactory::supportedStatusTypes()
{
    return m_supportedStatusTypes;
}

Status MatrixProtocolFactory::adaptStatus(Status status) const
{
    if (status.type() == StatusType::FreeForChat)
        status.setType(StatusType::Online);
    else if (status.type() == StatusType::Away || status.type() == StatusType::NotAvailable ||
             status.type() == StatusType::DoNotDisturb)
        status.setType(StatusType::None);
    else if (status.type() == StatusType::Invisible)
        status.setType(StatusType::Offline);

    return status;
}

QString MatrixProtocolFactory::idLabel()
{
    return tr("Matrix ID:");
}

QValidator::State MatrixProtocolFactory::validateId(QString id)
{
    int position = 0;
    MatrixIdValidator validator;
    return validator.validate(id, position);
}

bool MatrixProtocolFactory::canRegister()
{
    return false;
}

KaduIcon MatrixProtocolFactory::icon()
{
    // There is no Matrix artwork in the current icon theme.  Use Kadu's generic message icon
    // rather than an icon from a different protocol until the plugin grows its own asset.
    return KaduIcon("protocols/common/message");
}
