# Where to submit CTest results
set(CTEST_PROJECT_NAME "DynamoRIO")

if (SUBMIT_LOCAL)
  set(CTEST_DROP_METHOD "scp")
  if (WIN32)
    # Cygwin scp won't take : later in path (and interprets drive as host like
    # unix scp would) so we use cp, which is ok with drive-letter paths.
    # Note that CTEST_SCP_COMMAND must be a single executable so we can't
    # use "cmake -E copy".
    find_program(CTEST_SCP_COMMAND cp DOC "copy command for local copy of results")
  else (WIN32)
    find_program(CTEST_SCP_COMMAND scp DOC "scp command for local copy of results")
  endif (WIN32)
  set(CTEST_TRIGGER_SITE "")
  set(CTEST_DROP_SITE_USER "")
  # CTest does "scp file ${CTEST_DROP_SITE}:${CTEST_DROP_LOCATION}" so for
  # local copy w/o needing sshd on localhost we arrange to have : in the
  # absolute filepath (when absolute, scp interprets as local even if : later)
  if (NOT EXISTS "${CTEST_DROP_SITE}:${CTEST_DROP_LOCATION}")
    message(FATAL_ERROR 
      "must set ${CTEST_DROP_SITE}:${CTEST_DROP_LOCATION} to an existing directory")
  endif (NOT EXISTS "${CTEST_DROP_SITE}:${CTEST_DROP_LOCATION}")
else (SUBMIT_LOCAL)
  # FIXME i#11: this is untested and not finished
  set(CTEST_DROP_METHOD "http")
  set(CTEST_DROP_SITE "public.kitware.com")
  set(CTEST_DROP_LOCATION "/CDash/submit.php?project=DynamoRIO")
  set(CTEST_DROP_SITE_CDASH TRUE)
  # do we need this?
  set(CTEST_NIGHTLY_START_TIME "21:00:00 EDT")
endif (SUBMIT_LOCAL)
