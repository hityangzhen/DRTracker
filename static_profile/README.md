####each \*.out file corresponds to an evaluated programs in test/ dir
> \*ocean.out, \*radix.out, \*water.out, \*lu.out, \*fft.out, \*cholesky.out corresponds to evaluated programs from [splash2 test suit](http://www.capsl.udel.edu/splash/index.html)
***
####instrumented\_lines\_\*.out files contain instrumented line info.
* **instrumented\_ lines\_\*.out (each line)** 

> filename | instrumented line 

####static\_profile\_\*.out files contain potential racing stmt pair info.
* **static\_profile\_\*.out (each line)**

> filename1 | potential racing stmt1 line | filename2 | potential racing stmt2 line

####loop\_range\_\*.out files contain each loop's start line and end line info.
* **loop\_range\_\*.out (each line)**

> filename | loop start line | loop end line

####exiting\_cond\_lines\_\*.out files contain each loop's exiting condtion lines,which are spin read.
* **exiting\_cond\_lines\_\*.out (each line)**

> filename | procedure start line | procedure end line | loop start line | loop end line | first exiting condition line | second exiting condition line | ...

####cond\_wait\_lines\_\*.out files contain loop scope info and each loop's exiting condition line before the pthread_cond_wait sync operation info.
* **cond\_wait\_lines\_\*.out (each line)**

> filename | procedure start line | procedure end line | loop start line | loop end line | first exiting condition line | second exiting condition line | ...

####locksmith\_out\_parse.py profile the result of the [LOCKSMITH](http://www.cs.umd.edu/projects/PL/locksmith/) static analysis
> **input: \*.out from the locksmith output**

> **output: instrumented\_lines\_\*.out**

####relay\_out\_parse.py profile the result of the [RELAY](http://cseweb.ucsd.edu/~jvoung/race/) static analysis
> **input: warnings.xml and warnings2.xml from the relay output**

> **output: instrumented\_lines\_\*.out and static\_profile\_\*.out**

####static\_profile\_\*\_group dir contains the grouped potential racing stmt pairs

####group.py and group.sh are mainly to group the static\_profile\_\*.out
> **input: static\_profile\_\*.out**

> **output: static\_profile\_\*\_group dir which contains g\*.out files**