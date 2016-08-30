#ifndef _LVM_SNAPS_H
#define _LVM_SNAPS_H

#include "dict.h"

char *
glusterd_build_snap_device_path (char *device, char *snapname,
                                 int32_t brickcount);

int32_t
glusterd_take_lvm_snapshot (char *device_path,
                            char *origin_brick_path, 
			    char *origin_device);
int
glusterd_get_brick_lvm_details (dict_t *rsp_dict,char *volname,
                                char *device,char *key_prefix);

int32_t 
_glusterd_lvm_snapshot_remove(char *brick_path, char *hostname, const char *snap_device);

gf_boolean_t
mntopts_exists (const char *str, const char *opts);

int32_t
glusterd_mount_lvm_snapshot (char *brick_mount_path, char *device_path, 
			     char *mnt_opts, char *fstype);

#endif
