// histogram.h

/**
*    Copyright (C) 2008 10gen Inc.
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

#ifndef UTIL_HISTOGRAM_HEADER
#define UTIL_HISTOGRAM_HEADER

#include <stdint.h>
#include <string>

namespace mongo {

    using std::string;

    /**
     * A histogram for a 32-bit integer range.
     */
    class Histogram {
    public:
        /**
         * Construct a histogram with 'numBuckets' buckets, each or
         * which 'bucketSize' wide. Optionally have the first bucket
         * start at 'initialValue' rather than 0.
         */
        struct Options {
            uint32_t numBuckets;
            uint32_t bucketSize;
            uint32_t initialValue;

            Options() : numBuckets(0), bucketSize(0), initialValue(0){}
        };
        explicit Histogram( const Options& opts );
        ~Histogram();

        /**
         * Find the bucket that 'element' falls into and increment its count.
         */
        void insert( uint32_t element );

        /**
         * Render the histogram as string that can be used inside an
         * HTML doc.
         */
        string toHTML() const;

        // testing interface below -- consider it private

        /**
         * Return the count for the 'bucket'-th bucket.
         */
        uint64_t getCount( uint32_t bucket ) const;

        /**
         * Return the maximum element that would fall in the
         * 'bucket'-th bucket.
         */
        uint32_t getBoundary( uint32_t bucket ) const;

        /**
         * Return the number of buckets in this histogram.
         */
        uint32_t getBucketsNum() const;
        
    private:
        /**
         * Returns the bucket where 'element' should fall
         * into. Currently assumes that 'element' is greater than the
         * minimum 'inialValue'.
         */
        uint32_t findBucket( uint32_t element ) const;

        uint32_t  _initialValue;  // no value lower than it is recorded
        uint32_t  _numBuckets;    // total buckets in the histogram

        // all below owned here
        uint32_t* _boundaries;    // maximum element of each bucket
        uint64_t* _buckets;       // current count of each bucket

        Histogram( const Histogram& );
        Histogram& operator=( const Histogram& );
    };

}  // namespace mongo

#endif  //  UTIL_HISTOGRAM_HEADER
