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

#include "protocols/services/account-service.h"
#include "protocols/services/protocol-history-request.h"

#include <QtCore/QFuture>

class Message;

class KADUAPI ProtocolHistoryService : public AccountService
{
    Q_OBJECT

public:
    explicit ProtocolHistoryService(Account account, QObject *parent = nullptr);
    virtual ~ProtocolHistoryService();

    /**
     * @short Return whether local history is enabled for this protocol account.
     *
     * The protocol owns the local history implementation. It must use this method to enforce protocol-specific
     * constraints, for example never retaining decrypted content from an encrypted Matrix room.
     */
    virtual bool isLocalHistoryEnabled() const = 0;

    virtual void storeMessage(const Message &message) = 0;
    virtual QFuture<ProtocolHistoryPage> requestHistory(const ProtocolHistoryRequest &request) = 0;
};
