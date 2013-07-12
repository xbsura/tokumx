// client_load.cpp

/**
*    Copyright (C) 2013 Tokutek Inc.
*
*    This program is free software: you can redistribute it and/or  modify
*    it under the terms of the GNU Affero General Public License, version 3,
*    as published by the Free Software Foundation.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU Affero General Public License for more details.
*
*    You should have received a copy of the GNU Affero General Public License
*    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/**
 * Implements collection loading for a client.
 */

#include "pch.h"

#include "mongo/db/client.h"
#include "mongo/db/namespace_details.h"

namespace mongo {

    // The client begin/commit/abort load functions handles locking, context,
    // and ensuring that only one load exists at a time for this client.

    void Client::beginClientLoad(const StringData &ns, const vector<BSONObj> &indexes,
                                 const BSONObj &options) {
        uassert( 16864, "Cannot begin load, one is already in progress",
                        _bulkLoadNS.empty() );

        Client::WriteContext ctx(ns);
        beginBulkLoad(ns, indexes, options);
        _bulkLoadNS = ns.toString();
    }

    void Client::commitClientLoad() {
        uassert( 16876, "Cannot commit client load, none in progress.",
                        !_bulkLoadNS.empty() );
        const string ns = _bulkLoadNS;
        _bulkLoadNS.clear();

        verify(cc().hasTxn());
        Client::WriteContext ctx(ns);
        commitBulkLoad(ns);
    }

} // namespace mongo
