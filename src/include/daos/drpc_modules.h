/*
 * (C) Copyright 2019-2024 Intel Corporation.
 * (C) Copyright 2025 Hewlett Packard Enterprise Development LP
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#ifndef __DAOS_DRPC_MODULES_H__
#define __DAOS_DRPC_MODULES_H__

/**
 * DAOS dRPC Modules
 *
 * dRPC modules are used to multiplex communications over the Unix Domain Socket
 * to appropriate handlers. They are populated in the Drpc__Call structure.
 *
 * dRPC module IDs must be unique. This is a list of all DAOS dRPC modules.
 */

enum drpc_module {
	DRPC_MODULE_TEST		= 0,	/* Reserved for testing */
	DRPC_MODULE_SEC_AGENT		= 1,	/* daos_agent security */
	DRPC_MODULE_MGMT		= 2,	/* daos_server mgmt */
	DRPC_MODULE_SRV			= 3,	/* daos_server */
	DRPC_MODULE_SEC			= 4,	/* daos_server security */

	NUM_DRPC_MODULES			/* Must be last */
};

enum drpc_sec_agent_method {
	DRPC_METHOD_SEC_AGENT_REQUEST_CREDS	= 101,

	NUM_DRPC_SEC_AGENT_METHODS		/* Must be last */
};

enum drpc_mgmt_method {
	DRPC_METHOD_MGMT_KILL_RANK              = 201,
	DRPC_METHOD_MGMT_SET_RANK               = 202,
	DRPC_METHOD_MGMT_GET_ATTACH_INFO        = 206,
	DRPC_METHOD_MGMT_POOL_CREATE            = 207,
	DRPC_METHOD_MGMT_POOL_DESTROY           = 208,
	DRPC_METHOD_MGMT_SET_UP                 = 209,
	DRPC_METHOD_MGMT_BIO_HEALTH_QUERY       = 210,
	DRPC_METHOD_MGMT_SMD_LIST_DEVS          = 211,
	DRPC_METHOD_MGMT_SMD_LIST_POOLS         = 212,
	DRPC_METHOD_MGMT_POOL_GET_ACL           = 213,
	DRPC_METHOD_MGMT_POOL_OVERWRITE_ACL     = 215,
	DRPC_METHOD_MGMT_POOL_UPDATE_ACL        = 216,
	DRPC_METHOD_MGMT_POOL_DELETE_ACL        = 217,
	DRPC_METHOD_MGMT_PREP_SHUTDOWN          = 218,
	DRPC_METHOD_MGMT_DEV_SET_FAULTY         = 220,
	DRPC_METHOD_MGMT_DEV_REPLACE            = 221,
	DRPC_METHOD_MGMT_LIST_CONTAINERS        = 222,
	DRPC_METHOD_MGMT_POOL_QUERY             = 223,
	DRPC_METHOD_MGMT_POOL_SET_PROP          = 224,
	DRPC_METHOD_MGMT_PING_RANK              = 225,
	DRPC_METHOD_MGMT_POOL_REINT             = 226,
	DRPC_METHOD_MGMT_CONT_SET_OWNER         = 227,
	DRPC_METHOD_MGMT_POOL_EXCLUDE           = 228,
	DRPC_METHOD_MGMT_POOL_EXTEND            = 229,
	DRPC_METHOD_MGMT_POOL_EVICT             = 230,
	DRPC_METHOD_MGMT_POOL_DRAIN             = 231,
	DRPC_METHOD_MGMT_GROUP_UPDATE           = 232,
	DRPC_METHOD_MGMT_NOTIFY_EXIT            = 233,
	DRPC_METHOD_MGMT_NOTIFY_POOL_CONNECT    = 235,
	DRPC_METHOD_MGMT_NOTIFY_POOL_DISCONNECT = 236,
	DRPC_METHOD_MGMT_POOL_GET_PROP          = 237,
	DRPC_METHOD_MGMT_SET_LOG_MASKS          = 238,
	DRPC_METHOD_MGMT_POOL_UPGRADE           = 239,
	DRPC_METHOD_MGMT_POOL_QUERY_TARGETS     = 240,
	DRPC_METHOD_MGMT_LED_MANAGE             = 241,
	DRPC_METHOD_MGMT_CHK_START              = 242,
	DRPC_METHOD_MGMT_CHK_STOP               = 243,
	DRPC_METHOD_MGMT_CHK_QUERY              = 244,
	DRPC_METHOD_MGMT_CHK_PROP               = 245,
	DRPC_METHOD_MGMT_CHK_ACT                = 246,
	DRPC_METHOD_MGMT_SETUP_CLIENT_TELEM     = 247,

	NUM_DRPC_MGMT_METHODS /* Must be last */
};

enum drpc_srv_method {
	DRPC_METHOD_SRV_NOTIFY_READY      = 301,
	DRPC_METHOD_SRV_GET_POOL_SVC      = 303,
	DRPC_METHOD_SRV_CLUSTER_EVENT     = 304,
	DRPC_METHOD_SRV_POOL_FIND_BYLABEL = 305,
	DRPC_METHOD_CHK_LIST_POOL         = 306,
	DRPC_METHOD_CHK_REG_POOL          = 307,
	DRPC_METHOD_CHK_DEREG_POOL        = 308,
	DRPC_METHOD_CHK_REPORT            = 309,
	DRPC_METHOD_SRV_LIST_POOLS        = 310,

	NUM_DRPC_SRV_METHODS /* Must be last */
};

enum drpc_sec_method {
	DRPC_METHOD_SEC_VALIDATE_CREDS		= 401,

	NUM_DRPC_SEC_METHODS			/* Must be last */
};

#endif /* __DAOS_DRPC_MODULES_H__ */
