# Agent Instructions

The repository of this warehouse is to port Micropython to Renesas RA8. Board dir: `ports/renesas-ra8`.

## Rule: Workspace Cleaning

1. All intermediate files generated from PDF, image, and document parsing must be placed in the system temporary directory %TEMP%\codex-artifacts\<task-name>\, unless I explicitly specify a different location.

## Rule: Edit File

1. For the markdown format: there is no need to follow the rule of approximately 80 characters per line.
2. For C/C++ files and makefile files: Macro Name: It should be arranged in the order of the ASCII code table.Unless certain macros should be grouped together in terms of their meaning, for example, some macros are the detailed configurations for the same module's specific functional aspects.

## Build

Reference: `ports/renesas-ra8/Build.md`. However, there is no need to reinstall the environment every time, because for developers, the corresponding environment should already be set up locally. If any problems occur during the construction process, you should first report to the developers instead of installing any software by yourself. For the construction of e2studio, instruct the developers on how to operate in the IDE rather than attempting to do it through the command line. Unless explicitly requested by the developer.