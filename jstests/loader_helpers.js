// Common helper functions for loader tests.

function checkError(e, doAssert) {
    if (e && doAssert) {
        assert(!e);
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
    checkError(e);
}
function rollback() {
    db.runCommand({ 'rollbackTransaction' : 1 });
    e = db.getLastError();
    checkError(e);
}

// begin/commit load wrappers that may or may not assert on failure
function _beginLoad(ns, indexes, options, shouldFail) {
    db.runCommand({ 'beginLoad' : 1, 'ns' : ns, 'indexes' : indexes, 'options' : options });
    e = db.getLastError();
    if (e) {
        printjson(e);
    }
    if (!shouldFail) {
        checkError(e);
    }
}
function _commitLoad(shouldFail) {
    db.runCommand({ 'commitLoad' : 1 });
    e = db.getLastError();
    checkError(e, !shouldFail);
}
function _abortLoad(shouldFail) {
    db.runCommand({ 'abortLoad' : 1 });
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
function abortLoad() {
    _abortLoad(false);
}
function abortLoadShouldFail() {
    _abortLoad(true);
}

// No indexes, no options.

