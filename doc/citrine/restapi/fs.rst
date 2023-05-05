.. highlight:: rst

.. index::
   single: Filesystem API

fs
=====

fs ls
--------

List configured filesystems.

REST syntax
+++++++++++

.. code-block:: text

   http://<host>:8000/proc/admin/ | root://<host>//proc/admin/
     ?mgm.cmd=fs
     &mgm.subcmd=ls
     &eos.ruid=0
     &eos.rgid=0
     &mgm.format=json
     [&mgm.outformat=l|m|e|d|io|fsck]
     [&mgm.outhost=brief]
     [&mgm.selection=<match>]

CLI syntax
++++++++++

.. code-block:: text

   fs ls [-m|-l|-e|--io|--fsck|-d|--drain] [-s] [--brief|-b] [ [matchlist] ] :
      list all filesystems in default output format. <space> is an optional substring match for the space name and can be a comma separated list
      -m                                  : list all filesystem parameters in monitoring format
      --b,--brief                         : display host names without domain names
      -l                                  : display all filesystem parameters in long format
      -e                                  : display all filesystems in error state
      --io                                : display all filesystems in IO output format
      --fsck                              : display filesystem check statistics
      -d,--drain                          : display all filesystems in drain or draindead status with drain progress and statistics
      [matchlist]  : [matchlist] can be just the name of the space to display or a comma seperated list of spaces e.g 'default,space'
      [matchlist]  : [matchlist] can be a grep style list to filter certain filesystems e.g. 'fs ls -d drain,bootfailure'
      [matchlist]  : [matchlist] can be a combination of space filter and grep e.g. 'fs ls -l space:default,drain,bootfailure'


fs add
--------

Add a new filesystem.

REST syntax
+++++++++++

.. code-block:: text

   http://<host>:8000/proc/admin/ | root://<host>//proc/admin/
     ?mgm.cmd=fs
     &mgm.subcmd=add
     &eos.ruid=0
     &eos.rgid=0
     [&mgm.fs.fsid=<fsid>]
     &mgm.fs.uuid=<uuid>
     &mgm.fs.node=<node-queue>
     &mgm.fs.mountpoint=<mount-point>
     &mgm.fs.space=<space>
     &mgm.fs.configstatus=off|ro|rw

CLI syntax
++++++++++

.. code-block:: text

   fs add [-m|--manual <fsid>] <uuid> <node-queue>|<host>[:<port>] <mountpoint> [<schedgroup>] [<status] :
      add a filesystem and dynamically assign a filesystem id based on the unique identifier for the disk <uuid>
      -m,--manual <fsid>                  : add with user specified <fsid> and <schedgroup> - no automatic assignment
      <fsid>                              : numeric filesystem id 1..65535
      <uuid>                              : arbitrary string unique to this particular filesystem
      <node-queue>                        : internal EOS identifier for a node,port,mountpoint description ... /eos/<host>:<port>/fst e.g. /eos/myhost.cern.ch:1095/fst [you should prefer the host:port syntax]
      <host>                              : fully qualified hostname where the filesystem is mounted
      <port>                              : port where xrootd is running on the FST [normally 1095]
      <mountpoint>                        : local path of the mounted filesystem e.g. /data
      <schedgroup>                        : scheduling group where the filesystem should be inserted ... default is 'default'
      <status>                            : file system status after the insert ... default is 'off', in most cases should be 'rw'

fs mv
--------

Move a filesystem between spaces/groups.

REST syntax
+++++++++++

.. code-block:: text

   http://<host>:8000/proc/admin/ | root://<host>//proc/admin/
     ?mgm.cmd=fs
     &mgm.subcmd=mv
     &eos.ruid=0
     &eos.rgid=0
     &mgm.space=<dst-space>
     &mgm.fs.id=<fsid>|<space>

CLI syntax
++++++++++

.. code-block:: text


   fs mv <src-fsid|src-space> <dst-schedgroup|dst-space> :
      move a filesystem into a different scheduling group
      <src-fsid>                          : source filesystem id
      <src-space>                         : source space
      <dst-schedgroup>                    : destination scheduling group
      <dst-space>                         : destination space
   If the source is a <space> a filesystem will be chosen to fit into the destination group or space.
   If the target is a <space> : a scheduling group is auto-selected where the filesystem can be placed.
.. code-block:: text

fs config
---------

Configure filesystems.

REST syntax
+++++++++++

.. code-block:: text

   http://<host>:8000/proc/admin/ | root://<host>//proc/admin/
     ?mgm.cmd=fs
     &mgm.subcmd=config
     &eos.ruid=0
     &eos.rgid=0
     &mgm.fs.identifier=<fsid>
     &mgm.fs.key=<key>
     &mgm.fs.value=<value>

CLI syntax
++++++++++

.. code-block:: text

   fs config <host>:<port><path>|<fsid>|<uuid> <key>=<value> :
      configure filesystem parameter for a single filesystem identified by host:port/path, filesystem id or filesystem UUID.
   fs config <fsid> configstatus=rw|wo|ro|drain|off :
      <status> can be
      rw          : filesystem set in read write mode
      wo          : filesystem set in write-once mode
      ro          : filesystem set in read-only mode
      drain       : filesystem set in drain mode
      off         : filesystem set disabled
      empty       : filesystem is set to empty - possible only if there are no files stored anymorefs config <fsid> headroom=<size>
      <size> can be (>0)[BMGT]    : the headroom to keep per filesystem (e.g. you can write '1G' for 1 GB)
   fs config <fsid> scaninterval=<seconds>: 
      configures a scanner thread on each FST to recheck the file & block checksums of all stored files every <seconds> seconds. 0 disables the scanning.
   fs config <fsid> graceperiod=<seconds> :
      grace period before a filesystem with an operation error get's automatically drained
   fs config <fsid> drainperiod=<seconds> : 
      drain period a drain job is waiting to finish the drain procedure

fs rm
--------

Remove a configured filesystem.

REST syntax
+++++++++++

.. code-block:: text

   http://<host>:8000/proc/admin/ | root://<host>//proc/admin/
     ?mgm.cmd=fs
     &mgm.subcmd=rm
     &eos.ruid=0
     &eos.rgid=0
     &mgm.fs.id=<fsid|nodequeue>|&mgm.fs.node=<node>&mgm.fs.mountpoint=<mountpoint>

CLI syntax
++++++++++

.. code-block:: text


   fs rm    <fs-id>|<node-queue>|<mount-point>|<hostname> <mountpoint> :
      remove filesystem configuration by various identifiers

fs boot
--------

Boot a filesystem.

REST syntax
+++++++++++

.. code-block:: text

   http://<host>:8000/proc/admin/ | root://<host>//proc/admin/
     ?mgm.cmd=fs
     &mgm.subcmd=boot
     &eos.ruid=0
     &eos.rgid=0
     &mgm.fsid=<fsid>|&mgm.node=<node>
     [&mgm.fs.forcemgmsync=1]

CLI syntax
++++++++++

.. code-block:: text

   fs boot  <fs-id>|<node-queue>|* [--syncmgm]:
      boot filesystem with ID <fs-id> or name <node-queue> or all (*)
      --syncmgm : force an MGM resynchronization during the boot

fs dropdeletion
---------------

List configured filesystems.

REST syntax
+++++++++++

.. code-block:: text

   http://<host>:8000/proc/admin/ | root://<host>//proc/admin/
     ?mgm.cmd=fs
     &mgm.subcmd=ls
     &eos.ruid=0
     &eos.rgid=0
     &mgm.fs.id=<fsid>

CLI syntax
++++++++++

.. code-block:: text

   fs dropdeletion <fs-id> :
      allows to drop all pending deletions on <fs-id>

fs dumpmd
---------

Dump meta data of a filesystem.

REST syntax
+++++++++++

.. code-block:: text

   http://<host>:8000/proc/admin/ | root://<host>//proc/admin/
     ?mgm.cmd=fs
     &mgm.subcmd=dumpmd
     &eos.ruid=0
     &eos.rgid=0
     &mgm.fsid=<fsid
     [&mgm.dumpmd.option=m]
     [&mgm.dumpmd.fid=1]
     [&mgm.dumpmd.size=1]

CLI syntax
++++++++++

.. code-block:: text

   fs dumpmd [-s|-m] <fs-id> [-fid] [-path] :
      dump all file meta data on this filesystem in query format
      -s    : don't printout keep an internal reference
      -m    : print the full meta data record in env format
      -fid  : dump only a list of file id's stored on this filesystem
      -path : dump only a list of file names stored on this filesystem

fs satus
--------

Show status of a filesystem.

REST syntax
+++++++++++

.. code-block:: text

   http://<host>:8000/proc/admin/ | root://<host>//proc/admin/
     ?mgm.cmd=fs
     &mgm.subcmd=status
     &eos.ruid=0
     &eos.rgid=0
     &mgm.fs.id=<fsid>|&mgm.fs.node=<nodeid>&mgm.fs.mountpoint=<mountpoint>

CLI syntax
++++++++++

.. code-block:: text

   fs status [-l] <fs-id> :
      returns all status variables of a filesystem and calculates the risk of data loss if this filesystem get's removed
   fs status [-l] mount-point> :
      as before but accepts the mount point as input parameters and set's host=<this host>
   fs status [-l] <host> <mount-point> :
      as before but accepts the mount point and hostname as input parameters
      -l    : list all files at risk and files which are offline
