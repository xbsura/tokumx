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

var testValidIndexes = function() {
    t = db.loadvalidindexes;
    t.drop();
    begin();
    beginLoad('loadvalidindexes',
            [
                { key: { a: 1 }, ns: 'test.loadvalidindexes', name: 'a_1' },
                { key: { c: 1 }, ns: 'test.loadvalidindexes', name: 'c_1', clustering: true },
                { key: { a: 'hashed' }, ns: 'test.loadvalidindexes', name: 'a_hashed' },
                { key: { a: -1, b: -1, c: 1 }, ns: 'test.loadvalidindexes', name: 'a_-1_b-1_c_1', basement_size: 1024 }
            ], { });
    commitLoad();
    commit();
    assert.eq({ _id: 1 }, t.getIndexes()[0].key);
    assert.eq({ a: 1 }, t.getIndexes()[1].key);
    assert.eq('test.loadvalidindexes', t.getIndexes()[1].ns);
    assert.eq('a_1', t.getIndexes()[1].name);

    assert.eq({ c: 1 }, t.getIndexes()[2].key);
    assert.eq('test.loadvalidindexes', t.getIndexes()[2].ns);
    assert.eq('c_1', t.getIndexes()[2].name);
    assert.eq(true, t.getIndexes()[2].clustering);

    assert.eq({ a: 'hashed' }, t.getIndexes()[3].key);
    assert.eq('test.loadvalidindexes', t.getIndexes()[3].ns);
    assert.eq('a_hashed', t.getIndexes()[3].name);

    assert.eq({ a: -1, b: -1, c: 1 }, t.getIndexes()[4].key);
    assert.eq('test.loadvalidindexes', t.getIndexes()[4].ns);
    assert.eq('a_-1_b-1_c_1', t.getIndexes()[4].name);
    assert.eq(1024, t.getIndexes()[4].basement_size);
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

var testIndexedInsert = function() {
    t = db.loaderindexedinsert;
    t.drop();

    begin();
    beginLoad('loaderindexedinsert', [ { key: { a: 1 }, name: "a_1" } ], { } );
    t.insert({ a: 700 } );
    commitLoad();
    commit();
    assert.eq(1, t.count({ a: 700 }));
    assert.eq(1, t.find({a : 700}).hint({ a: 1 }).itcount());
}();
