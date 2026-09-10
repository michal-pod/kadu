/*
 * %kadu copyright begin%
 * Copyright 2013 Bartosz Brachaczek (b.brachaczek@gmail.com)
 * Copyright 2011, 2013, 2014 Rafał Przemysław Malinowski (rafal.przemyslaw.malinowski@gmail.com)
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

#include "model/roles.h"
#include "multilogon/multilogon-session.h"
#include "protocols/services/multilogon-service.h"

#include "multilogon-model.h"
#include "multilogon-model.moc"

MultilogonModel::MultilogonModel(MultilogonService *service, QObject *parent)
        : QAbstractTableModel(parent), Service(service)
{
    if (Service)
    {
        connect(
            Service, SIGNAL(multilogonSessionAboutToBeConnected(MultilogonSession)), this,
            SLOT(multilogonSessionAboutToBeConnected(MultilogonSession)));
        connect(
            Service, SIGNAL(multilogonSessionConnected(MultilogonSession)), this,
            SLOT(multilogonSessionConnected(MultilogonSession)));
        connect(
            Service, SIGNAL(multilogonSessionAboutToBeDisconnected(MultilogonSession)), this,
            SLOT(multilogonSessionAboutToBeDisconnected(MultilogonSession)));
        connect(
            Service, SIGNAL(multilogonSessionDisconnected(MultilogonSession)), this,
            SLOT(multilogonSessionDisconnected(MultilogonSession)));
        connect(Service, SIGNAL(sessionsAboutToBeReset()), this, SLOT(sessionsAboutToBeReset()));
        connect(Service, SIGNAL(sessionsReset()), this, SLOT(sessionsReset()));
    }
}

MultilogonModel::~MultilogonModel()
{
}

int MultilogonModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() || !Service ? 0 : Service->sessions().count();
}

int MultilogonModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 4;
}

QVariant MultilogonModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (Qt::Horizontal != orientation || Qt::DisplayRole != role)
        return QVariant();

    switch (section)
    {
    case 0:
        return tr("Name");
    case 1:
        return tr("Verification");
    case 2:
        return tr("IP address");
    case 3:
        return Service ? Service->activityColumnTitle() : tr("Activity");
    }

    return QVariant();
}

QVariant MultilogonModel::data(const QModelIndex &index, int role) const
{
    if (index.parent().isValid() || !Service)
        return QVariant();

    int row = index.row();
    if (row < 0 || row >= Service->sessions().count())
        return QVariant();

    MultilogonSession session = Service->sessions().at(row);
    if (role == MultilogonSessionRole)
        return QVariant::fromValue(session);

    if (role == Qt::ToolTipRole && session.current)
        return tr("This is the current session");

    if (Qt::DisplayRole != role)
        return QVariant();

    switch (index.column())
    {
    case 0:
        return session.current ? tr("%1 (this device)").arg(session.name) : session.name;
    case 1:
        switch (session.verificationState)
        {
        case MultilogonSessionVerificationState::NotAvailable:
            return QVariant{};
        case MultilogonSessionVerificationState::Unknown:
            return tr("Unknown");
        case MultilogonSessionVerificationState::Unverified:
            return tr("Unverified");
        case MultilogonSessionVerificationState::Verified:
            return tr("Verified");
        }
        return QVariant{};
    case 2:
        return session.remoteAddress;
    case 3:
        return session.activityTime;
    }

    return QVariant();
}

void MultilogonModel::multilogonSessionAboutToBeConnected(MultilogonSession session)
{
    Q_UNUSED(session)

    int row = rowCount();
    beginInsertRows(QModelIndex(), row, row);
}

void MultilogonModel::multilogonSessionConnected(MultilogonSession session)
{
    Q_UNUSED(session)

    endInsertRows();
}

void MultilogonModel::multilogonSessionAboutToBeDisconnected(MultilogonSession session)
{
    int row = Service->sessions().indexOf(session);
    if (-1 == row)
        return;

    beginRemoveRows(QModelIndex(), row, row);
}

void MultilogonModel::multilogonSessionDisconnected(MultilogonSession session)
{
    Q_UNUSED(session)

    endRemoveRows();
}

void MultilogonModel::sessionsAboutToBeReset()
{
    beginResetModel();
}

void MultilogonModel::sessionsReset()
{
    endResetModel();
}
