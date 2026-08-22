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

#include "matrix-id-validator.h"
#include "matrix-id-validator.moc"

namespace
{
bool isValidPort(const QString &port)
{
    bool converted = false;
    const auto number = port.toUShort(&converted);
    return converted && number != 0;
}
}

MatrixIdValidator::MatrixIdValidator(QObject *parent) : QValidator{parent}
{
}

QValidator::State MatrixIdValidator::validate(QString &input, int &position) const
{
    Q_UNUSED(position);

    if (input.isEmpty() || input == "@")
        return Intermediate;

    if (input != input.trimmed() || !input.startsWith('@'))
        return Invalid;

    const auto separator = input.indexOf(':', 1);
    if (separator < 0)
        return Intermediate;

    const auto localpart = input.mid(1, separator - 1);
    const auto server = input.mid(separator + 1);
    if (localpart.isEmpty() || localpart.contains(':') || localpart.contains('@'))
        return Invalid;
    if (server.isEmpty())
        return Intermediate;

    for (const auto character : input)
        if (character.isSpace() || character.isNull() || character.category() == QChar::Other_Control)
            return Invalid;

    // A homeserver part may include a port or an IPv6 literal.  Full server-name validation is
    // deliberately left to libQuotient once connection support exists.
    if (server.contains('/') || server.contains('?') || server.contains('#') || server.contains('@'))
        return Invalid;

    if (server.startsWith('['))
    {
        const auto closingBracket = server.indexOf(']');
        if (closingBracket < 0)
            return Intermediate;
        const auto suffix = server.mid(closingBracket + 1);
        if (closingBracket == 1 || (!suffix.isEmpty() && (!suffix.startsWith(':') || !isValidPort(suffix.mid(1)))))
            return Invalid;
    }
    else
    {
        if (server.count(':') > 1)
            return Invalid;

        const auto separator = server.indexOf(':');
        if (separator >= 0 &&
            (separator == 0 || separator + 1 == server.size() || !isValidPort(server.mid(separator + 1))))
            return Invalid;
    }

    return Acceptable;
}
