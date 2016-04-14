# What is RaceTrack

RaceTrack is a test tool to find the data races from the concurrent programs.The bottom architecture is inspired from the [Maple](https://github.com/jieyu/maple) and we directy intergrate the core and race two modules into RaceTrack. RaceTrack contains the following four parts:

## 1. Pure dynamic data race detectors (10)
We only implemented the kernel algorithm.

### 1.1 Lockset algorithm

* [Eraser](http://web2.cs.columbia.edu/~junfeng/10fa-e6998/papers/eraser.pdf)

### 1.2 Happens-before algorithm

* [Djit+](ftp://cs.umanitoba.ca/pub/IPDPS03/DATA/W20_PADTD_04.PDF)
* [FastTrack](http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.217.663&rep=rep1&type=pdf)
* [Loft](http://www.cs.cityu.edu.hk/~wkchan/papers/issre2011-cai+chan.pdf)

### 1.3 Hybrid algorithm
* [Helgrind+](http://ieeexplore.ieee.org/stamp/stamp.jsp?tp=&arnumber=5160998)
* [ThreadSanitizer](http://www.australianscience.com.au/research/google/35604.pdf)
* [Acculock](http://www.cse.unsw.edu.au/~jingling/papers/cgo11-xie.pdf)
* [MultiLock-HB](http://www.cse.unsw.edu.au/~jingling/papers/spe13.pdf)
* [SimpleLock](http://pdcat13.csie.ntust.edu.tw/download/papers/P10017.pdf)
* [SimpleLock+](http://comjnl.oxfordjournals.org/content/early/2014/11/10/comjnl.bxu119.full.pdf)

### 1.4 Parallel algorithm
The above pure dynamic data race detection algorithm can all be executed parallel. If you want to speed up the detection progress, you can set *<font color=#0099ff>parallel_detector_number</font>* to the value greater than 0.

## 2. Hybrid data race detectors
According to the results of the static data racedetectors, we can reduce the complexity of the dynamic binary instrumentation and only monitor the potential race points. We accept the static results which are formatted like instrumented\_lines\_\*\_.out and static\_profile\_\*\_.out files in the static_profile/dir. We currently only use the RELAY as the static race detector. We combine the implemented dynamic race detectors to discover the data race.

## 3. Hybrid data race verifier
Similarly, we use the results of the static race detector and instrument the delay in the potential points to verify a race.

## 4. Hybrid data race verifier and historical race detector
At the basis of the verifier, we incur the pure dynamic race detection ideas to make up for the missed races.

### 4.1 Parallel verification and detection
The above algorithm can be executed parallel. If you want to speed up the verification and detection progress, you can set *<font color=#0099ff>parallel_verifier_number</font>* to the value greater than 0(indicates that there are one verification thread and more historical detection thread), or you can set the value less than 0(indicates that there are only one verification thread and no historical detection thread).

## 5. Hybrid data race pruner
According to the results of the verifier and historical detector, we firstly use the static analysis techniques to accumulate the ad-hoc synchronization information (we can identify the most common potential ad-hoc synchronization that is between the remote write and the spinning read loop). And then we use the pure dynamic data race detector to identify the ad-hoc synchronizations so as to prune the benign and false races.

## 6. Hybrid group verifier
Referenced to [RaceChecker](http://ieeexplore.ieee.org/stamp/stamp.jsp?tp=&arnumber=7092703), we implemented the group algorithm. It is composed of two phases: (1) Record the vector clock informtaion online (2) Group the original potential races offline. After grouping, we use the verifier mentioned in 3 to verify the potential races from a group each time.

# Build the RaceTrack

## OS
RaceTrack is only supported in Linux platform. We have tested on the Ubuntu Desktop 12.04 (X86_64).

## Dependencies
* Python 2.7 or higher
* Google protobuf, version 2.4.1
* Pin, version 62732 or higher

## Make
First, you should replace the protobuf path in the profobuf.mk
Second, enter the following make commond in the RaceTrack root directory

    $ make PIN_ROOT=[pin_root_path]
