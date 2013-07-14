// Test that committing a load leaves the collection in the desired state.

load('jstests/loader_helpers.js');

var testValidOptions = function() {
    t = db.loadvalidoptions;
    t.drop();

    // Test default options
    begin();
    beginLoad('loadvalidoptions', [ ] , { });
    commitLoad();
    commit();
    assert.eq(null, db.system.namespaces.findOne({ 'name' : 'test.loadvalidoptions' }).options);
    assert.eq(null, t.getIndexes()[0].compression);
    assert.eq('zlib', t.stats().indexDetails[0].compression);

    // Test non-default options
    t.drop();
    begin();
    options = { compression: 'lzma', somethingSpecial: 'special' };
    beginLoad('loadvalidoptions', [ ] , options);
    commitLoad();
    commit();
    assert.eq(options, db.system.namespaces.findOne({ 'name' : 'test.loadvalidoptions' }).options);
    assert.eq(options.compression, t.getIndexes()[0].compression);
    assert.eq(options.compression, t.stats().indexDetails[0].compression);
}();

var testValidIndexes = function() {
    t = db.loadvalidindexes;
    t.drop();
    begin();
    beginLoad('loadvalidindexes', [ { key: { a: 1 }, ns: 'test.loadvalidindexes', name: 'a_1' } ], { });
    assert.eq({ _id: 1 }, t.getIndexes()[0].key);
    assert.eq({ a: 1 }, t.getIndexes()[1].key);
    assert.eq('test.loadvalidindexes', t.getIndexes()[1].ns);
    assert.eq('a_1', t.getIndexes()[1].name);
    commitLoad();
    commit();
}();

// Test that the ns field gets inherited in each index spec object
// when it isn't provided.
var testIndexSpecNs = function() {
    t = db.loadindexspecns;
    t.drop();
    begin();
    beginLoad('loadindexspecns', [ { key: { a: 1 }, name: 'a_1' }, { key: { foo : -1 }, name: 'foobar' } ], { });
    commitLoad();
    commit();
    assert.eq('test.loadindexspecns', t.getIndexes()[0].ns);
    assert.eq('test.loadindexspecns', t.getIndexes()[1].ns);
    assert.eq('test.loadindexspecns', t.getIndexes()[2].ns);
}();

var testBackToBackLoads = function() {
    t1 = db.testb2bloads1;
    t2 = db.testb2bloads2;
    t1.drop();
    t2.drop();
    begin();
    beginLoad('testb2bloads1', [ ], { });
    commitLoad();
    beginLoad('testb2bloads2', [ ], { });
    commitLoad();
    commit();
    assert.eq(1, db.system.namespaces.count({ "name" : "test.testb2bloads1" }));
    assert.eq(1, db.system.namespaces.count({ "name" : "test.testb2bloads2" }));

    // Test rollback
    t1.drop();
    t2.drop();
    begin();
    beginLoad('testb2bloads1', [ ], { });
    commitLoad();
    beginLoad('testb2bloads2', [ ], { });
    commitLoad();
    rollback();
    assert.eq(0, db.system.namespaces.count({ "name" : "test.testb2bloads1" }));
    assert.eq(0, db.system.namespaces.count({ "name" : "test.testb2bloads2" }));
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
