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

#pragma once

#include "chat/chat.h"
#include "exports.h"
#include "message/sorted-messages.h"

#include <QtCore/QByteArray>
#include <QtCore/QDateTime>
#include <QtCore/QString>

class KADUAPI ProtocolHistoryRequest
{
public:
    enum class Direction
    {
        Older,
        Newer
    };

    Chat chat() const;
    void setChat(const Chat &chat);

    QString text() const;
    void setText(const QString &text);

    QDateTime from() const;
    void setFrom(const QDateTime &from);

    QDateTime to() const;
    void setTo(const QDateTime &to);

    QByteArray cursor() const;
    void setCursor(const QByteArray &cursor);

    Direction direction() const;
    void setDirection(Direction direction);

    int limit() const;
    void setLimit(int limit);

private:
    Chat m_chat;
    QString m_text;
    QDateTime m_from;
    QDateTime m_to;
    QByteArray m_cursor;
    Direction m_direction = Direction::Older;
    int m_limit = 0;
};

class KADUAPI ProtocolHistoryPage
{
public:
    SortedMessages messages() const;
    void setMessages(const SortedMessages &messages);

    QByteArray cursor() const;
    void setCursor(const QByteArray &cursor);

    bool hasMore() const;
    void setHasMore(bool hasMore);

    QString error() const;
    void setError(const QString &error);

private:
    SortedMessages m_messages;
    QByteArray m_cursor;
    bool m_hasMore = false;
    QString m_error;
};
