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

#include "protocol-history-service.h"
#include "protocol-history-service.moc"

ProtocolHistoryService::ProtocolHistoryService(Account account, QObject *parent) : AccountService{account, parent}
{
}

ProtocolHistoryService::~ProtocolHistoryService()
{
}
