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

#include "html-sanitizer.h"

#include "html/html-conversion.h"
#include "html/html-string.h"
#include "html/sanitized-html-string.h"

#include <QtCore/QRegularExpression>
#include <QtCore/QSet>
#include <QtCore/QUrl>
#include <QtXml/QDomDocument>

#include <utility>

QString HtmlSanitizer::prepareFragment(QString html)
{
    static const auto voidElement = QRegularExpression{
        R"(<\s*(?:area|base|col|embed|hr|img|input|link|meta|param|source|track|wbr)\b[^>]*>)",
        QRegularExpression::CaseInsensitiveOption};
    static const auto breakElement = QRegularExpression{
        R"(<\s*br(?:\s+[^<>]*)?\s*/?\s*>)", QRegularExpression::CaseInsensitiveOption};

    html.remove(voidElement);
    return html.replace(breakElement, QStringLiteral("<br/>"));
}

bool HtmlSanitizer::isRejectedElement(const QString &tagName)
{
    static const auto rejectedElements = QSet<QString>{
        QStringLiteral("canvas"), QStringLiteral("iframe"), QStringLiteral("math"),
        QStringLiteral("object"), QStringLiteral("script"), QStringLiteral("style"),
        QStringLiteral("svg"), QStringLiteral("template")};
    return rejectedElements.contains(tagName);
}

bool HtmlSanitizer::isSafeHref(const QString &href)
{
    const auto scheme = QUrl{href.trimmed()}.scheme().toLower();
    return scheme != QStringLiteral("about") && scheme != QStringLiteral("data") && scheme != QStringLiteral("file") &&
           scheme != QStringLiteral("javascript") && scheme != QStringLiteral("qrc") &&
           scheme != QStringLiteral("resource") && scheme != QStringLiteral("vbscript");
}

QString HtmlSanitizer::applySupportedSpanFormatting(const QDomElement &element, QString content)
{
    auto bold = false;
    auto italic = false;
    auto underline = false;
    auto strikeThrough = false;

    const auto declarations = element.attribute(QStringLiteral("style")).split(';', Qt::SkipEmptyParts);
    for (const auto &declaration : declarations)
    {
        const auto separator = declaration.indexOf(':');
        if (separator < 0)
            continue;

        const auto property = declaration.left(separator).trimmed().toLower();
        const auto value = declaration.mid(separator + 1).trimmed().toLower();
        if (property == QStringLiteral("font-weight"))
        {
            bool numericWeight = false;
            const auto weight = value.toInt(&numericWeight);
            bold = bold || value == QStringLiteral("bold") || (numericWeight && weight >= 600);
        }
        else if (property == QStringLiteral("font-style"))
        {
            italic = italic || value == QStringLiteral("italic") || value == QStringLiteral("oblique");
        }
        else if (property == QStringLiteral("text-decoration") || property == QStringLiteral("text-decoration-line"))
        {
            underline = underline || value.contains(QStringLiteral("underline"));
            strikeThrough = strikeThrough || value.contains(QStringLiteral("line-through"));
        }
    }

    if (bold)
        content = QStringLiteral("<b>%1</b>").arg(content);
    if (italic)
        content = QStringLiteral("<i>%1</i>").arg(content);
    if (underline)
        content = QStringLiteral("<u>%1</u>").arg(content);
    if (strikeThrough)
        content = QStringLiteral("<s>%1</s>").arg(content);
    return content;
}

QString HtmlSanitizer::sanitizeElement(const QDomElement &element)
{
    const auto tagName = element.tagName().toLower();
    if (isRejectedElement(tagName))
        return {};

    if (tagName == QStringLiteral("br"))
        return QStringLiteral("<br/>");

    auto content = sanitizeChildren(element);
    if (tagName == QStringLiteral("span"))
        return applySupportedSpanFormatting(element, std::move(content));

    if (tagName == QStringLiteral("a"))
    {
        const auto href = element.attribute(QStringLiteral("href")).trimmed();
        return href.isEmpty() || !isSafeHref(href)
                   ? content
                   : QStringLiteral("<a href=\"%1\">%2</a>").arg(href.toHtmlEscaped(), content);
    }

    if (tagName == QStringLiteral("b") || tagName == QStringLiteral("strong"))
        return QStringLiteral("<b>%1</b>").arg(content);
    if (tagName == QStringLiteral("i") || tagName == QStringLiteral("em"))
        return QStringLiteral("<i>%1</i>").arg(content);
    if (tagName == QStringLiteral("s") || tagName == QStringLiteral("strike") || tagName == QStringLiteral("del"))
        return QStringLiteral("<s>%1</s>").arg(content);
    if (tagName == QStringLiteral("tt"))
        return QStringLiteral("<code>%1</code>").arg(content);

    static const auto allowedElements = QSet<QString>{
        QStringLiteral("blockquote"), QStringLiteral("code"), QStringLiteral("li"), QStringLiteral("ol"),
        QStringLiteral("p"), QStringLiteral("pre"), QStringLiteral("u"), QStringLiteral("ul")};
    if (allowedElements.contains(tagName))
        return QStringLiteral("<%1>%2</%1>").arg(tagName, content);
    if (tagName == QStringLiteral("div"))
        return QStringLiteral("<p>%1</p>").arg(content);

    return content;
}

QString HtmlSanitizer::sanitizeChildren(const QDomNode &node)
{
    auto result = QString{};
    for (auto child = node.firstChild(); !child.isNull(); child = child.nextSibling())
    {
        switch (child.nodeType())
        {
        case QDomNode::CDATASectionNode:
        case QDomNode::TextNode: result.append(child.nodeValue().toHtmlEscaped()); break;
        case QDomNode::ElementNode: result.append(sanitizeElement(child.toElement())); break;
        default: break;
        }
    }
    return result;
}

SanitizedHtmlString HtmlSanitizer::sanitize(const HtmlString &html) const
{
    auto document = QDomDocument{};
    const auto fragment = prepareFragment(html.string());
    if (!document.setContent(QStringLiteral("<kadu-html>%1</kadu-html>").arg(fragment)))
        return SanitizedHtmlString{plainToHtml(html.string()).string()};

    return SanitizedHtmlString{sanitizeChildren(document.documentElement())};
}
