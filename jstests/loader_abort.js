// Test that committing a load leaves the collection in the desired state.

load('jstests/loader_helpers.js');

var test = function() {
    
}();

var testIndexedInsertAbort = function() {
    t = db.loaderindexedinsertabort;
    t.drop();
    begin();
    beginLoad('loaderindexedinsertabort', [ { key: { a: 1 }, name: "a_1" } ], { } );
    t.insert({ a: 700 } );
    abortLoad();
    commit();
    assert.eq(0, t.count({ a: 700 }));
    // No index on a: 1 since we aborted the load (the hint should uassert)
    assert.throws(t.find({a : 700}).hint({ a: 1 }).itcount());
}();

var testUniquenessConstraintAbort = function() {
    t = db.loaderunique;
    t.drop();
    begin();
    beginLoad('loaderunique', [ { key: { a: 1 }, name: "a_1", unique: true } ], { } );
    t.insert({ a: 1 });
    assert(!db.getLastError());
    t.insert({ a: 2 });
    assert(!db.getLastError());
    t.insert({ a: 2 });
    assert(!db.getLastError());
    abortLoad();
    commit();
    assert.eq(0, t.count());

    // Test rollback (should be the same result)
    t.drop();
    begin();
    beginLoad('loaderunique', [ { key: { a: 1 }, name: "a_1", unique: true } ], { } );
    t.insert({ _id: 1 });
    assert(!db.getLastError());
    t.insert({ _id: 2 });
    assert(!db.getLastError());
    t.insert({ _id: 2 });
    assert(!db.getLastError());
    abortLoad();
    rollback();
    assert.eq(0, t.count());
}();
