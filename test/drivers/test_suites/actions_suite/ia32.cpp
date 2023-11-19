#include "../../event_class/event_class.h"

#if defined(__x86_64__)
TEST(Actions, ia32)
{
	/* Here we capture all syscalls... this process will send some
	 * specific syscalls and we have to check that they are extracted in order
	 * from the buffers.
	 */
	auto evt_test = get_syscall_event_test();

	evt_test->enable_capture();

	/*=============================== TRIGGER SYSCALL  ===========================*/
	pid_t ret_pid = syscall(__NR_fork);
	if(ret_pid == 0)
	{
		char* const argv[] = {NULL};
		char* const envp[] = {NULL};
		// Pin process to a single core, so that events get sent in order
		cpu_set_t my_set;
		CPU_ZERO(&my_set);
		CPU_SET(1, &my_set);
		sched_setaffinity(0, sizeof(cpu_set_t), &my_set);

		execve("/home/m/Documents/OSS/oss-dev-master/libs/build/test/drivers/ia32", argv, envp);
		exit(EXIT_FAILURE);
	}
	assert_syscall_state(SYSCALL_SUCCESS, "fork", ret_pid, NOT_EQUAL, -1);
	int status = 0;
	int options = 0;
	assert_syscall_state(SYSCALL_SUCCESS, "wait4", syscall(__NR_wait4, ret_pid, &status, options, NULL), NOT_EQUAL, -1);

	if(__WEXITSTATUS(status) == EXIT_FAILURE || __WIFSIGNALED(status) != 0)
	{
		FAIL() << "Fork failed..." << std::endl;
	}

	/* Disable the capture: no more events from now. */
	evt_test->disable_capture();

	/* Retrieve events in order. */
#ifdef __NR_openat2
	evt_test->assert_event_presence(ret_pid, PPME_SYSCALL_OPENAT2_E);
  evt_test->assert_event_presence(ret_pid, PPME_SYSCALL_OPENAT2_X);
#endif
	evt_test->assert_event_presence(ret_pid, PPME_SYSCALL_WRITE_E);
	evt_test->assert_event_presence(ret_pid, PPME_SYSCALL_WRITE_X);

  // getegid32 and geteuid32 are ia32 only syscalls and are translated to getegid and geteuid x86_64 syscalls
	evt_test->assert_event_presence(ret_pid, PPME_SYSCALL_GETEGID_E);
  evt_test->assert_event_presence(ret_pid, PPME_SYSCALL_GETEGID_X);
  evt_test->assert_event_presence(ret_pid, PPME_SYSCALL_GETEUID_E);
  evt_test->assert_event_presence(ret_pid, PPME_SYSCALL_GETEUID_X);

  // umount is ia32 only and is translated to x86_64 umount2
  evt_test->assert_event_presence(ret_pid, PPME_SYSCALL_UMOUNT2_E);
  evt_test->assert_event_presence(ret_pid, PPME_SYSCALL_UMOUNT2_X);

	// mmap2 is translated to x86_64 mmap
	evt_test->assert_event_presence(ret_pid, PPME_SYSCALL_MMAP_E);
  evt_test->assert_event_presence(ret_pid, PPME_SYSCALL_MMAP_X);
  evt_test->assert_event_presence(ret_pid, PPME_SYSCALL_MUNMAP_E);
  evt_test->assert_event_presence(ret_pid, PPME_SYSCALL_MUNMAP_X);
  evt_test->assert_event_presence(ret_pid, PPME_SYSCALL_MMAP_E);
  evt_test->assert_event_presence(ret_pid, PPME_SYSCALL_MMAP_X);
  evt_test->assert_event_presence(ret_pid, PPME_SYSCALL_MUNMAP_E);
  evt_test->assert_event_presence(ret_pid, PPME_SYSCALL_MUNMAP_X);
	evt_test->assert_event_presence(ret_pid, PPME_SOCKET_SOCKET_E);
	evt_test->assert_event_presence(ret_pid, PPME_SOCKET_SOCKET_X);
	evt_test->assert_event_presence(ret_pid, PPME_SOCKET_ACCEPT4_6_E);
	evt_test->assert_event_presence(ret_pid, PPME_SOCKET_ACCEPT4_6_X);

  /*
   * Special cases: socketcalls whose SYS_foo code is defined but the syscall is not.
   * See socketcall_to_syscall.h comment.
   */
	if(evt_test->is_modern_bpf_engine())
	{
	    /*
       * ModernBPF jump table is syscalls-indexed;
       * Since SYS_SEND exists but __NR_send does not on x86_64,
       * convert_network_syscalls() returns -1 and we don't push anything to consumers.
       */
    	evt_test->assert_event_absence(ret_pid, PPME_SOCKET_SEND_E);
    	evt_test->assert_event_absence(ret_pid, PPME_SOCKET_SEND_X);

    	/*
    	 * Same as above
    	 */
    	evt_test->assert_event_absence(ret_pid, PPME_SOCKET_ACCEPT4_6_E);
    	evt_test->assert_event_absence(ret_pid, PPME_SOCKET_ACCEPT4_6_X);

      /*
       * Last event sent by the script is a socketcall with non existing SYS_ code.
       * We don't expect any event generated.
       */
      evt_test->assert_event_absence(ret_pid, PPME_GENERIC_E);
      evt_test->assert_event_absence(ret_pid, PPME_GENERIC_X);
	}
	else
	{
	  /*
	   * Kmod and old bpf jump table is events-indexed.
	   * We are able to fallback at sending the events.
	   */
    evt_test->assert_event_presence(ret_pid, PPME_SOCKET_SEND_E);
    evt_test->assert_event_presence(ret_pid, PPME_SOCKET_SEND_X);

    /*
     * Same as above
     */
    evt_test->assert_event_presence(ret_pid, PPME_SOCKET_ACCEPT_5_E);
    evt_test->assert_event_presence(ret_pid, PPME_SOCKET_ACCEPT_5_X);

    /*
     * Last event sent by the script is a socketcall with non existing SYS_ code.
     * We expect generic events to be pushed.
     */
    evt_test->assert_event_presence(ret_pid, PPME_GENERIC_E);
    evt_test->assert_event_presence(ret_pid, PPME_GENERIC_X);
	}
}

#ifdef __NR_execve
// Check that we receive proper execve exit events, testing that it gets
// properly received, even if it comes from a x86_64 task that is execv'ing a COMPAT task.
TEST(Actions, ia32_execve_compat)
{
	auto evt_test = get_syscall_event_test(__NR_execve, EXIT_EVENT);

	evt_test->enable_capture();

	/*=============================== TRIGGER SYSCALL  ===========================*/
	pid_t ret_pid = syscall(__NR_fork);
	if(ret_pid == 0)
	{
		char* const argv[] = {NULL};
		char* const envp[] = {NULL};
		execve("/home/m/Documents/OSS/oss-dev-master/libs/build/test/drivers/ia32", argv, envp);
		exit(EXIT_FAILURE);
	}
	assert_syscall_state(SYSCALL_SUCCESS, "fork", ret_pid, NOT_EQUAL, -1);
	int status = 0;
	int options = 0;
	assert_syscall_state(SYSCALL_SUCCESS, "wait4", syscall(__NR_wait4, ret_pid, &status, options, NULL), NOT_EQUAL, -1);

	if(__WEXITSTATUS(status) == EXIT_FAILURE || __WIFSIGNALED(status) != 0)
	{
		FAIL() << "Fork failed..." << std::endl;
	}

	/* Disable the capture: no more events from now. */
	evt_test->disable_capture();

	/* We search for a child event. */
  evt_test->assert_event_presence(ret_pid);
}
#endif

#ifdef __NR_openat2
TEST(Actions, ia32_openat2_x)
{
	auto evt_test = get_syscall_event_test(__NR_openat2, EXIT_EVENT);

	evt_test->enable_capture();

	/*=============================== TRIGGER SYSCALL  ===========================*/
	pid_t ret_pid = syscall(__NR_fork);
	if(ret_pid == 0)
	{
		char* const argv[] = {NULL};
		char* const envp[] = {NULL};
		execve("/home/m/Documents/OSS/oss-dev-master/libs/build/test/drivers/ia32", argv, envp);
		exit(EXIT_FAILURE);
	}
	assert_syscall_state(SYSCALL_SUCCESS, "fork", ret_pid, NOT_EQUAL, -1);
	int status = 0;
	int options = 0;
	assert_syscall_state(SYSCALL_SUCCESS, "wait4", syscall(__NR_wait4, ret_pid, &status, options, NULL), NOT_EQUAL, -1);

	if(__WEXITSTATUS(status) == EXIT_FAILURE || __WIFSIGNALED(status) != 0)
	{
		FAIL() << "Fork failed..." << std::endl;
	}

	/* Disable the capture: no more events from now. */
	evt_test->disable_capture();

	/* Retrieve events in order. */
	evt_test->assert_event_presence(ret_pid);

	if(HasFatalFailure())
	{
  	return;
  }

  evt_test->parse_event();
  evt_test->assert_header();

  /*=============================== ASSERT PARAMETERS  ===========================*/

  /* Parameter 1: fd (type: PT_FD) */
  // can't assert return code; sometime is -EBADF, sometime -ENOTDIR
  //evt_test->assert_numeric_param(1, (int64_t)-EBADF);

  /* Parameter 2: dirfd (type: PT_FD) */
  evt_test->assert_numeric_param(2, (int64_t)11);

  /* Parameter 3: name (type: PT_FSPATH) */
  evt_test->assert_charbuf_param(3, "mock_path");

  /* Parameter 4: flags (type: PT_FLAGS32) */
  evt_test->assert_numeric_param(4, (uint32_t)PPM_O_RDWR);

  /* Parameter 5: mode (type: PT_UINT32) */
  evt_test->assert_numeric_param(5, (uint32_t)0);

  /* Parameter 6: resolve (type: PT_FLAGS32) */
  evt_test->assert_numeric_param(6, (uint32_t)PPM_RESOLVE_BENEATH | PPM_RESOLVE_NO_MAGICLINKS);

 	/*=============================== ASSERT PARAMETERS  ===========================*/

 	evt_test->assert_num_params_pushed(6);
}
#endif

#ifdef __NR_write
TEST(Actions, ia32_write_e)
{
	auto evt_test = get_syscall_event_test(__NR_write, ENTER_EVENT);

	evt_test->enable_capture();

	/*=============================== TRIGGER SYSCALL  ===========================*/
	pid_t ret_pid = syscall(__NR_fork);
	if(ret_pid == 0)
	{
		char* const argv[] = {NULL};
		char* const envp[] = {NULL};
		execve("/home/m/Documents/OSS/oss-dev-master/libs/build/test/drivers/ia32", argv, envp);
		exit(EXIT_FAILURE);
	}
	assert_syscall_state(SYSCALL_SUCCESS, "fork", ret_pid, NOT_EQUAL, -1);
	int status = 0;
	int options = 0;
	assert_syscall_state(SYSCALL_SUCCESS, "wait4", syscall(__NR_wait4, ret_pid, &status, options, NULL), NOT_EQUAL, -1);

	if(__WEXITSTATUS(status) == EXIT_FAILURE || __WIFSIGNALED(status) != 0)
	{
		FAIL() << "Fork failed..." << std::endl;
	}

	/* Disable the capture: no more events from now. */
	evt_test->disable_capture();

	/* Retrieve events in order. */
	evt_test->assert_event_presence(ret_pid);

	if(HasFatalFailure())
	{
  	return;
  }

  evt_test->parse_event();
  evt_test->assert_header();

  /*=============================== ASSERT PARAMETERS  ===========================*/

	/* Parameter 1: fd (type: PT_FD) */
  evt_test->assert_numeric_param(1, (int64_t)17);

  /* Parameter 2: size (type: PT_UINT32)*/
  evt_test->assert_numeric_param(2, (uint32_t)1013);

  /*=============================== ASSERT PARAMETERS  ===========================*/

  evt_test->assert_num_params_pushed(2);
}
#endif

#if defined(__NR_socket)
TEST(Actions, ia32_socket_e)
{
	auto evt_test = get_syscall_event_test(__NR_socket, ENTER_EVENT);

	evt_test->enable_capture();

	/*=============================== TRIGGER SYSCALL  ===========================*/
	pid_t ret_pid = syscall(__NR_fork);
	if(ret_pid == 0)
	{
		char* const argv[] = {NULL};
		char* const envp[] = {NULL};
		execve("/home/m/Documents/OSS/oss-dev-master/libs/build/test/drivers/ia32", argv, envp);
		exit(EXIT_FAILURE);
	}
	assert_syscall_state(SYSCALL_SUCCESS, "fork", ret_pid, NOT_EQUAL, -1);
	int status = 0;
	int options = 0;
	assert_syscall_state(SYSCALL_SUCCESS, "wait4", syscall(__NR_wait4, ret_pid, &status, options, NULL), NOT_EQUAL, -1);

	if(__WEXITSTATUS(status) == EXIT_FAILURE || __WIFSIGNALED(status) != 0)
	{
		FAIL() << "Fork failed..." << std::endl;
	}

	/* Disable the capture: no more events from now. */
	evt_test->disable_capture();

	/* Retrieve events in order. */
	evt_test->assert_event_presence(ret_pid);

	if(HasFatalFailure())
	{
  	return;
  }

  evt_test->parse_event();
  evt_test->assert_header();

  /*=============================== ASSERT PARAMETERS  ===========================*/

  /* Parameter 1: domain (type: PT_ENUMFLAGS32) */
  evt_test->assert_numeric_param(1, (uint32_t)PPM_AF_INET);

  /* Parameter 2: type (type: PT_UINT32) */
  evt_test->assert_numeric_param(2, (uint32_t)SOCK_RAW);

  /* Parameter 3: proto (type: PT_UINT32) */
  evt_test->assert_numeric_param(3, (uint32_t)PF_INET);

  /*=============================== ASSERT PARAMETERS  ===========================*/

  evt_test->assert_num_params_pushed(3);
}
#endif

#ifdef __NR_umount2
TEST(Actions, ia32_umount_e)
{
	auto evt_test = get_syscall_event_test(__NR_umount2, ENTER_EVENT);

	evt_test->enable_capture();

	/*=============================== TRIGGER SYSCALL  ===========================*/
	pid_t ret_pid = syscall(__NR_fork);
	if(ret_pid == 0)
	{
		char* const argv[] = {NULL};
		char* const envp[] = {NULL};
		execve("/home/m/Documents/OSS/oss-dev-master/libs/build/test/drivers/ia32", argv, envp);
		exit(EXIT_FAILURE);
	}
	assert_syscall_state(SYSCALL_SUCCESS, "fork", ret_pid, NOT_EQUAL, -1);
	int status = 0;
	int options = 0;
	assert_syscall_state(SYSCALL_SUCCESS, "wait4", syscall(__NR_wait4, ret_pid, &status, options, NULL), NOT_EQUAL, -1);

	if(__WEXITSTATUS(status) == EXIT_FAILURE || __WIFSIGNALED(status) != 0)
	{
		FAIL() << "Fork failed..." << std::endl;
	}

	/* Disable the capture: no more events from now. */
	evt_test->disable_capture();

	/* Retrieve events in order. */
	evt_test->assert_event_presence(ret_pid);

	if(HasFatalFailure())
	{
  	return;
  }

  evt_test->parse_event();
  evt_test->assert_header();

  /*=============================== ASSERT PARAMETERS  ===========================*/

  /* Parameter 1: flags (type: PT_FLAGS32) */
  evt_test->assert_numeric_param(1, 0);

 	/*=============================== ASSERT PARAMETERS  ===========================*/

 	evt_test->assert_num_params_pushed(1);
}

TEST(Actions, ia32_umount_x)
{
	auto evt_test = get_syscall_event_test(__NR_umount2, EXIT_EVENT);

	evt_test->enable_capture();

	/*=============================== TRIGGER SYSCALL  ===========================*/
	pid_t ret_pid = syscall(__NR_fork);
	if(ret_pid == 0)
	{
		char* const argv[] = {NULL};
		char* const envp[] = {NULL};
		execve("/home/m/Documents/OSS/oss-dev-master/libs/build/test/drivers/ia32", argv, envp);
		exit(EXIT_FAILURE);
	}
	assert_syscall_state(SYSCALL_SUCCESS, "fork", ret_pid, NOT_EQUAL, -1);
	int status = 0;
	int options = 0;
	assert_syscall_state(SYSCALL_SUCCESS, "wait4", syscall(__NR_wait4, ret_pid, &status, options, NULL), NOT_EQUAL, -1);

	if(__WEXITSTATUS(status) == EXIT_FAILURE || __WIFSIGNALED(status) != 0)
	{
		FAIL() << "Fork failed..." << std::endl;
	}

	/* Disable the capture: no more events from now. */
	evt_test->disable_capture();

	/* Retrieve events in order. */
	evt_test->assert_event_presence(ret_pid);

	if(HasFatalFailure())
	{
  	return;
  }

  evt_test->parse_event();
  evt_test->assert_header();

  /*=============================== ASSERT PARAMETERS  ===========================*/

  /* Parameter 1: fd (type: PT_ERRNO) */
  evt_test->assert_numeric_param(1, (int64_t)-ENOENT);

  /* Parameter 2: name (type: PT_FSPATH) */
  evt_test->assert_charbuf_param(2, "mock_path");

 	/*=============================== ASSERT PARAMETERS  ===========================*/

 	evt_test->assert_num_params_pushed(2);
}
#endif

#endif
