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

#include "mongo/pch.h"
#include "mongo/db/commands.h"
#include "mongo/db/client.h"
#include "mongo/db/repl/rs.h"

namespace mongo {

    // TODO: Do we need to catch uasserts, set ok = false, and then rethrow?
    // TODO: Is InformationCommand the interface we want to inherit from?

    class BeginLoadCmd : public InformationCommand {
    public:
        virtual bool adminOnly() const { return false; }
        virtual bool requiresAuth() { return true; }
        virtual LockType locktype() const { return OPLOCK; }
        virtual bool needsTxn() const { return false; }
        virtual bool logTheOp() { return true; }
        virtual bool slaveOk() const { return false; }
        virtual void help( stringstream& help ) const {
            help << "begin load" << endl << 
                "Begin a bulk load into a collection." << endl <<
                "Must be inside an existing multi-statement transaction." << endl <<
                "{ beginLoad: 1, ns : collName, indexes: [ { ... }, ... ], options: { ... }  }" << endl;
        }
        BeginLoadCmd() : InformationCommand("beginLoad") {}

        virtual bool run(const string& db, 
                         BSONObj& cmdObj, 
                         int options, 
                         string& errmsg, 
                         BSONObjBuilder& result, 
                         bool fromRepl) 
        {
            uassert( 16863, "Must be in a multi-statement transaction to begin a load.",
                            cc().hasTxn());
            uassert( 16882, "The ns field must be a string.",
                            cmdObj["ns"].type() == mongo::String );
            uassert( 16883, "The indexes field must be an array of index objects.",
                            cmdObj["indexes"].type() == mongo::Array );
            uassert( 16884, "The options field must be an object.",
                            !cmdObj["options"].ok() || cmdObj["options"].type() == mongo::Object );
            LOG(0) << "Beginning bulk load, cmd: " << cmdObj << endl;

            vector<BSONElement> indexElements = cmdObj["indexes"].Array();
            vector<BSONObj> indexes;
            for (vector<BSONElement>::const_iterator i = indexElements.begin(); i != indexElements.end(); i++) {
                uassert( 16885, "Each index spec must be an object describing the index to be built",
                                i->type() == mongo::Object );

                BSONObj obj = i->Obj();
                indexes.push_back(obj.copy());
            }

            Client::Transaction transaction(DB_SERIALIZABLE);
            const string ns = db + "." + cmdObj["ns"].String();
            cc().beginClientLoad(ns, indexes, cmdObj["options"].Obj());
            result.append("status", "load began");
            result.append("ok", true);
            transaction.commit();
            return true;
        }
    } beginLoadCmd;

    class CommitLoadCmd : public InformationCommand {
    public:
        virtual bool adminOnly() const { return false; }
        virtual bool requiresAuth() { return true; }
        virtual LockType locktype() const { return OPLOCK; }
        virtual bool needsTxn() const { return false; }
        virtual bool logTheOp() { return true; }
        virtual bool slaveOk() const { return false; }
        virtual void help( stringstream& help ) const {
            help << "commit load" << endl <<
                "Commits a load in progress." << endl <<
                "{ commitLoad }" << endl;
        }
        CommitLoadCmd() : InformationCommand("commitLoad") {}

        virtual bool run(const string& db, 
                         BSONObj& cmdObj, 
                         int options, 
                         string& errmsg, 
                         BSONObjBuilder& result, 
                         bool fromRepl) 
        {
            cc().commitClientLoad();
            result.append("status", "load committed");
            result.append("ok", true);
            return true;
        }
    } commitLoadCmd;
}
