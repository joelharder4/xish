# Xish Shell
### Assignment 1 | CIS\*3050 Systems Programming | Fall 2025

The name xish stands for "eXcellent Interprocess Communication Shell". Blame my professor for the horrible name.

This assignment was about writing a shell supporting inter-process communication (IPC) based on pipes and signals.

## Overview

This shell allows running of other programs in an environment that supports all of the following operations:
1) running processes in the foreground
2) pipe output between processes using the '|' separator
3) allow string variable substitution
4) support variable "globbing"
5) support the following "builtin" commands: `cd`, `pwd`, `intr` and `exit`
6) running processes in the background using the '&' operator
7) cancelling processes in the background -- can interrupt running background processes by sending them a signal when the command `intr` is supplied
8) respond to an interrupt signal (`^C`) by first cancelling all running using an interrupt signal and then exiting with a message

I describe more about my specific implementation in `IMPLEMENTATION.md`.

You can run any normal command that exists on your system, and it will search the PATH. I also provide two scripts, `timedCountdown` and `timedQuietCountdown` as examples that provide a task that takes a specific amount of time to perform.

## Testing Framework

There is a testing "framework" that consists of a series of test files that pipe into the shell, as well as the expected output for each one. Each test is named according to the pattern `test<N>.cmd`, the expected output for each test is named `test<N>.expected`.

You can run tests with `./runtests` and optionally add `-v` if you want to see the `diff` output for any tests that failed.

## Assumptions

This program assumes the following limits (to simplify memory management):

* The total length of an expanded line will never exceed 8kb
* The total number of expanded tokens on a line will never exceed 512
* The total number of variables will never exceed 128
* It is always valid to separate tokens by white space;
* Quotes can be ignored
