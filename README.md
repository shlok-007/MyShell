# MyShell
I coded it as a part of my Operating System Lab course's assignment. Hope you have fun trying it!

## Commands Implemented:

1. `myinfo` - gives username, OS name, machine architecture
2. `cd <path>` - changes directory to given path(relative or absolute) or to home directory if no path is given.
3. `history` - shows last 10 commands.
4. `exit` - exits the shell.
5. Complex commands with any valid number and combination of pipes, input and output redirection.
    - Note: 
        - (i) For input/output redirection, the input and/or output file name(s) should not precede any command argument(s).
            - Ex: `grep needle < haystack.txt`  ->  VALID
            - `grep < haystack.txt needle`  ->  INVALID (haystack.txt precedes needle(argument))
        - (ii) All commands should be separated by a space.
6. Background processes can be run by appending '&' at the end of the command. Ex: `sleep 10 &`.
    - Note: 
        - (i) Status of background processes can be seen by command: `jobs`.
        - (ii) Process can be foregrounded by command: `fg <job_id>`
        - (iii) Currently, you can't background a running foreground process.
7. All other commands available in UNIX shell.

## Usage

- It will only work on Unix-based systems.
- Execute the following commands to run it:
```shell
git clone https://github.com/shlok-007/MyShell.git
cd MyShell
gcc myShell.c -o myShell.out
./myShell.out
```