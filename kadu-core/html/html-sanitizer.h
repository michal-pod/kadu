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

#pragma once

#include "exports.h"

#include <QtCore/QString>

class HtmlString;
class SanitizedHtmlString;
class QDomElement;
class QDomNode;

class KADUAPI HtmlSanitizer
{
public:
    SanitizedHtmlString sanitize(const HtmlString &html) const;

private:
    static QString prepareFragment(QString html);
    static bool isRejectedElement(const QString &tagName);
    static bool isSafeHref(const QString &href);
    static QString applySupportedSpanFormatting(const QDomElement &element, QString content);
    static QString sanitizeElement(const QDomElement &element);
    static QString sanitizeChildren(const QDomNode &node);
};
