/**
 * (C) Copyright 2016-2024 Intel Corporation.
 * (C) Copyright 2025 Hewlett Packard Enterprise Development LP
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#include "dfuse_common.h"
#include "dfuse.h"

#include <daos_uns.h>

char *duns_xattr_name = DUNS_XATTR_NAME;

void
dfuse_reply_entry(struct dfuse_info *dfuse_info, struct dfuse_inode_entry *ie,
		  struct fuse_file_info *fi_out, bool is_new, fuse_req_t req)
{
	struct fuse_entry_param  entry = {0};
	d_list_t		*rlink;
	ino_t			wipe_parent = 0;
	char			wipe_name[NAME_MAX + 1];
	int			rc;

	D_ASSERT(ie->ie_parent);
	D_ASSERT(ie->ie_dfs);

	ie->ie_root = (ie->ie_stat.st_ino == ie->ie_dfs->dfs_ino);

	entry.attr = ie->ie_stat;
	entry.generation = 1;
	entry.ino = entry.attr.st_ino;
	DFUSE_TRA_DEBUG(ie, "Inserting inode %#lx mode 0%o",
			entry.ino, ie->ie_stat.st_mode);

	rlink = d_hash_rec_find_insert(&dfuse_info->dpi_iet, &ie->ie_stat.st_ino,
				       sizeof(ie->ie_stat.st_ino), &ie->ie_htl);

	if (rlink != &ie->ie_htl) {
		struct dfuse_inode_entry *inode;

		inode = container_of(rlink, struct dfuse_inode_entry, ie_htl);

		if (fi_out) {
			struct dfuse_obj_hdl *oh;
			/* DAOS-12714 If create returns an existing file then oh->doe_ie will point
			 * to the stale ie in this case.  This can probably only happen when there
			 * is a race between a create call from one client and rename from a
			 * different client.
			 */
			oh         = (struct dfuse_obj_hdl *)fi_out->fh;
			oh->doh_ie = inode;
		}

		/* The lookup has resulted in an existing file, so reuse that
		 * entry, drop the inode in the lookup descriptor and do not
		 * keep a reference on the parent.
		 */

		/* Update the existing object with the new name/parent */

		DFUSE_TRA_DEBUG(inode, "inode dfs %p %#lx hi %#lx lo %#lx",
				inode->ie_dfs,
				inode->ie_dfs->dfs_ino,
				inode->ie_oid.hi,
				inode->ie_oid.lo);

		DFUSE_TRA_DEBUG(ie, "inode dfs %p %#lx hi %#lx lo %#lx",
				ie->ie_dfs,
				ie->ie_dfs->dfs_ino,
				ie->ie_oid.hi,
				ie->ie_oid.lo);

		/* Check for conflicts, in either the dfs or oid space.  This
		 * can happen because of the fact we squash larger identifiers
		 * into a shorter 64 bit space, but if the bitshifting is right
		 * it shouldn't happen until there are a large number of active
		 * files. DAOS-4928 has more details.
		 */
		if (ie->ie_dfs != inode->ie_dfs) {
			DFUSE_TRA_ERROR(inode, "Duplicate inode found (dfs)");
			D_GOTO(out_err, rc = EIO);
		}

		/* Check the OID */
		if (ie->ie_oid.lo != inode->ie_oid.lo ||
		    ie->ie_oid.hi != inode->ie_oid.hi) {
			DFUSE_TRA_ERROR(inode, "Duplicate inode found (oid)");
			D_GOTO(out_err, rc = EIO);
		}

		/* Make the inode active for the create case */
		if (ie->ie_active) {
			D_ASSERT(atomic_load_relaxed(&ie->ie_open_count) == 1);
			active_ie_decref(dfuse_info, ie);
			rc = active_ie_init(inode, NULL);
			if (rc != -DER_SUCCESS) {
				atomic_fetch_sub_relaxed(&ie->ie_ref, 1);
				dfuse_ie_close(dfuse_info, ie);
				D_GOTO(out_err, rc);
			}
		}

		DFUSE_TRA_DEBUG(inode,
				"Maybe updating parent inode %#lx dfs_ino %#lx",
				entry.ino, ie->ie_dfs->dfs_ino);

		/** update the chunk size and oclass of inode entry */
		dfs_obj_copy_attr(inode->ie_obj, ie->ie_obj);

		if (ie->ie_stat.st_ino == ie->ie_dfs->dfs_ino) {
			DFUSE_TRA_DEBUG(inode, "Not updating parent");
		} else if ((inode->ie_parent != ie->ie_parent) ||
			(strncmp(inode->ie_name, ie->ie_name, NAME_MAX) != 0)) {
			DFUSE_TRA_DEBUG(inode, "File has moved from " DF_DE " to " DF_DE,
					DP_DE(inode->ie_name), DP_DE(ie->ie_name));

			dfs_update_parent(inode->ie_obj, ie->ie_obj, ie->ie_name);

			/* Save the old name so that we can invalidate it in later */
			wipe_parent = inode->ie_parent;
			strncpy(wipe_name, inode->ie_name, NAME_MAX);
			wipe_name[NAME_MAX] = '\0';

			inode->ie_parent = ie->ie_parent;
			strncpy(inode->ie_name, ie->ie_name, NAME_MAX);
			inode->ie_name[NAME_MAX] = '\0';
		}
		atomic_fetch_sub_relaxed(&ie->ie_ref, 1);
		dfuse_ie_close(dfuse_info, ie);
		ie = inode;
	}

	/* Set the attr caching attributes of this entry.  The lookup may have resulted in a
	 * already known inode for while the interception library is already in use so check
	 * this and disable caching in this case.
	 */
	if (S_ISDIR(ie->ie_stat.st_mode))
		entry.entry_timeout = ie->ie_dfs->dfc_dentry_dir_timeout;
	else
		entry.entry_timeout = ie->ie_dfs->dfc_dentry_timeout;

	if ((atomic_load_relaxed(&ie->ie_il_count)) == 0)
		entry.attr_timeout = ie->ie_dfs->dfc_attr_timeout;

	ie->ie_stat = entry.attr;

	if (fi_out) {
		/* Now set the value of keep_cache, this is for creat where we need to do the hash
		 * table lookup before setting this value.
		 */
		if (atomic_load_relaxed(&ie->ie_open_count) > 1) {
			fi_out->keep_cache = 1;
		} else if (dfuse_dcache_get_valid(ie, ie->ie_dfs->dfc_data_timeout)) {
			fi_out->keep_cache = 1;
		}

		DFUSE_REPLY_CREATE(ie, req, entry, fi_out);
	} else {
		DFUSE_REPLY_ENTRY(ie, req, entry);
	}

	if (wipe_parent == 0)
		return;

	rc = fuse_lowlevel_notify_inval_entry(dfuse_info->di_session, wipe_parent, wipe_name,
					      strnlen(wipe_name, NAME_MAX));
	if (rc && rc != -ENOENT)
		DS_ERROR(-rc, "inval_entry() failed");

	return;
out_err:
	/* TODO: Verify ie reference here */
	dfs_release(ie->ie_obj);
	DFUSE_REPLY_ERR_RAW(ie, req, rc);
}

/* Check for and set a unified namespace entry point.
 *
 * This function will check for and configure a inode as
 * a new entry point of possible, and modify the inode
 * as required.
 *
 * On failure it will return error.
 *
 */
int
check_for_uns_ep(struct dfuse_info *dfuse_info, struct dfuse_inode_entry *ie, char *attr,
		 daos_size_t len)
{
	int			rc;
	struct duns_attr_t	dattr = {};
	struct dfuse_cont	*dfs = NULL;
	struct dfuse_pool	*dfp = NULL;

	rc = duns_parse_attr(attr, len, &dattr);
	if (rc)
		return rc;

	if (dattr.da_type != DAOS_PROP_CO_LAYOUT_POSIX)
		D_GOTO(out_err, rc = ENOTSUP);

	/* Search the currently connect dfp list, if one matches then use that,
	 * otherwise allocate a new one.
	 */

	rc = dfuse_pool_get_handle(dfuse_info, dattr.da_puuid, &dfp);
	if (rc != 0) {
		if (rc == ENOENT)
			rc = ENOLINK;
		goto out_err;
	}

	rc = dfuse_cont_get_handle(dfuse_info, dfp, dattr.da_cuuid, &dfs);
	if (rc != 0) {
		if (rc == ENOENT)
			rc = ENOLINK;
		goto out_dfp;
	}

	rc = dfs_release(ie->ie_obj);
	if (rc) {
		DFUSE_TRA_ERROR(dfs, "dfs_release() failed: %d (%s)", rc, strerror(rc));
		D_GOTO(out_dfs, rc);
	}
	ie->ie_obj = NULL;

	rc = dfs_lookup(dfs->dfs_ns, "/", O_RDWR, &ie->ie_obj, NULL, &ie->ie_stat);
	if (rc) {
		if (rc == EINVAL) {
			rc = ENOLINK;
			DHS_INFO(dfs, rc, "dfs_lookup() failed");
		} else {
			DHS_WARN(dfs, rc, "dfs_lookup() failed");
		}
		goto out_dfs;
	}

	/* The inode has a reference to the dfs, so keep that. */
	d_hash_rec_decref(&dfuse_info->di_pool_table, &dfp->dfp_entry);

	ie->ie_stat.st_ino = dfs->dfs_ino;

	dfs_obj2id(ie->ie_obj, &ie->ie_oid);

	ie->ie_dfs = dfs;

	DFUSE_TRA_DEBUG(dfs, "UNS entry point activated, root %#lx", dfs->dfs_ino);

	duns_destroy_attr(&dattr);

	return rc;
out_dfs:
	d_hash_rec_decref(dfp->dfp_cont_table, &dfs->dfs_entry);
out_dfp:
	d_hash_rec_decref(&dfuse_info->di_pool_table, &dfp->dfp_entry);
out_err:
	duns_destroy_attr(&dattr);

	return rc;
}

void
dfuse_cb_lookup(fuse_req_t req, struct dfuse_inode_entry *parent,
		const char *name)
{
	struct dfuse_info        *dfuse_info = fuse_req_userdata(req);
	struct dfuse_inode_entry *ie;
	int                       rc;
	char                      out[DUNS_MAX_XATTR_LEN];
	char                     *outp     = &out[0];
	daos_size_t               attr_len = DUNS_MAX_XATTR_LEN;

	DFUSE_TRA_DEBUG(parent, "Parent:%#lx " DF_DE, parent->ie_stat.st_ino, DP_DE(name));

	D_ALLOC_PTR(ie);
	if (!ie)
		D_GOTO(out, rc = ENOMEM);

	DFUSE_TRA_UP(ie, parent, "inode");

	dfuse_ie_init(dfuse_info, ie);

	ie->ie_parent = parent->ie_stat.st_ino;
	ie->ie_dfs = parent->ie_dfs;

	rc = dfs_lookupx(parent->ie_dfs->dfs_ns, parent->ie_obj, name,
			 O_RDWR | O_NOFOLLOW, &ie->ie_obj, NULL, &ie->ie_stat,
			 1, &duns_xattr_name, (void **)&outp, &attr_len);
	if (rc) {
		DFUSE_TRA_DEBUG(parent, "dfs_lookup() returned: %d (%s)", rc, strerror(rc));

		D_GOTO(out_free, rc);
	}

	if (attr_len)
		DFUSE_TRA_DEBUG(ie, "Attr len is %zi", attr_len);

	strncpy(ie->ie_name, name, NAME_MAX);
	ie->ie_name[NAME_MAX] = '\0';

	dfs_obj2id(ie->ie_obj, &ie->ie_oid);

	dfuse_compute_inode(ie->ie_dfs, &ie->ie_oid,
			    &ie->ie_stat.st_ino);

	if (S_ISDIR(ie->ie_stat.st_mode) && attr_len) {
		rc = check_for_uns_ep(dfuse_info, ie, out, attr_len);
		DFUSE_TRA_DEBUG(ie, "check_for_uns_ep() returned %d", rc);
		if (rc != 0)
			D_GOTO(out_release, rc);
	}

	dfuse_reply_entry(dfuse_info, ie, NULL, false, req);
	return;

out_release:
	dfs_release(ie->ie_obj);
out_free:
	dfuse_ie_free(dfuse_info, ie);
out:
	if (rc == ENOENT && parent->ie_dfs->dfc_ndentry_timeout > 0)
		DFUSE_REPLY_NO_ENTRY(parent, req, parent->ie_dfs->dfc_ndentry_timeout);
	else
		DFUSE_REPLY_ERR_RAW(parent, req, rc);
}
