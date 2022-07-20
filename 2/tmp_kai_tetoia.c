running suite: pipe_tests
        test_pipe_open                            [cores= 1,term=0]: ok
        test_pipe_fails_on_exhausted_fid          [cores= 1,term=0]: ok
        test_pipe_close_reader                    [cores= 1,term=0]: ok
        test_pipe_close_writer                    [cores= 1,term=0]: ok
        test_pipe_single_producer                 [cores= 1,term=0]: ok
        test_pipe_multi_producer                  [cores= 1,term=0]: ok
        suite pipe_tests completed [tests=6, failed=0]
pipe_tests                                                 : ok

mini_zombie@minizombiespc:~/Dropbox/hmmy2/tinyos3$ ./validate_api socket_tests
running suite: socket_tests
        test_socket_constructor_many_per_port     [cores= 1,term=0]: ok
        test_socket_constructor_out_of_fids       [cores= 1,term=0]: ok
        test_socket_constructor_illegal_port      [cores= 1,term=0]: ok
        test_listen_success                       [cores= 1,term=0]: ok
        test_listen_fails_on_bad_fid              [cores= 1,term=0]: ok
        test_listen_fails_on_NOPORT               [cores= 1,term=0]: ok
        test_listen_fails_on_occupied_port        [cores= 1,term=0]: ok
        test_listen_fails_on_initialized_socket   [cores= 1,term=0]: ok
        test_accept_succeds                       [cores= 1,term=0]: ok
        test_accept_fails_on_bad_fid              [cores= 1,term=0]: ok
        test_accept_fails_on_unbound_socket       [cores= 1,term=0]: ok
        test_accept_fails_on_connected_socket     [cores= 1,term=0]: ok
        test_accept_reusable                      [cores= 1,term=0]: *** FAILED ***
        test_accept_fails_on_exhausted_fid        [cores= 1,term=0]: *** FAILED ***
        test_accept_unblocks_on_close             [cores= 1,term=0]: ok
        test_connect_fails_on_bad_fid             [cores= 1,term=0]: ok
        test_connect_fails_on_bad_socket          [cores= 1,term=0]: ok
        test_connect_fails_on_illegal_port        [cores= 1,term=0]: ok
        test_connect_fails_on_non_listened_port   [cores= 1,term=0]: ok
        test_socket_single_producer               [cores= 1,term=0]: *** FAILED ***
        test_socket_multi_producer                [cores= 1,term=0]: *** FAILED ***
        test_shutdown_read                        [cores= 1,term=0]: ok
        test_shutdown_write                       [cores= 1,term=0]: *** FAILED ***
        
        suite socket_tests completed [tests=24, failed=5]
		socket_tests                                       : *** FAILED ***

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

Starting tinyos shell
Type 'help' for help, 'exit' to quit.
% sysinfo
Number of cores         = 1
Number of serial devices= 1
  PID  PPID  State  Threads         Main program
    0    -1  ALIVE        0                    -
    1    -1  ALIVE        0                 init
    2     1  ALIVE        0                   sh
    3     2  ALIVE        0              sysinfo


   *** DISCONNECTED ***   

*** Booting TinyOS with 1 cores and 1 terminals
Switching standard streams
After loopPCB_CNT : 0 MAX_PROC : 65536 < PROCINFO
After loopPCB_CNT : 1 MAX_PROC : 65536 < PROCINFO
After loopPCB_CNT : 2 MAX_PROC : 65536 < PROCINFO
After loopPCB_CNT : 3 MAX_PROC : 65536 < PROCINFO
After loopPCB_CNT : 65536 MAX_PROC : 65536 >=PROCINFO
Segmentation fault (core dumped)
