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

    class BeginLoadCmd : public InformationCommand {
    public:
        virtual bool adminOnly() const { return false; }
        virtual bool requiresAuth() { return true; }
        virtual LockType locktype() const { return WRITE; }
        virtual void help( stringstream& help ) const {
            help << "begin load" << endl << 
                "Begin a bulk load into a collection." << endl <<
                "Must be inside an existing multi-statement transaction." << endl <<
                "{ beginLoad: ns, indexes: [ { ... }, ... ], options: { ... }  }" << endl;
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

            const string ns = cmdObj["beginLoad"].String(); 
            vector<BSONObj> indexes;
            vector<BSONElement> indexElements = cmdObj["indexes"].Array();
            for (vector<BSONElement>::const_iterator i = indexElements.begin(); i != indexElements.end(); i++) {
                BSONObj obj = i->Obj();
                indexes.push_back(obj.copy());
            }
            cc().beginClientLoad(ns, indexes, cmdObj["options"].Obj());
            result.append("status", "load began");
            result.append("ok", true);
            return true;
        }
    } beginLoadCmd;

    class CommitLoadCmd : public InformationCommand {
    public:
        virtual bool adminOnly() const { return false; }
        virtual bool requiresAuth() { return true; }
        virtual LockType locktype() const { return WRITE; }
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

    class AbortLoadCmd : public InformationCommand {
    public:
        virtual bool adminOnly() const { return false; }
        virtual bool requiresAuth() { return true; }
        virtual LockType locktype() const { return WRITE; }
        virtual void help( stringstream& help ) const {
            help << "abort load" << endl <<
                "Aborts a load in progress." << endl <<
                "{ abortLoad }" << endl;
        }
        AbortLoadCmd() : InformationCommand("abortLoad") {}

        virtual bool run(const string& db, 
                         BSONObj& cmdObj, 
                         int options, 
                         string& errmsg, 
                         BSONObjBuilder& result, 
                         bool fromRepl) 
        {
            cc().abortClientLoad();
            result.append("status", "load aborted");
            result.append("ok", true);
            return true;
        }
    } abortLoadCmd;
}
