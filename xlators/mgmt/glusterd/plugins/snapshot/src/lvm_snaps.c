#include <inttypes.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/statvfs.h>
#include <sys/mount.h>
#include <signal.h>
#include "glusterd-messages.h"
#include "glusterd-errno.h"

#define GF_LINUX_HOST_OS 1

#if defined(GF_LINUX_HOST_OS)
#include <mntent.h>
#else
#include "mntent_compat.h"
#endif

#ifdef __NetBSD__
#define umount2(dir, flags) unmount(dir, ((flags) != 0) ? MNT_FORCE : 0)
#endif

#if defined(GF_DARWIN_HOST_OS) || defined(__FreeBSD__)
#include <sys/param.h>
#include <sys/mount.h>
#define umount2(dir, flags) unmount(dir, ((flags) != 0) ? MNT_FORCE : 0)
#endif

#include <regex.h>
#include "globals.h"
#include "xlator.h"
#include "logging.h"
#include "run.h"
#include "syscall.h"


#include "lvm_snaps.h"
#include "lvm-defaults.h"

/* This function is called to get the device path of the snap lvm. Usually
   if /dev/mapper/<group-name>-<lvm-name> is the device for the lvm,
   then the snap device will be /dev/<group-name>/<snapname>.
   This function takes care of building the path for the snap device.
*/

char *
glusterd_build_snap_device_path (char *device, char *snapname,
                                 int32_t brickcount)
{
        char        snap[PATH_MAX]      = "";
        char        msg[1024]           = "";
        char        volgroup[PATH_MAX]  = "";
        char       *snap_device         = NULL;
        runner_t    runner              = {0,};
        char       *ptr                 = NULL;
        int         ret                 = -1;
	xlator_t             *this        = NULL;

	this = THIS;
	GF_ASSERT (this);

        if (!device) {
                gf_msg (this->name, GF_LOG_ERROR, EINVAL,
                        GD_MSG_INVALID_ENTRY,
                        "device is NULL");
                goto out;
        }
        if (!snapname) {
                gf_msg (this->name, GF_LOG_ERROR, EINVAL,
                        GD_MSG_INVALID_ENTRY,
                        "snapname is NULL");
                goto out;
        }

        runinit (&runner);
        runner_add_args (&runner, "/sbin/lvs", "--noheadings", "-o", "vg_name",
                         device, NULL);
        runner_redir (&runner, STDOUT_FILENO, RUN_PIPE);
        snprintf (msg, sizeof (msg), "Get volume group for device %s", device);
        runner_log (&runner, this->name, GF_LOG_DEBUG, msg);
        ret = runner_start (&runner);
        if (ret == -1) {
                gf_msg (this->name, GF_LOG_ERROR, errno,
                        GD_MSG_VG_GET_FAIL, "Failed to get volume group "
                        "for device %s", device);
                runner_end (&runner);
                goto out;
        }
        ptr = fgets(volgroup, sizeof(volgroup),
                    runner_chio (&runner, STDOUT_FILENO));
        if (!ptr || !strlen(volgroup)) {
                gf_msg (this->name, GF_LOG_ERROR, errno,
                        GD_MSG_VG_GET_FAIL, "Failed to get volume group "
                        "for snap %s", snapname);
                runner_end (&runner);
                ret = -1;
                goto out;
        }
        runner_end (&runner);

        snprintf (snap, sizeof(snap), "/dev/%s/%s_%d", gf_trim(volgroup),
                  snapname, brickcount);
        snap_device = gf_strdup (snap);
        if (!snap_device) {
                gf_msg (this->name, GF_LOG_WARNING, ENOMEM,
                        GD_MSG_NO_MEMORY,
                        "Cannot copy the snapshot device name for snapname: %s",
                        snapname);
        }

out:
        return snap_device;
}

/* This function actually calls the command (or the API) for taking the
   snapshot of the backend brick filesystem. If this is successful,
   then call the glusterd_snap_create function to create the snap object
   for glusterd
*/
int32_t
glusterd_take_lvm_snapshot (char *device_path,
                            char *origin_brick_path, 
			    char *origin_device)
{
        char             msg[NAME_MAX]    = "";
        char             buf[PATH_MAX]    = "";
        char            *ptr              = NULL;
        int              ret              = -1;
        gf_boolean_t     match            = _gf_false;
        runner_t         runner           = {0,};
	xlator_t             *this        = NULL;

	this = THIS;

	GF_ASSERT (this);
        GF_ASSERT (device_path);
        GF_ASSERT (origin_brick_path);

        /* Figuring out if setactivationskip flag is supported or not */
        runinit (&runner);
        snprintf (msg, sizeof (msg), "running lvcreate help");
        runner_add_args (&runner, LVM_CREATE, "--help", NULL);
        runner_log (&runner, "", GF_LOG_DEBUG, msg);
        runner_redir (&runner, STDOUT_FILENO, RUN_PIPE);
        ret = runner_start (&runner);
        if (ret) {
                gf_msg (this->name, GF_LOG_ERROR, errno,
                        GD_MSG_LVCREATE_FAIL,
                        "Failed to run lvcreate help");
                runner_end (&runner);
                goto out;
        }

        /* Looking for setactivationskip in lvcreate --help */
        do {
                ptr = fgets(buf, sizeof(buf),
                            runner_chio (&runner, STDOUT_FILENO));
                if (ptr) {
                        if (strstr(buf, "setactivationskip")) {
                                match = _gf_true;
                                break;
                        }
                }
        } while (ptr != NULL);
        runner_end (&runner);

        /* Taking the actual snapshot */
        runinit (&runner);
        snprintf (msg, sizeof (msg), "taking snapshot of the brick %s",
                  origin_brick_path);
        if (match == _gf_true)
                runner_add_args (&runner, LVM_CREATE, "-s", origin_device,
                                 "--setactivationskip", "n", "--name",
                                 device_path, NULL);
        else
                runner_add_args (&runner, LVM_CREATE, "-s", origin_device,
                                 "--name", device_path, NULL);
        runner_log (&runner, this->name, GF_LOG_DEBUG, msg);
        ret = runner_run (&runner);
        if (ret) {
                gf_msg (this->name, GF_LOG_ERROR, 0,
                        GD_MSG_SNAP_CREATION_FAIL, "taking snapshot of the "
                        "brick (%s) of device %s failed",
                        origin_brick_path, origin_device);
        }

out:
        return ret;
}

int
glusterd_get_brick_lvm_details (dict_t *rsp_dict,char *volname,
                                char *device,char *key_prefix)
{

        int                     ret             =       -1;
        runner_t                runner          =       {0,};
        char                    msg[PATH_MAX]   =       "";
        char                    buf[PATH_MAX]   =       "";
        char                    *ptr            =       NULL;
        char                    *token          =       NULL;
        char                    key[PATH_MAX]   =       "";
        char                    *value          =       NULL;
	xlator_t             *this        = NULL;

	this = THIS;

	GF_ASSERT (this);
        GF_ASSERT (rsp_dict);
        GF_ASSERT (volname);

        runinit (&runner);
        snprintf (msg, sizeof (msg), "running lvs command, "
                  "for getting snap status");
        /* Using lvs command fetch the Volume Group name,
         * Percentage of data filled and Logical Volume size
         *
         * "-o" argument is used to get the desired information,
         * example : "lvs /dev/VolGroup/thin_vol -o vgname,lv_size",
         * will get us Volume Group name and Logical Volume size.
         *
         * Here separator used is ":",
         * for the above given command with separator ":",
         * The output will be "vgname:lvsize"
         */
        runner_add_args (&runner, LVS, device, "--noheading", "-o",
                         "vg_name,data_percent,lv_size",
                         "--separator", ":", NULL);
        runner_redir (&runner, STDOUT_FILENO, RUN_PIPE);
        runner_log (&runner, "", GF_LOG_DEBUG, msg);
        ret = runner_start (&runner);
        if (ret) {
                gf_msg (this->name, GF_LOG_ERROR, errno,
                        GD_MSG_LVS_FAIL,
                        "Could not perform lvs action");
                goto end;
        }
        do {
                ptr = fgets (buf, sizeof (buf),
                             runner_chio (&runner, STDOUT_FILENO));

                if (ptr == NULL)
                        break;
                token = strtok (buf, ":");
                if (token != NULL) {
                        while (token && token[0] == ' ')
                                token++;
                        if (!token) {
                                ret = -1;
                                gf_msg (this->name, GF_LOG_ERROR, EINVAL,
                                        GD_MSG_INVALID_ENTRY,
                                        "Invalid vg entry");
                                goto end;
                        }
                        value = gf_strdup (token);
                        if (!value) {
                                ret = -1;
                                goto end;
                        }
                        ret = snprintf (key, sizeof (key), "%s.vgname",
                                        key_prefix);
                        if (ret < 0) {
                                goto end;
                        }

                        ret = dict_set_dynstr (rsp_dict, key, value);
                        if (ret) {
                                gf_msg (this->name, GF_LOG_ERROR, 0,
                                        GD_MSG_DICT_SET_FAILED,
                                        "Could not save vgname ");
                                goto end;
                        }
                }

                token = strtok (NULL, ":");
                if (token != NULL) {
                        value = gf_strdup (token);
                        if (!value) {
                                ret = -1;
                                goto end;
                        }
                        ret = snprintf (key, sizeof (key), "%s.data",
                                        key_prefix);
                        if (ret < 0) {
                                goto end;
                        }

                        ret = dict_set_dynstr (rsp_dict, key, value);
                        if (ret) {
                                gf_msg (this->name, GF_LOG_ERROR, 0,
                                        GD_MSG_DICT_SET_FAILED,
                                        "Could not save data percent ");
                                goto end;
                        }
                }
                token = strtok (NULL, ":");
                if (token != NULL) {
                        value = gf_strdup (token);
                        if (!value) {
                                ret = -1;
                                goto end;
                        }
                        ret = snprintf (key, sizeof (key), "%s.lvsize",
                                        key_prefix);
                        if (ret < 0) {
                                goto end;
                        }

                        ret = dict_set_dynstr (rsp_dict, key, value);
                        if (ret) {
                                gf_msg (this->name, GF_LOG_ERROR, 0,
                                        GD_MSG_DICT_SET_FAILED,
                                        "Could not save meta data percent ");
                                goto end;
                        }
                }

        } while (ptr != NULL);

        ret = 0;

end:
        runner_end (&runner);

out:
        if (ret && value) {
                GF_FREE (value);
        }

        return ret;
}

int32_t 
_glusterd_lvm_snapshot_remove(char *brick_path, char *hostname, const char *snap_device)
{
     
     runner_t                runner            = {0,};
     char                    msg[1024]         = {0, };
     int                     ret               = -1;
     xlator_t             *this        = NULL;

     this = THIS;
     GF_ASSERT (this);
     
     runinit (&runner);
     snprintf (msg, sizeof(msg), "remove snapshot of the brick %s:%s, "
	       "device: %s", hostname, brick_path,
	       snap_device);
     runner_add_args (&runner, LVM_REMOVE, "-f", snap_device, NULL);
     runner_log (&runner, "", GF_LOG_DEBUG, msg);

     ret = runner_run (&runner);
     if (ret) {
	  gf_msg (this->name, GF_LOG_ERROR, 0,
		  GD_MSG_SNAP_REMOVE_FAIL, "removing snapshot of the "
		  "brick (%s:%s) of device %s failed",
		  hostname, brick_path, snap_device);
	  goto out;
     }

out:     
     return ret;
}

gf_boolean_t
mntopts_exists (const char *str, const char *opts)
{
        char          *dup_val     = NULL;
        char          *savetok     = NULL;
        char          *token       = NULL;
        gf_boolean_t   exists      = _gf_false;

        GF_ASSERT (opts);

        if (!str || !strlen(str))
                goto out;

        dup_val = gf_strdup (str);
        if (!dup_val)
                goto out;

        token = strtok_r (dup_val, ",", &savetok);
        while (token) {
                if (!strcmp (token, opts)) {
                        exists = _gf_true;
                        goto out;
                }
                token = strtok_r (NULL, ",", &savetok);
        }

out:
        GF_FREE (dup_val);
        return exists;
}

int32_t
glusterd_mount_lvm_snapshot (char *brick_mount_path, 
			     char *device_path, 
			     char *_mnt_opts, 
			     char *fstype)
{
        char               msg[NAME_MAX]  = "";
        char               mnt_opts[1024] = "";
        int32_t            ret            = -1;
        runner_t           runner         = {0, };
	xlator_t             *this        = NULL;

	this = THIS;

	GF_ASSERT (this);
        GF_ASSERT (brick_mount_path);


        runinit (&runner);
        snprintf (msg, sizeof (msg), "mount %s %s",
                  device_path, brick_mount_path);

        strcpy (mnt_opts, _mnt_opts);

        /* XFS file-system does not allow to mount file-system with duplicate
         * UUID. File-system UUID of snapshot and its origin volume is same.
         * Therefore to mount such a snapshot in XFS we need to pass nouuid
         * option
         */
        if (!strcmp (fstype, "xfs") &&
            !mntopts_exists (mnt_opts, "nouuid")) {
                if (strlen (mnt_opts) > 0)
                        strcat (mnt_opts, ",");
                strcat (mnt_opts, "nouuid");
        }



        if (strlen (mnt_opts) > 0) {
                runner_add_args (&runner, "mount", "-o", mnt_opts,
                                device_path, brick_mount_path, NULL);
        } else {
                runner_add_args (&runner, "mount", device_path,
                                 brick_mount_path, NULL);
        }

        runner_log (&runner, this->name, GF_LOG_DEBUG, msg);
        ret = runner_run (&runner);
        if (ret) {
                gf_msg (this->name, GF_LOG_ERROR, 0,
                        GD_MSG_SNAP_MOUNT_FAIL, "mounting the snapshot "
                        "logical device %s failed (error: %s)",
                        device_path, strerror (errno));
                goto out;
        } else
                gf_msg_debug (this->name, 0, "mounting the snapshot "
                        "logical device %s successful", device_path);

out:
        gf_msg_trace (this->name, 0, "Returning with %d", ret);
        return ret;
}
