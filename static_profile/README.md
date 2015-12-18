**instrumented\_lines\_\*.out files contain instrumented line info.**

**static\_profile\_\*.out files contain potential raced stmt pair info.**

**loop\_range\_\*.out files contain each loop's start line and end line info.**

**exiting\_cond\_lines\_\*.out files contain each loop's exiting condtion lines,
which are spin read.**

**instrumented\_lines\_\*.out and static\_profile\_\*.out correspond to verifier\*.cpp
in test/verify dir.**

*	when use verify function, you should update the target in race_checker.sh

