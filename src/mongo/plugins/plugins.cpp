// @file plugin.cpp

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

// TODO: feature detection to disable plugin code if we don't have dlopen/dlsym
#include <dlfcn.h>

#include "mongo/db/commands.h"
#include "mongo/plugins/plugins.h"
#include "mongo/util/log.h"

namespace mongo {

    namespace plugin {

        class DLHandle : boost::noncopyable {
            void *_h;
          public:
            void *handle() const { return _h; }
            DLHandle(void *h) : _h(h) {}
            ~DLHandle() {
                int r = dlclose(_h);
                if (r != 0) {
                    log() << "error in dlclose: " << r << endl;
                }
            }
        };

        class PluginHandle : boost::noncopyable {
            const string _filename;
            scoped_ptr<DLHandle> _dlHandle;
            PluginInterface *_interface;

            void reportDlerror(string &errmsg) {
                char *err = dlerror();
                stringstream errs;
                errs << "error loading plugin from file " << _filename << ": ";
                if (err == NULL) {
                    errs << "unknown error";
                }
                else {
                    errs << err;
                }
                errmsg += errs.str();
            }

          public:
            explicit PluginHandle(const string &filename) : _filename(filename) {}

            const string &name() const { return _interface->name(); }

            bool load(string &errmsg, BSONObjBuilder &result) {
                void *handle = dlopen(_filename.c_str(), RTLD_NOW);
                if (handle == NULL) {
                    reportDlerror(errmsg);
                    return false;
                }
                _dlHandle.reset(new DLHandle(handle));
                GetInterfaceFunc getInterface = reinterpret_cast<GetInterfaceFunc>(dlsym(_dlHandle->handle(), "getInterface"));
                if (getInterface == NULL) {
                    reportDlerror(errmsg);
                    return false;
                }
                PluginInterface *interfacep = getInterface();
                if (interfacep == NULL) {
                    errmsg += "couldn't load plugin from " + _filename;
                    return false;
                }
                _interface = interfacep;
                bool ok = _interface->load(errmsg, result);
                if (ok) {
                    LOG(0) << "Loaded plugin " << name() << " from " << _filename << endl;
                }
                return ok;
            }

            bool unload(string &errmsg) {
                string nameCopy = name();
                _interface->unload(errmsg);
                _interface = NULL;
                _dlHandle.reset();
                LOG(0) << "Unloaded plugin " << nameCopy << " from " << _filename << endl;
                return true;
            }
        };

        class Loader : boost::noncopyable {
            typedef map<string, shared_ptr<PluginHandle> > PluginMap;
            PluginMap _plugins;
          public:
            bool load(const StringData &filename, string &errmsg, BSONObjBuilder &result) {
                shared_ptr<PluginHandle> pluginHandle(new PluginHandle(filename.toString()));
                bool ok = pluginHandle->load(errmsg, result);
                if (!ok) {
                    return false;
                }
                const string &name = pluginHandle->name();
                _plugins[name] = pluginHandle;
                result.append("loaded", name);
                return true;
            }
            bool unload(const StringData &name, string &errmsg, BSONObjBuilder &result) {
                PluginMap::iterator it = _plugins.find(name.toString());
                if (it == _plugins.end()) {
                    errmsg = "plugin '" + name.toString() + "' not found";
                    return false;
                }
                shared_ptr<PluginHandle> pluginHandle = it->second;
                _plugins.erase(it);
                pluginHandle->unload(errmsg);
                return true;
            }
        } loader;

    }  // namespace plugin

    /**
     * Load a plugin into mongod.
     */
    class LoadPluginCommand : public InformationCommand {
      public:
        // Be strict for now, relax later if we need to.
        virtual LockType locktype() const { return WRITE; }
        virtual bool lockGlobally() const { return true; }
        virtual bool requiresAuth() { return true; }
        virtual bool adminOnly() const { return true; }
        virtual void help(stringstream &h) const {
            h << "Load a plugin into mongod." << endl
              << "{ loadPlugin : <name> [, filename: <filename>] }" << endl;
        }
        LoadPluginCommand() : InformationCommand("loadPlugin", false) {}
        
        virtual bool run(const string &db, BSONObj &cmdObj, int options, string &errmsg, BSONObjBuilder &result, bool fromRepl) {
            if (db != "admin") {
                errmsg = "loadPlugin must be run on the admin db";
                return false;
            }
            BSONElement filenameElt = cmdObj["filename"];
            if (filenameElt.ok()) {
                if (filenameElt.type() != String) {
                    errmsg = "filename argument must be a string";
                    return false;
                }
                StringData filename = filenameElt.Stringdata();
                if (filename.empty()) {
                    errmsg = "filename argument must not be empty";
                    return false;
                }
                return plugin::loader.load(filename, errmsg, result);
            }
            else {
                BSONElement e = cmdObj.firstElement();
                if (e.type() != String) {
                    errmsg = "loadPlugin argument must be a string";
                    return false;
                }
                string name = e.str();
                if (name.empty()) {
                    errmsg = "loadPlugin argument must not be empty";
                    return false;
                }
                string filename = mongoutils::str::stream() << "lib" << name << ".so";
                result.append("filename", filename);
                return plugin::loader.load(filename, errmsg, result);
            }
        }
    } loadPluginCommand;

    /**
     * Unload a plugin from mongod.
     */
    class UnloadPluginCommand : public InformationCommand {
      public:
        // Be strict for now, relax later if we need to.
        virtual LockType locktype() const { return WRITE; }
        virtual bool lockGlobally() const { return true; }
        virtual bool requiresAuth() { return true; }
        virtual bool adminOnly() const { return true; }
        virtual void help(stringstream &h) const {
            h << "Unload a plugin from mongod." << endl
              << "{ unloadPlugin : <name> }" << endl;
        }
        UnloadPluginCommand() : InformationCommand("unloadPlugin", false) {}
        
        virtual bool run(const string &db, BSONObj &cmdObj, int options, string &errmsg, BSONObjBuilder &result, bool fromRepl) {
            if (db != "admin") {
                errmsg = "unloadPlugin must be run on the admin db";
                return false;
            }
            BSONElement e = cmdObj.firstElement();
            if (e.type() != String) {
                errmsg = "unloadPlugin argument must be a string";
                return false;
            }
            StringData name = e.Stringdata();
            if (name.empty()) {
                errmsg = "unloadPlugin argument must not be empty";
                return false;
            }
            return plugin::loader.unload(name, errmsg, result);
        }
    } unloadPluginCommand;

}  // namespace mongo

