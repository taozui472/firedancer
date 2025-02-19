#ifndef HEADER_fd_src_app_fdctl_config_h
#define HEADER_fd_src_app_fdctl_config_h

#include "../../disco/fd_disco.h"
#include "../../disco/topo/fd_topo.h"
#include "../../ballet/base58/fd_base58.h"

#include <net/if.h>
#include <linux/limits.h>

/* Maximum size of the name of this Firedancer instance. */
#define NAME_SZ 256

/* Maximum size of the string describing the CPU affinity of Firedancer */
#define AFFINITY_SZ 256

/* config_t represents all available configuration options that could be
   set in a user defined configuration toml file. For information about
   the options, see the `default.toml` file provided. */
struct fdctl_config {
  char name[ NAME_SZ ];
  char user[ 256 ];
  char hostname[ FD_LOG_NAME_MAX ];

  double tick_per_ns_mu;
  double tick_per_ns_sigma;

  fd_topo_t topo;

  char cluster[ 32 ];
  int is_live_cluster;

  uint uid;
  uint gid;

  char scratch_directory[ PATH_MAX ];

  char dynamic_port_range[ 32 ];

  struct {
    char path[ PATH_MAX ];
    char colorize[ 6 ];
    int  colorize1;
    char level_logfile[ 8 ];
    int  level_logfile1;
    char level_stderr[ 8 ];
    int  level_stderr1;
    char level_flush[ 8 ];
    int  level_flush1;

    /* File descriptor used for logging to the log file.  Stashed
       here for easy communication to child processes. */
    int  log_fd;

    /* Shared memfd_create file descriptor where the first 4
       bytes are the lock object for log sequencing.  Kind of
       gross to stash this in here. */
    int  lock_fd;
  } log;

  struct {
    char solana_metrics_config[ 512 ];
  } reporting;

  struct {
    char  path[ PATH_MAX ];
    char  accounts_path[ PATH_MAX ];
    uint  limit_size;
    ulong account_indexes_cnt;
    char  account_indexes[ 4 ][ 32 ];
    ulong account_index_include_keys_cnt;
    char  account_index_include_keys[ 32 ][ FD_BASE58_ENCODED_32_SZ ];
    ulong account_index_exclude_keys_cnt;
    char  account_index_exclude_keys[ 32 ][ FD_BASE58_ENCODED_32_SZ ];
    char  accounts_index_path[ PATH_MAX ];
    char  accounts_hash_cache_path[ PATH_MAX ];
    int   require_tower;
    char  snapshot_archive_format[ 10 ];
  } ledger;

  struct {
    ulong  entrypoints_cnt;
    char   entrypoints[ 16 ][ 256 ];
    int    port_check;
    ushort port;
    char   host[ 256 ];
  } gossip;

  struct {
    int    vote;
    char   identity_path[ PATH_MAX ];
    char   vote_account_path[ PATH_MAX ];
    ulong  authorized_voter_paths_cnt;
    char   authorized_voter_paths[ 16 ][ PATH_MAX ];
    int    snapshot_fetch;
    int    genesis_fetch;
    int    poh_speed_test;
    char   expected_genesis_hash[ FD_BASE58_ENCODED_32_SZ ];
    uint   wait_for_supermajority_at_slot;
    char   expected_bank_hash[ FD_BASE58_ENCODED_32_SZ ];
    ushort expected_shred_version;
    int    wait_for_vote_to_start_leader;
    ulong  hard_fork_at_slots_cnt;
    uint   hard_fork_at_slots[ 32 ];
    ulong  known_validators_cnt;
    char   known_validators[ 16 ][ 256 ];
    int    os_network_limits_test;
  } consensus;

  struct {
    ushort port;
    int    full_api;
    int    private;
    char   bind_address[ 16 ];
    int    transaction_history;
    int    extended_tx_metadata_storage;
    int    only_known;
    int    pubsub_enable_block_subscription;
    int    pubsub_enable_vote_subscription;
    int    bigtable_ledger_storage;
  } rpc;

  struct {
    int  enabled;
    int  incremental_snapshots;
    uint full_snapshot_interval_slots;
    uint incremental_snapshot_interval_slots;
    uint minimum_snapshot_download_speed;
    uint maximum_full_snapshots_to_retain;
    uint maximum_incremental_snapshots_to_retain;
    char path[ PATH_MAX ];
    char incremental_path[ PATH_MAX ];
  } snapshots;

  struct {
    char affinity[ AFFINITY_SZ ];
    char agave_affinity[ AFFINITY_SZ ];

    uint agave_unified_scheduler_handler_threads;
    uint net_tile_count;
    uint quic_tile_count;
    uint resolv_tile_count;
    uint verify_tile_count;
    uint bank_tile_count;
    uint shred_tile_count;
    uint exec_tile_count; /* TODO: redundant ish with bank tile cnt */
  } layout;

  struct {
    char gigantic_page_mount_path[ PATH_MAX ];
    char huge_page_mount_path[ PATH_MAX ];
    char mount_path[ PATH_MAX ];
    char max_page_size[ 16 ];
  } hugetlbfs;

  struct {
    ulong shred_max;
    ulong block_max;
    ulong idx_max;
    ulong txn_max;
    ulong alloc_max;
    char  file[PATH_MAX];
    char  checkpt[PATH_MAX];
    char  restore[PATH_MAX];
  } blockstore;

  struct {
    int sandbox;
    int no_clone;
    int no_agave;
    int bootstrap;
    uint debug_tile;

    struct {
      int  enabled;
      char interface0     [ 16 ];
      char interface0_mac [ 32 ];
      char interface0_addr[ 16 ];
      char interface1     [ 16 ];
      char interface1_mac [ 32 ];
      char interface1_addr[ 16 ];
    } netns;

    struct {
      int allow_private_address;
    } gossip;

    struct {
      ulong hashes_per_tick;
      ulong target_tick_duration_micros;
      ulong ticks_per_slot;
      ulong fund_initial_accounts;
      ulong fund_initial_amount_lamports;
      ulong vote_account_stake_lamports;
      int   warmup_epochs;
    } genesis;

    struct {
      uint  benchg_tile_count;
      uint  benchs_tile_count;
      char  affinity[ AFFINITY_SZ ];
      int   larger_max_cost_per_block;
      int   larger_shred_limits_per_block;
      ulong disable_blockstore_from_slot;
      int   disable_status_cache;
    } bench;

    struct {
      char affinity[ AFFINITY_SZ ];
      char fake_dst_ip[ 16 ];
    } pktgen;
  } development;

  struct {
    struct {
      char   interface[ IF_NAMESIZE ];
      uint   ip_addr;
      uchar  mac_addr[6];
      char   xdp_mode[ 8 ];
      int    xdp_zero_copy;

      uint xdp_rx_queue_size;
      uint xdp_tx_queue_size;
      uint flush_timeout_micros;

      uint send_buffer_size;
    } net;

    struct {
      ulong max_routes;
      ulong max_neighbors;
    } netlink;

    struct {
      ushort regular_transaction_listen_port;
      ushort quic_transaction_listen_port;

      uint txn_reassembly_count;
      uint max_concurrent_connections;
      uint max_concurrent_handshakes;
      uint idle_timeout_millis;
      uint ack_delay_millis;
      int  retry;

    } quic;

    struct {
      uint signature_cache_size;
      uint receive_buffer_size;
      uint mtu;
    } verify;

    struct {
      uint signature_cache_size;
    } dedup;

    struct {
      int  enabled;
      char url[ 256 ];
      char tls_domain_name[ 256 ];
    } bundle;

    struct {
      uint max_pending_transactions;
      int  use_consumed_cus;
    } pack;

    struct {
      int lagged_consecutive_leader_start;
    } poh;

    struct {
      uint   max_pending_shred_sets;
      ushort shred_listen_port;
    } shred;

    struct {
      char   prometheus_listen_address[ 16 ];
      ushort prometheus_listen_port;
    } metric;

    struct {
      int    enabled;
      char   gui_listen_address[ 16 ];
      ushort gui_listen_port;
    } gui;

    /* Firedancer-only tile configs */

    struct {
      ulong  entrypoints_cnt;
      char   entrypoints[16][256];
      ushort gossip_listen_port;
      ulong  peer_ports_cnt;
      ushort peer_ports[16];
    } gossip;

    struct {
      ushort repair_intake_listen_port;
      ushort repair_serve_listen_port;
      char   good_peer_cache_file[ PATH_MAX ];
    } repair;

    struct {
      char  capture[ PATH_MAX ];
      char  funk_checkpt[ PATH_MAX ];
      ulong funk_rec_max;
      ulong funk_sz_gb;
      ulong funk_txn_max;
      char  funk_file[ PATH_MAX ];
      char  genesis[ PATH_MAX ];
      char  incremental[ PATH_MAX ];
      char  slots_replayed[PATH_MAX ];
      char  snapshot[ PATH_MAX ];
      char  status_cache[ PATH_MAX ];
      ulong tpool_thread_count;
      char  cluster_version[ 32 ];
      int   in_wen_restart;
      char  tower_checkpt[ PATH_MAX ];
      char  wen_restart_coordinator[ FD_BASE58_ENCODED_32_SZ ];
    } replay;

    struct {
      char  slots_pending[PATH_MAX];
      char  shred_cap_archive[ PATH_MAX ];
      char  shred_cap_replay[ PATH_MAX ];
      ulong shred_cap_end_slot;
    } store_int;

    struct {
      ulong full_interval;
      ulong incremental_interval;
      char  out_dir[ PATH_MAX ];
      ulong hash_tpool_thread_count;
    } batch;

  } tiles;
};

typedef struct fdctl_config config_t;

FD_PROTOTYPES_BEGIN

/* memlock_max_bytes() returns, for the entire Firedancer application,
   what the maximum total amount of `mlock()`ed memory will be in any
   one process, aka. what the RLIMIT_MLOCK must be set to so that all
   `mlock()` calls we wish to make will always succeed. The upper
   bound here is the sum of all workspace sizes, but the real number
   is lower because no process uses all of the workspaces at once. */
ulong
memlock_max_bytes( config_t * const config );

/* fdctl_cfg_from_env() loads a full configuration object from the provided
   arguments or the environment. First, the `default.toml` file is
   loaded as a base, and then if a FIREDANCER_CONFIG_FILE environment
   variable is provided, or a --config <path> command line argument, the
   `toml` file at that path is loaded and applied on top of the default
   configuration. This exits the program if it encounters any issue
   while loading or parsing the configuration. */

void
fdctl_cfg_from_env( int *      pargc,
                    char ***   pargv,
                    config_t * config );

/* Create a memfd and write the contents of the config struct into it.
   Used when execve() a child process so that it can read back in the
   same config as we did. */

int
fdctl_cfg_to_memfd( config_t * config );

FD_PROTOTYPES_END

#endif /* HEADER_fd_src_app_fdctl_config_h */
