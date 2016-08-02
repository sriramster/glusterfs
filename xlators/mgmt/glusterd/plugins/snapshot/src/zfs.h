#ifndef _ZFS_H
#define _ZFS_H

char *
glusterd_build_zfs_snap_device_path (char *snapname, int32_t brickcount,
				     glusterd_brickinfo_t  *brickinfo);

int
glusterd_snapshot_zfs_restore_cleanup (dict_t *rsp_dict,
				       glusterd_volinfo_t *volinfo,
				       glusterd_snap_t *snap);

int32_t
glusterd_take_zfs_snapshot (glusterd_brickinfo_t *brickinfo,
                            char *origin_brick_path);

int
glusterd_zfs_snap_restore (dict_t *dict, dict_t *rsp_dict,
			   glusterd_volinfo_t *snap_vol,
			   glusterd_volinfo_t *orig_vol,
			   int32_t volcount);

int
glusterd_get_brick_zfs_details (dict_t *rsp_dict,
				glusterd_brickinfo_t *brickinfo, char *volname,
                                char *device, char *key_prefix);

#endif
