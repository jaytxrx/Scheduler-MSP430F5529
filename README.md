SchedulerMSP430F5529
====================

Multitask scheduler for MSP430F5529 launchpad - http://jeffrey.co.in/blog/2014/06/multitask-scheduler-for-msp430f5529-launchpad/

Compile the code using
```
msp430-gcc -mmcu=msp430f5529 -Os scheduler.c
```

Flash the launchpad using
```
sudo mspdebug tilib
```

Once you enter mspdebug, issue the below command
```
prog a.out
```
