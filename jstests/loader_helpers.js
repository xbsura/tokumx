// Common helper functions for loader tests.

function checkError(e, doAssert) {
    if (e) {
        //printjson(e);
        if (doAssert) {
            assert(!e);
        }
    }
}
function begin() {
    db.runCommand({ 'beginTransaction' : 1 });
    e = db.getLastError();
    checkError(e, true);
}
function commit() {
    db.runCommand({ 'commitTransaction' : 1 });
    e = db.getLastError();
    checkError(e, true);
}
function rollback() {
    db.runCommand({ 'rollbackTransaction' : 1 });
    e = db.getLastError();
    checkError(e, true);
}

// begin/commit load wrappers that may or may not assert on failure
function _beginLoad(ns, indexes, options, shouldFail) {
    db.runCommand({ 'beginLoad' : 1, 'ns' : ns, 'indexes' : indexes, 'options' : options });
    e = db.getLastError();
    if (e) {
        printjson(e);
    }
    checkError(e, !shouldFail);
}
function _commitLoad(shouldFail) {
    db.runCommand({ 'commitLoad' : 1 });
    e = db.getLastError();
    checkError(e, !shouldFail);
}
function beginLoad(ns, indexes, options) {
    _beginLoad(ns, indexes, options, false);
}
function beginLoadShouldFail(ns, indexes, options) {
    _beginLoad(ns, indexes, options, true);
}
function commitLoad() {
    _commitLoad(false);
}
function commitLoadShouldFail() {
    _commitLoad(true);
}

// No indexes, no options.

