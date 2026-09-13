/*
 * %kadu copyright begin%
 * Copyright 2011 Piotr Galiszewski (piotr.galiszewski@kadu.im)
 * Copyright 2012 Wojciech Treter (juzefwt@gmail.com)
 * Copyright 2011, 2012, 2013 Bartosz Brachaczek (b.brachaczek@gmail.com)
 * Copyright 2011, 2012, 2013, 2014 Rafał Przemysław Malinowski (rafal.przemyslaw.malinowski@gmail.com)
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

#include "icons-manager.h"
#include "icons-manager.moc"

#include "accounts/account-manager.h"
#include "configuration/configuration.h"
#include "configuration/deprecated-configuration-api.h"
#include "core/core.h"
#include "icons/kadu-icon-engine.h"
#include "icons/kadu-icon.h"
#include "misc/misc.h"
#include "protocols/protocol.h"
#include "themes/icon-theme-manager.h"

#include <memory>

#include <QtCore/QHash>
#include <QtCore/QRegularExpression>
#include <QtCore/QFileInfo>
#include <QtGui/QGuiApplication>
#include <QtGui/QPalette>

namespace
{
/**
 * @short Icon sizes a theme may provide, smallest first.
 *
 * 48x48 used to be missing from this list although the bundled themes ship 45 icons in it, so
 * those files were never offered to anyone.
 */
const QStringList &iconSizes()
{
    static const QStringList sizes{QStringLiteral("16x16"), QStringLiteral("22x22"), QStringLiteral("32x32"),
                                   QStringLiteral("48x48"), QStringLiteral("64x64"), QStringLiteral("96x96"),
                                   QStringLiteral("128x128"), QStringLiteral("256x256")};
    return sizes;
}

/**
 * @short The bundled icon behind each standard name Kadu now asks for.
 *
 * Following the note the icons' author left -- "the less icons here, the better because then we may
 * use system icons" -- these are asked for by their freedesktop names, so a desktop that has them
 * supplies them. The files that used to answer are still here under their old names, and answer
 * when the desktop has nothing and when the setting says to prefer Kadu's own.
 */
const QHash<QString, QString> &bundledEquivalent()
{
    static const QHash<QString, QString> equivalents{
        {QStringLiteral("help-about"), QStringLiteral("kadu_icons/about-kadu")},
        {QStringLiteral("system-users"), QStringLiteral("kadu_icons/conference")},
        {QStringLiteral("edit-copy"), QStringLiteral("kadu_icons/copy-personal-info")},
        {QStringLiteral("go-jump"), QStringLiteral("kadu_icons/enter")},
        {QStringLiteral("help-contents"), QStringLiteral("kadu_icons/get-involved")},
        {QStringLiteral("document-open-recent"), QStringLiteral("kadu_icons/history")},
        {QStringLiteral("preferences-other"), QStringLiteral("kadu_icons/section-kadu")},
        {QStringLiteral("go-next"), QStringLiteral("kadu_icons/stylesheet-branch-closed")},
        {QStringLiteral("go-down"), QStringLiteral("kadu_icons/stylesheet-branch-open")},
        {QStringLiteral("document-save"), QStringLiteral("kadu_icons/transfer-receive")},
        {QStringLiteral("document-send"), QStringLiteral("kadu_icons/transfer-send")},
        {QStringLiteral("edit-clear-history"), QStringLiteral("kadu_icons/clear-history")},
        {QStringLiteral("preferences-desktop-notification"), QStringLiteral("kadu_icons/enable-notifications")},
        {QStringLiteral("merge"), QStringLiteral("kadu_icons/merge-buddies")},
        {QStringLiteral("preferences-plugin"), QStringLiteral("kadu_icons/plugins")},
        {QStringLiteral("tools-report-bug"), QStringLiteral("kadu_icons/report-a-bug")},
        {QStringLiteral("tab-detach"), QStringLiteral("kadu_icons/tab-detach")},
    };
    return equivalents;
}
}

IconsManager::IconsManager(QObject *parent) : QObject{parent}, UseSystemIcons(true)
{
}

IconsManager::~IconsManager()
{
}

void IconsManager::setAccountManager(AccountManager *accountManager)
{
    m_accountManager = accountManager;
}

void IconsManager::setConfiguration(Configuration *configuration)
{
    m_configuration = configuration;
}

void IconsManager::setIconThemeManager(IconThemeManager *iconThemeManager)
{
    m_iconThemeManager = iconThemeManager;
}

void IconsManager::init()
{
    m_iconThemeManager->loadThemes();
    configurationUpdated();

    // TODO: localized protocol
    localProtocolPath = "gadu-gadu";
}

QString IconsManager::iconPath(
    const KaduIcon &icon, IconsManager::AllowEmpty allowEmpty, IconsManager::SizeMatch sizeMatch) const
{
    QString path = icon.path();
    QString size = icon.size();

    QFileInfo fileInfo(path);
    QString themePath = icon.themePath().isEmpty() ? m_iconThemeManager->currentTheme().path() : icon.themePath();
    QString name = fileInfo.fileName();
    QString realPath = fileInfo.path();

    auto fileForSize = [&themePath, &realPath, &name](const QString &wantedSize) {
        QFileInfo candidate{themePath + realPath + '/' + wantedSize + '/' + name + ".png"};
        if (candidate.isFile() && candidate.isReadable())
            return candidate.canonicalFilePath();

        candidate.setFile(themePath + realPath + '/' + wantedSize + '/' + name + ".gif");
        if (candidate.isFile() && candidate.isReadable())
            return candidate.canonicalFilePath();

        return QString{};
    };

    auto found = fileForSize(size);
    if (!found.isEmpty())
        return found;

    // A theme carries neither every icon in every size nor an entry for callers that ask for no
    // size at all: protocols/xmpp/xmpp exists only at 32x32 while the protocol asks for 16x16, and
    // the Gadu-Gadu factory names no size whatsoever. Dropping through to the placeholder threw
    // those icons away, so take the nearest size the theme does have.
    if (AnySize == sizeMatch)
    {
        auto const requested = size.section('x', 0, 0).toInt();
        auto sizes = iconSizes();
        std::sort(sizes.begin(), sizes.end(), [requested](const QString &left, const QString &right) {
            return qAbs(left.section('x', 0, 0).toInt() - requested) <
                   qAbs(right.section('x', 0, 0).toInt() - requested);
        });

        for (auto const &candidateSize : std::as_const(sizes))
        {
            found = fileForSize(candidateSize);
            if (!found.isEmpty())
                return found;
        }
    }

    if (realPath == QStringLiteral("protocols/common"))
    {
        QString protocolPath;
        if (m_accountManager->defaultAccount().protocolHandler())
            protocolPath = m_accountManager->defaultAccount().protocolHandler()->statusPixmapPath();
        else
            protocolPath = localProtocolPath;

        KaduIcon protocolPathIcon = icon;
        protocolPathIcon.setPath(QString("protocols/%1/%2").arg(protocolPath).arg(name));
        return iconPath(protocolPathIcon, allowEmpty, sizeMatch);
    }

    // Callers that need a file rather than an icon -- a stylesheet's url() -- ask here by a standard
    // name too, and this path knows nothing of the desktop's icon theme. go-down is the case in the
    // contact list: no bundled theme carries a file of that name, so the arrow of an expanded group
    // fell through to the placeholder. The drawing behind the name answers instead, the same one
    // iconByPath() falls back on.
    auto const bundled = bundledEquivalent().value(path);
    if (!bundled.isEmpty())
    {
        KaduIcon bundledIcon = icon;
        bundledIcon.setPath(bundled);
        return iconPath(bundledIcon, allowEmpty, sizeMatch);
    }

    if (EmptyAllowed == allowEmpty)
        return QString();
    else
        return iconPath(KaduIcon("kadu_icons/0", size), EmptyAllowed, sizeMatch);
}

QIcon IconsManager::buildPngIcon(const QString &themePath, const QString &path)
{
    // The engine is filled before it is wrapped, not through QIcon::addFile: that detaches first,
    // and detaching discards an engine whose isNull() is still true -- which an engine with no
    // files yet must answer, or IconsManager could not tell a missing icon from a present one.
    auto engine = std::make_unique<KaduIconEngine>();
    for (auto const &size : iconSizes())
    {
        KaduIcon kaduIcon(path, size);
        kaduIcon.setThemePath(themePath);

        // Only an exact match belongs in a multi-size icon: letting iconPath() substitute a
        // different size would add the same file several times over.
        QString fullPath = iconPath(kaduIcon, EmptyAllowed, ExactSizeOnly);
        if (!fullPath.isEmpty())
            engine->addFile(fullPath, QSize{}, QIcon::Normal, QIcon::Off);
    }

    if (engine->isNull())
        return QIcon{};

    return QIcon{engine.release()};
}

QIcon IconsManager::iconByPath(const QString &themePath, const QString &path, AllowEmpty allowEmpty)
{
    if (!IconCache.contains(themePath + path))
    {
        QIcon icon;

        QFileInfo fileInfo(path);
        if (fileInfo.isAbsolute() && fileInfo.isReadable())
            icon.addFile(path);
        else
        {
            // Icons named after the freedesktop standard -- the ones with no directory in their
            // path, such as application-exit or document-open -- are what the desktop's own theme
            // provides, in every size and usually as vectors. Half of the bundled ones exist at
            // 16x16 and nothing else, which is a poor showing on a magnified screen, so ask the
            // desktop first and keep the bundled files as the answer when it has nothing.
            if (UseSystemIcons && !path.contains('/'))
                icon = QIcon::fromTheme(path);

            if (icon.isNull())
                icon = buildPngIcon(themePath, path);

            // A standard name the desktop does not carry falls back to the file that used to answer
            // to Kadu's own name for it.
            if (icon.isNull())
            {
                auto const equivalent = bundledEquivalent().value(path);
                if (!equivalent.isEmpty())
                    icon = buildPngIcon(themePath, equivalent);
            }

            if (icon.isNull())
            {
                static const QRegularExpression commonRegexp{QStringLiteral("^protocols/common/(.+)$")};
                auto const commonMatch = commonRegexp.match(path);
                if (commonMatch.hasMatch())
                {
                    QString protocolpath;
                    if (m_accountManager->defaultAccount().protocolHandler())
                        protocolpath = m_accountManager->defaultAccount().protocolHandler()->statusPixmapPath();
                    else
                        protocolpath = localProtocolPath;
                    return iconByPath(themePath, QString("protocols/%1/%2").arg(protocolpath, commonMatch.captured(1)));
                }
            }

            if (icon.isNull() && EmptyNotAllowed == allowEmpty)
                icon = buildPngIcon(themePath, "kadu_icons/0");
        }

        IconCache.insert(themePath + path, icon);
    }

    return IconCache.value(themePath + path);
}

QIcon IconsManager::iconByPath(const KaduIcon &icon)
{
    const auto themePath = icon.themePath().isEmpty()
                               ? m_iconThemeManager->currentTheme().path()
                               : icon.themePath();
    return iconByPath(themePath, icon.path());
}

void IconsManager::clearCache()
{
    IconCache.clear();
}

void IconsManager::configurationUpdated()
{
    // A desktop draws its icons in its own colours, so one that turns dark hands out different
    // pictures under the same names -- and nothing in the configuration changes when it does. The
    // cached icons are therefore remembered along with the colour they were fetched under, and go
    // when that colour no longer matches.
    auto const iconsColor = QGuiApplication::palette().color(QPalette::WindowText);
    if (iconsColor != CachedIconsColor)
    {
        CachedIconsColor = iconsColor;
        clearCache();

        emit themeChanged();
    }

    bool const useSystemIcons = m_configuration->deprecatedApi()->readBoolEntry("Look", "UseSystemIcons", true);
    if (useSystemIcons != UseSystemIcons)
    {
        UseSystemIcons = useSystemIcons;
        clearCache();

        emit themeChanged();
    }

    bool themeWasChanged =
        m_configuration->deprecatedApi()->readEntry("Look", "IconTheme") != m_iconThemeManager->currentTheme().name();
    if (themeWasChanged)
    {
        clearCache();
        m_iconThemeManager->setCurrentTheme(m_configuration->deprecatedApi()->readEntry("Look", "IconTheme"));
        m_configuration->deprecatedApi()->writeEntry("Look", "IconTheme", m_iconThemeManager->currentTheme().name());

        emit themeChanged();
    }
}

QSize IconsManager::getIconsSize()
{
    return QSize(16, 16);
}
