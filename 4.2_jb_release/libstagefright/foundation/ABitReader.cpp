/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ABitReader.h"

#include <media/stagefright/foundation/ADebug.h>

namespace android {

ABitReader::ABitReader(const uint8_t *data, size_t size)
    : mData(data),
      mSize(size),
      mReservoir(0),
      mNumBitsLeft(0) {
}

void ABitReader::fillReservoir() {
    CHECK_GT(mSize, 0u);

    if (mSize >= 4)
    {
        #define PACK_LE(x)      (((uint32_t)x[0] << 24)|((uint32_t)x[1] << 16)|((uint32_t)x[2] << 8)|((uint32_t)x[3]))
        mReservoir = PACK_LE(mData);
        mData += 4;
        mSize -= 4;
        mNumBitsLeft = 32;
    }
    else
    {
    mReservoir = 0;
    size_t i;
    for (i = 0; mSize > 0 && i < 4; ++i) {
        mReservoir = (mReservoir << 8) | *mData;

        ++mData;
        --mSize;
    }

    mNumBitsLeft = 8 * i;
    mReservoir <<= 32 - mNumBitsLeft;
    }
}

uint32_t ABitReader::getBits(size_t n) {
    CHECK_LE(n, 32u);

    uint32_t result = 0;
    while (n > 0) {
        if (mNumBitsLeft == 0) {
            fillReservoir();
        }

        size_t m = n;
        if (m > mNumBitsLeft) {
            m = mNumBitsLeft;
        }

        result = (result << m) | (mReservoir >> (32 - m));
        mReservoir <<= m;
        mNumBitsLeft -= m;

        n -= m;
    }

    return result;
}

uint32_t ABitReader::showBits(size_t n) {
    CHECK_LE(n, 32u);
    uint32_t result = 0;
    if(n <= mNumBitsLeft)
    {
        result = (result << n) | (mReservoir >> (32 - n));
    }
    else
    {
        int m = n - mNumBitsLeft;
        uint32_t temp = 0;
        result = (result << mNumBitsLeft) | (mReservoir >> (32 - mNumBitsLeft));
        if (mSize >= 4)
        {
            #define PACK_LE(x)      (((uint32_t)x[0] << 24)|((uint32_t)x[1] << 16)|((uint32_t)x[2] << 8)|((uint32_t)x[3]))
            temp = PACK_LE(mData);
        }
        else
        {
            temp = 0;
            uint8_t *mTempData =(uint8_t *)mData;
            size_t i,j;
            j = mSize;
            for (i = 0; j > 0 && i < 4; ++i) {
                temp = (temp << 8) | *mTempData;
                ++mTempData;
                j --;
            }
        }
        result = (result << m) | (temp >> (32 - m));
    }
    return result;
}
void ABitReader::skipBits(size_t n) {
    while (n > 32) {
        getBits(32);
        n -= 32;
    }

    if (n > 0) {
        getBits(n);
    }
}

void ABitReader::putBits(uint32_t x, size_t n) {
    CHECK_LE(n, 32u);

    while (mNumBitsLeft + n > 32) {
        mNumBitsLeft -= 8;
        --mData;
        ++mSize;
    }

    mReservoir = (mReservoir >> n) | (x << (32 - n));
    mNumBitsLeft += n;
}

size_t ABitReader::numBitsLeft() const {
    return mSize * 8 + mNumBitsLeft;
}

const uint8_t *ABitReader::data() const {
    return mData - (mNumBitsLeft + 7) / 8;
}

}  // namespace android
