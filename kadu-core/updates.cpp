/*
 * %kadu copyright begin%
 * Copyright 2008, 2010, 2011 Piotr Galiszewski (piotr.galiszewski@kadu.im)
 * Copyright 2010, 2012 Wojciech Treter (juzefwt@gmail.com)
 * Copyright 2008, 2009, 2010 Tomasz Rostański (rozteck@interia.pl)
 * Copyright 2008, 2009 Michał Podsiadlik (michal@kadu.net)
 * Copyright 2010, 2011, 2012, 2013, 2014 Bartosz Brachaczek (b.brachaczek@gmail.com)
 * Copyright 2007 Marcin Ślusarz (joi@kadu.net)
 * Copyright 2007 Dawid Stawiarski (neeo@kadu.net)
 * Copyright 2008, 2009, 2010, 2011, 2013, 2014 Rafał Przemysław Malinowski (rafal.przemyslaw.malinowski@gmail.com)
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

#include <QtCore/QSysInfo>
#include <QtCore/QUrlQuery>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

#include "accounts/account-manager.h"
#include "accounts/account.h"
#include "configuration/configuration-manager.h"
#include "configuration/configuration.h"
#include "configuration/deprecated-configuration-api.h"
#include "core/injected-factory.h"
#include "core/version-service.h"
#include "windows/kadu-window-service.h"
#include "windows/kadu-window.h"
#include "windows/updates-dialog.h"

#include "updates.h"
#include "updates.moc"

Updates::Updates(QObject *parent) : QObject(parent), UpdateChecked{false}
{
}

Updates::~Updates()
{
}

void Updates::setAccountManager(AccountManager *accountManager)
{
    m_accountManager = accountManager;
}

void Updates::setConfigurationManager(ConfigurationManager *configurationManager)
{
    m_configurationManager = configurationManager;
}

void Updates::setConfiguration(Configuration *configuration)
{
    m_configuration = configuration;
}

void Updates::setInjectedFactory(InjectedFactory *injectedFactory)
{
    m_injectedFactory = injectedFactory;
}

void Updates::setMainWindowService(KaduWindowService *kaduWindowService)
{
    m_kaduWindowService = kaduWindowService;
}

void Updates::setVersionService(VersionService *versionService)
{
    m_versionService = versionService;
}

void Updates::init()
{
    buildQuery();
    triggerAllAccountsAdded(m_accountManager);
}

void Updates::accountAdded(Account account)
{
    connect(account, SIGNAL(connected()), this, SLOT(run()));
}

void Updates::accountRemoved(Account account)
{
    disconnect(account, 0, this, 0);
}

void Updates::buildQuery()
{
    Query = QUrl{QStringLiteral("http://www.kadu.im/update-new.php")};
    QUrlQuery parameters;
    auto addParameter = [&parameters](const QString &name, const QString &value) {
        parameters.addQueryItem(name, QString::fromLatin1(QUrl::toPercentEncoding(value)));
    };

    addParameter(QStringLiteral("uuid"), m_configurationManager->uuid().toString());
    addParameter(QStringLiteral("version"), m_versionService->version());

    if (m_configuration->deprecatedApi()->readBoolEntry("General", "SendSysInfo", true))
    {
        addParameter(QStringLiteral("kernel"), QSysInfo::kernelType());
        addParameter(QStringLiteral("product"), QSysInfo::productType());
        addParameter(QStringLiteral("release"), QSysInfo::productVersion());
    }

    Query.setQuery(parameters);
}

void Updates::run()
{
    if (UpdateChecked)
        return;

    UpdateChecked = true;

    auto manager = new QNetworkAccessManager{this};
    connect(manager, SIGNAL(finished(QNetworkReply *)), this, SLOT(gotUpdatesInfo(QNetworkReply *)));

    manager->get(QNetworkRequest{Query});
}

bool Updates::isNewerVersionThan(const QString &version)
{
    QStringList thisVersion = stripVersion(m_versionService->version()).split('.');
    QStringList queryVersion = stripVersion(version).split('.');

    for (int i = 0, end = qMin(thisVersion.size(), queryVersion.size()); i < end; ++i)
        if (queryVersion.at(i).toInt() != thisVersion.at(i).toInt())
            return queryVersion.at(i).toInt() > thisVersion.at(i).toInt();

    return (queryVersion.size() > thisVersion.size());
}

QString Updates::stripVersion(const QString &version)
{
    Qt::CaseSensitivity cs = Qt::CaseInsensitive;

    // We don't want to compare git versions at all.
    if (version.contains("-g", cs))
        return "9999";

    QString strippedVersion = version;
    // Use negative numbers so that 0.1.0 is considered newer than 0.1-alpha10.
    if (strippedVersion.contains("-alpha", cs))
        strippedVersion.replace("-alpha", ".-3.", cs);
    else if (strippedVersion.contains("-beta", cs))
        strippedVersion.replace("-beta", ".-2.", cs);
    else if (strippedVersion.contains("-rc", cs))
        strippedVersion.replace("-rc", ".-1.", cs);
    else
        strippedVersion.append(".0");

    return strippedVersion;
}

void Updates::gotUpdatesInfo(QNetworkReply *reply)
{
    reply->deleteLater();
    deleteLater();

    if (m_configuration->deprecatedApi()->readBoolEntry("General", "CheckUpdates"))
    {
        auto newestVersion = QString::fromUtf8(reply->readAll());
        if (newestVersion.size() > 31)
            return;

        if (isNewerVersionThan(newestVersion))
        {
            auto dialog =
                m_injectedFactory->makeInjected<UpdatesDialog>(newestVersion, m_kaduWindowService->kaduWindow());
            dialog->show();
        }
    }
}
