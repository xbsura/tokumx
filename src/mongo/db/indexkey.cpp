// index_key.cpp

/**
*    Copyright (C) 2008 10gen Inc.
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
#include "mongo/db/index.h"
#include "mongo/db/background.h"
#include "mongo/db/jsobj.h"
#include "mongo/db/queryutil.h"
#include "mongo/util/stringutils.h"
#include "mongo/util/mongoutils/str.h"
#include "mongo/util/text.h"

namespace mongo {

    map<string,IndexPlugin*> * IndexPlugin::_plugins;

    IndexType::IndexType( const IndexPlugin * plugin , const IndexSpec * spec )
        : _plugin( plugin ) , _spec( spec ) {

    }

    IndexType::~IndexType() {
    }

    const BSONObj& IndexType::keyPattern() const {
        return _spec->keyPattern;
    }

    IndexPlugin::IndexPlugin( const string& name )
        : _name( name ) {
        if ( ! _plugins )
            _plugins = new map<string,IndexPlugin*>();
        (*_plugins)[name] = this;
    }

    string IndexPlugin::findPluginName( const BSONObj& keyPattern ) {
        string pluginName = "";

        BSONObjIterator i( keyPattern );

        while( i.more() ) {
            BSONElement e = i.next();
            if ( e.type() != String )
                continue;

            uassert( 13007 , "can only have 1 index plugin / bad index key pattern" , pluginName.size() == 0 || pluginName == e.String() );
            pluginName = e.String();
        }

        return pluginName;
    }

    int IndexType::compare( const BSONObj& l , const BSONObj& r ) const {
        return l.woCompare( r , _spec->keyPattern );
    }

    void IndexSpec::_init() {
        verify( keyPattern.objsize() );

        // some basics
        _nFields = keyPattern.nFields();
        _sparse = info["sparse"].trueValue();
        uassert( 13529 , "sparse only works for single field keys" , ! _sparse || _nFields );


        {
            // build _nullKey

            BSONObjBuilder b;
            BSONObjIterator i( keyPattern );

            while( i.more() ) {
                BSONElement e = i.next();
                _fieldNames.push_back( e.fieldName() );
                _fixed.push_back( BSONElement() );
                b.appendNull( "" );
            }
            _nullKey = b.obj();
        }

        {
            // _nullElt
            BSONObjBuilder b;
            b.appendNull( "" );
            _nullObj = b.obj();
            _nullElt = _nullObj.firstElement();
        }

        {
            // _undefinedElt
            BSONObjBuilder b;
            b.appendUndefined( "" );
            _undefinedObj = b.obj();
            _undefinedElt = _undefinedObj.firstElement();
        }
        
        {
            // handle plugins
            string pluginName = IndexPlugin::findPluginName( keyPattern );
            if ( pluginName.size() ) {
                IndexPlugin * plugin = IndexPlugin::get( pluginName );
                if ( ! plugin ) {
                    log() << "warning: can't find plugin [" << pluginName << "]" << endl;
                }
                else {
                    _indexType.reset( plugin->generate( this ) );
                }
            }
        }

        _finishedInit = true;
    }

    void assertParallelArrays( const char *first, const char *second ) {
        stringstream ss;
        ss << "cannot index parallel arrays [" << first << "] [" << second << "]";
        uasserted( ParallelArraysCode ,  ss.str() );
    }

    class KeyGenerator {
    public:
        KeyGenerator( const IndexSpec &spec ) : _spec( spec ) {}
        
        void getKeys( const BSONObj &obj, BSONObjSet &keys ) const {
            if ( _spec._indexType.get() ) { //plugin (eg geo)
                _spec._indexType->getKeys( obj , keys );
                return;
            }
            vector<const char*> fieldNames( _spec._fieldNames );
            vector<BSONElement> fixed( _spec._fixed );
            _getKeys( fieldNames , fixed , obj, keys );
            if ( keys.empty() && ! _spec._sparse )
                keys.insert( _spec._nullKey );
        }     
        
    private:
        /**
         * @param arrayNestedArray - set if the returned element is an array nested directly within arr.
         */
        BSONElement extractNextElement( const BSONObj &obj, const BSONObj &arr, const char *&field, bool &arrayNestedArray ) const {
            string firstField = mongoutils::str::before( field, '.' );
            bool haveObjField = !obj.getField( firstField ).eoo();
            BSONElement arrField = arr.getField( firstField );
            bool haveArrField = !arrField.eoo();

            // An index component field name cannot exist in both a document array and one of that array's children.
            uassert( 15855 ,  mongoutils::str::stream() << "Ambiguous field name found in array (do not use numeric field names in embedded elements in an array), field: '" << arrField.fieldName() << "' for array: " << arr, !haveObjField || !haveArrField );

            arrayNestedArray = false;
			if ( haveObjField ) {
                return obj.getFieldDottedOrArray( field );
            }
            else if ( haveArrField ) {
                if ( arrField.type() == Array ) {
                    arrayNestedArray = true;
                }
                return arr.getFieldDottedOrArray( field );
            }
            return BSONElement();
        }
        
        void _getKeysArrEltFixed( vector<const char*> &fieldNames , vector<BSONElement> &fixed , const BSONElement &arrEntry, BSONObjSet &keys, int numNotFound, const BSONElement &arrObjElt, const set< unsigned > &arrIdxs, bool mayExpandArrayUnembedded ) const {
            // set up any terminal array values
            for( set<unsigned>::const_iterator j = arrIdxs.begin(); j != arrIdxs.end(); ++j ) {
                if ( *fieldNames[ *j ] == '\0' ) {
                    fixed[ *j ] = mayExpandArrayUnembedded ? arrEntry : arrObjElt;
                }
            }
            // recurse
            _getKeys( fieldNames, fixed, ( arrEntry.type() == Object ) ? arrEntry.embeddedObject() : BSONObj(), keys, numNotFound, arrObjElt.embeddedObject() );        
        }
        
        /**
         * @param fieldNames - fields to index, may be postfixes in recursive calls
         * @param fixed - values that have already been identified for their index fields
         * @param obj - object from which keys should be extracted, based on names in fieldNames
         * @param keys - set where index keys are written
         * @param numNotFound - number of index fields that have already been identified as missing
         * @param array - array from which keys should be extracted, based on names in fieldNames
         *        If obj and array are both nonempty, obj will be one of the elements of array.
         */        
        void _getKeys( vector<const char*> fieldNames , vector<BSONElement> fixed , const BSONObj &obj, BSONObjSet &keys, int numNotFound = 0, const BSONObj &array = BSONObj() ) const {
            BSONElement arrElt;
            set<unsigned> arrIdxs;
            bool mayExpandArrayUnembedded = true;
            for( unsigned i = 0; i < fieldNames.size(); ++i ) {
                if ( *fieldNames[ i ] == '\0' ) {
                    continue;
                }
                
                bool arrayNestedArray;
                // Extract element matching fieldName[ i ] from object xor array.
                BSONElement e = extractNextElement( obj, array, fieldNames[ i ], arrayNestedArray );
                
                if ( e.eoo() ) {
                    // if field not present, set to null
                    fixed[ i ] = _spec._nullElt;
                    // done expanding this field name
                    fieldNames[ i ] = "";
                    numNotFound++;
                }
                else if ( e.type() == Array ) {
                    arrIdxs.insert( i );
                    if ( arrElt.eoo() ) {
                        // we only expand arrays on a single path -- track the path here
                        arrElt = e;
                    }
                    else if ( e.rawdata() != arrElt.rawdata() ) {
                        // enforce single array path here
                        assertParallelArrays( e.fieldName(), arrElt.fieldName() );
                    }
                    if ( arrayNestedArray ) {
                        mayExpandArrayUnembedded = false;   
                    }
                }
                else {
                    // not an array - no need for further expansion
                    fixed[ i ] = e;
                }
            }
            
            if ( arrElt.eoo() ) {
                // No array, so generate a single key.
                if ( _spec._sparse && numNotFound == _spec._nFields ) {
                    return;
                }            
                BSONObjBuilder b(_spec._sizeTracker);
                for( vector< BSONElement >::iterator i = fixed.begin(); i != fixed.end(); ++i ) {
                    b.appendAs( *i, "" );
                }
                keys.insert( b.obj() );
            }
            else if ( arrElt.embeddedObject().firstElement().eoo() ) {
                // Empty array, so set matching fields to undefined.
                _getKeysArrEltFixed( fieldNames, fixed, _spec._undefinedElt, keys, numNotFound, arrElt, arrIdxs, true );
            }
            else {
                // Non empty array that can be expanded, so generate a key for each member.
                BSONObj arrObj = arrElt.embeddedObject();
                BSONObjIterator i( arrObj );
                while( i.more() ) {
                    _getKeysArrEltFixed( fieldNames, fixed, i.next(), keys, numNotFound, arrElt, arrIdxs, mayExpandArrayUnembedded );
                }
            }
        }
        
        const IndexSpec &_spec;
    };
    
    void IndexSpec::getKeys( const BSONObj &obj, BSONObjSet &keys ) const {
        KeyGenerator g( *this );
        g.getKeys( obj, keys );
    }


    IndexSuitability IndexSpec::suitability( const FieldRangeSet& queryConstraints ,
                                             const BSONObj& order ) const {
        if ( _indexType.get() )
            return _indexType->suitability( queryConstraints , order );
        return _suitability( queryConstraints , order );
    }

    IndexSuitability IndexSpec::_suitability( const FieldRangeSet& queryConstraints ,
                                              const BSONObj& order ) const {
        // This is a quick first pass to determine the suitability of the index.  It produces some
        // false positives (returns HELPFUL for some indexes which are not particularly). When we
        // return HELPFUL a more precise determination of utility is done by the query optimizer.

        // check whether any field in the index is constrained at all by the query
        BSONForEach( elt, keyPattern ){
            const FieldRange& frange = queryConstraints.range( elt.fieldName() );
            if( ! frange.universal() )
                return HELPFUL;
        }
        // or whether any field in the desired sort order is in the index
        set<string> orderFields;
        order.getFieldNames( orderFields );
        BSONForEach( k, keyPattern ) {
            if ( orderFields.find( k.fieldName() ) != orderFields.end() )
                return HELPFUL;
        }
        return USELESS;
    }

    IndexSuitability IndexType::suitability( const FieldRangeSet& queryConstraints ,
                                             const BSONObj& order ) const {
        return _spec->_suitability( queryConstraints , order );
    }
    
    bool IndexType::scanAndOrderRequired( const BSONObj& query , const BSONObj& order ) const {
        return ! order.isEmpty();
    }

}
