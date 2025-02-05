#include "fd_exec_sol_compat.h"
#include "../../nanopb/pb_encode.h"
#include "../../nanopb/pb_decode.h"
#include "generated/elf.pb.h"
#include "generated/invoke.pb.h"
#include "generated/shred.pb.h"
#include "generated/vm.pb.h"
#include <assert.h>
#include <stdlib.h>
#include "../../vm/fd_vm.h"
#include "fd_vm_test.h"
#include "fd_pack_test.h"
#include "../../features/fd_features.h"
#include "../fd_executor_err.h"
#include "../../fd_flamenco.h"
#include "../../../ballet/shred/fd_shred.h"
#include "../fd_acc_mgr.h"

/* FIXME: Spad isn't properly sized out or cleaned up */

/* This file defines stable APIs for compatibility testing.

   For the "compat" shared library used by the differential fuzzer,
   ideally the symbols defined in this file would be the only visible
   globals.  Unfortunately, we currently export all symbols, which leads
   to great symbol table bloat from fd_types.c. */

typedef struct {
  ulong   struct_size;
  ulong * hardcoded_features;
  ulong   hardcoded_features_cnt;
  ulong * supported_features;
  ulong   supported_feature_cnt;
} sol_compat_features_t;

static sol_compat_features_t features;
static uchar *               spad_mem;
static fd_wksp_t *           wksp = NULL;

#define WKSP_EXECUTE_ALLOC_TAG (2UL)
#define WKSP_INIT_ALLOC_TAG    (3UL)

void
sol_compat_init( int log_level ) {
  int argc = 1;
  char * argv[2] = { (char *)"fd_exec_sol_compat", NULL };
  char ** argv_ = argv;
  if ( !getenv( "FD_LOG_PATH" ) ) {
    setenv( "FD_LOG_PATH", "", 1 );
  }
  fd_log_enable_unclean_exit();
  fd_boot( &argc, &argv_ );
  fd_log_level_logfile_set( log_level );
  fd_flamenco_boot( NULL, NULL );
  fd_log_level_core_set(4);  /* abort on FD_LOG_ERR */

  sol_compat_wksp_init( FD_SHMEM_NORMAL_PAGE_SZ );
}

void
sol_compat_wksp_init( ulong wksp_page_sz ) {
  ulong cpu_idx = fd_tile_cpu_id( fd_tile_idx() );
  if( cpu_idx>=fd_shmem_cpu_cnt() ) cpu_idx = 0UL;
  switch( wksp_page_sz )
  {
  case FD_SHMEM_GIGANTIC_PAGE_SZ:
    wksp = fd_wksp_new_anonymous( FD_SHMEM_GIGANTIC_PAGE_SZ, 5UL, fd_shmem_cpu_idx( fd_shmem_numa_idx( cpu_idx ) ), "wksp", 0UL );
    break;
  case FD_SHMEM_NORMAL_PAGE_SZ:
    wksp = fd_wksp_new_anonymous( FD_SHMEM_NORMAL_PAGE_SZ, 512UL * 512UL * 5UL, fd_shmem_cpu_idx( fd_shmem_numa_idx( cpu_idx ) ), "wksp", 0UL );
    break;
  default:
    FD_LOG_ERR(( "Unsupported page size %lu", wksp_page_sz ));
  }
  FD_TEST( wksp );

  spad_mem = fd_wksp_alloc_laddr( wksp, FD_SPAD_ALIGN, FD_SPAD_FOOTPRINT( FD_RUNTIME_TRANSACTION_EXECUTION_FOOTPRINT_FUZZ ), WKSP_INIT_ALLOC_TAG ); /* 4738713960 B */
  FD_TEST( spad_mem );

  features.struct_size         = sizeof(sol_compat_features_t);
  features.hardcoded_features = fd_wksp_alloc_laddr( wksp, 8UL, FD_FEATURE_ID_CNT * sizeof(ulong), WKSP_INIT_ALLOC_TAG );
  features.supported_features  = fd_wksp_alloc_laddr( wksp, 8UL, FD_FEATURE_ID_CNT * sizeof(ulong), WKSP_INIT_ALLOC_TAG );

  for( const fd_feature_id_t * current_feature = fd_feature_iter_init(); !fd_feature_iter_done( current_feature ); current_feature = fd_feature_iter_next( current_feature ) ) {
    // Skip reverted features
    if( current_feature->reverted ) continue;

    // Only hardcode features that are activated on all clusters
    if( current_feature->activated_on_all_clusters ) {
      memcpy( &features.hardcoded_features[features.hardcoded_features_cnt++], &current_feature->id, sizeof(ulong) );
    } else {
      memcpy( &features.supported_features[features.supported_feature_cnt++], &current_feature->id, sizeof(ulong) );
    }
  }
}

void
sol_compat_fini( void ) {
  fd_wksp_free_laddr( spad_mem );
  fd_wksp_free_laddr( features.hardcoded_features );
  fd_wksp_free_laddr( features.supported_features );
  fd_wksp_delete_anonymous( wksp );
  wksp     = NULL;
  spad_mem = NULL;
}

void
sol_compat_check_wksp_usage( void ) {
  fd_wksp_usage_t usage[1];
  ulong tags[1] = { WKSP_EXECUTE_ALLOC_TAG };
  fd_wksp_usage( wksp, tags, 1, usage );
  if( usage->used_sz ) {
    FD_LOG_ERR(( "%lu bytes leaked in %lu allocations", usage->used_sz, usage->used_cnt ));
  }
}

sol_compat_features_t const *
sol_compat_get_features_v1( void ) {
  return &features;
}

fd_exec_instr_test_runner_t *
sol_compat_setup_runner( void ) {

  // Setup test runner
  void * runner_mem = fd_wksp_alloc_laddr( wksp, fd_exec_instr_test_runner_align(), fd_exec_instr_test_runner_footprint(), WKSP_EXECUTE_ALLOC_TAG );
  fd_exec_instr_test_runner_t * runner = fd_exec_instr_test_runner_new( runner_mem, spad_mem, WKSP_EXECUTE_ALLOC_TAG );
  return runner;
}

void
sol_compat_cleanup_runner( fd_exec_instr_test_runner_t * runner ) {
  /* Cleanup test runner */
  fd_wksp_free_laddr( fd_exec_instr_test_runner_delete( runner ) );
}

void *
sol_compat_decode( void *               decoded,
                   uchar const *        in,
                   ulong                in_sz,
                   pb_msgdesc_t const * decode_type ) {
  pb_istream_t istream = pb_istream_from_buffer( in, in_sz );
  int decode_ok = pb_decode_ex( &istream, decode_type, decoded, PB_DECODE_NOINIT );
  if( !decode_ok ) {
    pb_release( decode_type, decoded );
    return NULL;
  }
  return decoded;
}

void const *
sol_compat_encode( uchar *              out,
                   ulong *              out_sz,
                   void const *         to_encode,
                   pb_msgdesc_t const * encode_type ) {
  pb_ostream_t ostream = pb_ostream_from_buffer( out, *out_sz );
  int encode_ok = pb_encode( &ostream, encode_type, to_encode );
  if( !encode_ok ) {
    return NULL;
  }
  *out_sz = ostream.bytes_written;
  return to_encode;
}

typedef ulong( exec_test_run_fn_t )( fd_exec_instr_test_runner_t *,
                                     void const *,
                                     void **,
                                     void *,
                                     ulong );

void
sol_compat_execute_wrapper( fd_exec_instr_test_runner_t * runner,
                            void * input,
                            void ** output,
                            exec_test_run_fn_t * exec_test_run_fn ) {

  ulong out_bufsz = 100000000;  /* 100 MB */
  void * out0 = fd_spad_alloc( runner->spad, 1UL, out_bufsz );
  FD_TEST( out_bufsz <= fd_spad_alloc_max( runner->spad, 1UL ) );

  ulong out_used = exec_test_run_fn( runner, input, output, out0, out_bufsz );
  if( FD_UNLIKELY( !out_used ) ) {
    *output = NULL;
  }

}

/*
 * fixtures
 */

int
sol_compat_cmp_binary_strict( void const * effects,
                              void const * expected,
                              pb_msgdesc_t const * encode_type,
                              fd_spad_t * spad ) {
#define MAX_SZ 32*1024*1024
FD_SPAD_FRAME_BEGIN( spad ) {
  if( effects==NULL ) {
    FD_LOG_WARNING(( "No output effects" ));
    return 0;
  }

  /* Note: Most likely this spad allocation won't fail. If it does, you may need to bump
     the allocated spad memory amount in fd_exec_sol_compat.c. */
  ulong out_sz = MAX_SZ;
  uchar * out = fd_spad_alloc( spad, 1UL, out_sz );
  if( !sol_compat_encode( out, &out_sz, effects, encode_type ) ) {
    FD_LOG_WARNING(( "Error encoding effects" ));
    return 0;
  }

  ulong exp_sz = MAX_SZ;
  uchar * exp = fd_spad_alloc( spad, 1UL, exp_sz );
  if( !sol_compat_encode( exp, &exp_sz, expected, encode_type ) ) {
    FD_LOG_WARNING(( "Error encoding expected" ));
    return 0;
  }

  if( out_sz!=exp_sz ) {
    FD_LOG_WARNING(( "Binary cmp failed: different size. out_sz=%lu exp_sz=%lu", out_sz, exp_sz  ));
    return 0;
  }
  if( !fd_memeq( out, exp, out_sz ) ) {
    FD_LOG_WARNING(( "Binary cmp failed: different values." ));
    return 0;
  }

  return 1;
} FD_SPAD_FRAME_END;
#undef MAX_SIZE
}

static int
_diff_txn_acct( fd_exec_test_acct_state_t * expected,
                fd_exec_test_acct_state_t * actual ) {
  /* AcctState -> address (This must hold true when calling this function!) */
  assert( fd_memeq( expected->address, actual->address, sizeof(fd_pubkey_t) ) );

  /* AcctState -> lamports */
  if( expected->lamports != actual->lamports ) {
    FD_LOG_WARNING(( "Lamports mismatch: expected=%lu actual=%lu", expected->lamports, actual->lamports ));
    return 0;
  }

  /* AcctState -> data */
  if( expected->data != NULL || actual->data != NULL ) {
    if( expected->data == NULL ) {
      FD_LOG_WARNING(( "Expected account data is NULL, actual is non-NULL" ));
      return 0;
    }

    if( actual->data == NULL ) {
      FD_LOG_WARNING(( "Expected account data is NULL, actual is non-NULL" ));
      return 0;
    }

    if( expected->data->size != actual->data->size ) {
      FD_LOG_WARNING(( "Account data size mismatch: expected=%u actual=%u", expected->data->size, actual->data->size ));
      return 0;
    }

    if( !fd_memeq( expected->data->bytes, actual->data->bytes, expected->data->size ) ) {
      FD_LOG_WARNING(( "Account data mismatch" ));
      return 0;
    }
  }

  /* AcctState -> executable */
  if( expected->executable != actual->executable ) {
    FD_LOG_WARNING(( "Executable mismatch: expected=%d actual=%d", expected->executable, actual->executable ));
    return 0;
  }

  /* AcctState -> rent_epoch */
  if( expected->rent_epoch != actual->rent_epoch ) {
    FD_LOG_WARNING(( "Rent epoch mismatch: expected=%lu actual=%lu", expected->rent_epoch, actual->rent_epoch ));
    return 0;
  }

  /* AcctState -> owner */
  if( !fd_memeq( expected->owner, actual->owner, sizeof(fd_pubkey_t) ) ) {
    char a[ FD_BASE58_ENCODED_32_SZ ];
    char b[ FD_BASE58_ENCODED_32_SZ ];
    FD_LOG_WARNING(( "Owner mismatch: expected=%s, actual=%s", fd_acct_addr_cstr( a, expected->owner ), fd_acct_addr_cstr( b, actual->owner ) ));
    return 0;
  }

  return 1;
}


static int
_diff_resulting_states( fd_exec_test_resulting_state_t *  expected,
                        fd_exec_test_resulting_state_t *  actual ) {
  // Verify that the number of accounts are the same
  if( expected->acct_states_count != actual->acct_states_count ) {
    FD_LOG_WARNING(( "Account states count mismatch: expected=%u actual=%u", expected->acct_states_count, actual->acct_states_count ));
    return 0;
  }

  // Verify that the account states are the same
  for( ulong i = 0; i < expected->acct_states_count; ++i ) {
    for( ulong j = 0; j < actual->acct_states_count; ++j ) {
      if( fd_memeq( expected->acct_states[i].address, actual->acct_states[j].address, sizeof(fd_pubkey_t) ) ) {
        if( !_diff_txn_acct( &expected->acct_states[i], &actual->acct_states[j] ) ) {
          return 0;
        }
      }
    }
  }

  // TODO: resulting_state -> rent_debits, resulting_state->transaction_rent
  return 1;
}

int
sol_compat_cmp_txn( fd_exec_test_txn_result_t *  expected,
                    fd_exec_test_txn_result_t *  actual ) {
  /* TxnResult -> executed */
  if( expected->executed != actual->executed ) {
    FD_LOG_WARNING(( "Executed mismatch: expected=%d actual=%d", expected->executed, actual->executed ));
    return 0;
  }

  /* TxnResult -> sanitization_error */
  if( expected->sanitization_error != actual->sanitization_error ) {
    FD_LOG_WARNING(( "Sanitization error mismatch: expected=%d actual=%d", expected->sanitization_error, actual->sanitization_error ));
    return 0;
  }

  /* TxnResult -> resulting_state */
  if( !_diff_resulting_states( &expected->resulting_state, &actual->resulting_state ) ) {
    return 0;
  }

  /* TxnResult -> rent */
  if( expected->rent != actual->rent ) {
    FD_LOG_WARNING(( "Rent mismatch: expected=%lu actual=%lu", expected->rent, actual->rent ));
    return 0;
  }

  /* TxnResult -> is_ok */
  if( expected->is_ok != actual->is_ok ) {
    FD_LOG_WARNING(( "Is ok mismatch: expected=%d actual=%d", expected->is_ok, actual->is_ok ));
    return 0;
  }

  /* TxnResult -> status */
  if( expected->status != actual->status ) {
    FD_LOG_WARNING(( "Status mismatch: expected=%u actual=%u", expected->status, actual->status ));
    return 0;
  }

  /* TxnResult -> instruction_error */
  if( expected->instruction_error != actual->instruction_error ) {
    FD_LOG_WARNING(( "Instruction error mismatch: expected=%u actual=%u", expected->instruction_error, actual->instruction_error ));
    return 0;
  }

  if( expected->instruction_error ) {
    /* TxnResult -> instruction_error_index */
    if( expected->instruction_error_index != actual->instruction_error_index ) {
      FD_LOG_WARNING(( "Instruction error index mismatch: expected=%u actual=%u", expected->instruction_error_index, actual->instruction_error_index ));
      return 0;
    }

    /* TxnResult -> custom_error */
    if( expected->instruction_error == (ulong) -FD_EXECUTOR_INSTR_ERR_CUSTOM_ERR && expected->custom_error != actual->custom_error ) {
      FD_LOG_WARNING(( "Custom error mismatch: expected=%u actual=%u", expected->custom_error, actual->custom_error ));
      return 0;
    }
  }

  /* TxnResult -> return_data */
  if( expected->return_data != NULL || actual->return_data != NULL ) {
    if( expected->return_data == NULL ) {
      FD_LOG_WARNING(( "Expected return data is NULL, actual is non-NULL" ));
      return 0;
    }

    if( actual->return_data == NULL ) {
      FD_LOG_WARNING(( "Expected return data is NULL, actual is non-NULL" ));
      return 0;
    }

    if( expected->return_data->size != actual->return_data->size ) {
      FD_LOG_WARNING(( "Return data size mismatch: expected=%u actual=%u", expected->return_data->size, actual->return_data->size ));
      return 0;
    }

    if( !fd_memeq( expected->return_data->bytes, actual->return_data->bytes, expected->return_data->size ) ) {
      FD_LOG_WARNING(( "Return data mismatch" ));
      return 0;
    }
  }

  /* TxnResult -> executed_units */
  if( expected->executed_units != actual->executed_units ) {
    FD_LOG_WARNING(( "Executed units mismatch: expected=%lu actual=%lu", expected->executed_units, actual->executed_units ));
    return 0;
  }

  /* TxnResult -> fee_details */
  if( expected->fee_details.transaction_fee != actual->fee_details.transaction_fee ) {
    FD_LOG_WARNING(( "Transaction fee mismatch: expected=%lu actual=%lu", expected->fee_details.transaction_fee, actual->fee_details.transaction_fee ));
    return 0;
  }

  if( expected->fee_details.prioritization_fee != actual->fee_details.prioritization_fee ) {
    FD_LOG_WARNING(( "Priority fee mismatch: expected=%lu actual=%lu", expected->fee_details.prioritization_fee, actual->fee_details.prioritization_fee ));
    return 0;
  }

  return 1;
}

int
sol_compat_cmp_success_fail_only( void const * _effects,
                                  void const * _expected ) {
  fd_exec_test_instr_effects_t * effects  = (fd_exec_test_instr_effects_t *)_effects;
  fd_exec_test_instr_effects_t * expected = (fd_exec_test_instr_effects_t *)_expected;

  if( effects==NULL ) {
    FD_LOG_WARNING(( "No output effects" ));
    return 0;
  }

  if( effects->custom_err || expected->custom_err ) {
    FD_LOG_WARNING(( "Unexpected custom error" ));
    return 0;
  }

  int res = effects->result;
  int exp = expected->result;

  if( res==exp ) {
    return 1;
  }

  if( res>0 && exp>0 ) {
    FD_LOG_INFO(( "Accepted: res=%d exp=%d", res, exp ));
    return 1;
  }

  return 0;
}

int
sol_compat_instr_fixture( fd_exec_instr_test_runner_t * runner,
                          uchar const *                 in,
                          ulong                         in_sz ) {
  // Decode fixture
  fd_exec_test_instr_fixture_t fixture[1] = {0};
  void * res = sol_compat_decode( &fixture, in, in_sz, &fd_exec_test_instr_fixture_t_msg );
  if ( res==NULL ) {
    FD_LOG_WARNING(( "Invalid instr fixture." ));
    return 0;
  }

  int ok = 0;
  FD_SPAD_FRAME_BEGIN( runner->spad ) {
  // Execute
  void * output = NULL;
  sol_compat_execute_wrapper( runner, &fixture->input, &output, fd_exec_instr_test_run );

  // Compare effects
  ok = sol_compat_cmp_binary_strict( output, &fixture->output, &fd_exec_test_instr_effects_t_msg, runner->spad );
  } FD_SPAD_FRAME_END;

  // Cleanup
  pb_release( &fd_exec_test_instr_fixture_t_msg, fixture );
  return ok;
}

int
sol_compat_txn_fixture( fd_exec_instr_test_runner_t * runner,
                        uchar const *                 in,
                        ulong                         in_sz ) {
  // Decode fixture
  fd_exec_test_txn_fixture_t fixture[1] = {0};
  void * res = sol_compat_decode( &fixture, in, in_sz, &fd_exec_test_txn_fixture_t_msg );
  if ( res==NULL ) {
    FD_LOG_WARNING(( "Invalid txn fixture." ));
    return 0;
  }

  int ok = 0;
  FD_SPAD_FRAME_BEGIN( runner->spad ) {
  // Execute
  void * output = NULL;
  sol_compat_execute_wrapper( runner, &fixture->input, &output, fd_exec_txn_test_run );

  // Compare effects
  fd_exec_test_txn_result_t * effects = (fd_exec_test_txn_result_t *) output;
  ok = sol_compat_cmp_txn( &fixture->output, effects );
  } FD_SPAD_FRAME_END;

  // Cleanup
  pb_release( &fd_exec_test_txn_fixture_t_msg, fixture );
  return ok;
}

int
sol_compat_elf_loader_fixture( fd_exec_instr_test_runner_t * runner,
                               uchar const *                 in,
                               ulong                         in_sz ) {
  // Decode fixture
  fd_exec_test_elf_loader_fixture_t fixture[1] = {0};
  void * res = sol_compat_decode( &fixture, in, in_sz, &fd_exec_test_elf_loader_fixture_t_msg );
  if ( res==NULL ) {
    FD_LOG_WARNING(( "Invalid elf_loader fixture." ));
    return 0;
  }
  int ok = 0;
  FD_SPAD_FRAME_BEGIN( runner->spad ) {
  // Execute
  void * output = NULL;
  sol_compat_execute_wrapper( runner, &fixture->input, &output, fd_sbpf_program_load_test_run );

  // Compare effects
  ok = sol_compat_cmp_binary_strict( output, &fixture->output, &fd_exec_test_elf_loader_effects_t_msg, runner->spad );
  } FD_SPAD_FRAME_END;

  // Cleanup
  pb_release( &fd_exec_test_elf_loader_fixture_t_msg, fixture );
  return ok;
}

int
sol_compat_syscall_fixture( fd_exec_instr_test_runner_t * runner,
                            uchar const *                 in,
                            ulong                         in_sz ) {
  // Decode fixture
  fd_exec_test_syscall_fixture_t fixture[1] = {0};
  if ( !sol_compat_decode( &fixture, in, in_sz, &fd_exec_test_syscall_fixture_t_msg ) ) {
    FD_LOG_WARNING(( "Invalid syscall fixture." ));
    return 0;
  }

  int ok = 0;
  FD_SPAD_FRAME_BEGIN( runner->spad ) {
  // Execute
  void * output = NULL;
  sol_compat_execute_wrapper( runner, &fixture->input, &output, fd_exec_vm_syscall_test_run );

  // Compare effects
  ok = sol_compat_cmp_binary_strict( output, &fixture->output, &fd_exec_test_syscall_effects_t_msg, runner->spad );
  } FD_SPAD_FRAME_END;

  // Cleanup
  pb_release( &fd_exec_test_syscall_fixture_t_msg, fixture );
  return ok;
}

int
sol_compat_vm_interp_fixture( fd_exec_instr_test_runner_t * runner,
                              uchar const *                 in,
                              ulong                         in_sz ) {
  // Decode fixture
  fd_exec_test_syscall_fixture_t fixture[1] = {0};
  if ( !sol_compat_decode( &fixture, in, in_sz, &fd_exec_test_syscall_fixture_t_msg ) ) {
    FD_LOG_WARNING(( "Invalid syscall fixture." ));
    return 0;
  }

  int ok = 0;
  FD_SPAD_FRAME_BEGIN( runner->spad ) {
  // Execute
  void * output = NULL;
  sol_compat_execute_wrapper( runner, &fixture->input, &output, (exec_test_run_fn_t *)fd_exec_vm_interp_test_run );

  // Compare effects
  ok = sol_compat_cmp_binary_strict( output, &fixture->output, &fd_exec_test_syscall_effects_t_msg, runner->spad );
  } FD_SPAD_FRAME_END;

  // Cleanup
  pb_release( &fd_exec_test_syscall_fixture_t_msg, fixture );
  return ok;
}

/*
 * execute_v1
 */

int
sol_compat_instr_execute_v1( uchar *       out,
                             ulong *       out_sz,
                             uchar const * in,
                             ulong         in_sz ) {
  // Setup
  fd_exec_instr_test_runner_t * runner = sol_compat_setup_runner();

  // Decode context
  fd_exec_test_instr_context_t input[1] = {0};
  void * res = sol_compat_decode( &input, in, in_sz, &fd_exec_test_instr_context_t_msg );
  if ( res==NULL ) {
    sol_compat_cleanup_runner( runner );
    return 0;
  }

  int ok = 0;
  FD_SPAD_FRAME_BEGIN( runner->spad ) {
  // Execute
  void * output = NULL;
  sol_compat_execute_wrapper( runner, input, &output, fd_exec_instr_test_run );

  // Encode effects
  if( output ) {
    ok = !!sol_compat_encode( out, out_sz, output, &fd_exec_test_instr_effects_t_msg );
  }
  } FD_SPAD_FRAME_END;

  // Cleanup
  pb_release( &fd_exec_test_instr_context_t_msg, input );
  sol_compat_cleanup_runner( runner );

  // Check wksp usage is 0
  sol_compat_check_wksp_usage();

  return ok;
}

int
sol_compat_txn_execute_v1( uchar *       out,
                           ulong *       out_sz,
                           uchar const * in,
                           ulong         in_sz ) {
  // Setup
  fd_exec_instr_test_runner_t * runner = sol_compat_setup_runner();

  // Decode context
  fd_exec_test_txn_context_t input[1] = {0};
  void * res = sol_compat_decode( &input, in, in_sz, &fd_exec_test_txn_context_t_msg );
  if ( res==NULL ) {
    sol_compat_cleanup_runner( runner );
    return 0;
  }

  int ok = 0;
  FD_SPAD_FRAME_BEGIN( runner->spad ) {
  // Execute
  void * output = NULL;
  sol_compat_execute_wrapper( runner, input, &output, fd_exec_txn_test_run );

  // Encode effects
  if( output ) {
    ok = !!sol_compat_encode( out, out_sz, output, &fd_exec_test_txn_result_t_msg );
  }
  } FD_SPAD_FRAME_END;

  // Cleanup
  pb_release( &fd_exec_test_txn_context_t_msg, input );
  sol_compat_cleanup_runner( runner );

  // Check wksp usage is 0
  sol_compat_check_wksp_usage();
  return ok;
}

int
sol_compat_vm_syscall_execute_v1( uchar *       out,
                                  ulong *       out_sz,
                                  uchar const * in,
                                  ulong         in_sz ) {
  // Setup
  fd_exec_instr_test_runner_t * runner = sol_compat_setup_runner();

  // Decode context
  fd_exec_test_syscall_context_t input[1] = {0};
  void * res = sol_compat_decode( &input, in, in_sz, &fd_exec_test_syscall_context_t_msg );
  if ( res==NULL ) {
    sol_compat_cleanup_runner( runner );
    return 0;
  }

  int ok = 0;
  FD_SPAD_FRAME_BEGIN( runner->spad ) {
  // Execute
  void * output = NULL;
  sol_compat_execute_wrapper( runner, input, &output, fd_exec_vm_syscall_test_run );

  // Encode effects
  if( output ) {
    ok = !!sol_compat_encode( out, out_sz, output, &fd_exec_test_syscall_effects_t_msg );
  }
  } FD_SPAD_FRAME_END;

  // Cleanup
  pb_release( &fd_exec_test_syscall_context_t_msg, input );
  sol_compat_cleanup_runner( runner );

  // Check wksp usage is 0
  sol_compat_check_wksp_usage();

  return ok;
}

/* We still need a separate entrypoint since other harnesses (namely sfuzz-agave)
   do something other than wrap their vm_syscall equivalent */
int
sol_compat_vm_cpi_syscall_v1( uchar *       out,
                              ulong *       out_sz,
                              uchar const * in,
                              ulong         in_sz ) {
  /* Just a wrapper to vm_syscall_execute_v1 */
  return sol_compat_vm_syscall_execute_v1( out, out_sz, in, in_sz );
}

int
sol_compat_vm_interp_v1( uchar *       out,
                         ulong *       out_sz,
                         uchar const * in,
                         ulong         in_sz ) {
  // Setup
  fd_exec_instr_test_runner_t * runner = sol_compat_setup_runner();

  // Decode context
  fd_exec_test_syscall_context_t input[1] = {0};
  void * res = sol_compat_decode( &input, in, in_sz, &fd_exec_test_syscall_context_t_msg );
  if ( res==NULL ) {
    sol_compat_cleanup_runner( runner );
    return 0;
  }

  int ok = 0;

  FD_SPAD_FRAME_BEGIN( runner->spad ) {
  // Execute
  void * output = NULL;
  sol_compat_execute_wrapper( runner, input, &output, (exec_test_run_fn_t *)fd_exec_vm_interp_test_run );

  // Encode effects
  if( output ) {
    ok = !!sol_compat_encode( out, out_sz, output, &fd_exec_test_syscall_effects_t_msg );
  }
  } FD_SPAD_FRAME_END;

  // Cleanup
  pb_release( &fd_exec_test_syscall_context_t_msg, input );
  sol_compat_cleanup_runner( runner );

  // Check wksp usage is 0
  sol_compat_check_wksp_usage();

  return ok;
}

int sol_compat_shred_parse_v1( uchar *       out,
                               ulong *       out_sz,
                               uchar const * in,
                               ulong         in_sz ) {
    fd_exec_test_shred_binary_t input[1] = {0};
    void                      * res      = sol_compat_decode( &input, in, in_sz, &fd_exec_test_shred_binary_t_msg );
    if( FD_UNLIKELY( res==NULL ) ) {
        return 0;
    }
    if( FD_UNLIKELY( input[0].data==NULL ) ) {
        pb_release( &fd_exec_test_shred_binary_t_msg, input );
        return 0;
    }
    fd_exec_test_accepts_shred_t output[1] = {0};
    output[0].valid                        = !!fd_shred_parse( input[0].data->bytes, input[0].data->size );
    pb_release( &fd_exec_test_shred_binary_t_msg, input );
    return !!sol_compat_encode( out, out_sz, output, &fd_exec_test_accepts_shred_t_msg );
}

int
sol_compat_pack_compute_budget_v1( uchar *       out,
                                   ulong *       out_sz,
                                   uchar const * in,
                                   ulong         in_sz ) {
  fd_exec_instr_test_runner_t * runner = sol_compat_setup_runner( );

  fd_exec_test_pack_compute_budget_context_t input[1] = {0};
  void * res = sol_compat_decode( &input, in, in_sz, &fd_exec_test_pack_compute_budget_context_t_msg );
  if( res==NULL ) {
    sol_compat_cleanup_runner( runner );
    return 0;
  }

  int ok = 0;
  FD_SPAD_FRAME_BEGIN( runner->spad ) {
  void * output = NULL;
  sol_compat_execute_wrapper( runner, input, &output, fd_exec_pack_cpb_test_run );

  if( output ) {
    ok = !!sol_compat_encode( out, out_sz, output, &fd_exec_test_pack_compute_budget_effects_t_msg );
  }
  } FD_SPAD_FRAME_END;

  pb_release( &fd_exec_test_pack_compute_budget_context_t_msg, input );
  sol_compat_cleanup_runner( runner );

  // Check wksp usage is 0
  sol_compat_check_wksp_usage();
  return ok;
}
