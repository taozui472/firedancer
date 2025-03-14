#include "../fd_util.h"

#if FD_HAS_INT128

#include <math.h>

/* Create random bit patterns with lots of leading and/or trailing zeros
   or ones to really stress limits of implementations. */

static inline ulong
make_test_rand_ulong( ulong x,        /* Random 64-bit */
                       uint *_ctl ) { /* Least significant 8 bits random, uses them up */
  uint ctl = *_ctl;
  int s = (int)(ctl & 63U); ctl >>= 6; /* Shift, in [0,63] */
  int d = (int)(ctl &  1U); ctl >>= 1; /* Direction, in [0,1] */
  int i = (int)(ctl &  1U); ctl >>= 1; /* Invert, in [0,1] */
  *_ctl = ctl;
  x = d ? (x<<s) : (x>>s);
  return i ? (~x) : x;
}

static inline uint
make_test_rand_uint( uint x,        /* Random 32-bit */
                     uint *_ctl ) { /* Least significant 8 bits random, uses them up */
  uint ctl = *_ctl;
  int s = (int)(ctl & 31U); ctl >>= 6; /* Shift, in [0,31] */
  int d = (int)(ctl &  1U); ctl >>= 1; /* Direction, in [0,1] */
  int i = (int)(ctl &  1U); ctl >>= 1; /* Invert, in [0,1] */
  *_ctl = ctl;
  x = d ? (x<<s) : (x>>s);
  return i ? (~x) : x;
}

#if FD_HAS_INT128

static inline uint128
make_test_rand_uint128( uint128 x,       /* Random 32-bit */
                        uint *  _ctl ) { /* Least significant 8 bits random, uses them up */
  uint ctl = *_ctl;
  int s = (int)(ctl & 31U); ctl >>= 6; /* Shift, in [0,31] */
  int d = (int)(ctl &  1U); ctl >>= 1; /* Direction, in [0,1] */
  int i = (int)(ctl &  1U); ctl >>= 1; /* Invert, in [0,1] */
  *_ctl = ctl;
  x = d ? (x<<s) : (x>>s);
  return i ? (~x) : x;
}

#endif

static inline ulong
fd_ulong_sat_add_ref( ulong x,
                        ulong y ) {
  uint128 ref = x;
  ref += y;

  if( ref > ULONG_MAX ) {
    return ULONG_MAX;
  } else {
    return (ulong) ref;
  }
}

static inline ulong
fd_ulong_sat_sub_ref( ulong x,
                        ulong y ) {
  uint128 ref = x;
  ref -= y;

  if( y > x ) {
    return 0;
  } else {
    return (ulong) ref;
  }
}

static inline ulong
fd_ulong_sat_mul_ref( ulong x,
                       ulong y ) {
  uint128 ref = x;
  ref *= y;

  if( x == 0 || y == 0 ) {
    return 0;
  }

  if( ( ref < x ) || ( ref < y ) || ( ( ref/x ) != y ) || (ref > ULONG_MAX)) {
    return ULONG_MAX;
  } else {
    return (ulong) ref;
  }
}

static inline long
fd_long_sat_add_ref( long x,
                     long y ) {
  int128 ref = x;
  ref += y;

  if( ref > LONG_MAX ) {
    return LONG_MAX;
  } else if( ref < LONG_MIN ) {
    return LONG_MIN;
  } else {
    return (long) ref;
  }
}

static inline long
fd_long_sat_sub_ref( long x,
                     long y ) {
  int128 ref = x;
  ref -= y;

  if( ref > LONG_MAX ) {
    return LONG_MAX;
  } else if( ref < LONG_MIN ) {
    return LONG_MIN;
  } else {
    return (long) ref;
  }
}

static inline uint
fd_uint_sat_add_ref( uint x,
                       uint y ) {
  uint128 ref = x;
  ref += y;

  if( ref > UINT_MAX ) {
    return UINT_MAX;
  } else {
    return (uint) ref;
  }
}

static inline uint
fd_uint_sat_sub_ref( uint x,
                       uint y ) {
  uint128 ref = x;
  ref -= y;

  if( y > x ) {
    return 0;
  } else {
    return (uint) ref;
  }
}

static inline uint
fd_uint_sat_mul_ref( uint x,
                      uint y ) {
  ulong ref = x;
  ref *= y;

  if( x == 0 || y == 0 ) {
    return 0;
  }

  if( ( ref < x ) || ( ref < y ) || ( ( ref / x ) != y )  || (ref > UINT_MAX) ) {
    return UINT_MAX;
  } else {
    return (uint) ref;
  }
}

#if FD_HAS_INT128

static inline uint128
fd_uint128_sat_add_ref( uint128 x, uint128 y ) {
  uint128 res = x + y;
  return fd_uint128_if( res < x, UINT128_MAX, res );
}

static inline uint128
fd_uint128_sat_mul_ref( uint128 x, uint128 y ) {
  uint128 res = x * y;
  uchar overflow = ( x != 0 ) && ( y != 0 ) && ( ( res < x ) || ( res < y ) || ( ( res / x ) != y ) );
  return fd_uint128_if( overflow, UINT128_MAX, res );
}

static inline uint128
fd_uint128_sat_sub_ref( uint128 x, uint128 y ) {
  uint128 res = x - y;
  return fd_uint128_if( res > x, 0, res );
}

#endif

int
main( int     argc,
      char ** argv ) {

  fd_boot( &argc, &argv );

#   define TEST(op,x,y)                                   \
    do {                                              \
      ulong ref   = fd_ulong_sat_##op##_ref ( x, y ); \
      ulong res   = fd_ulong_sat_##op ( x, y );    \
      if( ref != res ) { \
        FD_LOG_ERR(( "FAIL: fd_ulong_sat_" #op " x %lu y %lu ref %lu res %lu", (ulong)x, (ulong)y, ref, res )); \
      } \
    } while(0)

    TEST(add,0,ULONG_MAX);
    TEST(add,ULONG_MAX,10);
    TEST(add,ULONG_MAX - 10,ULONG_MAX - 10);
    TEST(sub,0,ULONG_MAX);
    TEST(add,ULONG_MAX,10);
    TEST(sub,ULONG_MAX - 10,ULONG_MAX - 10);
    TEST(mul,0,ULONG_MAX);
    TEST(mul,ULONG_MAX,10);
    TEST(mul,ULONG_MAX - 10,ULONG_MAX - 10);

#   undef TEST

#   define TEST(op,x,y)                                   \
    do {                                              \
      long ref   = fd_long_sat_##op##_ref ( x, y ); \
      long res   = fd_long_sat_##op ( x, y );    \
      if( ref != res ) { \
        FD_LOG_ERR(( "FAIL: fd_long_sat_" #op " x %ld y %ld ref %ld res %ld", (long)x, (long)y, ref, res )); \
      } \
    } while(0)

    TEST(add,0,LONG_MAX);
    TEST(add,LONG_MAX,10);
    TEST(add,LONG_MAX - 10,LONG_MAX - 10);
    TEST(sub,0,LONG_MAX);
    TEST(add,LONG_MAX,10);
    TEST(sub,LONG_MAX - 10,LONG_MAX - 10);

#   undef TEST

#   define TEST(op,x,y)                                \
    do {                                               \
      uint ref   = fd_uint_sat_##op##_ref ( x, y ); \
      uint res   = fd_uint_sat_## op      ( x, y ); \
      if( ref != res ) { \
        FD_LOG_ERR(( "FAIL: fd_uint_sat_" #op " x %u y %u ref %u res %u", x, y, ref, res )); \
      } \
    } while(0)

    TEST(add,0u,UINT_MAX);
    TEST(add,UINT_MAX,10u);
    TEST(add,UINT_MAX - 10u,UINT_MAX - 10u);
    TEST(sub,0u,UINT_MAX);
    TEST(add,UINT_MAX,10u);
    TEST(sub,UINT_MAX - 10u,UINT_MAX - 10u);
    TEST(mul,0u,UINT_MAX);
    TEST(mul,UINT_MAX,10u);
    TEST(mul,UINT_MAX - 10u,UINT_MAX - 10u);

#   undef TEST

#if FD_HAS_INT128

#   define TEST(op,x,y)                                \
    do {                                               \
      uint128 ref   = fd_uint128_sat_##op##_ref ( x, y ); \
      uint128 res   = fd_uint128_sat_## op      ( x, y ); \
      if( ref != res ) { \
        FD_LOG_ERR(( "FAIL: fd_uint128_sat_" #op " x %016lx%016lx y %016lx%016lx ref %016lx%016lx res %016lx%016lx", \
                     (ulong)(((uint128)x)>>64), (ulong)(x), \
                     (ulong)(((uint128)y)>>64), (ulong)(y), \
                     (ulong)((ref)>>64), (ulong)(ref),      \
                     (ulong)((res)>>64), (ulong)(res) ));   \
      } \
    } while(0)

    TEST(add,0,UINT128_MAX);
    TEST(add,UINT128_MAX,10);
    TEST(add,UINT128_MAX - 10,UINT128_MAX - 10);
    TEST(sub,0,UINT128_MAX);
    TEST(add,UINT128_MAX,10);
    TEST(sub,UINT128_MAX - 10,UINT128_MAX - 10);
    TEST(mul,0,UINT128_MAX);
    TEST(mul,UINT128_MAX,10);
    TEST(mul,UINT128_MAX - 10,UINT128_MAX - 10);

#   undef TEST

#endif

  fd_rng_t _rng[1];
  fd_rng_t * rng = fd_rng_join( fd_rng_new( _rng, 0U, 0UL ) );

  int ctr = 0;
  for( int i=0; i<100000000; i++ ) {
    if( !ctr ) { FD_LOG_NOTICE(( "Completed %i iterations", i )); ctr = 10000000; }
    ctr--;

#   define TEST(op)                                  \
    do {                                             \
      uint  t =  fd_rng_uint ( rng );                             \
      ulong x  = make_test_rand_ulong( fd_rng_ulong( rng ), &t ); \
      ulong y  = make_test_rand_ulong( fd_rng_ulong( rng ), &t ); \
      ulong ref   = fd_ulong_sat_##op##_ref ( x, y ); \
      ulong res   = fd_ulong_sat_##op       ( x, y ); \
      if( ref != res ) { \
        FD_LOG_ERR(( "FAIL: %i fd_ulong_sat_" #op " x %lu y %lu ref %lu res %lu", i, (ulong)x, (ulong)y, ref, res )); \
      } \
    } while(0)

    TEST(add);
    TEST(sub);
    TEST(mul);

#   undef TEST

#   define TEST(op)                                  \
    do {                                             \
      uint  t =  fd_rng_uint ( rng );                             \
      long x  = (long)make_test_rand_ulong( fd_rng_ulong( rng ), &t ); \
      long y  = (long)make_test_rand_ulong( fd_rng_ulong( rng ), &t ); \
      long ref   = fd_long_sat_##op##_ref ( x, y ); \
      long res   = fd_long_sat_##op       ( x, y ); \
      if( ref != res ) { \
        FD_LOG_ERR(( "FAIL: %i fd_long_sat_" #op " x %ld y %ld ref %ld res %ld", i, (long)x, (long)y, ref, res )); \
      } \
    } while(0)

    TEST(add);
    TEST(sub);

#   undef TEST

#   define TEST(op)                                  \
    do {                                             \
      uint t =  fd_rng_uint ( rng );                \
      uint x  = make_test_rand_uint( fd_rng_uint( rng ), &t ); \
      uint y  = make_test_rand_uint( fd_rng_uint( rng ), &t ); \
      uint ref   = fd_uint_sat_##op##_ref ( x, y ); \
      uint res   = fd_uint_sat_##op       ( x, y ); \
      if( ref != res ) { \
        FD_LOG_ERR(( "FAIL: %i fd_uint_sat_" #op " x %u y %u ref %u res %u", i, x, y, ref, res )); \
      } \
    } while(0)

    TEST(add);
    TEST(sub);
    TEST(mul);

#   undef TEST

#if FD_HAS_INT128

#   define TEST(op)                                  \
    do {                                             \
      uint    t =  fd_rng_uint ( rng );              \
      uint128 x  = make_test_rand_uint128( fd_rng_uint128( rng ), &t ); \
      uint128 y  = make_test_rand_uint128( fd_rng_uint128( rng ), &t ); \
      uint128 ref   = fd_uint128_sat_##op##_ref ( x, y ); \
      uint128 res   = fd_uint128_sat_##op       ( x, y ); \
      if( ref != res ) { \
        FD_LOG_ERR(( "FAIL: %i fd_uint128_sat_" #op " x %016lx%016lx y %016lx%016lx ref %016lx%016lx res %016lx%016lx", \
                     i,                              \
                     (ulong)(x  >>64), (ulong)(x),   \
                     (ulong)(y  >>64), (ulong)(y),   \
                     (ulong)(ref>>64), (ulong)(ref), \
                     (ulong)(res>>64), (ulong)(res) )); \
      } \
    } while(0)

    TEST(add);
    TEST(sub);
    TEST(mul);

#   undef TEST

#endif

  }

  fd_rng_delete( fd_rng_leave( rng ) );

  FD_LOG_NOTICE(( "pass" ));
  fd_halt();
  return 0;
}

#else

int
main( int     argc,
      char ** argv ) {
  fd_boot( &argc, &argv );
  FD_LOG_WARNING(( "skip: unit test requires FD_HAS_INT128 capability" ));
  fd_halt();
  return 0;
}

#endif
