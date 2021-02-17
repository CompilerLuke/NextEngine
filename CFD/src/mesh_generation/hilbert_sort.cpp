#include "core/core.h"
#include "mesh.h"
#include "graphics/culling/aabb.h"
#include "geo/predicates.h"
#include "core/container/tvector.h"

/* create a nextbefore macro
 * for a strictly positive value, round to previous double value */
#if (defined (__STD_VERSION__) && (__STD_VERSION__ >= 199901L)) // nextafter only from C99
#include <math.h>
#define nextbefore(x) nextafter(x,0.0);
#else
#include <float.h>
#define nextbefore(x) (x*(1.0-DBL_EPSILON))
#endif

#ifdef NE_PLATFORM_WINDOWS 
#include <intrin.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

DWORD __inline clz(uint32_t value) {
    DWORD leading_zero = 0;

    if (_BitScanForward(&leading_zero, value)) return 31 - leading_zero;
    return 32;
}
#else
#define clz __builtin_clz
#endif

#define MIN(x,y) ((x)<(y)?(x):(y))
#define SWAP(x,y) do{uint32_t tmp=x; x=y; y=tmp;}while(0)
#define INVE(x) x=s-1-x

uint hilbert_advised_bits(uint32_t n)
{
  uint nlog2 = (32-clz(n))*3/2; // if we have n points, we want approximately n^1.5 possible hilbert coordinates
  return MIN(63, nlog2>15?(nlog2+10)/11*11:nlog2); // we prefer multiple of 11
}

/*================================= compute the hilberts distance ===========================================
 *
 * This part is for computing the hilbert coordinates of the vertices (X,Y,Z)
 */
void hilbert_dist(CFDVolume& mesh, const AABB& aabb, vertex_handle* vertices, u64* dist, uint n, uint32_t* nbits) {
    uint level;
    if(*nbits==0) *nbits = hilbert_advised_bits(n);
    else if(*nbits>63) *nbits = 63;

    level = (*nbits+2)/3;
    *nbits = level*3;

    uint nmax = 1U<<level;

    real divX = nextbefore(nmax/(aabb.max.x-aabb.min.x));
    real divY = nextbefore(nmax/(aabb.max.y-aabb.min.y));
    real divZ = nextbefore(nmax/(aabb.max.z-aabb.min.z));
    
    for (uint i = 0; i < n; i++) {
        vec3 position = mesh[vertices[i]].position;
          
        uint x = (position.x - aabb.min.x)*divX;
        uint y = (position.y - aabb.min.y)*divY;
        uint z = (position.z - aabb.min.z)*divZ;
        u64 bits = 0;

// #ifdef DEBUG
//      if(x<nmax && y<nmax && z<nmax,"coordinate out of bbox")
//       i=n+1;
// #endif

        uint s;
        for (s=nmax>>1; s; s>>=1) {
          uint rx = (x & s) != 0;
          uint ry = (y & s) != 0;
          uint rz = (z & s) != 0;

          uint gc = (7*rx)^((3*ry)^rz);

          // __assume(gc<=7);

          bits += (u64) s*s*s* gc;

          if(gc & 4){
            if(gc & 2){
              if(gc & 1){ // 7
                SWAP(x,z);
                INVE(x);
                INVE(z);
              }
              else{ // 6
                SWAP(x,y);
                INVE(x);
                INVE(y);
              }
            }
            else{
              if(!(gc & 1)){ // 4
                SWAP(x,z);
                SWAP(y,z);
              }
            }
          }
          else{
            if(gc & 2){
              if(gc & 1){ // 3
                SWAP(x,z);
                SWAP(y,z);
                INVE(x);
                INVE(y);
              }
            }
            else{
              if(gc & 1) // 1
                SWAP(x,y);
              else // 0
                SWAP(x,z);
            }
          }
        }
        dist[i] = bits;
    }
}

/************************************** sort functions ***********************************************/

// quickest sort for less than 32 entries
static inline void insertion_sort(vertex_handle* array, u64* dist, uint from, uint to){
    uint r;

    for (r=from+1; r<to; r++) {
        vertex_handle tmp = array[r];
        u64 a = dist[r];

        uint32_t l;
        for (l=r; l>from && dist[l-1]>a; l--){
            array[l] = array[l-1];
            dist[l] = dist[l-1];
        }
        array[l] = tmp;
        dist[l] = a;
  }
}


// quickest sort for less than 10000 entries
void radix2_sort_in_place(vertex_handle* array, u64* dist, uint from, uint to, uint mask)
{
    uint l = from, r;

    while(mask && to > from + 32){
        r = to - 1;
        while (1) {
            /* find left most with bit, and right most without bit, swap */
            while (l < r && (dist[l] & mask)==0) l++;
            while (l < r && (dist[r] & mask)) r--;
            if (l >= r) break;
            
            std::swap(array[l], array[r]);
            std::swap(dist[l], dist[r]);
        }

        if (!(mask & dist[l]) && l < to) l++;
        mask >>= 1;

        radix2_sort_in_place(array, dist, from, l, mask);

        from=l;
    }

    insertion_sort(array, dist, from, to);
}

/********************************************/

// exclusive scan prefix sum
static inline void scan2048(uint* h) {
    uint i;
    uint sum = 0;
    for (i=0; i<2048; i++) {
        uint tsum = h[i] + sum;
        h[i] = sum; // sum of the preceding, but not with itself !!
        sum = tsum;
    }
}

static inline void rad11_lsb(vertex_handle* array, u64* dist, vertex_handle* buffer, u64* buffer_dist, uint n, uint npass) {
    for (uint pass=0; pass<npass; pass++) {
        uint h[2048] = {0};

        for (uint i=0; i<n; i++)
        h[(dist[i] >> (11*pass)) & 2047]++;

        scan2048(h);

        for (uint32_t i=0; i<n; i++) {
            vertex_handle ai = array[i];
            uint index = h[(dist[i] >> (11*pass)) & 2047]++;
            buffer[index] = ai;
            buffer_dist[index] = dist[i];
        }
        std::swap(array,buffer);
        std::swap(dist,buffer_dist);
    }
}


// quickest sort for over 10000 entries
static void vertices_sort_no_copy(vertex_handle* array, u64* dist, vertex_handle* buffer, u64* buffer_dist, const uint32_t n, uint32_t nbits, int* to_copy)
{
  uint hist[2048] = {0};
  
  const uint bits_rem = (nbits<=11)?0:nbits-11; // bits remaining after first pass
  const uint pass_rem = (bits_rem+10)/11;
  *to_copy = (pass_rem&1)==0;
  
  /***** we first make a MSB radix11 sort pass to ensure locality of data ***/
  for (uint i=0; i<n; i++)
    hist[dist[i]>>bits_rem & 2047]++;

  // calculate total sums
  scan2048(hist);

  // read/write histogram, copy
  for (uint32_t i=0; i < n; i++) {
      uint index = hist[dist[i]>>bits_rem & 2047]++;
      buffer[index] = array[i];
      buffer_dist[index] = dist[i];
  }

  /***** then we make a LSB radix sort pass with each of the 2048 pieces of array ***/
  uint start = 0;
  for (uint i=0; i<2048; i++) {
    uint end = hist[i];
    rad11_lsb(buffer + start, buffer_dist + start, array + start, dist + start, end-start, pass_rem);
    start = end;
  }
}

/* wrapper over HXT_vertices_sort_no_copy that copy the result in the initial array if needed
 + use HXT_radix_sort2_in_place if the array is small */
void sort_vertices(vertex_handle* array, u64* dist, uint n, uint nbits) {
    assert(nbits!=0);
  
    if(n<=16384){
        if(nbits>64){
          nbits = 64;
        }
        u64 one = 1;
        radix2_sort_in_place(array, dist, 0, n, one<<(nbits-1));
    }
    else {
        vertex_handle* buffer = TEMPORARY_ARRAY(vertex_handle, n);
        u64* buffer_dist = TEMPORARY_ARRAY(u64, n);
        
        int to_copy = 0;

        vertices_sort_no_copy(array, dist, buffer, buffer_dist, n, nbits, &to_copy);

        if(to_copy) memcpy_t(array, buffer, n);
    }
}


/*********************************** shuffle functions ***********************************************/
static inline uint32_t fast_hash(uint32_t x) {
  x = ((x >> 16) ^ x) * 0x45d9f3b;
  x = ((x >> 16) ^ x) * 0x45d9f3b;
  x = (x >> 16) ^ x;
  return x;
}


/****************************************** BRIO *****************************************************/
/* automatic biased ranomized insertion order */
void brio_vertices(CFDVolume& mesh, const AABB& aabb, slice<vertex_handle> vertices) {
    LinearRegion region(get_temporary_allocator());
    u64* dist = TEMPORARY_ARRAY(u64, vertices.length);
    uint n = vertices.length;
    
    for (uint i=0; i<n; i++){
        dist[i] = fast_hash(i);
    }

    sort_vertices(vertices.data, dist, n, 22); // just make two pass of the sort (no copy needed)

    uint32_t nbits=0;
    hilbert_dist(mesh, aabb, vertices.data, dist, n, &nbits);

    // sort the 32768 first number, than, because there will be approximately 7* more tetrahedron, make it times 7 every time
    uint end = n;
    // int to_copy = 1;

    while(end > 1500) {
        uint start = end/7.5;
        sort_vertices(vertices.data+start, dist+start, end-start, nbits);
        end = start;
    }
}
