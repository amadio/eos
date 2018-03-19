.. index::
   single: Namespace in QuarkDB Configuration

.. _ns_quarkdb_configure:

Namespace in QuarkDB configuration
===================================

.. image:: quarkdb.jpg
   :scale: 40%
   :align: left

The following steps assume we are configuring an MGM node together with a QuarkDB cluster on the same machine. The QuarkDB cluster will consist of three instances running on different ports. The operating system is **CentOS7**.

| 

Installing packages
--------------------

Add the YUM repositories for **QuarkDB**, **EOS Citrine** and their dependencies.

.. code-block:: bash

  yum-config-manager --add-repo=http://storage-ci.web.cern.ch/storage-ci/quarkdb/tag/el7/x86_64/
  Loaded plugins: fastestmirror, kernel-module, ovl, protectbase, versionlock
  adding repo from: http://storage-ci.web.cern.ch/storage-ci/quarkdb/tag/el7/x86_64/

  [storage-ci.web.cern.ch_storage-ci_quarkdb_tag_el7_x86_64_]
  name=added from: http://storage-ci.web.cern.ch/storage-ci/quarkdb/tag/el7/x86_64/
  baseurl=http://storage-ci.web.cern.ch/storage-ci/quarkdb/tag/el7/x86_64/
  enabled=1

  yum-config-manager --add-repo=http://storage-ci.web.cern.ch/storage-ci/eos/citrine/tag/el-7/x86_64/
  Loaded plugins: fastestmirror, kernel-module, ovl, protectbase, versionlock
  adding repo from: http://storage-ci.web.cern.ch/storage-ci/eos/citrine/tag/el-7/x86_64/

  [storage-ci.web.cern.ch_storage-ci_eos_citrine_tag_el-7_x86_64_]
  name=added from: http://storage-ci.web.cern.ch/storage-ci/eos/citrine/tag/el-7/x86_64/
  baseurl=http://storage-ci.web.cern.ch/storage-ci/eos/citrine/tag/el-7/x86_64/
  enabled=1

  yum-config-manager --add-repo=http://storage-ci.web.cern.ch/storage-ci/eos/citrine-depend/el-7/x86_64/
  Loaded plugins: fastestmirror, kernel-module, ovl, protectbase, versionlock
  adding repo from: http://storage-ci.web.cern.ch/storage-ci/eos/citrine-depend/el-7/x86_64/

  [storage-ci.web.cern.ch_storage-ci_eos_citrine-depend_el-7_x86_64_]
  name=added from: http://storage-ci.web.cern.ch/storage-ci/eos/citrine-depend/el-7/x86_64/
  baseurl=http://storage-ci.web.cern.ch/storage-ci/eos/citrine-depend/el-7/x86_64/
  enabled=1

Install the **XRootD** packages (>= 4.8.1) from the *EPEL* repository.

.. code-block:: bash

  yum --disablerepo="*" --enablerepo="epel" install xrootd* --exclude=xrootd-fuse

Install the **QuarkDB** and **EOS** packages:

.. code-block:: bash

  yum install -y quarkdb quarkdb-debuginfo redis
  yum install -y --nogpgcheck --disablerepo="*" --enablerepo="storage*" libmicrohttpd*
  yum install -y --nogpgcheck --disablerepo="cern*" eos-server eos-client eos-rocksdb eos-testkeytab


Setup the QuarkDB cluster
-------------------------

We will run three instances of **QuarkDB** on ports 7001, 7002 and 7003. The default contents of ``/etc/xrootd/xrootd-quarkdb.cfg`` should be the following:

.. code-block:: bash

  if exec xrootd
    xrd.port 7777
    xrd.protocol redis:7777 /usr/lib64/libXrdQuarkDB.so
    redis.mode standalone
    redis.database /var/quarkdb
  fi

Using this as a reference, we start customizing the configuration files for our three QuarkDB instances:

.. code-block:: bash

   for i in {1..3}; do
     # Create one configuration file per instance
     cp /etc/xrootd/xrootd-quarkdb.cfg /etc/xrootd/xrootd-quarkdb${i}.cfg
     # Customize the port
     sed -i 's/7777/700'"${i}"'/g' /etc/xrootd/xrootd-quarkdb${i}.cfg
     # Customize the storage location
     sed -i 's/\/var\/quarkdb/\/var\/lib\/quarkdb\/qdb'"${i}"'/g' /etc/xrootd/xrootd-quarkdb${i}.cfg
     # Set the instance to run in "raft" mode
     sed -i 's/standalone/raft/g' /etc/xrootd/xrootd-quarkdb${i}.cfg
     # Add myself entry to the config
     sed -i 's/fi/ redis.myself localhost:700'"${i}"'\n&/' /etc/xrootd/xrootd-quarkdb${i}.cfg
     # Prepare the log and working directories for the instances
     mkdir -p /var/log/quarkdb/
     mkdir -p /var/spool/quarkdb/
     chown -R daemon:daemon /var/log/quarkdb/
     chown -R daemon:daemon /var/spool/quarkdb/
   done

All instances will run as user *daemon*. The ownership of the storage locations needs to be changed accordingly. For changing the ownership of the processes and the location of the log files, we can customize the systemd start-up script as follows:

.. code-block:: bash

   for i in {1..3}; do
     mkdir -p /usr/lib/systemd/system/xrootd@quarkdb${i}.service.d
   echo -e "[Service] \nExecStart= \nExecStart=/usr/bin/xrootd -l /var/log/quarkdb/xrootd.log -c /etc/xrootd/xrootd-%i.cfg -k fifo -s /var/run/quarkdb/xrootd-%i.pid -n %i \nUser=daemon \nGroup=daemon \n" > /usr/lib/systemd/system/xrootd@quarkdb${i}.service.d/custom.conf
   done

The next step is to initialize the **QuarkDB** database directory using the ``quarkdb-create`` command. For more details please consult the `QuarkDB documentation <https://quarkdb.web.cern.ch/quarkdb/docs/master/>`_.

.. code-block:: bash

   for i in {1..3}; do
     quarkdb-create --path /var/lib/quarkdb/qdb${i}/ --clusterID 0123456789 --nodes localhost:7001,localhost:7002,localhost:7003
     # Change ownership to daemon:daemon
     chown -R daemon:daemon /var/lib/quarkdb/qdb${i}/
   done

We can now start the **three QuarkDB** instances and they should soon reach a stable configuration with one master and two followers.

.. code-block:: bash

   # Start all the QuarkDB instances
   for i {1..3}; do
     systemctl start xrootd@quarkdb${i};
   done

   sleep 2

   # Check their status
   for i in {1..3}; do
     systemctl status xrootd@quarkdb${i}
   done

At this point the **QuarkDB** cluster should be up and running. The logs from the individual instances can be found in ``/var/log/quarkdb/quarkdb[1-3]/xrootd.log``. Using the redis comand line interface, we can inspect the status of our cluster.

.. code-block:: bash

   redis-cli -p 7001 raft_info
   1) TERM 324
   2) LOG-START 0
   3) LOG-SIZE 2
   4) LEADER localhost:7001
   5) CLUSTER-ID 0123456789
   6) COMMIT-INDEX 1
   7) LAST-APPLIED 1
   8) BLOCKED-WRITES 0
   9) LAST-STATE-CHANGE 48 (48 seconds)
  10) ----------
  11) MYSELF localhost:7001
  12) STATUS LEADER
  13) ----------
  14) MEMBERSHIP-EPOCH 0
  15) NODES localhost:7001,localhost:7002,localhost:7003
  16) OBSERVERS
  17) ----------
  18) REPLICA localhost:7002 ONLINE | UP-TO-DATE | NEXT-INDEX 2
  19) REPLICA localhost:7003 ONLINE | UP-TO-DATE | NEXT-INDEX 2


Setup MGM with namespace in QuarkDB
-----------------------------------

To integrate the MGM service with the **QuarkDB** cluster we need to make several modifications to the default configuration file ``/etc/xrd.cf.mgm``.

  * Update the **mgm.nslib** directive to load the namespace in QuarkDB implementation:

    .. code-block:: bash

       mgm.nslib /usr/lib64/libEosNsQuarkdb.so

  * List the endpoints of the QuarkDB cluster which are used by the MGM daemon to connect to the back-end service:

    .. code-block:: bash

       mgm.qdbcluster localhost:7001 localhost:7002 localhost:7003


Start the MGM daemon as a master:

 .. code-block:: bash

    systemctl start eos@master
    systemctl start eos@mgm

At this point you should have the following process running on the local machine:

 .. code-block:: bash

    ps aux | grep xrootd 
    daemon    3658  0.5  0.3 1920252 28028 ?       Ssl  Mar15  30:25 /usr/bin/xrootd -l /var/log/quarkdb/xrootd.log -c /etc/xrootd/xrootd-quarkdb1.cfg -k fifo -s /var/run/quarkdb/xrootd-quarkdb1.pid -n quarkdb1
    daemon    4369  0.1  0.2 1067196 21688 ?       Ssl  Mar15  10:10 /usr/bin/xrootd -l /var/log/quarkdb/xrootd.log -c /etc/xrootd/xrootd-quarkdb2.cfg -k fifo -s /var/run/quarkdb/xrootd-quarkdb2.pid -n quarkdb2
    daemon    4409  0.1  0.2 1133760 17600 ?       Ssl  Mar15  10:01 /usr/bin/xrootd -l /var/log/quarkdb/xrootd.log -c /etc/xrootd/xrootd-quarkdb3.cfg -k fifo -s /var/run/quarkdb/xrootd-quarkdb3.pid -n quarkdb3
    daemon    4896  0.0  0.2 110324 18240 ?        Ssl  Mar15   0:00 /usr/bin/xrootd -n sync -c /etc/xrd.cf.sync -l /var/log/eos/xrdlog.sync -s /tmp/xrootd.sync.pid -Rdaemon
    daemon    5105  0.5  3.0 1457476 225548 ?      Ssl  Mar15  31:22 /usr/bin/xrootd -n mgm -c /etc/xrd.cf.mgm -l /var/log/eos/xrdlog.mgm -s /tmp/xrootd.mgm.pid -Rdaemon
    daemon    5146  0.0  0.3 340884 22972 ?        S    Mar15   0:00 /usr/bin/xrootd -n mgm -c /etc/xrd.cf.mgm -l /var/log/eos/xrdlog.mgm -s /tmp/xrootd.mgm.pid -Rdaemon


In a production environment the MGM daemon and each of the QuarkDB instances of the cluster should run on different machines. Futhermore, for optimal performance of the **QuarkDB** backend, at least the QuarkDB master should have the ``/var/lib/quarkdb/`` directory stored on an **SSD** partition. 

For further information see :ref:`quarkdbconf`.
