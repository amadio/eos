chown
-----

.. code-block:: text

  chown [-r] [-h --nodereference] <owner>[:<group>] <path>
    chown [-r] :<group> <path>
  '[eos] chown ..' provides the change owner interface of EOS.
  <path> is the file/directory to modify, <owner> has to be a user id or user name. <group> is optional and has to be a group id or group name.
  To modify only the group use :<group> as identifier!
  Remark: if you use the -r -h option and path points to a link the owner of the link parent will also be updated!Options:
    -r : recursive
