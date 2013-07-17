// Test that the rules are properly enforced during load.

load("jstests/loader_helpers.js");

var testOtherClientsGetRejected = function() {
    t = db.loaderclientreject;
    t2 = db.loaderclientreject2;
    t.drop();
    t2.drop();
    begin();
    beginLoad('loaderclientreject', [ ], { });
    s = startParallelShell('db.loaderclientreject.count(); assert(db.getLastError()); db.loaderclientreject2.insert({ ok: 1 });');
    s();
    s = startParallelShell('db.loaderclientreject.insert({}); assert(db.getLastError()); db.loaderclientreject2.insert({ ok: 1 });');
    s();
    abortLoad();
    rollback();
    assert.eq(0, t.count());
    assert.eq(2, t2.count());
}();
