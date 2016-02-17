/*
   This file is part of bpvo.

   bpvo is free software: you can redistribute it and/or modify
   it under the terms of the Lesser GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   bpvo is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   Lesser GNU General Public License for more details.

   You should have received a copy of the Lesser GNU General Public License
   along with bpvo.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
 * Contributor: halismai@cs.cmu.edu
 */

#include "bpvo/imgproc.h"
#include "bpvo/debug.h"
#include "bpvo/simd.h"
#include <string.h>
#include <algorithm>

namespace bpvo {

template <bool Align> FORCE_INLINE
__m128 gradientAbsMag(const float* src, int stride)
{
  const __m128 sy0 = simd::load<Align>(src - stride);
  const __m128 sy1 = simd::load<Align>(src + stride);
  const __m128 sx0 = simd::load<false>(src - 1);
  const __m128 sx1 = simd::load<false>(src + 1);
  const __m128 Ix = simd::abs(_mm_sub_ps(sx0, sx1));
  const __m128 Iy = simd::abs(_mm_sub_ps(sy0, sy1));
  return _mm_add_ps(Ix, Iy);
}

template <bool Aligned> FORCE_INLINE
void gradientAbsoluteMagnitude(const float* src_ptr, int rows, int cols,
                               float* dst_ptr)
{
  std::fill_n(dst_ptr, cols, 0.0f);

  auto src = src_ptr + cols;
  auto dst = dst_ptr + cols;

  int n = cols & ~(4-1);

  for(int r = 2; r < rows; ++r)
  {
    int x = 0;
    for( ; x < n; x += 4)
    {
      simd::store<Aligned>(dst + x, gradientAbsMag<Aligned>(src + x, cols));
    }

    for( ; x < cols; ++x) {
      dst[x] = fabs(src[x+1]-src[x-1]) + fabs(src[x+cols] + src[x-cols]);
    }

    dst[x] = 0.0f;
    dst[cols-1] = 0.0f;

    dst += cols;
    src += cols;

  }

  //std::fill_n(dst_ptr + (rows-1)*cols, cols, 0.0f);
  std::fill_n(dst, cols, 0.0f);
}

/*
template <bool Aligned> FORCE_INLINE
void gradientAbsoluteMagnitude(const float* src_ptr, int rows, int cols, float* dst_ptr)
{
  for(int y = 0; y < rows; ++y)
  {
    auto s0 = src_ptr + (y > 0 ? y-1 : rows > 1 ? 1 : 0)*cols;
    auto s1 = src_ptr + y*cols;
    auto s2 = src_ptr - (y < rows - 1 ? y+1 : rows > 1 ? rows - 2 : 0);

    int x = 0;
    for( ; x <= cols - 4; x += 4)
    {
    }
  }
}
*/


void gradientAbsoluteMagnitude(const cv::Mat_<float>& src, cv::Mat_<float>& dst)
{
  assert( src.channels() == 1 );

  int rows = src.rows, cols = src.cols;
  dst.create(rows, cols);

  auto src_ptr = src.ptr<const float>();
  auto dst_ptr = dst.ptr<float>();

  if(simd::isAligned<16>(src_ptr) && simd::isAligned<16>(dst_ptr) && simd::isAligned<16>(cols))
  {
    gradientAbsoluteMagnitude<true>(src_ptr, rows, cols, dst_ptr);
  } else
  {
    gradientAbsoluteMagnitude<false>(src_ptr, rows, cols, dst_ptr);
  }
}

template <bool Aligned> FORCE_INLINE
void gradientAbsoluteMagnitudeAcc(const float* src, int rows, int cols, float* dst)
{
  constexpr int S = 4;
  const int n = cols & ~(S-1);

  src += cols;
  dst += cols;
  for(int r = 2; r < rows; ++r, src += cols, dst += cols) {
    int x = 0;
    for( ; x < n; x += S) {
      const __m128 g = _mm_add_ps(simd::load<Aligned>(dst+x),
                                  gradientAbsMag<Aligned>(src+x, cols));
      simd::store<Aligned>(dst, g);
    }

    for( ; x < cols; ++x) {
      dst[x] += fabs(src[x-1]-src[x+1]) + fabs(src[x-cols] + src[x+cols]);
    }

    dst[x] = 0.0f;
    dst[cols-1] = 0.0f;
  }
}

void gradientAbsoluteMagnitudeAcc(const cv::Mat_<float>& src, float* dst)
{
  assert( src.channels() == 1 );
  auto rows = src.rows, cols = src.cols;
  auto src_ptr = src.ptr<const float>();

  if(simd::isAligned<16>(src_ptr) && simd::isAligned<16>(cols) && simd::isAligned<16>(dst))
  {
    gradientAbsoluteMagnitudeAcc<true>(src_ptr, rows, cols, dst);
  } else
  {
    gradientAbsoluteMagnitudeAcc<false>(src_ptr, rows, cols, dst);
  }
}

}; // bpvo

