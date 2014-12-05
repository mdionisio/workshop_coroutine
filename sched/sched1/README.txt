BUILD:

mkdir debug
cd debug

cmake -DCMAKE_BUILD_TYPE=Debug ../

[per usare clang]
cmake -DCMAKE_C_COMPILER=clang-3.5

make

RUN:

./sched1

RESULT:

-> newtask ttid=1                                                                CREATED 1st TASK (main.c line: 488)
-> newtask ttid=2                                                                CREATED 2nd TASK (main.c line: 489)
-> newtask ttid=3                                                                CREATED 3rd TASK (main.c line: 490)
-> newtask ttid=4                                                                CREATED 4th TASK (main.c line: 491)
-> newtask ttid=5                                                                CREATED 5th TASK (main.c line: 492)


                                                                                 scheduler start (main.c line 496)
scheduler work on task ttid=1 status=2                                           (scheduler choose a task to be executed)
run_task tid=1 2                                                                 (starting task 1)
Hello t0 1! 1                                                                    (executing task 1)
                                                                                 task 1 suspend itself (main.c line: 366)
scheduler work on task ttid=2 status=2                                           (scheduler choose a task to be executed)
run_task tid=2 2                                                                 ...
Hello t1 1! 2
scheduler work on task ttid=3 status=2
run_task tid=3 2
Hello t2 1! 3
scheduler work on task ttid=4 status=2                                           (scheduler choose a task to be executed)
run_task tid=4 2
Hello t3 1! 4                                                                    this task is really terminated now (main.c line:431)
                                                                                 but return value is not called. The scheduler has to deschedule it
                                                                                 but there is a problem in status variable because it is marcked as RUNNING
                                                                                 but it is not managed from the scheduler. We will see that the sceduler
                                                                                 is able to manage this condition.

scheduler work on task ttid=5 status=2                                           (scheduler choose a task to be executed)
run_task tid=5 2
Hello t5 1! 5                                                                    task t5 call a subroutine (main.c line: 470)
function func1 start                                                             subroutine execute but it suspend the task (main.c line: 457)
scheduler work on task ttid=1 status=2                                           (scheduler choose a task to be executed, start egain from task 1)
run_task tid=1 2
Hello t0 2! 1
scheduler work on task ttid=2 status=2
run_task tid=2 2
Hello t1 2! 2
scheduler work on task ttid=3 status=2
run_task tid=3 2
Hello t2 2! 3
scheduler work on task ttid=4 status=0                                           (scheduler choose a task to be executed)
task ttid=4 terminate without call setReturnValue_task                           detected missing call of setReturnValue (main.c line:319)

scheduler work on task ttid=5 status=2
run_task tid=5 2
function func1 end
Hello t5 2! 5
scheduler work on task ttid=1 status=2
run_task tid=1 2
Hello t0 3! 1
scheduler work on task ttid=2 status=2
run_task tid=2 2
Hello t1 3! 2
scheduler work on task ttid=3 status=2
run_task tid=3 2
Hello t2 3! 3
-> newtask ttid=6                                                                 nice!! now it is created a new task dinamically
Hello t2 4! 3
scheduler work on task ttid=4 status=1
task terminated with return: 0 so remove it from availale task                    task 4 terminated
scheduler work on task ttid=5 status=1
task terminated with return: 1 so remove it from availale task                    task 5 terminated
scheduler work on task ttid=1 status=2
run_task tid=1 2
scheduler work on task ttid=2 status=1                                            task 2 terminated
task terminated with return: 1 so remove it from availale task
scheduler work on task ttid=6 status=2
run_task tid=6 2                                                                  new dynamic task is scheduled
Hello t4 0! 6                                                                     this is the new one
scheduler work on task ttid=3 status=2
run_task tid=3 2
scheduler work on task ttid=1 status=1
task terminated with return: 0 so remove it from availale task
scheduler work on task ttid=6 status=2
run_task tid=6 2
Hello t4 1! 6
scheduler work on task ttid=3 status=1
task terminated with return: 1 so remove it from availale task
scheduler work on task ttid=6 status=2
run_task tid=6 2
Hello t4 2! 6
scheduler work on task ttid=6 status=2
run_task tid=6 2
Hello t4 3! 6
scheduler work on task ttid=6 status=2
run_task tid=6 2
Hello t4 4! 6
scheduler work on task ttid=6 status=2
run_task tid=6 2
Hello t4 5! 6
scheduler work on task ttid=6 status=2
run_task tid=6 2
Hello t4 6! 6
scheduler work on task ttid=6 status=2
run_task tid=6 2
Hello t4 7! 6
scheduler work on task ttid=6 status=2
run_task tid=6 2
Hello t4 8! 6
scheduler work on task ttid=6 status=2
run_task tid=6 2
Hello t4 9! 6
scheduler work on task ttid=6 status=2
run_task tid=6 2
scheduler work on task ttid=6 status=1
task terminated with return: 1 so remove it from availale task
                                                                                    there is no other task available a the mainloop terminate (main.c line: 291)

