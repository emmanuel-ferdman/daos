"""
  (C) Copyright 2020-2023 Intel Corporation.
  (C) Copyright 2025 Hewlett Packard Enterprise Development LP

  SPDX-License-Identifier: BSD-2-Clause-Patent
"""
from apricot import TestWithServers
from command_utils_base import BasicParameter, ObjectWithParameters


class RebuildTestParams(ObjectWithParameters):
    # pylint: disable=too-few-public-methods
    """Class for gathering test parameters."""

    def __init__(self):
        """Initialize a RebuildTestParams object."""
        super().__init__("/run/rebuild/*")
        self.object_class = BasicParameter(None)
        self.rank = BasicParameter(None)


class RebuildTestBase(TestWithServers):
    """Base rebuild test class.

    :avocado: recursive
    """

    def __init__(self, *args, **kwargs):
        """Initialize a RebuildTestBase object."""
        super().__init__(*args, **kwargs)
        self.inputs = RebuildTestParams()
        self.targets = None
        self.server_count = 0
        self.info_checks = None
        self.rebuild_checks = None

    def setUp(self):
        """Set up each test case."""
        # Start the servers and agents
        super().setUp()

        # Get the test parameters
        self.inputs.get_params(self)

        # Get the number of targets per engine for pool info calculations
        self.targets = self.server_managers[0].get_config_value("targets")

        self.server_count = len(self.hostlist_servers)

    def setup_test_pool(self):
        """Define a TestPool object."""
        self.add_pool(create=False)

    def setup_test_container(self):
        """Define a TestContainer object."""
        self.add_container(self.pool, create=False)

    def setup_pool_verify(self):
        """Set up pool verification initial expected values."""
        self.info_checks = {
            "pi_uuid": self.pool.uuid,
            "pi_nnodes": self.server_count,
            "pi_ntargets": (self.server_count * self.targets),
            "pi_ndisabled": 0,
        }
        self.rebuild_checks = {
            "rs_state": 1,
            "rs_obj_nr": 0,
            "rs_rec_nr": 0,
            "rs_errno": 0,
        }

    def update_pool_verify(self):
        """Update the pool verification expected values."""
        self.info_checks["pi_ndisabled"] = ">0"
        self.rebuild_checks["rs_state"] = 2
        self.rebuild_checks["rs_obj_nr"] = ">=0"
        self.rebuild_checks["rs_rec_nr"] = ">=0"

    def execute_pool_verify(self, msg=None):
        """Verify the pool info.

        Args:
            msg (str, optional): additional information to include in the error
                message. Defaults to None.
        """
        status = self.pool.check_pool_info(**self.info_checks)
        status &= self.pool.check_rebuild_status(**self.rebuild_checks)
        self.assertTrue(
            status,
            "Error confirming pool info{}".format("" if msg is None else msg))

    def create_test_pool(self):
        """Create the pool and verify its info."""
        # Create a pool
        self.pool.create()

        # Verify the pool information before rebuild
        self.setup_pool_verify()
        self.execute_pool_verify(" before rebuild")

    def create_test_container(self):
        """Create a container and write objects."""
        if self.container is not None:
            self.container.create()
            self.container.write_objects(
                self.inputs.rank.value, self.inputs.object_class.value)

    def verify_rank_has_objects(self):
        """Verify the rank to be excluded has at least one object."""
        if self.container is not None:
            rank = self.inputs.rank.value
            rank_list = self.container.get_target_rank_lists(" before rebuild")
            qty = self.container.get_target_rank_count(rank, rank_list)
            self.assertGreater(
                qty, 0, "No objects written to rank {}".format(rank))

    def verify_rank_has_no_objects(self):
        """Verify the excluded rank has zero objects."""
        if self.container is not None:
            rank = self.inputs.rank.value
            rank_list = self.container.get_target_rank_lists(" after rebuild")
            qty = self.container.get_target_rank_count(rank, rank_list)
            self.assertEqual(
                qty, 0, "Excluded rank {} still has objects".format(rank))

    def start_rebuild(self):
        """Start the rebuild process."""
        # Exclude the rank from the pool to initiate rebuild
        if isinstance(self.inputs.rank.value, list):
            self.server_managers[0].stop_ranks(self.inputs.rank.value, force=True)
        else:
            self.server_managers[0].stop_ranks([self.inputs.rank.value], force=True)

        # Wait for rebuild to start
        self.pool.wait_for_rebuild_to_start(1)

    def execute_during_rebuild(self):
        """Execute test steps during rebuild."""

    def verify_container_data(self, txn=None):
        """Verify the container data.

        Args:
            txn (int, optional): transaction timestamp to read. Defaults to None
                which uses the last timestamp written.
        """
        if self.container is not None:
            self.assertTrue(
                self.container.read_objects(txn),
                "Error verifying container data")

    def execute_rebuild_test(self, create_container=True):
        """Execute the rebuild test steps.

        Args:
            create_container (bool, optional): should the test create a
                container. Defaults to True.
        """
        # Get the test params
        self.setup_test_pool()
        if create_container:
            self.setup_test_container()

        # Create a pool and verify the pool information before rebuild
        self.create_test_pool()

        # Create a container and write objects
        self.create_test_container()

        # Verify the rank to be excluded has at least one object
        self.verify_rank_has_objects()

        # Start the rebuild process
        self.start_rebuild()

        # Execute the test steps during rebuild
        self.execute_during_rebuild()

        # Confirm rebuild completes
        self.pool.wait_for_rebuild_to_end(1)

        # clear container status for the RF issue
        self.container.set_prop(prop="status", value="healthy")

        # Refresh local pool and container
        self.pool.check_pool_info()
        self.container.query()

        # Verify the excluded rank is no longer used with the objects
        self.verify_rank_has_no_objects()

        # Verify the pool information after rebuild
        self.update_pool_verify()
        self.execute_pool_verify(" after rebuild")

        # Verify the container data can still be accessed
        self.verify_container_data()

        self.log.info("Test passed")
