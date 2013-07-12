// Test that a simple commit or abort load works.

load("jstests/loader_helpers.js");

var testNoTransaction = function() {
    t = db.loadnotxn;
    t.drop();
    beginLoadShouldFail('loadnotxn', [ ] , { });
}

var testSimpleCommit = function() {
    t = db.loadcommit;
    t.drop();
    begin();
    beginLoad('loadcommit', [ ] , { });
    commitLoad();
    commit();
    assert.eq(1, db.system.namespaces.count({ "name" : "test.loadcommit" }));
}();

var testSimpleAbort = function() {
    t = db.loadabort;
    t.drop();
    begin();
    beginLoad('loadabort', [ ] , { });
    rollback();
    assert.eq(0, db.system.namespaces.count({ "name" : "test.loadabort" }));
    t.insert({});
    assert.eq(1, t.count()); // should be re-created by insert aborted load
}();

