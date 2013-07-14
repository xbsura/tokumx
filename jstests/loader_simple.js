// Test that a simple commit or abort load works.

load("jstests/loader_helpers.js");

var testCommitLoadNoBegin = function() {
    commitLoadShouldFail();
}();

var testAbortLoadNoBegin = function() {
    abortLoadShouldFail();
}();

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
    abortLoad();
    commit();
    assert.eq(0, db.system.namespaces.count({ "name" : "test.loadabort" }));
    t.insert({});
    assert.eq(1, t.count()); // should be re-created by insert aborted load
}();

var testSimpleRollback = function() {
    t = db.loadrollback;
    t.drop();
    begin();
    beginLoad('loadrollback', [ ] , { });
    commitLoad();
    rollback();
    assert.eq(0, db.system.namespaces.count({ "name" : "test.loadrollback" }));

    t.drop();
    begin();
    beginLoad('loadrollback', [ ] , { });
    abortLoad();
    rollback();
    assert.eq(0, db.system.namespaces.count({ "name" : "test.loadrollback" }));
}();

var testCommitCommit = function() {
    t1 = db.loadcommit1;
    t2 = db.loadcommit2;
    t1.drop();
    t2.drop();
    begin();
    beginLoad('loadcommit1', [ ], { });
    commitLoad();
    beginLoad('loadcommit2', [ ], { });
    commitLoad();
    commit();
    assert.eq(1, db.system.namespaces.count({ "name" : "test.loadcommit1" }));
    assert.eq(1, db.system.namespaces.count({ "name" : "test.loadcommit2" }));

    // Test rollback
    t1.drop();
    t2.drop();
    begin();
    beginLoad('testb2bloads1', [ ], { });
    commitLoad();
    beginLoad('testb2bloads2', [ ], { });
    commitLoad();
    rollback();
    assert.eq(0, db.system.namespaces.count({ "name" : "test.loadcommit1" }));
    assert.eq(0, db.system.namespaces.count({ "name" : "test.loadcommit2" }));
}();

var testAbortAbort = function() {
    t1 = db.loadabort1;
    t2 = db.loadabort2;
    t1.drop();
    t2.drop();
    begin();
    beginLoad('loadabort1', [ ], { });
    abortLoad();
    beginLoad('loadabort2', [ ], { });
    abortLoad();
    commit();
    assert.eq(0, db.system.namespaces.count({ "name" : "test.loadabort1" }));
    assert.eq(0, db.system.namespaces.count({ "name" : "test.loadabort2" }));

    // Test rollback
    t1.drop();
    t2.drop();
    begin();
    beginLoad('loadabort1', [ ], { });
    abortLoad();
    beginLoad('loadabort2', [ ], { });
    abortLoad();
    rollback();
    assert.eq(0, db.system.namespaces.count({ "name" : "test.loadabort1" }));
    assert.eq(0, db.system.namespaces.count({ "name" : "test.loadabort2" }));
}();

var testCommitAbort = function() {
    t1 = db.loadcommitabort1;
    t2 = db.loadcommitabort2;
    t1.drop();
    t2.drop();
    begin();
    beginLoad('loadcommitabort1', [ ], { });
    commitLoad();
    beginLoad('loadcommitabort2', [ ], { });
    abortLoad();
    commit();
    assert.eq(1, db.system.namespaces.count({ "name" : "test.loadcommitabort1" }));
    assert.eq(0, db.system.namespaces.count({ "name" : "test.loadcommitabort2" }));

    // Test rollback
    t1.drop();
    t2.drop();
    begin();
    beginLoad('loadcommitabort1', [ ], { });
    commitLoad();
    beginLoad('loadcommitabort2', [ ], { });
    abortLoad();
    rollback();
    assert.eq(0, db.system.namespaces.count({ "name" : "test.loadcommitabort1" }));
    assert.eq(0, db.system.namespaces.count({ "name" : "test.loadcommitabort2" }));
}();

var testAbortCommit = function() {
    t1 = db.loadabortcommit1;
    t2 = db.loadabortcommit2;
    t1.drop();
    t2.drop();
    begin();
    beginLoad('loadabortcommit1', [ ], { });
    abortLoad();
    beginLoad('loadabortcommit2', [ ], { });
    commitLoad();
    commit();
    assert.eq(0, db.system.namespaces.count({ "name" : "test.loadabortcommit1" }));
    assert.eq(1, db.system.namespaces.count({ "name" : "test.loadabortcommit2" }));

    // Test rollback
    t1.drop();
    t2.drop();
    begin();
    beginLoad('loadabortcommit1', [ ], { });
    commitLoad();
    beginLoad('loadabortcommit2', [ ], { });
    abortLoad();
    rollback();
    assert.eq(0, db.system.namespaces.count({ "name" : "test.loadabortcommit1" }));
    assert.eq(0, db.system.namespaces.count({ "name" : "test.loadabortcommit2" }));
}();

var testSimpleInsert = function() {
    t = db.loadsimpleinsert;
    t.drop();
    begin();
    beginLoad('loadsimpleinsert', [ ], { });
    t.insert({ bulkLoaded: 1 });
    commitLoad();
    commit();
    assert.eq(1, t.count());
    assert.eq(1, t.count({ bulkLoaded: 1 }));
    
    // Test rollback
    t.drop();
    begin();
    beginLoad('loadsimpleinsert', [ ], { });
    t.insert({ bulkLoaded: 1 });
    commitLoad();
    rollback();
    assert.eq(0, t.count());
    assert.eq(0, t.count({ bulkLoaded: 1 }));
}();

