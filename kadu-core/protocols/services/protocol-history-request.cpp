/*
 * %kadu copyright begin%
 * Copyright 2026 Kadu Qt6 port
 * %kadu copyright end%
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#include "protocol-history-request.h"

Chat ProtocolHistoryRequest::chat() const
{
    return m_chat;
}

void ProtocolHistoryRequest::setChat(const Chat &chat)
{
    m_chat = chat;
}

QString ProtocolHistoryRequest::text() const
{
    return m_text;
}

void ProtocolHistoryRequest::setText(const QString &text)
{
    m_text = text;
}

QDateTime ProtocolHistoryRequest::from() const
{
    return m_from;
}

void ProtocolHistoryRequest::setFrom(const QDateTime &from)
{
    m_from = from;
}

QDateTime ProtocolHistoryRequest::to() const
{
    return m_to;
}

void ProtocolHistoryRequest::setTo(const QDateTime &to)
{
    m_to = to;
}

QByteArray ProtocolHistoryRequest::cursor() const
{
    return m_cursor;
}

void ProtocolHistoryRequest::setCursor(const QByteArray &cursor)
{
    m_cursor = cursor;
}

ProtocolHistoryRequest::Direction ProtocolHistoryRequest::direction() const
{
    return m_direction;
}

void ProtocolHistoryRequest::setDirection(Direction direction)
{
    m_direction = direction;
}

int ProtocolHistoryRequest::limit() const
{
    return m_limit;
}

void ProtocolHistoryRequest::setLimit(int limit)
{
    m_limit = limit;
}

SortedMessages ProtocolHistoryPage::messages() const
{
    return m_messages;
}

void ProtocolHistoryPage::setMessages(const SortedMessages &messages)
{
    m_messages = messages;
}

QByteArray ProtocolHistoryPage::cursor() const
{
    return m_cursor;
}

void ProtocolHistoryPage::setCursor(const QByteArray &cursor)
{
    m_cursor = cursor;
}

bool ProtocolHistoryPage::hasMore() const
{
    return m_hasMore;
}

void ProtocolHistoryPage::setHasMore(bool hasMore)
{
    m_hasMore = hasMore;
}

QString ProtocolHistoryPage::error() const
{
    return m_error;
}

void ProtocolHistoryPage::setError(const QString &error)
{
    m_error = error;
}
