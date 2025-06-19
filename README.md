# mercury-init-test

mercury-init-test: init mercury using the requested plugin and
print out our self-address in text format.

<pre>
usage: mercury-init-test [flags] spec

where spec is 'bmi+tcp' etc.

flags:
 -a      - enable auto_sm mode
 -d log  - run as daemon and print output to given log file
 -n      - init with na listen=false (def=true)
 -s      - IP subnet spec (def=NULL)
</pre>


To compile: build Mercury first.   Then either use cmake
(with either CMAKE_INSTALL_PREFIX or CMAKE_PREFIX_PATH pointed
at the installed location of Mercury) or compile manually.

Example with cmake and Mercury installed in /tmp/mt:
<pre>
h0:src/mercury-init-test % ls
CMakeLists.txt	README.md  mercury-init-test.c
h0:src/mercury-init-test % mkdir build
h0:src/mercury-init-test % cd build
h0:mercury-init-test/build % cmake -DCMAKE_PREFIX_PATH=/tmp/mt ..
-- The C compiler identification is GNU 9.3.0
-- Check for working C compiler: /bin/cc
-- Check for working C compiler: /bin/cc -- works
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Detecting C compile features
-- Detecting C compile features - done
-- Looking for pthread.h
-- Looking for pthread.h - found
-- Performing Test CMAKE_HAVE_LIBC_PTHREAD
-- Performing Test CMAKE_HAVE_LIBC_PTHREAD - Failed
-- Looking for pthread_create in pthreads
-- Looking for pthread_create in pthreads - not found
-- Looking for pthread_create in pthread
-- Looking for pthread_create in pthread - found
-- Found Threads: TRUE  
-- Configuring done
-- Generating done
-- Build files have been written to: /tmp/mt/src/mercury-init-test/build
h0:mercury-init-test/build % make
Scanning dependencies of target mercury-init-test
[ 50%] Building C object CMakeFiles/mercury-init-test.dir/mercury-init-test.c.o
[100%] Linking C executable mercury-init-test
[100%] Built target mercury-init-test
h0:mercury-init-test/build % 
</pre>

Example manual build on Linux with Mercury installed in /tmp/mt
(adjust compile flags as needed):

<pre>
h0:src/mercury-init-test % ls
CMakeLists.txt	README.md  mercury-init-test.c
h0:src/mercury-init-test % cc -I/tmp/mt/include \
    -L/tmp/mt/lib -Wl,-rpath=/tmp/mt/lib \
    -O -o mercury-init-test mercury-init-test.c -lmercury -pthread
h0:src/mercury-init-test % ./mercury-init-test 
usage: ./mercury-init-test [flags] spec
where spec is 'bmi+tcp' etc.

flags:
	-a     - enable auto_sm mode
	-d log - run in daemon mode, output to log file
	-n     - init with na listen=false (def=true)
	-s s   - IP subnet spec (def=NULL)

h0:src/mercury-init-test % 
</pre>

Example runs:
<pre>
h0:src/mercury-init-test % ./mercury-init-test bmi+tcp
mercury-init-test on bmi+tcp
	auto_sm mode = 0
	daemon = off
	listen mode = 1
	subnet: <none>
network thread running

requested addr buf size: 29
listening at: bmi+tcp://10.111.4.178:43875

network thread complete
destroy context and finalize mercury
h0:src/mercury-init-test % ./mercury-init-test -a bmi+tcp
mercury-init-test on bmi+tcp
	auto_sm mode = 1
	daemon = off
	listen mode = 1
	subnet: <none>
network thread running

requested addr buf size: 63
listening at: uid://1862971908#na+sm://702675-0#bmi+tcp://10.111.4.178:40699

network thread complete
destroy context and finalize mercury
h0:src/mercury-init-test % ./mercury-init-test -d ./daemon.log -a bmi+tcp
mercury-init-test on bmi+tcp
	auto_sm mode = 1
	daemon = on (logfile=./daemon.log)
	listen mode = 1
	subnet: <none>
h0:src/mercury-init-test % cat daemon.log 
network thread running

requested addr buf size: 63
listening at: uid://1862971908#na+sm://702678-0#bmi+tcp://10.111.4.178:39259

network thread complete
destroy context and finalize mercury
h0:src/mercury-init-test % ./mercury-init-test -s 10.94 bmi+tcp
mercury-init-test on bmi+tcp
	auto_sm mode = 0
	daemon = off
	listen mode = 1
	subnet: 10.94
network thread running

requested addr buf size: 28
listening at: bmi+tcp://10.94.1.178:44013

network thread complete
destroy context and finalize mercury
h0:src/mercury-init-test % 
</pre>
