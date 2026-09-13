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

#include "matrix-account-form-utils.h"

#include <QtCore/QUrl>

QStringList MatrixAccountForm::suggestedHomeservers()
{
    return {QStringLiteral("matrix.org"), QStringLiteral("tchncs.de"),
            QStringLiteral("unredacted.org"), QStringLiteral("nope.chat")};
}

QString MatrixAccountForm::loginFromMatrixId(const QString &matrixId)
{
    const auto separator = matrixId.indexOf(QLatin1Char(':'), matrixId.startsWith(QLatin1Char('@')) ? 1 : 0);
    const auto begin = matrixId.startsWith(QLatin1Char('@')) ? 1 : 0;
    return separator < 0 ? matrixId.mid(begin) : matrixId.mid(begin, separator - begin);
}

QString MatrixAccountForm::matrixId(const QString &login, const QString &serverName)
{
    return QStringLiteral("@%1:%2").arg(login.trimmed(), serverName.trimmed());
}

QString MatrixAccountForm::homeserverHost(const QString &homeserverUrl)
{
    auto value = homeserverUrl.trimmed();
    auto url = QUrl{value};
    if (url.scheme().isEmpty())
        url = QUrl{QStringLiteral("https://") + value};
    auto host = url.host();
    if (host.contains(QLatin1Char(':')) && !host.startsWith(QLatin1Char('[')))
        host = QStringLiteral("[%1]").arg(host);
    if (url.port() >= 0)
        host += QStringLiteral(":%1").arg(url.port());
    return host;
}

QString MatrixAccountForm::homeserverUrl(const QString &homeserverHost)
{
    return QStringLiteral("https://") + homeserverHost.trimmed();
}

bool MatrixAccountForm::isLoginValid(const QString &login)
{
    if (login.isEmpty() || login != login.trimmed() || login.contains(QLatin1Char('@'))
        || login.contains(QLatin1Char(':')))
        return false;

    for (const auto character : login)
        if (character.isSpace() || character.isNull() || character.category() == QChar::Other_Control)
            return false;
    return true;
}

bool MatrixAccountForm::isHomeserverHostValid(const QString &homeserverHost)
{
    const auto value = homeserverHost.trimmed();
    if (value.isEmpty() || value != homeserverHost || value.contains(QLatin1Char('/'))
        || value.contains(QLatin1Char('?')) || value.contains(QLatin1Char('#'))
        || value.contains(QLatin1Char('@')))
        return false;

    const auto url = QUrl{homeserverUrl(value)};
    return url.isValid() && url.scheme() == QStringLiteral("https") && !url.host().isEmpty()
           && url.path().isEmpty() && url.query().isEmpty() && url.fragment().isEmpty();
}
